import unittest
import _low_latency_cpp as hft
from strategy.alpha_engine import MicrostructureAlphaEngine

class TestAlphaEngine(unittest.TestCase):

    def setUp(self):
        self.book = hft.FlatOrderBook()
        self.engine = MicrostructureAlphaEngine(self.book, obi_threshold=0.5)

    def test_balanced_book_signals(self):
        # 100 shares @ $100 Bid, 100 shares @ $102 Ask
        self.book.add_order(1, 100, 100, hft.Side.BID)
        self.book.add_order(2, 102, 100, hft.Side.ASK)

        sig = self.engine.calculate_signals(timestamp_ns=1000)

        self.assertEqual(sig.best_bid, 100)
        self.assertEqual(sig.best_ask, 102)
        self.assertAlmostEqual(sig.mid_price, 101.0)
        self.assertAlmostEqual(sig.micro_price, 101.0)
        self.assertAlmostEqual(sig.obi, 0.0)
        self.assertEqual(sig.signal, 0)

    def test_bullish_imbalance_and_microprice_shift(self):
        # 900 shares @ $100 Bid (Heavy demand), 100 shares @ $102 Ask (Thin offer)
        self.book.add_order(1, 100, 900, hft.Side.BID)
        self.book.add_order(2, 102, 100, hft.Side.ASK)

        sig = self.engine.calculate_signals(timestamp_ns=2000)

        # OBI = (900 - 100) / 1000 = 0.8
        self.assertAlmostEqual(sig.obi, 0.8)
        
        # Micro-price shifts upwards towards Ask ($101.8)
        self.assertAlmostEqual(sig.micro_price, 101.8)
        
        # Signal triggers BUY (+1) since OBI (0.8) > threshold (0.5)
        self.assertEqual(sig.signal, 1)

    def test_bearish_imbalance_signal(self):
        # 100 shares @ $100 Bid, 900 shares @ $102 Ask
        self.book.add_order(1, 100, 100, hft.Side.BID)
        self.book.add_order(2, 102, 900, hft.Side.ASK)

        sig = self.engine.calculate_signals(timestamp_ns=3000)

        # OBI = (100 - 900) / 1000 = -0.8
        self.assertAlmostEqual(sig.obi, -0.8)
        self.assertAlmostEqual(sig.micro_price, 100.2)
        self.assertEqual(sig.signal, -1)

if __name__ == "__main__":
    unittest.main()