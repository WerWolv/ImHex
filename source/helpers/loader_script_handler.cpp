#include "helpers/loader_script_handler.hpp"

#include "views/view.hpp"
#include "helpers/utils.hpp"
#include "providers/provider.hpp"

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <cstring>
#include <filesystem>

namespace hex {

    PyObject* LoaderScript::Py_getFilePath(PyObject *self, PyObject *args) {
        return PyUnicode_FromString(LoaderScript::s_filePath.c_str());
    }

    PyObject* LoaderScript::Py_addPatch(PyObject *self, PyObject *args) {
        u64 address;
        u8 *patches;
        Py_ssize_t count;

        if (!PyArg_ParseTuple(args, "K|y#", &address, &patches, &count)) {
            PyErr_BadArgument();
            return nullptr;
        }

        if (patches == nullptr || count == 0) {
            PyErr_SetString(PyExc_TypeError, "Invalid patch provided");
            return nullptr;
        }

        if (address >= LoaderScript::s_dataProvider->getActualSize()) {
            PyErr_SetString(PyExc_IndexError, "address out of range");
            return nullptr;
        }

        LoaderScript::s_dataProvider->write(address, patches, count);

        Py_RETURN_NONE;
    }

    PyObject* LoaderScript::Py_addBookmark(PyObject *self, PyObject *args) {
        Bookmark bookmark;

        char *name = nullptr;
        char *comment = nullptr;

        if (!PyArg_ParseTuple(args, "K|n|s|s", &bookmark.region.address, &bookmark.region.size, &name, &comment)) {
            PyErr_BadArgument();
            return nullptr;
        }

        if (name == nullptr || comment == nullptr) {
            PyErr_SetString(PyExc_IndexError, "address out of range");
            return nullptr;
        }

        std::copy(name, name + std::strlen(name), std::back_inserter(bookmark.name));
        std::copy(comment, comment + std::strlen(comment), std::back_inserter(bookmark.comment));

        View::postEvent(Events::AddBookmark, &bookmark);

        Py_RETURN_NONE;
    }

    bool LoaderScript::processFile(std::string_view scriptPath) {
        Py_SetProgramName(Py_DecodeLocale(__argv[0], nullptr));

        Py_SetPythonHome(Py_DecodeLocale(std::filesystem::path(__argv[0]).parent_path().string().c_str(), nullptr));

        PyImport_AppendInittab("imhex", []() -> PyObject* {
            static PyMethodDef ImHexMethods[] = {
                { "get_file_path", &LoaderScript::Py_getFilePath, METH_NOARGS, "Returns the path of the file being loaded." },
                { "patch", &LoaderScript::Py_addPatch, METH_VARARGS, "Patches a region of memory" },
                { "add_bookmark", &LoaderScript::Py_addBookmark, METH_VARARGS, "Adds a bookmark" },
                { nullptr, nullptr, 0, nullptr }
            };

            static PyModuleDef ImHexModule = {
                PyModuleDef_HEAD_INIT, "imhex", nullptr, -1, ImHexMethods, nullptr, nullptr, nullptr, nullptr
            };

            return PyModule_Create(&ImHexModule);
        });

        Py_Initialize();

        FILE *scriptFile = fopen(scriptPath.data(), "r");
        PyRun_SimpleFile(scriptFile, scriptPath.data());

        fclose(scriptFile);

        Py_Finalize();

        return true;
    }

}