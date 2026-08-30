#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#include "market_event.hpp"
#include "order_book.hpp"

namespace py = pybind11;

PYBIND11_MODULE(_low_latency_cpp, m) {
    m.doc() = "Low-Latency Engine Python Bindings";

    // 1. Bind Side Enum
    py::enum_<Side>(m, "Side")
        .value("BID", Side::BID)
        .value("ASK", Side::ASK)
        .export_values();

    // 2. Bind MarketEvent Struct
    py::class_<MarketEvent>(m, "MarketEvent")
        .def(py::init<>())
        .def_readwrite("price", &MarketEvent::price)
        .def_readwrite("qty", &MarketEvent::qty)
        .def_readwrite("side", &MarketEvent::side)
        .def_readwrite("order_id", &MarketEvent::order_id)
        .def_readwrite("timestamp_ns", &MarketEvent::timestamp_ns);

    // 3. Bind LimitLevel Struct
    py::class_<LimitLevel>(m, "LimitLevel")
        .def_readonly("price", &LimitLevel::price)
        .def_readonly("total_volume", &LimitLevel::total_volume)
        .def_readonly("order_count", &LimitLevel::order_count);

    // 4. Bind FlatOrderBook
    py::class_<FlatOrderBook>(m, "FlatOrderBook")
        .def(py::init<>())
        .def("add_order", &FlatOrderBook::add_order)
        .def("cancel_order", &FlatOrderBook::cancel_order)
        .def("get_level", &FlatOrderBook::get_level)
        .def("get_order", &FlatOrderBook::get_order)

        // Lambda wrapper mapping apply_event to add_order without altering order_book.hpp
        .def("apply_event", [](FlatOrderBook& book, const MarketEvent& event) {
            return book.add_order(event.order_id, event.price, event.qty, event.side);
        })

        // Zero-copy volume views mapped directly onto total_volume using struct strides
        .def("get_bids_zerocopy", [](const FlatOrderBook& book) {
            return py::array_t<uint32_t>(
                {MAX_PRICE_LEVELS},
                {sizeof(LimitLevel)},
                &book.get_bids_raw_ptr()[0].total_volume,
                py::cast(book)
            );
        })
        .def("get_asks_zerocopy", [](const FlatOrderBook& book) {
            return py::array_t<uint32_t>(
                {MAX_PRICE_LEVELS},
                {sizeof(LimitLevel)},
                &book.get_asks_raw_ptr()[0].total_volume,
                py::cast(book)
            );
        });
}