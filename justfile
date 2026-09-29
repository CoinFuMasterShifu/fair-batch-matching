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

# Bump version in meson.build, commit, and tag locally. Push yourself.
bump part="patch":
    #!/usr/bin/env bash
    set -euo pipefail
    cur=$(grep -oP "^\s*version\s*:\s*'\K[^']+" meson.build)
    IFS='.' read -r -a parts <<< "$cur"
    major="${parts[0]:-0}"; minor="${parts[1]:-0}"; patch="${parts[2]:-0}"
    case "{{part}}" in
      major) major=$((major + 1)); minor=0; patch=0 ;;
      minor) minor=$((minor + 1)); patch=0 ;;
      patch) patch=$((patch + 1)) ;;
      *) echo "usage: just bump [major|minor|patch]" >&2; exit 1 ;;
    esac
    new="${major}.${minor}.${patch}"
    sed -i -E "s/(version\s*:\s*')$cur'/\1${new}'/" meson.build
    git add meson.build
    git commit -m "bump version to ${new}"
    git tag -a "v${new}" -m "Release v${new}"
    echo "Committed and tagged v${new}. Push with:"
    echo "    git push origin master --follow-tags"

# Tag HEAD at the existing version (re-tag after a fixup). Push yourself.
release:
    #!/usr/bin/env bash
    set -euo pipefail
    v=$(grep -oP "^\s*version\s*:\s*'\K[^']+" meson.build)
    tag="v${v}"
    git tag -a "$tag" -m "Release $tag"
    echo "Tagged $tag. Push with:"
    echo "    git push origin master --follow-tags"
