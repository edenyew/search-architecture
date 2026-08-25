# Search Backend

A distributed search backend in C++. Five services communicating over gRPC,
a REST edge, Redis cache, hybrid BM25/vector retrieval, and request tracing
via a propagated correlation ID. Runs locally, no cloud dependency.

Scope note: ranking quality is not the focus — BM25 + vector fusion is
functional but basic. The project is about service architecture: fan-out/
fan-in, caching, parallel execution, request correlation across process
boundaries.

---

## Request flow

```
curl "localhost:8080/search?q=heart+attack&top_k=5"
```

Gateway (REST) → Controller (orchestration) → Retrieval / Cache / Reranker,
all over gRPC. One request, five processes, one trace ID threaded through
every hop via gRPC metadata.

```
                curl / load harness
                       │  REST/JSON
                 ┌─────▼─────┐
                 │  Gateway  │   REST edge, generates trace ID
                 └─────┬─────┘
                       │  gRPC (trace ID propagated via metadata)
                ┌──────▼──────┐
                │ Controller  │   classify() + orchestrate(): fan-out, merge
                └──┬───┬───┬──┘
             gRPC │   │   │ gRPC
         ┌─────────▼┐┌▼─────┐┌▼────────┐
         │Retrieval ││Cache ││Reranker │
         │BM25 ‖    ││Redis ││(rerank  │
         │ vector,  ││client││ seam)   │
         │RRF fusion││      ││         │
         └────┬─────┘└──────┘└─────────┘
              │
       ┌──────▼──────┐
       │ Index files │   built offline by `indexer`, loaded into RAM
       │ + embeddings│   on startup — never rebuilt on the hot path
       └─────────────┘
```

**Cache-aside.** Controller checks Cache first. Hit → return, skip Retrieval
and Reranker. Miss → call Retrieval, call Reranker, write the final
(post-rerank) result back to Cache. Cache stores the finished answer, so a
hit never re-runs reranking.

**Retrieval.** BM25 and vector search run concurrently, dispatched to a
bounded worker pool (not raw `std::async` per call), against the same
query, then get combined with Reciprocal Rank Fusion. RRF combines by
rank position, not raw score — BM25 scores and cosine similarities aren't
on comparable scales.

**Tracing.** Trace ID generated once at Gateway, attached to every outgoing
gRPC call's metadata, read back and re-attached at each hop. Every service
logs its own stage timing tagged with that ID. One trace ID, grepped across
all five services' logs, reconstructs a request's full path and timing.

**Reliability.** One absolute deadline for the whole request, set at Gateway
and propagated unchanged to every downstream call — caps total request time
instead of compounding a fresh timeout at each hop. Controller retries
Retrieval up to 3 times, only on `UNAVAILABLE`. A circuit breaker trips after
3 consecutive Retrieval failures and short-circuits further calls instantly
— no attempt made — until a reset timeout elapses; recovery is then tested
with a dedicated gRPC health check (`Check()`) rather than a real search
request. Retrieval itself sheds load outright (`RESOURCE_EXHAUSTED`) once
its bounded worker pool's queue is full, instead of queueing unbounded work.

---

## Tech stack

| Concern                   | Choice                                                                          |
| ------------------------- | ------------------------------------------------------------------------------- |
| Services / indexer        | C++20                                                                           |
| Internal RPC              | gRPC + Protobuf                                                                 |
| Edge REST                 | Crow                                                                            |
| Concurrency               | Bounded thread pool (Retrieval) + `std::future` for async results               |
| BM25 index                | Custom inverted index, custom binary serialization                              |
| Tokenization (BM25)       | Custom whitespace/punctuation tokenizer                                         |
| Tokenization (embeddings) | Custom WordPiece tokenizer — verified against HuggingFace's tokenizer output    |
| Embedding model           | `all-MiniLM-L6-v2`, exported to ONNX offline once, run via ONNX Runtime C++ API |
| Vector index              | `hnswlib` (vendored, header-only)                                               |
| Fusion                    | Custom RRF implementation                                                       |
| Cache                     | Redis + `redis-plus-plus`                                                       |
| Tracing                   | Trace ID via gRPC metadata + structured logs                                    |
| Build                     | CMake                                                                           |

---

## Build and run

Toolchain (macOS/Homebrew):

```bash
brew install cmake protobuf grpc abseil crow nlohmann-json redis hiredis onnxruntime

# redis-plus-plus has no formula — build from source:
git clone https://github.com/sewenew/redis-plus-plus.git
cmake -S redis-plus-plus -B redis-plus-plus/build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/opt/homebrew -DCMAKE_INSTALL_PREFIX=/opt/homebrew \
  -DREDIS_PLUS_PLUS_CXX_STANDARD=17 -DREDIS_PLUS_PLUS_BUILD_TEST=OFF
cmake --build redis-plus-plus/build && cmake --install redis-plus-plus/build
```

Build:

```bash
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j
```

Build the index (offline, ~5 min — computes an ONNX embedding per document):

```bash
./build/indexer/indexer
```

Start Redis and all five services:

```bash
redis-server --daemonize yes --logfile /tmp/redis.log

./build/services/retrieval/retrieval > /tmp/retrieval.log 2>&1 &
./build/services/cache/cache         > /tmp/cache.log     2>&1 &
./build/services/reranker/reranker   > /tmp/reranker.log  2>&1 &
sleep 2
./build/services/controller/controller > /tmp/controller.log 2>&1 &
sleep 1
./build/services/gateway/gateway       > /tmp/gateway.log    2>&1 &
```

Query:

```bash
curl "localhost:8080/search?q=heart+attack&top_k=5"
```

Trace a request:

```bash
# get the trace ID from gateway's log, then:
grep "\[<trace_id>\]" /tmp/*.log
```
