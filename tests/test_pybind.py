import unittest
import numpy as np
import _low_latency_cpp as hft

class TestPybindZeroCopy(unittest.TestCase):

    def test_zero_copy_bid_updates(self):
        book = hft.FlatOrderBook()

        # Get zero-copy NumPy view once
        bids_view = book.get_bids_zerocopy()

        # Verify initial shape and zero allocation
        self.assertIsInstance(bids_view, np.ndarray)
        self.assertEqual(bids_view.shape[0], 10000)
        self.assertEqual(bids_view[100], 0)

        # 1. Add order in C++ -> check Python view updates automatically
        order_idx1 = book.add_order(101, 100, 500, hft.Side.BID)
        self.assertEqual(bids_view[100], 500)

        # 2. Add second order at same price level -> aggregate volume updates
        order_idx2 = book.add_order(102, 100, 300, hft.Side.BID)
        self.assertEqual(bids_view[100], 800)

        # 3. Cancel first order -> aggregate volume decreases
        book.cancel_order(order_idx1)
        self.assertEqual(bids_view[100], 300)

        # 4. Cancel second order -> volume drops back to zero
        book.cancel_order(order_idx2)
        self.assertEqual(bids_view[100], 0)

    def test_zero_copy_ask_updates(self):
        book = hft.FlatOrderBook()
        asks_view = book.get_asks_zerocopy()

        order_idx = book.add_order(201, 500, 1200, hft.Side.ASK)
        self.assertEqual(asks_view[500], 1200)

        book.cancel_order(order_idx)
        self.assertEqual(asks_view[500], 0)

if __name__ == "__main__":
    unittest.main()