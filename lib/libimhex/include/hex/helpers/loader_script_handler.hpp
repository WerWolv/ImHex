#pragma once

#include <string>
#include <string_view>

#include <hex/helpers/fs.hpp>

struct _object;
typedef struct _object PyObject;

namespace hex {

    namespace prv {
        class Provider;
    }

    class LoaderScript {
    public:
        LoaderScript() = delete;

        static bool processFile(const std::fs::path &scriptPath);

        static void setFilePath(const std::fs::path &filePath) { LoaderScript::s_filePath = filePath; }
        static void setDataProvider(prv::Provider *provider) { LoaderScript::s_dataProvider = provider; }

    private:
        static inline std::fs::path s_filePath;
        static inline prv::Provider *s_dataProvider;

        static PyObject *Py_getFilePath(PyObject *self, PyObject *args);
        static PyObject *Py_addPatch(PyObject *self, PyObject *args);
        static PyObject *Py_addBookmark(PyObject *self, PyObject *args);

        static PyObject *Py_addStruct(PyObject *self, PyObject *args);
        static PyObject *Py_addUnion(PyObject *self, PyObject *args);
    };

}