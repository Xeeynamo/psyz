#version 450

layout(location = 0) in vec4 vertexColor;
layout(location = 1) in vec2 texCoord;
layout(location = 2) flat in uint tpage;
layout(location = 3) flat in uint clut;
layout(location = 4) flat in uint textureMode;
layout(location = 5) flat in float vramScaleX;
layout(location = 6) flat in uint subPixelMask;
layout(location = 7) flat in uint texelShift;
layout(location = 8) flat in uint indexShift;
layout(location = 9) flat in uint indexMask;

layout(set = 2, binding = 0) uniform sampler2D texVram;

layout(set = 3, binding = 0) uniform FragUBO { int dither; };

layout(location = 0) out vec4 FragColor;

uint rgb5551ToU16(vec4 c) {
    uint r = uint(c.r * 31.0 + 0.5);
    uint g = uint(c.g * 31.0 + 0.5);
    uint b = uint(c.b * 31.0 + 0.5);
    uint a = uint(c.a + 0.5);
    return r | (g << 5u) | (b << 10u) | (a << 15u);
}

const mat4 ditherMatrix = mat4(
    -4.0, +0.0, -3.0, +1.0,
    +2.0, -2.0, +3.0, -1.0,
    -3.0, +1.0, -4.0, +0.0,
    +3.0, -1.0, +2.0, -2.0);
vec3 applyDither(vec3 c) {
    if (dither == 0) return c;
    int dx = int(gl_FragCoord.x) & 3;
    int dy = int(gl_FragCoord.y) & 3;
    float off = ditherMatrix[dy][dx];
    vec3 c8 = floor(c * 255.0 + 0.5) + off;
    vec3 c5 = clamp(floor(c8 / 8.0), 0.0, 31.0);
    return c5 / 31.0;
}

void main() {
    vec4 texColor;
    if (textureMode == 0u) { // untextured
        texColor = vec4(1, 1, 1, 2);
    } else if (textureMode == 1u) { // 16-bit bitmap
        texColor = texture(texVram, texCoord);
    } else { // indexed
        vec2 vramCoord = texCoord * vec2(vramScaleX, 512.0);
        int pixelX = int(floor(vramCoord.x));
        uint subPixel = uint(pixelX) & subPixelMask;
        ivec2 texelPos = ivec2(pixelX >> texelShift, int(vramCoord.y));
        vec4 texel = texelFetch(texVram, texelPos, 0);
        uint word16 = rgb5551ToU16(texel);
        uint colorIdx = (word16 >> (subPixel * indexShift)) & indexMask;
        ivec2 clutBase = ivec2((clut % 64u) * 16u, clut / 64u);
        texColor = texelFetch(texVram, clutBase + ivec2(colorIdx, 0), 0);
    }
    // check for full transparency
    if (texColor == vec4(0, 0, 0, 0)) {
        FragColor = vec4(0);
        return;
    }
    // check for setSemiTrans(p, 1)
    bool isSemiTrans = vertexColor.a < 0.75;
    // when a color has the 0x8000 bit left then it has the semitrans flag on
    bool colorSemiTrans = texColor.a > 0;
    // PS1-accurate texture-color modulation: (tex5 * col8) >> 7, clamp to 31
    vec3 modColor;
    if (textureMode == 0u) {
        // untextured: PS1 uses color directly, no modulation
        modColor = texColor.rgb * vertexColor.rgb;
    } else {
        vec3 tex5 = floor(texColor.rgb * 31.0 + 0.5);
        vec3 col8 = min(floor(vertexColor.rgb * 127.5 + 0.5), vec3(255.0));
        modColor = min(floor(tex5 * col8 / 128.0), vec3(31.0)) / 31.0;
    }
    modColor = applyDither(modColor);
    // pre-multiplied alpha output for ONE, ONE_MINUS_SRC_ALPHA blending
    if (colorSemiTrans && isSemiTrans) {
        uint abr = (tpage & 0x60u) >> 5u;
        if (abr == 0u) {
            FragColor = vec4(modColor * 0.5, 0.5); // 50% blend
        } else if (abr == 1u) {
            FragColor = vec4(modColor, 0.0); // additive
        } else if (abr == 2u) {
            FragColor = vec4(modColor, 0.0); // subtractive
        } else {                                  // abr == 3u
            FragColor = vec4(modColor * 0.25, 0.0); // B + F/4
        }
    } else {
        FragColor = vec4(modColor, 1.0); // full opacity
    }
}
