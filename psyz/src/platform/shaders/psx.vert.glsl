#version 450

layout(location = 0) in ivec2 pos;  // SDL_GPU_VERTEXELEMENTFORMAT_SHORT2
layout(location = 1) in uvec4 tex;  // SDL_GPU_VERTEXELEMENTFORMAT_USHORT4
layout(location = 2) in vec4 color; // SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM

layout(set = 1, binding = 0) uniform UBO { vec2 drawOffset; };

layout(location = 0) out vec4 vertexColor;
layout(location = 1) out vec2 texCoord;
layout(location = 2) flat out uint tpage;
layout(location = 3) flat out uint clut;
// Pre-computed pixel shader parameters
layout(location = 4) flat out uint textureMode;  // 0=untextured, 1=16-bit, 2=indexed
layout(location = 5) flat out float vramScaleX;  // X scale for indexed modes
layout(location = 6) flat out uint subPixelMask; // Sub-pixel mask (8-bit:1, 4-bit:3)
layout(location = 7) flat out uint texelShift;   // Right shift for texel X
layout(location = 8) flat out uint indexShift;   // Shift for index extraction
layout(location = 9) flat out uint indexMask;    // Mask for color index

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
    texCoord = vec2(float(tex.x) / 4096.0, float(tex.y) / 512.0);
    // Determine texture mode and pre-compute parameters
    vramScaleX = 0.0;
    subPixelMask = 0u;
    texelShift = 0u;
    indexShift = 0u;
    indexMask = 0u;
    if ((texWord & 0x8000u) != 0u) {
        textureMode = 0u; // untextured
    } else if ((tpage & 0x180u) >= 0x100u) {
        textureMode = 1u; // 16-bit direct
        texCoord.x *= 4;
        vertexColor.rgb *= 2.0;
    } else {
        textureMode = 2u; // indexed
        vertexColor.rgb *= 2.0;
        if ((tpage & 0x80u) != 0u) { // 8-bit indexed
            texCoord.x *= 2;
            vramScaleX = 2048.0;
            subPixelMask = 1u;
            texelShift = 1u;
            indexShift = 8u;
            indexMask = 0xFFu;
        } else { // 4-bit indexed
            vramScaleX = 4096.0;
            subPixelMask = 3u;
            texelShift = 2u;
            indexShift = 4u;
            indexMask = 0xFu;
        }
    }
    vec2 page = vec2(
        float((tpage % 32u) % 16u) / 16.0,
        float((tpage % 32u) / 16u) / 2.0);
    texCoord += page;
}
