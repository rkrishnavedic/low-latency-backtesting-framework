from dataclasses import dataclass, field
import time
import numpy as np
import _low_latency_cpp as hft
from strategy.alpha_engine import MicrostructureAlphaEngine, AlphaSignal


@dataclass(slots=True)
class TradeRecord:
    timestamp_ns: int
    side: str
    price: float
    qty: int
    realized_pnl: float


@dataclass(slots=True)
class BacktestMetrics:
    total_events: int
    total_trades: int
    final_position: int
    realized_pnl: float
    unrealized_pnl: float
    total_pnl: float
    sharpe_ratio: float
    max_drawdown: float
    execution_time_sec: float
    throughput_events_per_sec: float


class HybridBacktestDriver:
    def __init__(
        self,
        obi_threshold: float = 0.6,
        order_qty: int = 10,
        transaction_fee_bps: float = 0.5,
        latency_penalty_events: int = 1,
    ):
        self.book = hft.FlatOrderBook()
        self.alpha_engine = MicrostructureAlphaEngine(self.book, obi_threshold=obi_threshold)
        self.order_qty = order_qty
        self.fee_rate = transaction_fee_bps / 10000.0
        self.latency_penalty = latency_penalty_events

        # Portfolio state
        self.position: int = 0
        self.cash: float = 0.0
        self.realized_pnl: float = 0.0
        self.avg_entry_price: float = 0.0

        # Execution delay queue
        self.pending_signals: list[tuple[int, int, AlphaSignal]] = []  # (trigger_idx, latency, signal)

        # Performance tracking
        self.trades: list[TradeRecord] = []
        self.equity_curve: list[float] = []
        self.event_count: int = 0

    def process_event(self, event: hft.MarketEvent) -> AlphaSignal | None:
        """Applies market event to C++ order book, evaluates signals, and executes delayed orders."""
        self.event_count += 1

        # 1. Update C++ L2 Order Book
        self.book.apply_event(event)

        # 2. Execute pending orders whose latency penalty has expired
        self._process_pending_executions(event.timestamp_ns)

        # 3. Compute microstructure signals via Zero-Copy buffer
        signal = self.alpha_engine.calculate_signals(timestamp_ns=event.timestamp_ns)

        # 4. Queue signal for execution if actionable
        if signal.signal != 0:
            self.pending_signals.append((self.event_count, self.latency_penalty, signal))

        # 5. Track Mark-to-Market Equity
        mtm_equity = self._calculate_current_equity(signal.mid_price)
        self.equity_curve.append(mtm_equity)

        return signal

    def _process_pending_executions(self, current_ts: int) -> None:
        """Simulates execution latency delays to prevent lookahead bias."""
        ready_indices = []
        for i, (trigger_idx, latency, signal) in enumerate(self.pending_signals):
            if self.event_count >= trigger_idx + latency:
                self._execute_trade(signal, current_ts)
                ready_indices.append(i)

        for i in reversed(ready_indices):
            self.pending_signals.pop(i)

    def _execute_trade(self, signal: AlphaSignal, current_ts: int) -> None:
        """Simulates fill price at Ask (for BUY) or Bid (for SELL) with fee deduction."""
        if signal.signal == 1 and signal.best_ask > 0:  # BUY
            fill_price = float(signal.best_ask)
            fee = fill_price * self.order_qty * self.fee_rate
            cost = (fill_price * self.order_qty) + fee

            # PnL accounting
            if self.position < 0:  # Closing short
                pnl = (self.avg_entry_price - fill_price) * min(abs(self.position), self.order_qty) - fee
                self.realized_pnl += pnl
            else:
                pnl = -fee

            self.cash -= cost
            new_pos = self.position + self.order_qty
            if new_pos != 0:
                self.avg_entry_price = (
                    (self.avg_entry_price * max(0, self.position)) + (fill_price * self.order_qty)
                ) / max(1, new_pos)
            self.position = new_pos

            self.trades.append(
                TradeRecord(
                    timestamp_ns=current_ts,
                    side="BUY",
                    price=fill_price,
                    qty=self.order_qty,
                    realized_pnl=pnl,
                )
            )

        elif signal.signal == -1 and signal.best_bid > 0:  # SELL
            fill_price = float(signal.best_bid)
            fee = fill_price * self.order_qty * self.fee_rate
            revenue = (fill_price * self.order_qty) - fee

            if self.position > 0:  # Closing long
                pnl = (fill_price - self.avg_entry_price) * min(self.position, self.order_qty) - fee
                self.realized_pnl += pnl
            else:
                pnl = -fee

            self.cash += revenue
            new_pos = self.position - self.order_qty
            if new_pos != 0:
                self.avg_entry_price = fill_price
            self.position = new_pos

            self.trades.append(
                TradeRecord(
                    timestamp_ns=current_ts,
                    side="SELL",
                    price=fill_price,
                    qty=self.order_qty,
                    realized_pnl=pnl,
                )
            )

    def _calculate_current_equity(self, mid_price: float) -> float:
        unrealized_pnl = 0.0
        if self.position != 0 and mid_price > 0:
            unrealized_pnl = (mid_price - self.avg_entry_price) * self.position
        return self.cash + (self.position * mid_price) + unrealized_pnl

    def run_backtest(self, events: list[hft.MarketEvent]) -> BacktestMetrics:
        """Runs batch backtest loop over a pre-loaded market event stream."""
        start_time = time.perf_counter()

        for event in events:
            self.process_event(event)

        elapsed_sec = time.perf_counter() - start_time
        latest_sig = self.alpha_engine.calculate_signals(0)
        unrealized = (
            (latest_sig.mid_price - self.avg_entry_price) * self.position
            if (self.position != 0 and latest_sig.mid_price > 0)
            else 0.0
        )

        total_pnl = self.realized_pnl + unrealized

        # Compute drawdown and Sharpe ratio
        equity = np.array(self.equity_curve)
        if equity.size > 1:
            returns = np.diff(equity)
            std_ret = float(np.std(returns))
            sharpe = (float(np.mean(returns)) / std_ret * np.sqrt(252)) if std_ret > 0 else 0.0

            peak = np.maximum.accumulate(equity)
            drawdowns = peak - equity
            max_dd = float(np.max(drawdowns)) if drawdowns.size > 0 else 0.0
        else:
            sharpe = 0.0
            max_dd = 0.0

        return BacktestMetrics(
            total_events=self.event_count,
            total_trades=len(self.trades),
            final_position=self.position,
            realized_pnl=self.realized_pnl,
            unrealized_pnl=unrealized,
            total_pnl=total_pnl,
            sharpe_ratio=sharpe,
            max_drawdown=max_dd,
            execution_time_sec=elapsed_sec,
            throughput_events_per_sec=self.event_count / elapsed_sec if elapsed_sec > 0 else 0.0,
        )