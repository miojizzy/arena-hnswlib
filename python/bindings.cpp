#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include "arena-hnswlib/arena_hnswlib.h"
#include "arena-hnswlib/bruteforce.h"
#include "arena-hnswlib/hnswalg.h"
#include "arena-hnswlib/space_l2.h"
#include "arena-hnswlib/space_ip.h"

#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace py = pybind11;
using namespace arena_hnswlib;

// Helper: create a SpacePtr based on metric string
static SpacePtr<float> makeSpace(const std::string& metric, size_t dim) {
    if (metric == "l2" || metric == "L2") {
        return std::make_unique<L2Space<float>>(dim);
    } else if (metric == "ip" || metric == "IP" || metric == "cosine") {
        return std::make_unique<InnerProductSpace<float>>(dim);
    }
    throw std::invalid_argument("Unknown metric: " + metric + ". Use 'l2' or 'ip'.");
}

// ---------------------------------------------------------------------------
// BruteForce Python wrapper
// ---------------------------------------------------------------------------
class PyBruteForce {
    size_t dim_;
    std::string metric_;
    std::unique_ptr<BruteForceSearch<float>> index_;

 public:
    PyBruteForce(size_t dim, size_t max_elements = 1000000, const std::string& metric = "l2")
        : dim_(dim), metric_(metric) {
        index_ = std::make_unique<BruteForceSearch<float>>(makeSpace(metric, dim), max_elements);
    }

    void add_items(py::array_t<float, py::array::c_style | py::array::forcecast> data,
                   py::array_t<uint32_t, py::array::c_style | py::array::forcecast> ids) {
        auto d = data.unchecked<2>();
        auto i = ids.unchecked<1>();
        if (d.shape(0) != i.shape(0)) {
            throw std::invalid_argument("data and ids must have the same number of rows");
        }
        if (static_cast<size_t>(d.shape(1)) != dim_) {
            throw std::invalid_argument("data dimension mismatch");
        }
        for (ssize_t row = 0; row < d.shape(0); ++row) {
            index_->addPoint(&d(row, 0), static_cast<LabelType>(i(row)));
        }
    }

    // Returns (ids, distances) as numpy arrays
    // queries shape: (n_queries, dim) or (dim,) for a single query
    std::tuple<py::array_t<uint32_t>, py::array_t<float>>
    query(py::array_t<float, py::array::c_style | py::array::forcecast> queries, size_t k) {
        auto buf = queries.unchecked<2>();
        ssize_t n_queries = buf.shape(0);

        py::array_t<uint32_t> out_ids({n_queries, (ssize_t)k});
        py::array_t<float>    out_dists({n_queries, (ssize_t)k});
        auto ids_ptr = out_ids.mutable_unchecked<2>();
        auto dists_ptr = out_dists.mutable_unchecked<2>();

        for (ssize_t qi = 0; qi < n_queries; ++qi) {
            auto pq = index_->searchKnn(&buf(qi, 0), k);
            // pq is a max-heap of (dist, label), size may be < k
            size_t result_count = pq.size();
            // fill in reverse order (smallest dist last from max-heap)
            for (size_t ri = result_count; ri-- > 0; ) {
                ids_ptr(qi, ri)   = pq.top().second;
                dists_ptr(qi, ri) = pq.top().first;
                pq.pop();
            }
            // fill any unfilled slots with sentinel
            for (size_t ri = result_count; ri < k; ++ri) {
                ids_ptr(qi, ri)   = static_cast<uint32_t>(-1);
                dists_ptr(qi, ri) = std::numeric_limits<float>::infinity();
            }
        }
        return {out_ids, out_dists};
    }
};

// ---------------------------------------------------------------------------
// HNSW Python wrapper
// ---------------------------------------------------------------------------

// Factory: instantiate the concrete template specialisation based on metric string.
// Returns AlgorithmInterface<float> so PyHNSW remains unaware of the SpaceT type.
// The concrete type (L2Space / InnerProductSpace) is baked in at compile time,
// enabling the compiler to inline and auto-vectorize the distance function.
static std::unique_ptr<AlgorithmInterface<float>>
makeHNSW(const std::string& metric, size_t dim, size_t max_elements,
         size_t m, size_t ef_construction) {
    if (metric == "l2" || metric == "L2") {
        return std::make_unique<HierarchicalNSW<float, L2Space<float>>>(
            L2Space<float>(dim), max_elements, m, ef_construction);
    } else if (metric == "ip" || metric == "IP" || metric == "cosine") {
        return std::make_unique<HierarchicalNSW<float, InnerProductSpace<float>>>(
            InnerProductSpace<float>(dim), max_elements, m, ef_construction);
    }
    throw std::invalid_argument("Unknown metric: " + metric + ". Use 'l2' or 'ip'.");
}

