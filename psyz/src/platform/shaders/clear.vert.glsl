#version 450

layout(location = 0) in ivec2 pos;  // SDL_GPU_VERTEXELEMENTFORMAT_SHORT2
layout(location = 1) in vec4 color; // SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM

layout(location = 0) out vec4 vertexColor;

void main() {
    float x = (float(pos.x) / (1024.0 / 2.0)) - 1.0;
    float y = (float(pos.y) / (512.0 / 2.0)) - 1.0;
    gl_Position = vec4(x, -y, 0.0, 1.0);
    vertexColor = color;
}
