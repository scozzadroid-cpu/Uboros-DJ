#include "engine/cachingreader/cachingreaderworker.h"

#include <QAtomicInt>
#include <QFileInfo>
#include <QtDebug>
#include <algorithm>

#include "util/threadpriority.h"
#include "analyzer/analyzersilence.h"
#include "moc_cachingreaderworker.cpp"
#include "sources/soundsourceproxy.h"
#ifdef __STEM__
#include "sources/soundsourcestem.h"
#include "sources/stemsidecar.h"
#ifdef __STEM_SEPARATION__
#include "sources/audiosourceprogressivestemproxy.h"
#include "stems/htdemucs.h"
#include "stems/stemcache.h"
#include "stems/stemseparationservice.h"
#endif
#include "engine/engine.h"
#include "track/steminfoimporter.h"
#endif
#include "track/track.h"
#include "util/compatibility/qmutex.h"
#include "util/event.h"
#include "util/fifo.h"
#include "util/logger.h"
#include "util/span.h"

namespace {

mixxx::Logger kLogger("CachingReaderWorker");

// we need the last silence frame and the first sound frame
constexpr SINT kNumSoundFrameToVerify = 2;

// Re-opening after a failed read (see reopenAudioSource()). The waits
// between tries double from kReopenBackoffMinMillis up to
// kReopenBackoffMaxMillis: 8 real attempts cover ~30 s, enough for a USB
// stick to re-enumerate and for vold to mount it again.
constexpr int kMaxReopenAttempts = 8;
constexpr int kReopenBackoffMinMillis = 250;
constexpr int kReopenBackoffMaxMillis = 8000;

// The re-open functions only keep a weak reference: the worker must not
// extend the lifetime of a track beyond its deck.
std::function<mixxx::AudioSourcePointer()> openAudioSourceOfTrack(
        const TrackPointer& pTrack,
        const mixxx::AudioSource::OpenParams& config) {
    return [pWeakTrack = TrackWeakPointer(pTrack),
                   config]() -> mixxx::AudioSourcePointer {
        const TrackPointer pTrack = pWeakTrack.lock();
        if (!pTrack) {
            return nullptr;
        }
        return SoundSourceProxy(pTrack).openAudioSource(config);
    };
}

} // anonymous namespace

CachingReaderWorker::CachingReaderWorker(
        const QString& group,
        FIFO<CachingReaderChunkReadRequest>* pChunkReadRequestFIFO,
        FIFO<ReaderStatusUpdate>* pReaderStatusFIFO,
        mixxx::audio::ChannelCount maxSupportedChannel)
        : m_group(group),
          m_tag(QString("CachingReaderWorker %1").arg(m_group)),
          m_pChunkReadRequestFIFO(pChunkReadRequestFIFO),
          m_pReaderStatusFIFO(pReaderStatusFIFO),
          m_reopenAttempts(0),
          m_reopenBackoffMillis(0),
          m_maxSupportedChannel(maxSupportedChannel) {
}

