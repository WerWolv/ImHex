#pragma once

#include <string>
#include <string_view>

struct _object;
typedef struct _object PyObject;

namespace hex {

    namespace prv { class Provider; }

    class LoaderScript {
    public:
        LoaderScript() = delete;

        static bool processFile(std::string_view scriptPath);

        static void setFilePath(std::string_view filePath) { LoaderScript::s_filePath = filePath; }
        static void setDataProvider(prv::Provider* provider) { LoaderScript::s_dataProvider = provider; }
    private:
        static inline std::string s_filePath;
        static inline prv::Provider* s_dataProvider;

        static PyObject* Py_getFilePath(PyObject *self, PyObject *args);
        static PyObject* Py_addPatch(PyObject *self, PyObject *args);
        static PyObject* Py_addBookmark(PyObject *self, PyObject *args);
    };

}