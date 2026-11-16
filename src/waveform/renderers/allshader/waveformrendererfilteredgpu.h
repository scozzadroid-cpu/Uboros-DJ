#pragma once

#include <QElapsedTimer>

#include "rendergraph/geometrynode.h"
#include "track/track_decl.h"
#include "util/class.h"
#include "waveform/renderers/allshader/waveformrenderersignalbase.h"
#include "waveform/waveform.h"

namespace allshader {
class WaveformRendererFilteredGpu;
} // namespace allshader

/// The filtered (three band, Pioneer style) waveform drawn by the GPU (QML
/// only). The waveform data is uploaded once into a texture, and a vertex
/// shader (waveformfiltered.vert) gives the rectangles of the pixel columns,
/// a geometry made once, their heights with the same formulas as
/// WaveformRendererFiltered. On every frame only a few uniforms change: no
/// geometry is built or uploaded, and the scene graph has nothing to sort
/// again. WaveformRendererFiltered built ~3 x 6 vertices per pixel column on
/// every frame (~0.5 MB per deck on a phone screen) and the scene graph
/// copied them into new GPU buffers.
///
/// Needs vertex texture fetch (OpenGL ES 3): QmlWaveformRendererFiltered
/// creates WaveformRendererFiltered when the GPU waveform is off.
class allshader::WaveformRendererFilteredGpu final
        : public allshader::WaveformRendererSignalBase,
          public rendergraph::GeometryNode {
  public:
    explicit WaveformRendererFilteredGpu(WaveformWidgetRenderer* waveformWidget,
            ::WaveformRendererSignalBase::Options options =
                    ::WaveformRendererSignalBase::Option::None);

    // Pure virtual from WaveformRendererSignalBase, not used
    void onSetup(const QDomNode& node) override;

    // Virtuals for rendergraph::Node
    void preprocess() override;

  private:
    bool preprocessInner();
    /// The rectangles of the axis and of the pixel columns, band by band
    void buildGeometry(int pixelLength);
    /// Uploads the waveform data if it changed (analysis in progress)
    bool updateTexture(const ConstWaveformPointer& pWaveform);

    int m_pixelLength;
    // The waveform data in the texture
    const Waveform* m_pTextureWaveform;
    int m_textureCompletion;
    int m_textureDataSize;
    int m_textureHeight;
    QElapsedTimer m_textureAge;

    DISALLOW_COPY_AND_ASSIGN(WaveformRendererFilteredGpu);
};
