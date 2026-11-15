#include "qml/qmlwaveformdisplay.h"

#include <QQuickWindow>
#include <QSGFlatColorMaterial>
#include <QSGSimpleRectNode>
#include <QSGVertexColorMaterial>
#include <Qt>
#include <QtQuick/QSGGeometryNode>
#include <QtQuick/QSGMaterial>
#include <QtQuick/QSGRectangleNode>
#include <QtQuick/QSGTexture>
#include <QtQuick/QSGTextureProvider>
#include <cmath>

#include "mixer/basetrackplayer.h"
#include "moc_qmlwaveformdisplay.cpp"
#include "qml/qmlplayerproxy.h"
#include "rendergraph/context.h"
#include "rendergraph/node.h"
#include "util/assert.h"

using namespace allshader;

namespace {
constexpr int kDefaultSyncInternalMs = 100;
// Frames without movement before switching to the idle redraw rate (~0.5 s)
constexpr int kStillFramesBeforeIdle = 30;
// Idle redraw interval: catches cue/loop/waveform changes of a paused deck
// and the start of playback (at most this delay before 60 fps again)
constexpr int kIdleRedrawIntervalMs = 100;
} // namespace

namespace mixxx {
namespace qml {

QmlWaveformDisplay::QmlWaveformDisplay(QQuickItem* parent)
        : QQuickItem(parent),
          WaveformWidgetRenderer(),
          m_syncInterval(kDefaultSyncInternalMs),
          m_pPlayer(nullptr) {
    setFlag(QQuickItem::ItemHasContents, true);

    connect(this,
            &QmlWaveformDisplay::windowChanged,
            this,
            &QmlWaveformDisplay::slotWindowChanged,
            Qt::DirectConnection);
    // Restart the redraw cycle when the waveform is shown again
    connect(this, &QQuickItem::visibleChanged, this, [this]() { update(); });
    m_idleTimer.setSingleShot(true);
    m_idleTimer.setInterval(kIdleRedrawIntervalMs);
    connect(&m_idleTimer, &QTimer::timeout, this, [this]() { update(); });
    slotWindowChanged(window());
}

QmlWaveformDisplay::~QmlWaveformDisplay() {
    // The stack contains references to Renderer that are owned and cleared by a BaseNode
    m_rendererStack.clear();
}

void QmlWaveformDisplay::componentComplete() {
    qDebug() << "QmlWaveformDisplay ready for group" << getGroup() << "with"
             << m_waveformRenderers.count() << "renderer(s)";
    QQuickItem::componentComplete();
}

void QmlWaveformDisplay::slotWindowChanged(QQuickWindow* window) {
    m_rendererStack.clear();

    m_dirtyFlag.setFlag(DirtyFlag::Window, true);
    if (window) {
        connect(window, &QQuickWindow::afterFrameEnd, this, &QmlWaveformDisplay::slotFrameSwapped);
    }
    m_timer.restart();
}

std::chrono::microseconds QmlWaveformDisplay::fromTimerToNextSync(const PerformanceTimer& timer) {
    // TODO @m0dB probably better to use a singleton instead of deriving QmlWaveformDisplay from
    // ISyncTimeProvider and have each keep track of this.
    return m_syncInterval + std::chrono::microseconds(m_timer.difference(timer).toIntegerMicros());
}

void QmlWaveformDisplay::slotFrameSwapped() {
    m_timer.restart();

    // Continuous redraw, but only while there is something to show. Redrawing
    // hidden or empty waveforms every frame keeps the whole window rendering
    // at the display refresh rate (120 Hz on many phones) for nothing.
    if (isVisible() && getTrackInfo()) {
        if (m_stillFrames.load(std::memory_order_relaxed) < kStillFramesBeforeIdle) {
            update();
        } else if (!m_idleTimer.isActive()) {
            // Paused deck: nothing moves, check again a few times per second
            m_idleTimer.start();
        }
    }
}

void QmlWaveformDisplay::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
    m_dirtyFlag.setFlag(DirtyFlag::Geometry, true);
    update();
    QQuickItem::geometryChange(newGeometry, oldGeometry);
}

QSGNode* QmlWaveformDisplay::updatePaintNode(QSGNode* node, UpdatePaintNodeData*) {
    if (m_dirtyFlag.testFlag(DirtyFlag::Window)) {
        delete node;
        node = nullptr;
        m_dirtyFlag.setFlag(DirtyFlag::Window, false);
    }

    DEBUG_ASSERT(!node || node->childCount() == 1);

    // No dynamic_cast here: on Android, dynamic_cast to Qt scene graph
    // classes (defined in libQt6Quick, another shared library) fails, so
    // the whole renderer tree (marks, textures, ...) was destroyed and
    // rebuilt on every frame. The node structure is ours: the clip node
    // has exactly one child, the background rectangle (a geometry node).
    auto* pClipNode = node && node->type() == QSGNode::ClipNodeType
            ? static_cast<QSGClipNode*>(node)
            : nullptr;
    auto* pBgNode = pClipNode && pClipNode->childCount() == 1 &&
                    pClipNode->lastChild()->type() == QSGNode::GeometryNodeType
            ? static_cast<QSGSimpleRectNode*>(pClipNode->lastChild())
            : nullptr;

    if (!pBgNode) {
        if (pClipNode) {
            delete pClipNode;
        }
        pClipNode = new QSGClipNode();
        pBgNode = new QSGSimpleRectNode();
        m_dirtyFlag.setFlag(DirtyFlag::Background, true);
        m_dirtyFlag.setFlag(DirtyFlag::Geometry, true);

        pClipNode->setIsRectangular(true);
        pClipNode->setGeometry(pBgNode->geometry());

        pClipNode->setClipRect(boundingRect());
        pBgNode->setRect(boundingRect());

        if (getContext()) {
            delete getContext();
        }
        setContext(new rendergraph::Context(window()));
        rendergraph::Node* pTopNode = new rendergraph::Node;

        m_rendererStack.clear();
        for (auto* pQmlRenderer : std::as_const(m_waveformRenderers)) {
            if (!pQmlRenderer->isSupported()) {
                qDebug() << "Ignoring the unsupported" << pQmlRenderer << "renderer";
            }
            auto renderer = pQmlRenderer->create(this);
#ifdef __STEM__
            VERIFY_OR_DEBUG_ASSERT(renderer.renderer) {
#else
            // It is expected for the stem renderer to return a null value in
            // case STEM are not enabled.
            if (!renderer.renderer) {
#endif
                continue;
            }
            addRenderer(renderer.renderer);
            pTopNode->appendChildNode(std::move(renderer.node));
        }

        pBgNode->appendChildNode(pTopNode);
        pClipNode->appendChildNode(pBgNode);
        init();
    }

    if (m_dirtyFlag.testFlag(DirtyFlag::Background)) {
        m_dirtyFlag.setFlag(DirtyFlag::Background, false);
        pBgNode->setColor(m_backgroundColor);
    }

    if (m_dirtyFlag.testFlag(DirtyFlag::Geometry)) {
        m_dirtyFlag.setFlag(DirtyFlag::Geometry, false);
        pBgNode->setRect(boundingRect());
        pClipNode->setClipRect(boundingRect());
        resizeRenderer(boundingRect().width(),
                boundingRect().height(),
                window()->devicePixelRatio());
    }

    onPreRender(this);

    {
        const int posVSample = getPlayPosVSample();
        const double zoom = getZoom();
        if (posVSample != m_lastPosVSample || zoom != m_lastZoom) {
            m_lastPosVSample = posVSample;
            m_lastZoom = zoom;
            m_stillFrames.store(0, std::memory_order_relaxed);
        } else {
            m_stillFrames.fetch_add(1, std::memory_order_relaxed);
        }
    }

    for (auto* pRenderer : std::as_const(m_rendererStack)) {
        pRenderer->update();
    }

    pBgNode->markDirty(QSGNode::DirtyForceUpdate);
    return pClipNode;
}

QmlPlayerProxy* QmlWaveformDisplay::getPlayer() const {
    return m_pPlayer;
}

void QmlWaveformDisplay::setPlayer(QmlPlayerProxy* pPlayer) {
    if (m_pPlayer == pPlayer) {
        return;
    }

    if (m_pPlayer != nullptr) {
        m_pPlayer->internalTrackPlayer()->disconnect(this);
    }

    m_pPlayer = pPlayer;

    if (m_pPlayer != nullptr) {
        setCurrentTrack(m_pPlayer->internalTrackPlayer()->getLoadedTrack());
        connect(m_pPlayer->internalTrackPlayer(),
                &BaseTrackPlayer::newTrackLoaded,
                this,
                &QmlWaveformDisplay::slotTrackLoaded);
        connect(m_pPlayer->internalTrackPlayer(),
                &BaseTrackPlayer::loadingTrack,
                this,
                &QmlWaveformDisplay::slotTrackLoading);
        connect(m_pPlayer->internalTrackPlayer(),
                &BaseTrackPlayer::playerEmpty,
                this,
                &QmlWaveformDisplay::slotTrackUnloaded);
    }

    emit playerChanged();
    update();
}

void QmlWaveformDisplay::setGroup(const QString& group) {
    if (getGroup() == group) {
        return;
    }

    WaveformWidgetRenderer::setGroup(group);
    emit groupChanged(group);
}

void QmlWaveformDisplay::slotTrackLoaded(TrackPointer pTrack) {
    // TODO: Investigate if it's a bug that this debug assertion fails when
    // passing tracks on the command line
    // DEBUG_ASSERT(m_pCurrentTrack == pTrack);
    setCurrentTrack(pTrack);
}

void QmlWaveformDisplay::slotTrackLoading(TrackPointer pNewTrack, TrackPointer pOldTrack) {
    Q_UNUSED(pOldTrack); // only used in DEBUG_ASSERT
    DEBUG_ASSERT(getTrackInfo() == pOldTrack);
    setCurrentTrack(pNewTrack);
}

void QmlWaveformDisplay::slotTrackUnloaded() {
    setCurrentTrack(nullptr);
}

void QmlWaveformDisplay::setCurrentTrack(TrackPointer pTrack) {
    m_stillFrames.store(0, std::memory_order_relaxed);
    auto pCurrentTrack = getTrackInfo();
    // TODO: Check if this is actually possible
    if (pCurrentTrack == pTrack) {
        return;
    }

    if (pCurrentTrack != nullptr) {
        disconnect(pCurrentTrack.get(), nullptr, this, nullptr);
    }

    setTrack(pTrack);
    if (pTrack != nullptr) {
        connect(pTrack.get(),
                &Track::waveformSummaryUpdated,
                this,
                &QmlWaveformDisplay::slotWaveformUpdated);
    }
    slotWaveformUpdated();
}

void QmlWaveformDisplay::slotWaveformUpdated() {
    m_stillFrames.store(0, std::memory_order_relaxed);
    update();
}

QQmlListProperty<QmlWaveformRendererFactory> QmlWaveformDisplay::renderers() {
    return {this,
            nullptr,
            &QmlWaveformDisplay::renderers_append,
            &QmlWaveformDisplay::renderers_count,
            &QmlWaveformDisplay::renderers_at,
            &QmlWaveformDisplay::renderers_clear};
}

// Static
void QmlWaveformDisplay::renderers_append(
        QQmlListProperty<QmlWaveformRendererFactory>* pList,
        QmlWaveformRendererFactory* value) {
    QmlWaveformDisplay* pWaveform = static_cast<QmlWaveformDisplay*>(pList->object);
    VERIFY_OR_DEBUG_ASSERT(pWaveform) {
        return;
    }
    pWaveform->m_dirtyFlag.setFlag(DirtyFlag::Window, true);
    pWaveform->m_waveformRenderers.append(value);
}

// Static
qsizetype QmlWaveformDisplay::renderers_count(QQmlListProperty<QmlWaveformRendererFactory>* pList) {
    QmlWaveformDisplay* pWaveform = static_cast<QmlWaveformDisplay*>(pList->object);
    VERIFY_OR_DEBUG_ASSERT(pWaveform) {
        return 0;
    }
    // Read-only access: must not mark the renderer stack dirty, otherwise all
    // renderers are destroyed and recreated on the next frame every time the
    // list is read (seen several times per second while loading tracks).
    return pWaveform->m_waveformRenderers.count();
}

// Static
QmlWaveformRendererFactory* QmlWaveformDisplay::renderers_at(
        QQmlListProperty<QmlWaveformRendererFactory>* pList, qsizetype index) {
    VERIFY_OR_DEBUG_ASSERT(pList && pList->object) {
        return nullptr;
    }
    QmlWaveformDisplay* pWaveform = static_cast<QmlWaveformDisplay*>(pList->object);
    // Read-only access: see renderers_count()
    return pWaveform->m_waveformRenderers.at(index);
}

// Static
void QmlWaveformDisplay::renderers_clear(QQmlListProperty<QmlWaveformRendererFactory>* pList) {
    QmlWaveformDisplay* pWaveform = static_cast<QmlWaveformDisplay*>(pList->object);
    VERIFY_OR_DEBUG_ASSERT(pWaveform) {
        return;
    }
    pWaveform->m_dirtyFlag.setFlag(DirtyFlag::Window, true);
    return pWaveform->m_waveformRenderers.clear();
}

} // namespace qml
} // namespace mixxx