ReaderStatusUpdate CachingReaderWorker::processReadRequest(
        const CachingReaderChunkReadRequest& request) {
    CachingReaderChunk* pChunk = request.chunk;
    DEBUG_ASSERT(pChunk);

    // Before trying to read any data we need to check if the audio source
    // is available and if any audio data that is needed by the chunk is
    // actually available.
    auto chunkFrameIndexRange = pChunk->frameIndexRange(m_pAudioSource);
    DEBUG_ASSERT(!m_pAudioSource ||
            chunkFrameIndexRange.isSubrangeOf(m_pAudioSource->frameIndexRange()));
    if (chunkFrameIndexRange.empty()) {
        ReaderStatusUpdate result;
        result.init(CHUNK_READ_INVALID, pChunk, m_pAudioSource ? m_pAudioSource->frameIndexRange() : mixxx::IndexRange());
        return result;
    }

#ifdef __STEM_SEPARATION__
    // Before reading: if the stems become ready meanwhile, the chunk is
    // simply read once more
    pChunk->setStemsReady(m_pStemCache && m_pStemCache->isChunkReady(pChunk->getIndex()));
#endif
    // Try to read the data required for the chunk from the audio source
    mixxx::IndexRange bufferedFrameIndexRange = pChunk->bufferSampleFrames(
            m_pAudioSource,
            mixxx::SampleBuffer::WritableSlice(m_tempReadBuffer));
    if (bufferedFrameIndexRange != chunkFrameIndexRange) {
        // Less than the source claims to hold. Assume a broken file handle
        // (I/O error on the USB stick), not the end of the audio: open the
        // file again and repeat the read.
        const mixxx::IndexRange firstAttemptFrameIndexRange = bufferedFrameIndexRange;
        if (reopenAudioSource()) {
            chunkFrameIndexRange = pChunk->frameIndexRange(m_pAudioSource);
            bufferedFrameIndexRange = chunkFrameIndexRange.empty()
                    ? mixxx::IndexRange()
                    : pChunk->bufferSampleFrames(m_pAudioSource,
                              mixxx::SampleBuffer::WritableSlice(m_tempReadBuffer));
            if (!bufferedFrameIndexRange.empty() &&
                    bufferedFrameIndexRange != chunkFrameIndexRange &&
                    bufferedFrameIndexRange == firstAttemptFrameIndexRange) {
                // A fresh decoder on a fresh file handle stopped at exactly
                // the same frame: this is about the content, not the
                // storage. Accept that the decodable audio ends here, so the
                // tail is not re-opened again and again (from BiteDJ).
                if (m_pAudioSource->shrinkReadableFrameIndexRange(
                            bufferedFrameIndexRange)) {
                    chunkFrameIndexRange = intersect(
                            chunkFrameIndexRange, m_pAudioSource->frameIndexRange());
                }
            }
        }
    }
    if (bufferedFrameIndexRange == chunkFrameIndexRange) {
        resetReopenState();
    }
    DEBUG_ASSERT(!m_pAudioSource ||
            bufferedFrameIndexRange.isSubrangeOf(m_pAudioSource->frameIndexRange()));
    // The readable frame range might have changed
    chunkFrameIndexRange = intersect(chunkFrameIndexRange, m_pAudioSource->frameIndexRange());
    DEBUG_ASSERT(bufferedFrameIndexRange.empty() ||
            bufferedFrameIndexRange.isSubrangeOf(chunkFrameIndexRange));

    ReaderStatus status = bufferedFrameIndexRange.empty() ? CHUNK_READ_EOF : CHUNK_READ_SUCCESS;
    if (bufferedFrameIndexRange != chunkFrameIndexRange) {
        kLogger.warning()
                << m_group
                << "Failed to read chunk samples for frame index range:"
                << "expected =" << chunkFrameIndexRange
                << ", actual =" << bufferedFrameIndexRange;
        if (bufferedFrameIndexRange.empty()) {
            status = CHUNK_READ_INVALID; // overwrite EOF (see above)
        }
    }

    // This call here assumes that the caching reader will read the first sound cue at
    // one of the first chunks. The check serves as a sanity check to ensure that the
    // sample data has not changed since it has ben analyzed. This could happen because
    // of a change in actual audio data or because the file was decoded using a different
    // decoder
    // This is part of a first prove of concept and needs to be replaces with a different
    // solution which is still under discussion. This might be also extended
    // to further checks whether a automatic offset adjustment is possible or a the
    // sample position metadata shall be treated as outdated.
    // Failures of the sanity check only result in an entry into the log at the moment.
    verifyFirstSound(pChunk, m_pAudioSource->getSignalInfo().getChannelCount());

    ReaderStatusUpdate result;
    result.init(status, pChunk, m_pAudioSource ? m_pAudioSource->frameIndexRange() : mixxx::IndexRange());
    return result;
}

