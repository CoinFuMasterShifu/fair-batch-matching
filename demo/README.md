# FBM Demo

A self-contained demo of the Fair Batch Matching (FBM) engine.

## Run

    python3 server.py

Then open http://localhost:8000/demo.html in a browser.

## Files

- `demo.html` — the demo UI
- `fbm.js`    — Embind loader for the wasm
- `fbm.wasm`  — the matching engine
- `server.py` — tiny Python static server with permissive CORS headers

## Embed the engine in your own page

See https://github.com/warthog-network/defi-demo for the API.
