# Fair Batch Matching (FBM) Demo

This is my custom matching engine that I wrote for [Warthog Network](https://www.warthog.network/). It matches both, buy and sell orders and also pool liquidity.

## Purpose

This repo ships the WASM build of the Warthog Fair Batch Matching
(FBM) engine together with a small reference UI. The matching
algorithm is described in [the FBM paper](https://warthog.network/FairBatchMatching.pdf);
this repo is the runnable artefact.

## What this repo provides

- A C++23 implementation of FBM under `src/`.
- A WASM build (Embind) exposing six JS-callable functions:
  `addBuy`, `addSell`, `editPool`, `deleteOrder`, `setFee`,
  `clearAndSetBaseDecimals`.
- A standalone reference UI in `demo/`.

## Live demo

<https://warthog.network/defi-demo>

## Releases

Pre-built artefacts are attached to each GitHub Release. Trigger a
release by pushing a tag matching `v*`, or by running the workflow
manually from the Actions tab.

Three assets per release:

- **`fbm.wasm`** — the wasm binary, ready to embed.
- **`fbm.js`** — the Embind loader that fetches and instantiates
  `fbm.wasm` (place both files side by side).
- **`fbm-demo.zip`** — a self-contained demo bundle containing
  `demo.html`, `fbm.js`, `fbm.wasm`, `server.py`, and a usage
  README.

  ```sh
  unzip fbm-demo.zip && cd fbm-demo && python3 server.py
  # open http://localhost:8000/demo.html
  ```

## Embedding in your own page

Download `fbm.wasm` and `fbm.js`, drop them next to your html, and
import the loader:

```html
<script type="module">
    import init from "./fbm.js";
    const Module = await init();
    Module.addBuy({ price: "1.5", amount: "5" });
</script>
```

`fbm.js` fetches `fbm.wasm` from the same directory; serve both
files from the same origin.

## API

All six functions take a plain JS object and return a plain JS
object (or `{ error: "..." }` on failure).

| Function | Input keys | Output shape | Side effects |
|---|---|---|---|
| `addBuy` | `price` (str), `amount` (str, WART) | `{ parseErrors, match: { buys, sells, poolBefore, poolAfter, toPool, filled, matched } }` | inserts a buy order |
| `addSell` | `price` (str), `amount` (str, TOKEN) | same | inserts a sell order |
| `editPool` | `token` (str), `wart` (str) | same | updates pool reserves |
| `deleteOrder` | `base` (bool), `index` (int) | same | removes one order |
| `setFee` | `E4` (int, 0–9999) | same | updates pool fee |
| `clearAndSetBaseDecimals` | `baseDecimals` (int, 1–254) | same | resets book + pool |

The returned `match` object is omitted when pool reserves fail to
parse (`{ parseErrors: { poolToken, poolWart } }`).

### Example

```js
Module.editPool({ token: "100", wart: "200" });
Module.addSell({ price: "1.5", amount: "10" });
const res = Module.addBuy({ price: "1.5", amount: "5" });
console.log(res.match.buys);
//   [{ amount: "5.00000000", filled: "5.00000000", limit: 1.5 }]
console.log(res.match.toPool);
//   null, or { isQuote, base, quote, price }
```

## Why does the world need this?

*Decentralized Finance* (DeFi) is becoming a killer application of crypto and is only going to grow in importance. However it suffers from a fundamental problem: transactions can be reordered within a block.

Since there is no direct concept of time in a block chain, but instead the blocks form the discrete-time sequence of events, there is no notion of which order came before or after within a block. Block builders artificially specify an order in which DeFi orders are processed. This leads to the concept of *Maximal Extractable Value* (MEV) which describes this property and the possibility to reorder, add and/or omit specific orders such that an arbitrage-like opportunity is formed and exploited.

The most notable incarnation of this practice is the dreaded *Sandwich* which describes carefully crafted front-running and back-running of a victim's order such that it is pushed to the order to the specified limit price. For example it works by buying before you buy and selling after you buy and this is done precisely as much as your order's limit price allows.
</br>

**Basically the sandwich is doing this to your order:**
<p align="center">
  <img src="https://external-content.duckduckgo.com/iu/?u=https%3A%2F%ae01.alicdn.com%2Fkf%2FHTB1D5POX5LxK1Rjy0Ffq6zYdVXa6%2F8-styles-Funny-Cartoon-Animal-Small-Squeeze-Antistress-Toy-Pop-Out-Eyes-Doll-Stress-Relief-Venting.jpg&f=1&nofb=1&ipt=62da1656015b17c22ce5dd0db0bb7430c50a524a5819e74d296cfcd33c6bb509&ipo=images" alt="Sublime's custom image", width= "40%";/>
</p>

The struggle is real. One method to avoid this problem is to be secretive with your order but this does not always work well nor is it practical. Therefore we need to make DeFi great again and fight back. The solution to this problem is simple to formulate but difficult to implement: **We need to get rid of the ordering of transactions within a block. Each transaction shall be treated equally**. Obviously then front and back-running is not possible anymore and so won't be sandwiches.

I propose a new matching engine that finds the same price for all buys and all sells for a market. This price is fairly determined by supply and demand and also by pool liquidity.

The goal is to implement this matching engine in Warthog Network at some later stage together with hard-coded DeFi capabilities.

## Building from source

Requires the Emscripten SDK on PATH (`fish> emsdk_setup` /
`bash$ source ./emsdk/emsdk_env.sh`) and `just`.

```sh
just build         # configure + compile + stage out/
just run           # serve out/ on http://localhost:8000/demo.html
just clean         # rm -rf build-wasm out
just reconfigure   # re-run meson setup
```

`out/` is gitignored — regenerated by every `just build`.

## Layout: dev vs bundle

`demo/` is a dev workspace. `out/` is the released bundle. They share
the same file layout — `demo.html`, `fbm.js`, `fbm.wasm`, `server.py`,
`README.md` — so a workflow that works in `demo/` works the same in
`out/`.

In `demo/`, `fbm.js` and `fbm.wasm` are **symlinks** into `build-wasm/`:

```
demo/fbm.js   → ../build-wasm/fbm.js
demo/fbm.wasm → ../build-wasm/fbm.wasm
```

This is for real-time development: editing source under `src/` and
running `meson compile -C build-wasm` (or just `ninja` from
`build-wasm/`) refreshes the files those symlinks point at. With the
dev server running, a browser refresh picks up the new wasm and js —
no copying, no install step, no bundle rebuild.

In `out/` (and in `fbm-demo.zip`), `fbm.js` and `fbm.wasm` are real
files, copied by `meson install`. No symlinks ship to users.

To start the dev server:

```sh
just dev          # configure + compile + serve demo/ on :8000
# edit src/wasm_callbacks.cpp
# meson compile -C build-wasm
# refresh browser
```

To build the bundle and serve it:

```sh
just build        # configure + compile + stage out/
just run          # serve out/ on :8000
```

Both flows open `http://localhost:8000/demo.html`.

## Layout

- `demo/` — dev workspace, mirrors the bundle layout
  - `demo.html` — reference UI
  - `fbm.js` — symlink → `../build-wasm/fbm.js`
  - `fbm.wasm` — symlink → `../build-wasm/fbm.wasm`
  - `server.py` — Python static server
  - `README.md` — bundle usage doc
- `src/` — C++ engine + WASM entry points
- `src/wasm_callbacks.cpp` — the only file that talks to JS
- `crosscompile/emscripten.txt` — meson cross file (wasm32)
- `subprojects/json.wrap` — nlohmann/json subproject
- `meson.build` — build config + install rules
- `justfile` — `build`, `run`, `dev`, `clean`, `reconfigure`
- `.github/workflows/release.yml` — release pipeline
- `build-wasm/` — meson build dir (gitignored)
- `out/` — generated bundle (gitignored)
