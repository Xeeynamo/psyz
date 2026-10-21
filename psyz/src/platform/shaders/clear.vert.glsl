#version 450

layout(location = 0) in ivec2 pos;  // SDL_GPU_VERTEXELEMENTFORMAT_SHORT2
layout(location = 1) in vec4 color; // SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM

layout(location = 0) out vec4 vertexColor;

layout(set = 1, binding = 0) uniform UBO { vec2 targetSize; };

void main() {
    float x = (float(pos.x) / (targetSize.x / 2.0)) - 1.0;
    float y = (float(pos.y) / (targetSize.y / 2.0)) - 1.0;
    gl_Position = vec4(x, -y, 0.0, 1.0);
    vertexColor = color;
}