class PyHNSW {
    size_t dim_;
    std::string metric_;
    std::unique_ptr<AlgorithmInterface<float>> index_;

 public:
    PyHNSW(size_t dim, size_t max_elements, size_t m = 16,
           size_t ef_construction = 200, const std::string& metric = "l2")
        : dim_(dim), metric_(metric) {
        index_ = makeHNSW(metric, dim, max_elements, m, ef_construction);
    }

    size_t get_ef_search() const { return index_->getEfSearch(); }
    void set_ef_search(size_t ef) { index_->setEfSearch(ef); }

    void add_items(py::array_t<float, py::array::c_style | py::array::forcecast> data,
                   py::array_t<uint32_t, py::array::c_style | py::array::forcecast> ids) {
        auto d = data.unchecked<2>();
        auto i = ids.unchecked<1>();
        if (d.shape(0) != i.shape(0)) {
            throw std::invalid_argument("data and ids must have the same number of rows");
        }
        if (static_cast<size_t>(d.shape(1)) != dim_) {
            throw std::invalid_argument("data dimension mismatch");
        }
        for (ssize_t row = 0; row < d.shape(0); ++row) {
            index_->addPoint(&d(row, 0), static_cast<LabelType>(i(row)));
        }
    }

    // Returns (ids, distances) numpy arrays of shape (n_queries, k)
    std::tuple<py::array_t<uint32_t>, py::array_t<float>>
    query(py::array_t<float, py::array::c_style | py::array::forcecast> queries, size_t k) {
        // Accept 1D (single query) or 2D (batch)
        if (queries.ndim() != 1 && queries.ndim() != 2) {
            throw std::invalid_argument("queries must be 1D or 2D array");
        }

        ssize_t n_queries;
        const float* data_ptr;

        if (queries.ndim() == 1) {
            if (static_cast<size_t>(queries.shape(0)) != dim_) {
                throw std::invalid_argument("query dimension mismatch");
            }
            n_queries = 1;
            data_ptr = queries.data();
        } else {
            if (static_cast<size_t>(queries.shape(1)) != dim_) {
                throw std::invalid_argument("query dimension mismatch");
            }
            n_queries = queries.shape(0);
            data_ptr = queries.data();
        }

        py::array_t<uint32_t> out_ids({n_queries, (ssize_t)k});
        py::array_t<float>    out_dists({n_queries, (ssize_t)k});
        auto ids_ptr   = out_ids.mutable_unchecked<2>();
        auto dists_ptr = out_dists.mutable_unchecked<2>();

        for (ssize_t qi = 0; qi < n_queries; ++qi) {
            auto pq = index_->searchKnn(data_ptr + qi * dim_, k);
            size_t result_count = pq.size();
            for (size_t ri = result_count; ri-- > 0; ) {
                ids_ptr(qi, ri)   = pq.top().second;
                dists_ptr(qi, ri) = pq.top().first;
                pq.pop();
            }
            for (size_t ri = result_count; ri < k; ++ri) {
                ids_ptr(qi, ri)   = static_cast<uint32_t>(-1);
                dists_ptr(qi, ri) = std::numeric_limits<float>::infinity();
            }
        }
        return {out_ids, out_dists};
    }
};

// ---------------------------------------------------------------------------
// pybind11 module definition
// ---------------------------------------------------------------------------
PYBIND11_MODULE(_arena_hnswlib_ext, m) {
    m.doc() = "arena-hnswlib Python bindings";

    py::class_<PyBruteForce>(m, "BruteForce")
        .def(py::init<size_t, size_t, const std::string&>(),
             py::arg("dim"),
             py::arg("max_elements") = 1000000,
             py::arg("metric") = "l2")
        .def("add_items", &PyBruteForce::add_items,
             py::arg("data"), py::arg("ids"))
        .def("query", &PyBruteForce::query,
             py::arg("queries"), py::arg("k"));

    py::class_<PyHNSW>(m, "HNSW")
        .def(py::init<size_t, size_t, size_t, size_t, const std::string&>(),
             py::arg("dim"),
             py::arg("max_elements"),
             py::arg("m") = 16,
             py::arg("ef_construction") = 200,
             py::arg("metric") = "l2")
        .def("add_items", &PyHNSW::add_items,
             py::arg("data"), py::arg("ids"))
        .def("query", &PyHNSW::query,
             py::arg("queries"), py::arg("k"))
        .def_property("ef_search", &PyHNSW::get_ef_search, &PyHNSW::set_ef_search);
}
