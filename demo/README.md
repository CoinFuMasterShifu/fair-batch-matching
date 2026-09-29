# FBM Demo

A self-contained demo of the Fair Batch Matching (FBM) engine.

Latest version: [github.com/warthog-network/fair-batch-matching/releases/latest](https://github.com/warthog-network/fair-batch-matching/releases/latest)

## Run

    python3 server.py

Then open http://localhost:8000/demo.html in a browser.

## Files

- `demo.html` — the demo UI
- `fbm.js`    — Embind loader for the wasm
- `fbm.wasm`  — the matching engine
- `server.py` — tiny Python static server with permissive CORS headers

`fbm.js` and `fbm.wasm` always go together. `fbm.js` looks for `fbm.wasm`
next to itself; renaming either breaks the pairing.

## Embedding the engine

Drop `fbm.js` and `fbm.wasm` next to your html, then import the loader:

```html
<script type="module">
    import init from "./fbm.js";
    const Module = await init();
    Module.addBuy({ price: "1.5", amount: "5" });
</script>
```

`fbm.js` looks for `fbm.wasm` next to itself; rename neither.

## API

All six functions take a plain JS object and return a plain JS object
(or `{ error: "..." }` on failure).

| Function | Input keys | Output shape | Side effects |
|---|---|---|---|
| `addBuy` | `price` (str), `amount` (str, WART) | `{ parseErrors, match: { buys, sells, poolBefore, poolAfter, toPool, filled, matched } }` | inserts a buy order |
| `addSell` | `price` (str), `amount` (str, TOKEN) | same | inserts a sell order |
| `editPool` | `token` (str), `wart` (str) | same | updates pool reserves |
| `deleteOrder` | `base` (bool), `index` (int) | same | removes one order |
| `setFee` | `E4` (int, 0–9999) | same | updates pool fee |
| `clearAndSetBaseDecimals` | `baseDecimals` (int, 1–254) | same | resets book + pool |

The returned `match` object is omitted when pool reserves fail to parse
(`{ parseErrors: { poolToken, poolWart } }`).

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

For details on the FBM algorithm itself, see
[the FBM paper](https://warthog.network/FairBatchMatching.pdf).
