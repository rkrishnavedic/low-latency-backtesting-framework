import unittest
import numpy as np
import _low_latency_cpp as hft
from strategy.backtest_driver import HybridBacktestDriver


def generate_synthetic_stream(num_events: int = 5000) -> list[hft.MarketEvent]:
    """Generates synthetic tick sequence with order imbalance regimes."""
    events = []
    base_price = 500
    np.random.seed(42)

    for i in range(num_events):
        event = hft.MarketEvent()
        event.order_id = i + 1
        event.timestamp_ns = (i + 1) * 1000  # 1 microsecond increments

        # Create alternating bullish/bearish order book imbalance regimes
        if (i // 500) % 2 == 0:
            # Bullish Regime: Heavy Bid Volume, Light Ask Volume
            if i % 2 == 0:
                event.side = hft.Side.BID
                event.price = base_price
                event.qty = int(np.random.randint(500, 1500))
            else:
                event.side = hft.Side.ASK
                event.price = base_price + 2
                event.qty = int(np.random.randint(50, 150))
        else:
            # Bearish Regime: Light Bid Volume, Heavy Ask Volume
            if i % 2 == 0:
                event.side = hft.Side.BID
                event.price = base_price
                event.qty = int(np.random.randint(50, 150))
            else:
                event.side = hft.Side.ASK
                event.price = base_price + 2
                event.qty = int(np.random.randint(500, 1500))

        events.append(event)
    return events


class TestHybridBacktestDriver(unittest.TestCase):

    def test_end_to_end_backtest_execution(self):
        stream = generate_synthetic_stream(num_events=2000)
        driver = HybridBacktestDriver(
            obi_threshold=0.5,
            order_qty=10,
            transaction_fee_bps=0.1,
            latency_penalty_events=2,
        )

        metrics = driver.run_backtest(stream)

        # Assertions to ensure full execution flow works as expected
        self.assertEqual(metrics.total_events, 2000)
        self.assertGreater(metrics.total_trades, 0)
        self.assertGreater(metrics.throughput_events_per_sec, 10000.0)  # >10k events/sec target
        self.assertIsInstance(metrics.total_pnl, float)

    def test_latency_penalty_defers_execution(self):
        driver = HybridBacktestDriver(obi_threshold=0.5, latency_penalty_events=5)

        # Trigger bullish signal
        e1 = hft.MarketEvent()
        e1.order_id = 1
        e1.price = 100
        e1.qty = 1000
        e1.side = hft.Side.BID
        driver.process_event(e1)

        e2 = hft.MarketEvent()
        e2.order_id = 2
        e2.price = 102
        e2.qty = 100
        e2.side = hft.Side.ASK
        driver.process_event(e2)

        # Signal queued, but trades count should still be 0 due to latency delay
        self.assertEqual(len(driver.trades), 0)

        # Advance events to trigger latency delay
        for i in range(5):
            e_dummy = hft.MarketEvent()
            e_dummy.order_id = 3 + i
            e_dummy.price = 100
            e_dummy.qty = 10
            e_dummy.side = hft.Side.BID
            driver.process_event(e_dummy)

        # Trade must execute after latency penalty expires
        self.assertGreaterEqual(len(driver.trades), 1)


if __name__ == "__main__":
    unittest.main()