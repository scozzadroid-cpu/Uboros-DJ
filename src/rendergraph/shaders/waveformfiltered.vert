#version 440

// The filtered (three band) waveform drawn by the GPU
// (allshader::WaveformRendererFilteredGpu). The geometry is made once: a
// rectangle per pixel column and band, plus the axis. This shader gives each
// rectangle its height from the waveform data in a texture, as
// WaveformRendererFiltered does on the CPU: the band maximum of the left
// channel above the axis, of the right channel below it. Only the uniforms
// change from frame to frame.

layout(std140, binding = 0) uniform buf {
    highp mat4 matrix;
    highp vec4 lowColor;
    highp vec4 midColor;
    highp vec4 highColor;
    highp vec4 axesColor;
    // The visual frame (left/right pair) at the first pixel column
    highp float firstFrame;
    highp float framesPerPixel;
    highp float frameCount;
    highp float textureWidth;
    highp float textureHeight;
    highp float halfBreadth;
    // allGain * halfBreadth: the data is normalized
    highp float heightFactor;
    highp float lowGain;
    highp float midGain;
    highp float highGain;
    highp float invDevicePixelRatio;
    highp float halfPixelSize;
    highp float waveformLength;
}
ubuf;

// The visual samples, left and right interleaved, row by row: low, mid,
// high and all in r, g, b and a
layout(binding = 1) uniform sampler2D waveformData;

// x: the pixel column, -1 for the axis; y and z: the corner (-1 or 1);
// w: the band (0 low, 1 mid, 2 high)
layout(location = 0) in highp vec4 vertexIn;
layout(location = 0) out highp vec3 vColor;

// More frames per pixel than this are not looked at (beyond the widest zoom)
const int kMaxFramesPerPixel = 64;

highp vec4 sampleAt(highp float index) {
    highp float row = floor(index / ubuf.textureWidth);
    highp float column = index - row * ubuf.textureWidth;
    return textureLod(waveformData,
            vec2((column + 0.5) / ubuf.textureWidth, (row + 0.5) / ubuf.textureHeight),
            0.0);
}

void main() {
    highp float column = vertexIn.x;
    highp vec2 corner = vertexIn.yz;
    if (column < 0.0) {
        vColor = ubuf.axesColor.rgb;
        gl_Position = ubuf.matrix *
                vec4(corner.x < 0.0 ? 0.0 : ubuf.waveformLength,
                        ubuf.halfBreadth + 0.5 * corner.y,
                        0.0,
                        1.0);
        return;
    }

    // The frames of the column, rounded like lround() in
    // WaveformRendererFiltered
    highp float frame = ubuf.firstFrame + column * ubuf.framesPerPixel;
    highp float range = ubuf.framesPerPixel * 0.5;
    highp float start = floor(frame - range + 0.5);
    highp float stop = max(floor(frame + range + 0.5), start + 1.0);
    highp vec3 left = vec3(0.0);
    highp vec3 right = vec3(0.0);
    for (int i = 0; i < kMaxFramesPerPixel; ++i) {
        highp float current = start + float(i);
        if (current >= stop) {
            break;
        }
        if (current >= 0.0 && current < ubuf.frameCount) {
            left = max(left, sampleAt(current * 2.0).rgb);
            right = max(right, sampleAt(current * 2.0 + 1.0).rgb);
        }
    }

    highp float band = vertexIn.w;
    highp float value;
    highp float gain;
    if (band < 0.5) {
        value = corner.y < 0.0 ? left.r : right.r;
        gain = ubuf.lowGain;
        vColor = ubuf.lowColor.rgb;
    } else if (band < 1.5) {
        value = corner.y < 0.0 ? left.g : right.g;
        gain = ubuf.midGain;
        vColor = ubuf.midColor.rgb;
    } else {
        value = corner.y < 0.0 ? left.b : right.b;
        gain = ubuf.highGain;
        vColor = ubuf.highColor.rgb;
    }

    gl_Position = ubuf.matrix *
            vec4(column * ubuf.invDevicePixelRatio + corner.x * ubuf.halfPixelSize,
                    ubuf.halfBreadth + corner.y * ubuf.heightFactor * gain * value,
                    0.0,
                    1.0);
}
