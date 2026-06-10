#/bin/sh
# Link from inside build/ so the object paths embedded in the ELF string
# table stay 'obj/$1/...': the ELF metadata is part of the splat target.
# --build-id=none keeps distro-default build-id notes out of the image.
# LC_ALL=C sort -d keeps the link order independent of the host locale.
cd build
mipsel-linux-gnu-gcc -O0 -g -o psyq$1_org.elf -T ../psyq.ld \
    -fno-toplevel-reorder -nostartfiles -nostdlib \
    -Wl,--whole-archive -Wl,--build-id=none \
    -Wl,-Map=psyq$1_org.map \
    $(find obj/$1/lib* -name '*.o' | LC_ALL=C sort -d)
