# From the repository root:
# docker buildx build --platform linux/amd64 \
#   -f samples/movie/Dockerfile \
#   --output type=local,dest=build/movie .

FROM debian:trixie-slim@sha256:a29215f6a35e51e22adffa17f89e9d2ef06214e64a2bad10d765c46aea49f11f AS tools

ENV DEBIAN_FRONTEND=noninteractive TZ=UTC LC_ALL=C.UTF-8
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates cmake ffmpeg gcc-mipsel-linux-gnu \
    git libavcodec-dev libavformat-dev libavutil-dev libswresample-dev \
    libswscale-dev meson ninja-build pkg-config python3 unzip wget xz-utils \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
ARG JOBS=8
ARG NUGGET_REVISION=020793ec2a3ad95509de2c0c52110ac53d757b0f
ARG PSYQ2ELF_REVISION=cba9b60d2f349a55e49530a28e01a10cae8771f1
RUN git init nugget \
    && git -C nugget remote add origin https://github.com/pcsx-redux/nugget.git \
    && git -C nugget fetch --depth 1 origin "$NUGGET_REVISION" \
    && git -C nugget checkout --detach FETCH_HEAD \
    && git clone https://gitlab.com/jype/psyq2elf.git decomp/bin/psyq2elf \
    && git -C decomp/bin/psyq2elf checkout --detach "$PSYQ2ELF_REVISION"

COPY Makefile ./
COPY bin/psyqlib.sh bin/psyqlib.sh
COPY decomp/sdk/Makefile decomp/sdk/Psy-Q_47.zip.sha256 decomp/sdk/
COPY decomp/bin/psyq-obj-parser.xz.sha256 decomp/bin/
RUN test "$(uname -m)" = x86_64 \
    && make -C decomp/bin/psyq2elf -j "$JOBS" \
    && cp decomp/bin/psyq2elf/psyqdump decomp/bin/psyqdump \
    && make -j "$JOBS" psyq_libs psyq_incs

COPY samples/movie/build_movie.py samples/movie/psxavenc-*.patch samples/movie/
RUN python3 samples/movie/build_movie.py --prepare-tools --jobs "$JOBS"

FROM tools AS disc
COPY psyz/include/ psyz/include/
COPY samples/movie/Makefile samples/movie/movie.c samples/movie/CREDITS.txt samples/movie/
COPY samples/movie/disc/disc.xml samples/movie/disc/SYSTEM.CNF samples/movie/disc/
ARG SOURCE=samples/movie/bunny.mp4
COPY ${SOURCE} /input/bunny.mp4
RUN python3 samples/movie/build_movie.py --source /input/bunny.mp4 \
    --prefix mipsel-linux-gnu --format elf32-tradlittlemips --jobs "$JOBS" \
    && mkdir /out \
    && cp samples/movie/disc/bunny.bin samples/movie/disc/bunny.cue \
          samples/movie/disc/bunny.str samples/movie/disc/bunny.lba \
          samples/movie/movie.ps-exe samples/movie/CREDITS.txt /out/ \
    && cp samples/movie/generated/manifest.json samples/movie/generated/*.log /out/

FROM scratch AS artifacts
COPY --from=disc /out/ /
