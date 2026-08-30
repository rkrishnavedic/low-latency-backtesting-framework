# Low-Latency Hybrid L2 Order Book & Backtesting Framework

A high-performance, hybrid C++/Python quantitative trading framework designed for ultra-low latency Level 2 (L2) order book construction and real-time microstructure alpha signal generation.

The core market data engine is implemented in C++20 with custom arena-allocated memory pools and zero-copy NumPy bindings via `pybind11`. Microstructure signals and latency-aware backtesting are driven in Python without language interop memory overhead.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          C++20 Core Engine                              │
│                                                                         │
│  ┌───────────────────────┐             ┌─────────────────────────────┐  │
│  │   ArenaPool Allocator │             │  FlatOrderBook (L2 Book)    │  │
│  │  (Fixed-size prealloc)├────────────►│  (Price Level Doubly-Linked)│  │
│  └───────────────────────┘             └──────────────┬──────────────┘  │
└───────────────────────────────────────────────────────┼─────────────────┘
                                                        │
                         Zero-Copy Pointer Strides      │
                         (pybind11 Buffer Protocol)     │
                                                        ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         Python Strategy Engine                          │
│                                                                         │
│  ┌────────────────────────┐             ┌────────────────────────────┐  │
│  │  Microstructure Alpha  │             │   Hybrid Backtest Driver   │  │
│  │  (OBI & Micro-Price)   ├────────────►│   (Latency Deferred PnL)   │  │
│  └────────────────────────┘             └────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘

```

---

## Key Features

* **$O(1)$ Memory Arena Allocation:** Custom lock-free pre-allocated pool (`ArenaPool`) eliminates heap allocation delays during event processing.
* **Cache-Aligned Order Book:** Compact array-backed doubly-linked lists keep price level state cache-line friendly for minimal memory access latency.
* **Zero-Copy Memory Interface:** Exposes raw C++ memory pointers directly to Python NumPy arrays via buffer protocols, avoiding serialization or memory duplication overhead.
* **Microstructure Alpha Engine:** Computes real-time Order Book Imbalance (OBI) and volume-weighted Micro-Price signals.
* **Latency-Aware Backtest Driver:** Simulates execution delays, queue positions, taker fees, and slippage to eliminate lookahead bias in high-frequency signal backtesting.

---

## Prerequisites & Dependencies

* **C++ Compiler:** GCC 10+ or Clang 11+ with C++20 support
* **Build System:** CMake 3.18+
* **Python Environment:** Python 3.10+
* **Python Libraries:** `numpy`

*Google Benchmark and Pybind11 are automatically fetched via CMake `FetchContent` during the build process.*

---

## Build & Installation

### 1. Compile the C++ Shared Object (`_low_latency_cpp`)

```bash
# Create build directory
mkdir -p build && cd build

# Configure with Release flags
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build C++ Python extension module
make _low_latency_cpp

```

### 2. Set Python Path

Set `PYTHONPATH` to point to the build directory containing the generated shared library (`.so` or `.pyd`):

```bash
export PYTHONPATH=$(pwd)/build:$PYTHONPATH

```

---

## Quickstart & Usage

### Running Unit Tests

```bash
# Run strategy & alpha engine unit tests
PYTHONPATH=build:. python3 tests/test_alpha_engine.py
PYTHONPATH=build:. python3 tests/test_backtest_driver.py

```

### Python API Example

```python
import _low_latency_cpp as hft
from strategy.alpha_engine import MicrostructureAlphaEngine

# 1. Initialize Order Book Engine
book = hft.FlatOrderBook()

# 2. Get Zero-Copy NumPy views over C++ memory
bids_view = book.get_bids_zerocopy()
asks_view = book.get_asks_zerocopy()

# 3. Apply a Market Event
event = hft.MarketEvent()
event.order_id = 1001
event.price = 500
event.qty = 1500
event.side = hft.Side.BID
event.timestamp_ns = 1_000_000_000

book.apply_event(event)

# Instant memory update reflected in NumPy view without interop overhead
print(f"Bid Volume at level 500: {bids_view[500]}")  # Output: 1500

# 4. Compute Signals
engine = MicrostructureAlphaEngine(book, obi_threshold=0.5)
signal = engine.calculate_signals(timestamp_ns=event.timestamp_ns)

print(f"OBI Signal: {signal.obi:.4f} | Direction: {signal.signal}")

```

### Running the Backtester

```python
import _low_latency_cpp as hft
from strategy.backtest_driver import HybridBacktestDriver, generate_synthetic_stream

# Generate test data stream
events = generate_synthetic_stream(num_events=10_000)

# Run hybrid backtest
driver = HybridBacktestDriver(
    obi_threshold=0.5,
    order_qty=10,
    transaction_fee_bps=0.1,
    latency_penalty_events=2
)

metrics = driver.run_backtest(events)

print(f"Total Events Processed : {metrics.total_events:,}")
print(f"Total Executed Trades  : {metrics.total_trades}")
print(f"Final Realized PnL    : ${metrics.realized_pnl:.2f}")
print(f"Throughput             : {metrics.throughput_events_per_sec:,.2f} events/sec")

```

---
