#!/bin/sh

# Compiles the SDL_GPU shaders to SPIR-V (Vulkan), MSL (Metal) and DXIL (D3D12)
# Requirements: glslangValidator (glslang), xxd, spirv-cross, dxc.

# DXIL/Windows requires signing. The best way to pre-compile the shader is
# C:\Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/dxc.exe
# running via Wine.

set -eu
cd "$(dirname "$0")"

if ! command -v spirv-cross > /dev/null 2>&1; then
    echo "build_shaders.sh: spirv-cross not found; needed for MSL/HLSL headers" >&2
    exit 1
fi

if ! command -v dxc > /dev/null 2>&1; then
    echo "build_shaders.sh: dxc not found; needed for DXIL (D3D12) headers" >&2
    exit 1
fi

emit_header() {
    src=$1        # intermediate file, e.g. psx.vert.spv
    name=$2       # canonical base, e.g. psx_vert
    suffix=$3     # spv, msl or dxil
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
    case "$stage" in
        vert) profile=vs_6_0 ;;
        frag) profile=ps_6_0 ;;
    esac
    spirv-cross --hlsl --shader-model 60 "$shader.spv" --output "$shader.hlsl"
    dxc -T "$profile" -E main "$shader.hlsl" -Fo "$shader.dxil"
    emit_header "$shader.dxil" "$name" dxil
    rm -f "$shader.spv" "$shader.metal" "$shader.hlsl" "$shader.dxil"
done
