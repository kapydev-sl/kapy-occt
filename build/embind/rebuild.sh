#!/bin/sh
# engine/kernels/occt/build/embind/rebuild.sh
#
# Links the bindings (including the C API, bindings/kapy_capi.h) against the
# OCCT toolkits of the builder image and prints the sha256 of the result. With
# `--twice` it links a second time from scratch and fails unless both links
# give the same bytes (the determinism the kernel promises).
#
#   engine/kernels/occt/build/embind/rebuild.sh [--twice] [--publish]
#
# `--publish` copies kapy-occt.{js,wasm} to engine/kernels/occt/dist/. A kernel
# binary is committed only at the checkpoints the plan names (D175); the rest
# of the time it is published locally and stays out of the commit.
#
# Needs the image: docker build --platform linux/amd64 -t kapy-occt-builder:wasm-eh \
#   engine/kernels/occt/build/embind
set -eu

here=$(cd "$(dirname "$0")" && pwd)
image=${KAPY_OCCT_BUILDER:-kapy-occt-builder:wasm-eh}
out=${KAPY_OCCT_LINK_OUT:-${TMPDIR:-/tmp}/kapy-occt-link}

link() {
    dir=$1
    rm -rf "$dir"
    mkdir -p "$dir"
    # The source is mounted read-only and the build tree is a scratch volume,
    # so a link never writes into the checkout.
    docker run --rm --platform linux/amd64 \
        -v "$here:/work:ro" -v "$dir:/out" -w /work "$image" \
        sh -c 'emcmake cmake -S /work -B /tmp/build -G Ninja >/dev/null && cmake --build /tmp/build -j && cp /tmp/build/kapy-occt.js /tmp/build/kapy-occt.wasm /out/'
    shasum -a 256 "$dir/kapy-occt.wasm" "$dir/kapy-occt.js"
}

link "$out/a"
if [ "${1:-}" = "--twice" ] || [ "${2:-}" = "--twice" ]; then
    link "$out/b"
    cmp "$out/a/kapy-occt.wasm" "$out/b/kapy-occt.wasm"
    cmp "$out/a/kapy-occt.js" "$out/b/kapy-occt.js"
    echo "deterministic: two links, same bytes"
fi
if [ "${1:-}" = "--publish" ] || [ "${2:-}" = "--publish" ]; then
    cp "$out/a/kapy-occt.js" "$out/a/kapy-occt.wasm" "$here/../../dist/"
    echo "published to engine/kernels/occt/dist"
fi
