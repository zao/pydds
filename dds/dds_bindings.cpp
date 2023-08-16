#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "process_image.hpp"

#include <stdexcept>

struct ImageHandleObject {
    PyObject_HEAD
    Image* img;
};

static void ImageHandle_dealloc(ImageHandleObject* self) {
    delete self->img;
    self->img = nullptr;
    ((freefunc)PyType_GetSlot(Py_TYPE(self), Py_tp_free))((PyObject*)self);
}

static PyObject* ImageHandle_extent(ImageHandleObject* self, void* closure) {
    auto extent = self->img->extent;
    return Py_BuildValue("ii", extent.x, extent.y);
}

static PyObject* ImageHandle_pixels(ImageHandleObject* self, void* closure) {
    auto& pixels = self->img->data;
    char const* data_ptr = reinterpret_cast<char const*>(pixels.data());
    Py_ssize_t data_len = static_cast<Py_ssize_t>(pixels.size());
    return Py_BuildValue("y#", data_ptr, data_len);
}

static PyMethodDef ImageHandle_methods[] = {
    {"extent", (PyCFunction)ImageHandle_extent, METH_NOARGS, "image size in pixels"},
    {"pixels", (PyCFunction)ImageHandle_pixels, METH_NOARGS, "raw image data"},
    {nullptr},
};

static PyObject* ImageHandle_getcomponents(ImageHandleObject* self, void* closure) {
    return PyLong_FromLong(self->img->components);
}

static PyGetSetDef ImageHandle_getsetters[] = {
    {"components", (getter)ImageHandle_getcomponents, nullptr, "components per pixel", nullptr},
    {nullptr},
};

static PyTypeObject* ImageHandleType;

static PyObject* dds_sys_decompress_with_crop(PyObject* self, PyObject* args) {
    char const* src_data;
    Py_ssize_t src_len;
    PyObject* cropTuple{};

    // Parse out the outer arguments.
    if (!PyArg_ParseTuple(args, "y#O", &src_data, &src_len, &cropTuple)) {
        return nullptr;
    }

    gsl::span srcSpan(reinterpret_cast<uint8_t const *>(src_data), static_cast<size_t>(src_len));
    int x0, y0, w, h;
    std::optional<Rect> cropRect;

    // Pull apart the optional crop tuple if present.
    if (cropTuple != Py_None) {
        if (!PyArg_ParseTuple(cropTuple, "iiii", &x0, &y0, &w, &h)) {
            return nullptr;
        }
        cropRect = Rect{gli::ivec2(x0, y0), gli::ivec2(w, h)};
    }

    std::unique_ptr<Image> img;
    try {
        img = ConvertCommand(srcSpan, cropRect);
    } catch (std::runtime_error &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }

    auto* ret = (ImageHandleObject*)PyObject_New(ImageHandleObject, ImageHandleType);
    if (ret) {
        ret->img = img.release();
        return (PyObject*)ret;
    }

    Py_RETURN_NONE;
}

static PyMethodDef DdsSysMethods[] = {
    {"decompress_with_crop", dds_sys_decompress_with_crop, METH_VARARGS, "Decode a DDS file with optional cropping."},
    {nullptr, nullptr, 0, nullptr},
};

static char const* dds_sys_doc = PyDoc_STR("Bindings around Compressonator and image-process for DDS.");

static struct PyModuleDef dds_sysmodule = {
    PyModuleDef_HEAD_INIT,
    "dds_sys",
    dds_sys_doc,
    -1,
    DdsSysMethods,
};

static PyType_Slot ImageHandleSlots[] = {
    {Py_tp_dealloc, (void*)(destructor)ImageHandle_dealloc},
    {Py_tp_methods, ImageHandle_methods},
    {Py_tp_getset, ImageHandle_getsetters},
    {Py_tp_doc, (void*)PyDoc_STR("Image handle")},
    {0},
};

static PyType_Spec ImageHandleSpec = {
    .name = "dds.ImageHandle",
    .basicsize = sizeof(ImageHandleObject),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = ImageHandleSlots,
};

PyMODINIT_FUNC
PyInit_dds_sys(void) {
    PyObject* m;
    m = PyModule_Create(&dds_sysmodule);
    if (!m) {
        return nullptr;
    }

    ImageHandleType = (PyTypeObject*)PyType_FromSpec(&ImageHandleSpec);
    if (!ImageHandleType) {
        Py_DECREF(m);        
        return nullptr;
    }

    Py_INCREF(ImageHandleType);
    if (PyModule_AddObject(m, "ImageHandle", (PyObject*)ImageHandleType) < 0) {
        Py_DECREF(ImageHandleType);
        Py_DECREF(m);
        return nullptr;
    }

    return m;
}
