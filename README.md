# Fair Batch Matching (FBM) Demo

This is my custom matching engine that I wrote for [Warthog Network](https://www.warthog.network/). It matches both, buy and sell orders and also pool liquidity. 

## Why does the world need this?
*Decentralized Finance* (DeFi) is becoming a killer application of crypto and is only going to grow in importance. However it suffers from a fundamental problem: transactions can be reordered within a block. 

Since there is no direct concept of time in a block chain, but instead the blocks form the discrete-time sequence of events, there is no notion of which order came before or after within a block. Block builders artificially specify an order in which DeFi orders are processed. This leads to the concept of *Maximal Extractable Value* (MEV) which describes this property and the possibility to reorder, add and/or omit specific orders such that an arbitrage-like opportunity is formed and exploited.

The most notable incarnation of this practice is the dreaded *Sandwich* which describes carefully crafted front-running and back-running of a victim's order such that it is pushed to the order to the specified limit price. For example it works by buying before you buy and selling after you buy and this is done precisely as much as your order's limit price allows.
</br>

**Basically the sandwich is doing this to your order:**
<p align="center">
  <img src="https://external-content.duckduckgo.com/iu/?u=https%3A%2F%2Fae01.alicdn.com%2Fkf%2FHTB1D5POX5LxK1Rjy0Ffq6zYdVXa6%2F8-styles-Funny-Cartoon-Animal-Small-Squeeze-Antistress-Toy-Pop-Out-Eyes-Doll-Stress-Relief-Venting.jpg&f=1&nofb=1&ipt=62da1656015b17c22ce5dd0db0bb7430c50a524a5819e74d296cfcd33c6bb509&ipo=images" alt="Sublime's custom image", width= "40%";/>
</p>

The struggle is real. One method to avoid this problem is to be secretive with your order but this does not always work well nor is it practical. Therefore we need to make DeFi great again and fight back. The solution to this problem is simple to formulate but difficult to implement: **We need to get rid of the ordering of transactions within a block. Each transaction shall be treated equally**. Obviously then front and back-running is not possible anymore and so won't be sandwiches.

I propose a new matching engine that finds the same price for all buys and all sells for a market. This price is fairly determined by supply and demand and also by pool liquidity.

The goal is to implement this matching engine in Warthog Network at some later stage together with hard-coded DeFi capabilities.

## What does the Repo contain?
The repository contains a C++ implementation of my *Fair Batch Matching (FBM)* engine. It can be compiled to webassembly using this commands:
```
meson setup build-wasm --cross-file=crosscompile/emscripten.txt
cd build-wasm
ninja
```
when the Emscripten SDK is installed. Then a `python3` server can be launched at the top level of the repo:
```
python3 server.py
```
to play with the demo website at `localhost:8000/website/demo.html`.
