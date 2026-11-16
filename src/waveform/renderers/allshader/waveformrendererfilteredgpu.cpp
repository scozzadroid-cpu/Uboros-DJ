#include "waveform/renderers/allshader/waveformrendererfilteredgpu.h"

#include <QImage>
#include <QMatrix4x4>
#include <QSGMaterial>
#include <QSGTexture>
#include <QVector4D>

#include "rendergraph/material.h"
#include "rendergraph/materialshader.h"
#include "rendergraph/materialtype.h"
#include "rendergraph/texture.h"
#include "rendergraph/uniformset.h"
#include "track/track.h"
#include "waveform/renderers/allshader/waveformgpudata.h"
#include "waveform/renderers/waveformwidgetrenderer.h"

using namespace rendergraph;

namespace {

/// The uniforms of waveformfiltered.vert, in the order of its uniform block
enum Uniform {
    kMatrix,
    kLowColor,
    kMidColor,
    kHighColor,
    kAxesColor,
    kFirstFrame,
    kFramesPerPixel,
    kFrameCount,
    kTextureWidth,
    kTextureHeight,
    kHalfBreadth,
    kHeightFactor,
    kLowGain,
    kMidGain,
    kHighGain,
    kInvDevicePixelRatio,
    kHalfPixelSize,
    kLength,
};

class WaveformFilteredGpuMaterial : public Material {
  public:
    WaveformFilteredGpuMaterial()
            : Material(uniforms()) {
        // The vertex shader computes the positions: the scene graph must not
        // merge this geometry with others, transforming it on the CPU
        setFlag(QSGMaterial::RequiresFullMatrix);
    }

    static const AttributeSet& attributes() {
        static AttributeSet set = makeAttributeSet<QVector4D>({"vertexIn"});
        return set;
    }

    static const UniformSet& uniforms() {
        static UniformSet set = makeUniformSet<QMatrix4x4,
                QVector4D,
                QVector4D,
                QVector4D,
                QVector4D,
                float,
                float,
                float,
                float,
                float,
                float,
                float,
                float,
                float,
                float,
                float,
                float,
                float>({"ubuf.matrix",
                "ubuf.lowColor",
                "ubuf.midColor",
                "ubuf.highColor",
                "ubuf.axesColor",
                "ubuf.firstFrame",
                "ubuf.framesPerPixel",
                "ubuf.frameCount",
                "ubuf.textureWidth",
                "ubuf.textureHeight",
                "ubuf.halfBreadth",
                "ubuf.heightFactor",
                "ubuf.lowGain",
                "ubuf.midGain",
                "ubuf.highGain",
                "ubuf.invDevicePixelRatio",
                "ubuf.halfPixelSize",
                "ubuf.waveformLength"});
        return set;
    }

    MaterialType* type() const override {
        static MaterialType type;
        return &type;
    }

    std::unique_ptr<MaterialShader> createShader() const override {
        return std::make_unique<MaterialShader>("waveformfiltered.vert",
                "waveformfiltered.frag",
                uniforms(),
                attributes());
    }

    Texture* texture(int /*binding*/) const override {
        return m_pTexture.get();
    }

    void setTexture(std::unique_ptr<Texture> pTexture) {
        m_pTexture = std::move(pTexture);
    }

  private:
    std::unique_ptr<Texture> m_pTexture;
};

struct ColumnVertex {
    QVector4D vertex;
};

/// The two triangles of a rectangle of a column (-1 for the axis) and band
void addRectangle(ColumnVertex* pVertices, float column, float band) {
    constexpr float kCorners[6][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, -1}, {-1, 1}, {1, 1}};
    for (int i = 0; i < 6; ++i) {
        pVertices[i].vertex = QVector4D(column, kCorners[i][0], kCorners[i][1], band);
    }
}

constexpr int kBands = 3;
constexpr int kVerticesPerRectangle = 6;

} // namespace

