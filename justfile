set shell := ["bash", "-uc"]

default: build

build: configure compile stage

configure:
    meson setup build-wasm --cross-file=crosscompile/emscripten.txt --prefix=/

compile:
    meson compile -C build-wasm

stage:
    DESTDIR="$(pwd)/out" meson install -C build-wasm

dev: configure compile
    (cd demo && python3 server.py)

run: build
    (cd out && python3 server.py)

reconfigure:
    meson setup build-wasm --cross-file=crosscompile/emscripten.txt --reconfigure --prefix=/

clean:
    rm -rf build-wasm out
