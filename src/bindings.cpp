#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#include "order_book.hpp"

namespace py = pybind11;

PYBIND11_MODULE(_low_latency_cpp, m) {
    m.doc() = "Low-Latency Engine Python Bindings";

    // Bind Side Enum
    py::enum_<Side>(m, "Side")
        .value("BID", Side::BID)
        .value("ASK", Side::ASK)
        .export_values();

    // Bind LimitLevel
    py::class_<LimitLevel>(m, "LimitLevel")
        .def_readonly("price", &LimitLevel::price)
        .def_readonly("total_volume", &LimitLevel::total_volume)
        .def_readonly("order_count", &LimitLevel::order_count);

    // Bind FlatOrderBook
    py::class_<FlatOrderBook>(m, "FlatOrderBook")
        .def(py::init<>())
        .def("add_order", &FlatOrderBook::add_order)
        .def("cancel_order", &FlatOrderBook::cancel_order)
        .def("get_level", &FlatOrderBook::get_level)
        .def("get_order", &FlatOrderBook::get_order)

        // Zero-copy volume views mapped directly onto total_volume using struct strides
        .def("get_bids_zerocopy", [](const FlatOrderBook& book) {
            return py::array_t<uint32_t>(
                {MAX_PRICE_LEVELS},                        // Shape: [10000]
                {sizeof(LimitLevel)},                      // Stride: Step by sizeof(LimitLevel) bytes
                &book.get_bids_raw_ptr()[0].total_volume,  // Pointer to first total_volume
                py::cast(book)                             // Keep reference alive
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