from dataclasses import dataclass
import numpy as np
import _low_latency_cpp as hft

@dataclass(slots=True)
class AlphaSignal:
    timestamp_ns: int
    best_bid: int
    best_ask: int
    mid_price: float
    micro_price: float
    obi: float
    signal: int  # +1: BUY, -1: SELL, 0: HOLD

class MicrostructureAlphaEngine:
    def __init__(self, book: hft.FlatOrderBook, obi_threshold: float = 0.6):
        self.book = book
        self.obi_threshold = obi_threshold
        
        # Zero-copy buffer views mapped to C++ memory
        self.bids_view: np.ndarray = book.get_bids_zerocopy()
        self.asks_view: np.ndarray = book.get_asks_zerocopy()

    def _get_top_of_book(self) -> tuple[int, int, int, int]:
        """Scans non-zero price levels to locate Best Bid and Best Ask."""
        nonzero_bids = np.flatnonzero(self.bids_view)
        nonzero_asks = np.flatnonzero(self.asks_view)

        best_bid = int(nonzero_bids[-1]) if nonzero_bids.size > 0 else 0
        v_bid = int(self.bids_view[best_bid]) if best_bid > 0 else 0

        best_ask = int(nonzero_asks[0]) if nonzero_asks.size > 0 else 0
        v_ask = int(self.asks_view[best_ask]) if best_ask > 0 else 0

        return best_bid, v_bid, best_ask, v_ask

    def calculate_signals(self, timestamp_ns: int = 0) -> AlphaSignal:
        """Computes OBI, Micro-price, and execution triggers over zero-copy memory."""
        p_bid, v_bid, p_ask, v_ask = self._get_top_of_book()

        if p_bid == 0 or p_ask == 0 or (v_bid + v_ask) == 0:
            return AlphaSignal(
                timestamp_ns=timestamp_ns,
                best_bid=p_bid,
                best_ask=p_ask,
                mid_price=0.0,
                micro_price=0.0,
                obi=0.0,
                signal=0
            )

        total_vol = v_bid + v_ask
        mid_price = (p_bid + p_ask) / 2.0
        
        # Micro-Price and Order Book Imbalance calculations
        micro_price = (v_bid * p_ask + v_ask * p_bid) / total_vol
        obi = (v_bid - v_ask) / total_vol

        # Signal Generation Logic
        signal = 0
        if obi >= self.obi_threshold:
            signal = 1   # High buying pressure -> Go Long
        elif obi <= -self.obi_threshold:
            signal = -1  # High selling pressure -> Go Short

        return AlphaSignal(
            timestamp_ns=timestamp_ns,
            best_bid=p_bid,
            best_ask=p_ask,
            mid_price=mid_price,
            micro_price=micro_price,
            obi=obi,
            signal=signal
        )