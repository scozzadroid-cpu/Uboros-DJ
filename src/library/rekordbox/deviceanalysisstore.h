#pragma once

#include <QString>

#include "preferences/configobject.h"
#include "track/track_decl.h"

class AnalysisDao;

/// Keeps the Mixxx analysis of the tracks of a removable device ON the device
/// ("the analysis must stay on the medium the audio file comes from"), so that
/// the device can be used on another phone without analyzing again; Mixxx's
/// own database is only a cache.
///
/// One file per track in <device root>/MIXXX/ANLZ/<hash of the relative
/// path>.mxa, next to (never inside) the PIONEER folder, so the device stays
/// readable by rekordbox and the CDJs (they ignore unknown folders and only
/// list audio files). A file per track instead of a database: on FAT32 an
/// interrupted write damages one track at most, and every write is atomic
/// (temporary file + rename).
///
/// Content: BPM/beatgrid, key, cues/loops/hotcues, waveform and waveform
/// summary, plus the size and modification time of the audio file: a sidecar
/// that does not match the file any more is ignored.
///
/// All functions must be called from the main thread (they use the main
/// database connection through AnalysisDao).
namespace mixxx::deviceanalysis {

/// Analyze all the tracks of a USB drive in the background when it is
/// opened (default on)
inline ConfigKey preanalysisConfigKey() {
    return ConfigKey(QStringLiteral("[Library]"), QStringLiteral("DevicePreanalysis"));
}

/// Store the analyses of a drive's tracks on the drive itself (default on)
inline ConfigKey storeOnDeviceConfigKey() {
    return ConfigKey(QStringLiteral("[Library]"), QStringLiteral("DeviceAnalysisStore"));
}

/// The root of the removable device holding the file (the directory that
/// contains PIONEER/), or an empty string when there is none.
QString deviceRootForFile(const QString& location);

/// Imports the analysis stored on the device into the track and into
/// Mixxx's analysis cache. Returns true when a valid sidecar was applied.
/// Does nothing when the cache already has the waveform of this track.
bool importIfAvailable(const TrackPointer& pTrack, AnalysisDao& analysisDao);

/// True when the track has an analysis (beats and waveform) that is not on
/// the device yet or differs from the one on the device.
bool needsExport(const TrackPointer& pTrack, AnalysisDao& analysisDao);

enum class ExportResult {
    Written,
    Unchanged,
    NotOnDevice,
    NothingToExport,
    ReadOnly,
    Failed,
};

/// Writes (atomically) the analysis of the track to its device.
ExportResult exportAnalysis(const TrackPointer& pTrack, AnalysisDao& analysisDao);

/// The two halves of exportAnalysis(), so that the slow part (comparing with
/// the file on the drive and writing it) can run outside the GUI thread.
struct ExportJob {
    ExportResult result = ExportResult::NothingToExport; // final unless content is set
    QString root;
    QString relPath;
    QString path;
    QByteArray content;
};
/// GUI thread (library database): builds the sidecar content
ExportJob prepareExport(const TrackPointer& pTrack, AnalysisDao& analysisDao);
/// Any thread: compares with the file on the drive and writes it
ExportResult writeExport(const ExportJob& job);

} // namespace mixxx::deviceanalysis
