#!/bin/sh
# Compiles the SDL_GPU shaders to SPIR-V (Vulkan/D3D12) and MSL (Metal)
# Requirements: glslangValidator (glslang), xxd, spirv-cross.
set -eu
cd "$(dirname "$0")"

if ! command -v spirv-cross > /dev/null 2>&1; then
    echo "build_shaders.sh: spirv-cross not found; needed for MSL (Metal) headers" >&2
    exit 1
fi

emit_header() {
    src=$1        # intermediate file, e.g. psx.vert.spv
    name=$2       # canonical base, e.g. psx_vert
    suffix=$3     # spv or msl
    {
        echo "// clang-format off"
        xxd -i "$src" | sed \
            -e "s/unsigned char [A-Za-z0-9_]*\[\]/unsigned char ${name}_${suffix}[]/" \
            -e "s/unsigned int [A-Za-z0-9_]*_len/unsigned int ${name}_${suffix}_len/"
    } > "${name}_${suffix}.h"
}

for shader in psx.vert psx.frag clear.vert clear.frag; do
    name=$(echo "$shader" | tr . _)
    stage=${shader##*.}
    glslangValidator -V --target-env vulkan1.0 -S "$stage" \
        "$shader.glsl" -o "$shader.spv"
    emit_header "$shader.spv" "$name" spv
    spirv-cross --msl "$shader.spv" --output "$shader.metal"
    emit_header "$shader.metal" "$name" msl
    rm -f "$shader.spv" "$shader.metal"
done
