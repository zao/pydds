#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "process_image.hpp"

namespace py = pybind11;

PYBIND11_MODULE(dds_sys, m) {
    m.doc() = "Bindings for DDS decompression and cropping.";

    py::class_<Image, std::shared_ptr<Image>>(m, "Image")
        .def("pixels",
             [](Image const &img) -> py::bytes {
                 return py::bytes(reinterpret_cast<char const *>(img.data.data()), img.data.size());
             })
        .def_readonly("components", &Image::components)
        .def("extent", [](Image const &img) -> py::tuple { return py::make_tuple(img.extent.x, img.extent.y); });

    m.def(
        "decompress_with_crop",
        [](py::bytes const &srcData, std::optional<std::array<int, 4>> crop) -> std::shared_ptr<Image> {
            py::buffer_info info = py::buffer(srcData).request();

            gsl::span srcSpan(reinterpret_cast<uint8_t const *>(info.ptr), static_cast<size_t>(info.size));
            std::optional<Rect> cropRect;
            if (crop) {
                auto [x0, y0, w, h] = *crop;
                cropRect = Rect{gli::ivec2(x0, y0), gli::ivec2(w, h)};
            }
            return ConvertCommand(srcSpan, cropRect);
        },
        py::arg("src_data"), py::arg("crop") = py::none());
}