// WARNING: Always called from a different thread (GUI)
#ifdef __STEM__
void CachingReaderWorker::newTrack(TrackPointer pTrack, mixxx::StemChannelSelection stemMask) {
#else
void CachingReaderWorker::newTrack(TrackPointer pTrack) {
#endif
    {
        const auto locker = lockMutex(&m_newTrackMutex);
#ifdef __STEM__
        m_pNewTrack = NewTrackRequest{
                pTrack,
                stemMask};
#else
        m_pNewTrack = pTrack;
#endif
        m_newTrackAvailable.storeRelease(1);
    }
    workReady();
}

void CachingReaderWorker::run() {
    // the id of this thread, for debugging purposes
    static auto lastId = QAtomicInt(0);
    const auto id = lastId.fetchAndAddRelaxed(1) + 1;
    QThread::currentThread()->setObjectName(
            QStringLiteral("CachingReaderWorker ") + QString::number(id));
    // This thread feeds the audio engine from the disk: it must win against
    // analysis, GUI and library work, otherwise playback runs dry.
    // (QThread::HighPriority is ignored on Linux/Android.)
    mixxx::setCurrentThreadNice(mixxx::kThreadNiceAudio);

    Event::start(m_tag);
    while (!m_stop.loadAcquire()) {
        // Request is initialized by reading from FIFO
        CachingReaderChunkReadRequest request;
        if (m_newTrackAvailable.loadAcquire()) {
#ifdef __STEM__
            NewTrackRequest pLoadTrack;
#else
            TrackPointer pLoadTrack;
#endif
            { // locking scope
                const auto locker = lockMutex(&m_newTrackMutex);
                pLoadTrack = m_pNewTrack;
                m_newTrackAvailable.storeRelease(0);
            } // implicitly unlocks the mutex
#ifdef __STEM__
            if (pLoadTrack.track) {
                // in this case the engine is still running with the old track
                loadTrack(pLoadTrack.track, pLoadTrack.stemMask);
#else
            if (pLoadTrack) {
                // in this case the engine is still running with the old track
                loadTrack(pLoadTrack);
#endif
            } else {
                // here, the engine is already stopped
                unloadTrack();
            }
        } else if (m_pChunkReadRequestFIFO->read(&request, 1) == 1) {
            // Read the requested chunk and send the result
            const ReaderStatusUpdate update = processReadRequest(request);
            m_pReaderStatusFIFO->writeBlocking(&update, 1);
        } else {
            Event::end(m_tag);
            m_semaRun.acquire();
            Event::start(m_tag);
        }
    }
}

void CachingReaderWorker::discardAllPendingRequests() {
    CachingReaderChunkReadRequest request;
    while (m_pChunkReadRequestFIFO->read(&request, 1) == 1) {
        const auto update = ReaderStatusUpdate::readDiscarded(request.chunk);
        m_pReaderStatusFIFO->writeBlocking(&update, 1);
    }
}

void CachingReaderWorker::closeAudioSource() {
    discardAllPendingRequests();

    if (m_pAudioSource) {
        // Closes open file handles of the old track.
        m_pAudioSource->close();
        m_pAudioSource.reset();
    }
    m_openAudioSource = nullptr;
    m_openedSignalInfo = mixxx::audio::SignalInfo();
    m_openedFrameIndexRange = mixxx::IndexRange();
    resetReopenState();

    // This function has to be called with the engine stopped only
    // to avoid collecting new requests for the old track
    DEBUG_ASSERT(!m_pChunkReadRequestFIFO->readAvailable());
}

void CachingReaderWorker::unloadTrack() {
    closeAudioSource();

    const auto update = ReaderStatusUpdate::trackUnloaded();
    m_pReaderStatusFIFO->writeBlocking(&update, 1);
}

#ifdef __STEM__
void CachingReaderWorker::loadTrack(
        const TrackPointer& pTrack, mixxx::StemChannelSelection stemMask) {
#else
void CachingReaderWorker::loadTrack(const TrackPointer& pTrack) {
#endif
    // This emit is directly connected and returns synchronized
    // after the engine has been stopped.
    emit trackLoading();

    closeAudioSource();

    if (!pTrack->getFileInfo().checkFileExists()) {
        kLogger.warning()
                << m_group
                << "File not found"
                << pTrack->getFileInfo();
        const auto update = ReaderStatusUpdate::trackUnloaded();
        m_pReaderStatusFIFO->writeBlocking(&update, 1);
        emit trackLoadFailed(pTrack,
                tr("The file '%1' could not be found.")
                        .arg(QDir::toNativeSeparators(pTrack->getLocation())));
        return;
    }

    mixxx::AudioSource::OpenParams config;
    config.setChannelCount(m_maxSupportedChannel);
#ifdef __STEM__
    config.setStemMask(stemMask);
#endif
#ifdef __STEM__
    // A track of a USB drive may have its stems prepared on the drive
    // (MIXXX/STEMS/<hash>.mxstem): play them instead of the file. Stem files
    // themselves import their stems from their own manifest.
    if (!mixxx::StemInfoImporter::maybeStemFile(pTrack->getLocation())) {
        const QString sidecar = mixxx::stemSidecarForTrack(pTrack->getLocation());
        QList<StemInfo> stemInfos;
        if (!sidecar.isEmpty()) {
            stemInfos = mixxx::StemInfoImporter::importStemInfos(sidecar);
            if (stemInfos.size() == mixxx::kMaxSupportedStems - 1) {
                // 3 stems (drums, bass + melody, vocals): SoundSourceSTEM plays
                // them as 4 with a silent third slot
                stemInfos.insert(2, StemInfo(QString(), QColor(Qt::darkGray)));
            }
            if (stemInfos.size() != mixxx::kMaxSupportedStems) {
                kLogger.warning() << m_group << "Ignoring stem sidecar" << sidecar
                                  << "with" << stemInfos.size() << "stems";
                stemInfos.clear();
            }
        }
        // Also clears stems left from a sidecar that is gone
        pTrack->setStemInfos(stemInfos);
        if (!stemInfos.isEmpty()) {
            const QUrl sidecarUrl = QUrl::fromLocalFile(sidecar);
            auto openSidecar = [pWeakTrack = TrackWeakPointer(pTrack),
                                       sidecarUrl,
                                       config]() -> mixxx::AudioSourcePointer {
                const TrackPointer pTrack = pWeakTrack.lock();
                if (!pTrack) {
                    return nullptr;
                }
                return SoundSourceProxy(pTrack,
                        sidecarUrl,
                        std::make_shared<mixxx::SoundSourceProviderSTEM>())
                        .openAudioSource(config);
            };
            m_pAudioSource = openSidecar();
            if (m_pAudioSource) {
                kLogger.info() << m_group << "Playing the stems of" << sidecar;
                m_openAudioSource = openSidecar;
            } else {
                kLogger.warning() << m_group << "Failed to open stem sidecar" << sidecar;
                pTrack->setStemInfos({});
            }
        }
    }
#ifdef __STEM_SEPARATION__
    // Instant stems: a track without stems in a main deck is played as a stem
    // track whose stems are computed on the device on request (SMART CFX):
    // until then slot 2 carries the whole mix and the deck sounds as usual
    std::shared_ptr<mixxx::stems::StemCache> pStemCache;
    const bool realStems = m_pAudioSource != nullptr ||
            mixxx::StemInfoImporter::maybeStemFile(pTrack->getLocation());
    if (!m_pAudioSource && !realStems &&
            m_maxSupportedChannel == mixxx::audio::ChannelCount::stem() &&
            !stemMask && mixxx::stems::instantStemsEnabled()) {
        mixxx::AudioSource::OpenParams stereoConfig;
        stereoConfig.setChannelCount(mixxx::audio::ChannelCount::stereo());
        auto openStereo = [pWeakTrack = TrackWeakPointer(pTrack),
                                  stereoConfig]() -> mixxx::AudioSourcePointer {
            const TrackPointer pTrack = pWeakTrack.lock();
            if (!pTrack) {
                return nullptr;
            }
            return SoundSourceProxy(pTrack).openAudioSource(stereoConfig);
        };
        auto pStereoSource = openStereo();
        if (pStereoSource &&
                pStereoSource->getSignalInfo().getChannelCount() ==
                        mixxx::audio::ChannelCount::stereo() &&
                pStereoSource->getSignalInfo().getSampleRate().value() ==
                        mixxx::stems::kSampleRate &&
                pStereoSource->frameIndexRange().start() == 0) {
            pStemCache = mixxx::stems::StemCache::open(pTrack->getLocation(),
                    pStereoSource->frameLength(),
                    mixxx::stems::kSampleRate);
        }
        if (pStemCache) {
            m_pAudioSource = std::make_shared<mixxx::AudioSourceProgressiveStemProxy>(
                    pStereoSource, pStemCache);
            m_openAudioSource = [openStereo, pStemCache]() -> mixxx::AudioSourcePointer {
                auto pSource = openStereo();
                if (!pSource ||
                        pSource->getSignalInfo().getChannelCount() !=
                                mixxx::audio::ChannelCount::stereo()) {
                    return nullptr;
                }
                return std::make_shared<mixxx::AudioSourceProgressiveStemProxy>(
                        std::move(pSource), pStemCache);
            };
            pTrack->setStemInfos(mixxx::stems::instantStemInfos());
            kLogger.info() << m_group << "Instant stems for" << pTrack->getLocation()
                           << pStemCache->readyChunkCount() << "/"
                           << pStemCache->chunkCount() << "chunks ready";
        } else {
            m_pAudioSource = pStereoSource;
            m_openAudioSource = openStereo;
        }
    }
    m_pPreviousStemCache = m_pStemCache;
    m_pStemCache = pStemCache;
    m_pStemCacheForEngine.store(pStemCache.get(), std::memory_order_release);
    mixxx::stems::StemSeparationService::deckLoaded(m_group, pTrack, pStemCache, realStems);
#endif
    if (!m_pAudioSource) {
        m_openAudioSource = openAudioSourceOfTrack(pTrack, config);
        m_pAudioSource = m_openAudioSource();
    }
#else
    m_openAudioSource = openAudioSourceOfTrack(pTrack, config);
    m_pAudioSource = m_openAudioSource();
#endif
    if (!m_pAudioSource) {
        kLogger.warning()
                << m_group
                << "Failed to open file"
                << pTrack->getFileInfo();
        const auto update = ReaderStatusUpdate::trackUnloaded();
        m_pReaderStatusFIFO->writeBlocking(&update, 1);
        emit trackLoadFailed(pTrack,
                tr("The file '%1' could not be loaded.")
                        .arg(QDir::toNativeSeparators(pTrack->getLocation())));
        return;
    }

    // It is critical that the audio source doesn't contain more channels than
    // requested as this could lead to overflow when reading chunks
    VERIFY_OR_DEBUG_ASSERT(m_pAudioSource->getSignalInfo().getChannelCount() >=
                    mixxx::audio::ChannelCount::mono() &&
            m_pAudioSource->getSignalInfo().getChannelCount() <=
                    m_maxSupportedChannel) {
        m_pAudioSource.reset(); // Close open file handles
        const auto update = ReaderStatusUpdate::trackUnloaded();
        m_pReaderStatusFIFO->writeBlocking(&update, 1);
        emit trackLoadFailed(pTrack,
                tr("The file '%1' could not be loaded because it contains %2 "
                   "channels, and only 1 to %3 are supported.")
                        .arg(QDir::toNativeSeparators(pTrack->getLocation()),
                                QString::number(m_pAudioSource->getSignalInfo()
                                                        .getChannelCount()),
                                QString::number(m_maxSupportedChannel)));
        return;
    }

    // Initially assume that the complete content offered by audio source
    // is available for reading. Later if read errors occur this value will
    // be decreased to avoid repeated reading of corrupt audio data.
    if (m_pAudioSource->frameIndexRange().empty()) {
        m_pAudioSource.reset(); // Close open file handles
        kLogger.warning()
                << m_group
                << "Failed to open empty file"
                << pTrack->getFileInfo();
        const auto update = ReaderStatusUpdate::trackUnloaded();
        m_pReaderStatusFIFO->writeBlocking(&update, 1);
        emit trackLoadFailed(pTrack,
                tr("The file '%1' is empty and could not be loaded.")
                        .arg(QDir::toNativeSeparators(pTrack->getLocation())));
        return;
    }

    // Adjust the internal buffer
    const SINT tempReadBufferSize =
            m_pAudioSource->getSignalInfo().frames2samples(
                    CachingReaderChunk::kFrames);
    if (m_tempReadBuffer.size() != tempReadBufferSize) {
        mixxx::SampleBuffer(tempReadBufferSize).swap(m_tempReadBuffer);
    }

    m_openedSignalInfo = m_pAudioSource->getSignalInfo();
    m_openedFrameIndexRange = m_pAudioSource->frameIndexRange();

    const auto update =
            ReaderStatusUpdate::trackLoaded(
                    m_pAudioSource->frameIndexRange());
    m_pReaderStatusFIFO->writeBlocking(&update, 1);

    // Emit that the track is loaded.

    // This code is a workaround until we have found a better solution to
    // verify and correct offsets.
    CuePointer pN60dBSound =
            pTrack->findCueByType(mixxx::CueType::N60dBSound);
    if (pN60dBSound) {
        m_firstSoundFrameToVerify = pN60dBSound->getPosition();
    }

    // The engine must not request any chunks before receiving the
    // trackLoaded() signal
    DEBUG_ASSERT(!m_pChunkReadRequestFIFO->readAvailable());

    emit trackLoaded(
            pTrack,
            m_pAudioSource->getSignalInfo().getSampleRate(),
            m_pAudioSource->getSignalInfo().getChannelCount(),
            mixxx::audio::FramePos(m_pAudioSource->frameLength()));
}

void CachingReaderWorker::resetReopenState() {
    m_reopenAttempts = 0;
    m_reopenBackoffMillis = 0;
    m_reopenBackoffTimer.invalidate();
}

bool CachingReaderWorker::reopenAudioSource() {
    if (!m_pAudioSource || !m_openAudioSource ||
            m_reopenAttempts >= kMaxReopenAttempts) {
        return false;
    }
    if (m_reopenBackoffTimer.isValid() &&
            m_reopenBackoffTimer.elapsed() < m_reopenBackoffMillis) {
        return false;
    }
    // The first try of a burst runs immediately, later ones wait longer
    m_reopenBackoffMillis = m_reopenBackoffTimer.isValid()
            ? std::min(m_reopenBackoffMillis * 2, kReopenBackoffMaxMillis)
            : kReopenBackoffMinMillis;
    m_reopenBackoffTimer.start();

    const QString fileName = m_pAudioSource->getUrl().toLocalFile();
    if (!QFileInfo::exists(fileName)) {
        // The stick is gone (or not mounted again yet). A stat is cheap,
        // so this does not use up an attempt: it is tried again every few
        // seconds until the file is back.
        return false;
    }
    ++m_reopenAttempts;

    // Open into a local first: on failure the deck keeps its source
    mixxx::AudioSourcePointer pAudioSource = m_openAudioSource();
    if (!pAudioSource) {
        kLogger.warning() << m_group << "Failed to re-open" << fileName
                          << "attempt" << m_reopenAttempts << "of" << kMaxReopenAttempts;
        return false;
    }
    if (pAudioSource->getSignalInfo() != m_openedSignalInfo ||
            pAudioSource->frameIndexRange() != m_openedFrameIndexRange) {
        // Same path, other content: all positions of the deck would be wrong
        kLogger.warning() << m_group << "Re-opened file has changed:" << fileName
                          << pAudioSource->getSignalInfo() << pAudioSource->frameIndexRange()
                          << "instead of" << m_openedSignalInfo << m_openedFrameIndexRange;
        pAudioSource->close();
        m_reopenAttempts = kMaxReopenAttempts; // don't try this file again
        return false;
    }
    kLogger.info() << m_group << "Re-opened" << fileName << "after a failed read,"
                   << "attempt" << m_reopenAttempts << "of" << kMaxReopenAttempts;
    m_pAudioSource->close();
    m_pAudioSource = std::move(pAudioSource);
    return true;
}

void CachingReaderWorker::quitWait() {
    m_stop = 1;
    m_semaRun.release();
    wait();
}

void CachingReaderWorker::verifyFirstSound(const CachingReaderChunk* pChunk,
        mixxx::audio::ChannelCount channelCount) {
    if (!m_firstSoundFrameToVerify.isValid()) {
        return;
    }

    const int firstSoundIndex =
            CachingReaderChunk::indexForFrame(static_cast<SINT>(
                    m_firstSoundFrameToVerify.toLowerFrameBoundary()
                            .value()));
    if (pChunk->getIndex() == firstSoundIndex) {
        mixxx::SampleBuffer sampleBuffer(kNumSoundFrameToVerify * channelCount);
        SINT end = static_cast<SINT>(m_firstSoundFrameToVerify.toLowerFrameBoundary().value());
        pChunk->readBufferedSampleFrames(sampleBuffer.data(),
                channelCount,
                mixxx::IndexRange::forward(end - 1, kNumSoundFrameToVerify));
        if (AnalyzerSilence::verifyFirstSound(sampleBuffer.span(),
                    mixxx::audio::FramePos(1),
                    channelCount)) {
            qDebug() << "First sound found at the previously stored position";
        } else {
            // This can happen in case of track edits or replacements, changed
            // encoders or encoding issues.
            qWarning() << "First sound has been moved! The beatgrid and "
                          "other annotations are no longer valid"
                       << m_pAudioSource->getUrlString();
        }
        m_firstSoundFrameToVerify = mixxx::audio::FramePos();
    }
}
