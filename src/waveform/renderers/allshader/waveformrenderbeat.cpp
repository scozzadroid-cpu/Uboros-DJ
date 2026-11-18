#include "waveform/renderers/allshader/waveformrenderbeat.h"

#include <QDomNode>

#include "moc_waveformrenderbeat.cpp"
#include "rendergraph/geometry.h"
#include "rendergraph/material/unicolormaterial.h"
#include "rendergraph/vertexupdaters/vertexupdater.h"
#include "skin/legacy/skincontext.h"
#include "track/track.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "widget/wskincolor.h"

using namespace rendergraph;

namespace allshader {

WaveformRenderBeat::WaveformRenderBeat(WaveformWidgetRenderer* waveformWidget,
        ::WaveformRendererAbstract::PositionSource type)
        : ::WaveformRendererAbstract(waveformWidget),
          m_isSlipRenderer(type == ::WaveformRendererAbstract::Slip) {
    initForRectangles<UniColorMaterial>(0);
    setUsePreprocess(true);

    // Child node drawn after (above) the normal beat lines: bar starts (red).
    auto pNode = std::make_unique<GeometryNode>();
    m_pDownbeatNode = pNode.get();
    m_pDownbeatNode->initForRectangles<UniColorMaterial>(0);
    appendChildNode(std::move(pNode));
}

void WaveformRenderBeat::setup(const QDomNode& node, const SkinContext& skinContext) {
    m_color = QColor(skinContext.selectString(node, QStringLiteral("BeatColor")));
    m_color = WSkinColor::getCorrectColor(m_color).toRgb();
}

void WaveformRenderBeat::draw(QPainter* painter, QPaintEvent* event) {
    Q_UNUSED(painter);
    Q_UNUSED(event);
    DEBUG_ASSERT(false);
}

void WaveformRenderBeat::preprocess() {
    if (!preprocessInner()) {
        geometry().allocate(0);
        markDirtyGeometry();
        m_pDownbeatNode->geometry().allocate(0);
        m_pDownbeatNode->markDirtyGeometry();
    }
}

bool WaveformRenderBeat::preprocessInner() {
    const TrackPointer trackInfo = m_waveformRenderer->getTrackInfo();

    if (!trackInfo || (m_isSlipRenderer && !m_waveformRenderer->isSlipActive())) {
        return false;
    }

    auto positionType = m_isSlipRenderer ? ::WaveformRendererAbstract::Slip
                                         : ::WaveformRendererAbstract::Play;

    mixxx::BeatsPointer trackBeats = trackInfo->getBeats();
    if (!trackBeats) {
        return false;
    }

#ifndef __SCENEGRAPH__
    int alpha = m_waveformRenderer->getBeatGridAlpha();
    if (alpha == 0) {
        return false;
    }
    m_color.setAlphaF(alpha / 100.0f);
#endif

    if (!m_color.alpha()) {
        // Don't render the beatgrid lines is there are fully transparent
        return true;
    }

    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();

    const double trackSamples = m_waveformRenderer->getTrackSamples();
    if (trackSamples <= 0.0) {
        return false;
    }

    const double firstDisplayedPosition =
            m_waveformRenderer->getFirstDisplayedPosition(positionType);
    const double lastDisplayedPosition =
            m_waveformRenderer->getLastDisplayedPosition(positionType);

    const auto startPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            firstDisplayedPosition * trackSamples);
    const auto endPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            lastDisplayedPosition * trackSamples);

    if (!startPosition.isValid() || !endPosition.isValid()) {
        return false;
    }

    const float rendererBreadth = m_waveformRenderer->getBreadth();

    const int numVerticesPerLine = 6; // 2 triangles

    // Count the number of beats in the range to reserve space in the m_vertices vector.
    // Note that we could also use
    //   int numBearsInRange = trackBeats->numBeatsInRange(startPosition, endPosition);
    // for this, but there have been reports of that method failing with a DEBUG_ASSERT.
    // Bar start = every 4th beat counted from the first beat of the grid
    // (constant grids are anchored on a downbeat; for rekordbox imports the
    // anchor is the first PQTZ beat_number==1). Index is relative to the first
    // marker, computed once and then incremented.
    constexpr int kBeatsPerBar = 4;
    const auto firstIt = trackBeats->iteratorFrom(startPosition);
    int beatIndex = firstIt - trackBeats->cfirstmarker();

    int numBeatsInRange = 0;
    int numDownbeats = 0;
    {
        int idx = beatIndex;
        for (auto it = firstIt; it != trackBeats->cend() && *it <= endPosition; ++it, ++idx) {
            if (((idx % kBeatsPerBar) + kBeatsPerBar) % kBeatsPerBar == 0) {
                numDownbeats++;
            } else {
                numBeatsInRange++;
            }
        }
    }

    geometry().allocate(numBeatsInRange * numVerticesPerLine);
    m_pDownbeatNode->geometry().allocate(numDownbeats * numVerticesPerLine);

    VertexUpdater vertexUpdater{geometry().vertexDataAs<Geometry::Point2D>()};
    VertexUpdater downbeatUpdater{
            m_pDownbeatNode->geometry().vertexDataAs<Geometry::Point2D>()};

    const float height = m_isSlipRenderer ? rendererBreadth / 2 : rendererBreadth;
    for (auto it = firstIt; it != trackBeats->cend() && *it <= endPosition; ++it, ++beatIndex) {
        double beatPosition = it->toEngineSamplePos();
        double xBeatPoint =
                m_waveformRenderer->transformSamplePositionInRendererWorld(
                        beatPosition, positionType);

        xBeatPoint = qRound(xBeatPoint * devicePixelRatio) / devicePixelRatio;

        const float x1 = static_cast<float>(xBeatPoint);
        if (((beatIndex % kBeatsPerBar) + kBeatsPerBar) % kBeatsPerBar == 0) {
            // thick (3 logical px), full height, opaque red
            downbeatUpdater.addRectangle({x1 - 1.f, 0.f}, {x1 + 2.f, height});
        } else {
            vertexUpdater.addRectangle({x1, 0.f}, {x1 + 1.f, height});
        }
    }
    markDirtyGeometry();
    m_pDownbeatNode->markDirtyGeometry();
    m_pDownbeatNode->material().setUniform(1, m_downbeatColor);
    m_pDownbeatNode->markDirtyMaterial();

    DEBUG_ASSERT(numBeatsInRange * numVerticesPerLine == vertexUpdater.index());
    DEBUG_ASSERT(numDownbeats * numVerticesPerLine == downbeatUpdater.index());

    material().setUniform(1, m_color);
    markDirtyMaterial();

    return true;
}

} // namespace allshader