namespace allshader {

WaveformRendererFilteredGpu::WaveformRendererFilteredGpu(
        WaveformWidgetRenderer* waveformWidget, ::WaveformRendererSignalBase::Options options)
        : WaveformRendererSignalBase(waveformWidget, options),
          m_pixelLength(-1),
          m_pTextureWaveform(nullptr),
          m_textureCompletion(-1),
          m_textureDataSize(0),
          m_textureHeight(1) {
    setGeometry(std::make_unique<Geometry>(WaveformFilteredGpuMaterial::attributes(), 0));
    setMaterial(std::make_unique<WaveformFilteredGpuMaterial>());
    geometry().setDrawingMode(Geometry::DrawingMode::Triangles);
    setUsePreprocess(true);
}

void WaveformRendererFilteredGpu::onSetup(const QDomNode&) {
}

void WaveformRendererFilteredGpu::preprocess() {
    if (!preprocessInner()) {
        if (geometry().vertexCount() != 0) {
            geometry().allocate(0);
            markDirtyGeometry();
            m_pixelLength = -1;
        }
    }
}

void WaveformRendererFilteredGpu::buildGeometry(int pixelLength) {
    // The axis first, then the bands in the order of WaveformRendererFiltered
    // (the high band on top)
    geometry().allocate(kVerticesPerRectangle * (kBands * pixelLength + 1));
    auto* pVertices = geometry().vertexDataAs<ColumnVertex>();
    addRectangle(pVertices, -1.f, 0.f);
    pVertices += kVerticesPerRectangle;
    for (int band = 0; band < kBands; ++band) {
        for (int column = 0; column < pixelLength; ++column) {
            addRectangle(pVertices, static_cast<float>(column), static_cast<float>(band));
            pVertices += kVerticesPerRectangle;
        }
    }
    markDirtyGeometry();
    m_pixelLength = pixelLength;
}

bool WaveformRendererFilteredGpu::updateTexture(const ConstWaveformPointer& pWaveform) {
    const int dataSize = pWaveform->getDataSize();
    const int completion = pWaveform->getCompletion();
    auto& material = static_cast<WaveformFilteredGpuMaterial&>(this->material());
    const bool sameData = pWaveform.get() == m_pTextureWaveform &&
            dataSize == m_textureDataSize && material.texture(0);
    if (sameData &&
            (completion == m_textureCompletion ||
                    m_textureAge.elapsed() < waveformgpudata::kAnalysisUploadMillis)) {
        return true;
    }
    rendergraph::Context* pContext = m_waveformRenderer->getContext();
    if (!pContext) {
        return false;
    }
    auto pTexture = std::make_unique<Texture>(pContext,
            waveformgpudata::image(*pWaveform, waveformgpudata::Content::Bands));
    // Exact values, not mixed with the neighbours
    pTexture->backendTexture()->setFiltering(QSGTexture::Nearest);
    pTexture->backendTexture()->setMipmapFiltering(QSGTexture::None);
    material.setTexture(std::move(pTexture));
    markDirtyMaterial();

    m_pTextureWaveform = pWaveform.get();
    m_textureCompletion = completion;
    m_textureDataSize = dataSize;
    m_textureHeight = waveformgpudata::textureHeight(dataSize);
    m_textureAge.start();
    return true;
}

bool WaveformRendererFilteredGpu::preprocessInner() {
    const TrackPointer pTrack = m_waveformRenderer->getTrackInfo();
    if (!pTrack) {
        return false;
    }
    const ConstWaveformPointer pWaveform = pTrack->getWaveform();
    if (pWaveform.isNull()) {
        return false;
    }
    const int dataSize = pWaveform->getDataSize();
    if (dataSize <= 1 || pWaveform->data() == nullptr) {
        return false;
    }
#ifdef __STEM__
    // The stem renderer draws stem tracks
    if (!pTrack->getStemInfo().isEmpty() && pWaveform->hasStem() && !m_ignoreStem) {
        return false;
    }
#endif

    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();
    const int length = static_cast<int>(m_waveformRenderer->getLength());
    const int pixelLength = static_cast<int>(length * devicePixelRatio);
    if (pixelLength <= 0) {
        return false;
    }

    // As in WaveformRendererFiltered
    const int visualFramesSize = dataSize / 2;
    const double firstVisualFrame =
            m_waveformRenderer->getFirstDisplayedPosition() * visualFramesSize;
    const double lastVisualFrame =
            m_waveformRenderer->getLastDisplayedPosition() * visualFramesSize;
    const double visualIncrementPerPixel =
            (lastVisualFrame - firstVisualFrame) / static_cast<double>(pixelLength);
    if (visualIncrementPerPixel == 0.0) {
        return false;
    }
    const double xVisualFrame =
            qRound(firstVisualFrame / visualIncrementPerPixel) * visualIncrementPerPixel;

    float allGain = 1.0f;
    float lowGain = 1.0f;
    float midGain = 1.0f;
    float highGain = 1.0f;
    getGains(&allGain, &lowGain, &midGain, &highGain);
    const float halfBreadth = static_cast<float>(m_waveformRenderer->getBreadth()) / 2.0f;

    if (!updateTexture(pWaveform)) {
        return false;
    }
    if (pixelLength != m_pixelLength) {
        buildGeometry(pixelLength);
    }

    Material& material = this->material();
    material.setUniform(kLowColor,
            QVector4D(static_cast<float>(m_lowColor_r),
                    static_cast<float>(m_lowColor_g),
                    static_cast<float>(m_lowColor_b),
                    1.f));
    material.setUniform(kMidColor,
            QVector4D(static_cast<float>(m_midColor_r),
                    static_cast<float>(m_midColor_g),
                    static_cast<float>(m_midColor_b),
                    1.f));
    material.setUniform(kHighColor,
            QVector4D(static_cast<float>(m_highColor_r),
                    static_cast<float>(m_highColor_g),
                    static_cast<float>(m_highColor_b),
                    1.f));
    material.setUniform(kAxesColor,
            QVector4D(static_cast<float>(m_axesColor_r),
                    static_cast<float>(m_axesColor_g),
                    static_cast<float>(m_axesColor_b),
                    1.f));
    material.setUniform(kFirstFrame, static_cast<float>(xVisualFrame));
    material.setUniform(kFramesPerPixel, static_cast<float>(visualIncrementPerPixel));
    material.setUniform(kFrameCount, static_cast<float>(visualFramesSize));
    material.setUniform(kTextureWidth, static_cast<float>(waveformgpudata::kTextureWidth));
    material.setUniform(kTextureHeight, static_cast<float>(m_textureHeight));
    material.setUniform(kHalfBreadth, halfBreadth);
    // The texture holds value / 255, WaveformRendererFiltered divides by
    // m_maxValue (255) too
    material.setUniform(kHeightFactor, allGain * halfBreadth);
    material.setUniform(kLowGain, lowGain);
    material.setUniform(kMidGain, midGain);
    material.setUniform(kHighGain, highGain);
    material.setUniform(kInvDevicePixelRatio, 1.f / devicePixelRatio);
    material.setUniform(kHalfPixelSize, 0.5f / devicePixelRatio);
    material.setUniform(kLength, static_cast<float>(length));
    markDirtyMaterial();
    return true;
}

} // namespace allshader
