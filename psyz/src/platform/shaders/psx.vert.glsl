#version 450

layout(location = 0) in ivec2 pos;  // SDL_GPU_VERTEXELEMENTFORMAT_SHORT2
layout(location = 1) in uvec4 tex;  // SDL_GPU_VERTEXELEMENTFORMAT_USHORT4
layout(location = 2) in vec4 color; // SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM
layout(location = 3) in uvec4 twin; // SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4

layout(set = 1, binding = 0) uniform UBO { vec2 drawOffset; };

layout(location = 0) out vec4 vertexColor;
layout(location = 1) out vec2 rawUV;
layout(location = 2) flat out uint tpage;
layout(location = 3) flat out uint clut;
// Pre-computed pixel shader parameters
layout(location = 4) flat out uint textureMode;  // 0=untextured, 1=16-bit, 2=indexed
layout(location = 5) flat out uint subPixelMask; // Sub-pixel mask (8-bit:1, 4-bit:3)
layout(location = 6) flat out uint texelShift;   // Right shift for texel X
layout(location = 7) flat out uint indexShift;   // Shift for index extraction
layout(location = 8) flat out uint indexMask;    // Mask for color index
layout(location = 9) flat out uint dither;      // 1 when this primitive dithers
layout(location = 10) flat out ivec2 pageBase;   // texture page origin, in VRAM pixels
layout(location = 11) flat out uvec4 texWindow;  // GP0(E2h) as {and.xy, or.zw}

void main() {
    float x = ((float(pos.x) + drawOffset.x) / (1024.0 / 2.0)) - 1.0;
    float y = ((float(pos.y) + drawOffset.y) / (512.0 / 2.0)) - 1.0;
    // SDL_GPU NDC y=-1 is the bottom while texture row 0 is the top; negate Y
    // so VRAM row 0 lands on texture row 0, like the GL FBO convention.
    gl_Position = vec4(x, -y, 0.0, 1.0);
    // gouraud colors
    vertexColor = color;
    // select the right texture coords based on the tpage
    clut = tex.z;
    uint texWord = tex.w;
    tpage = texWord & 0x1FFu;
    dither = (texWord & 0x4000u) != 0u ? 1u : 0u;
    rawUV = vec2(tex.xy);
    // Determine texture mode and pre-compute parameters
    subPixelMask = 0u;
    texelShift = 0u;
    indexShift = 0u;
    indexMask = 0u;
    if ((texWord & 0x8000u) != 0u) {
        textureMode = 0u; // untextured
    } else if ((tpage & 0x180u) >= 0x100u) {
        textureMode = 1u; // 16-bit direct
        vertexColor.rgb *= 2.0;
    } else {
        textureMode = 2u; // indexed
        vertexColor.rgb *= 2.0;
        if ((tpage & 0x80u) != 0u) { // 8-bit indexed
            subPixelMask = 1u;
            texelShift = 1u;
            indexShift = 8u;
            indexMask = 0xFFu;
        } else { // 4-bit indexed
            subPixelMask = 3u;
            texelShift = 2u;
            indexShift = 4u;
            indexMask = 0xFu;
        }
    }
    pageBase = ivec2(int((tpage % 32u) % 16u) * 64, int((tpage % 32u) / 16u) * 256);
    texWindow = twin;
}
