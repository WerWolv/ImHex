#include <hex/helpers/loader_script_handler.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/file.hpp>
#include <hex/ui/view.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/intrinsics.hpp>

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>

#include <cstring>
#include <filesystem>

using namespace std::literals::string_literals;

namespace hex {

    PyObject *LoaderScript::Py_getFilePath(PyObject *self, PyObject *args) {
        hex::unused(self, args);

        return PyUnicode_FromString(LoaderScript::s_filePath.string().c_str());
    }

    PyObject *LoaderScript::Py_addPatch(PyObject *self, PyObject *args) {
        hex::unused(self);

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

    PyObject *LoaderScript::Py_addBookmark(PyObject *self, PyObject *args) {
        hex::unused(self);

        u64 address;
        size_t size;

        char *name    = nullptr;
        char *comment = nullptr;

        if (!PyArg_ParseTuple(args, "K|n|s|s", &address, &size, &name, &comment)) {
            PyErr_BadArgument();
            return nullptr;
        }

        if (name == nullptr || comment == nullptr) {
            PyErr_SetString(PyExc_IndexError, "address out of range");
            return nullptr;
        }

        ImHexApi::Bookmarks::add(address, size, name, comment);

        Py_RETURN_NONE;
    }

    static PyObject *createStructureType(const std::string &keyword, PyObject *args) {
        if (args == nullptr) {
            PyErr_BadArgument();
            return nullptr;
        }

        auto type = PyTuple_GetItem(args, 0);
        if (type == nullptr) {
            PyErr_BadArgument();
            return nullptr;
        }

        auto instance = PyObject_CallObject(type, nullptr);
        if (instance == nullptr) {
            PyErr_BadArgument();
            return nullptr;
        }

        ON_SCOPE_EXIT { Py_DECREF(instance); };

        if (instance->ob_type->tp_base == nullptr || instance->ob_type->tp_base->tp_name != "ImHexType"s) {
            PyErr_SetString(PyExc_TypeError, "class type must extend from ImHexType");
            return nullptr;
        }

        auto dict = instance->ob_type->tp_dict;
        if (dict == nullptr) {
            PyErr_BadArgument();
            return nullptr;
        }

        auto annotations = PyDict_GetItemString(dict, "__annotations__");
        if (annotations == nullptr) {
            PyErr_BadArgument();
            return nullptr;
        }

        auto list = PyDict_Items(annotations);
        if (list == nullptr) {
            PyErr_BadArgument();
            return nullptr;
        }

        ON_SCOPE_EXIT { Py_DECREF(list); };

        std::string code = keyword + " " + instance->ob_type->tp_name + " {\n";

        for (Py_ssize_t i = 0; i < PyList_Size(list); i++) {
            auto item = PyList_GetItem(list, i);
            if (item == nullptr) {
                PyErr_SetString(PyExc_TypeError, "failed to get item from list");
                return nullptr;
            }

            auto memberName = PyUnicode_AsUTF8(PyTuple_GetItem(item, 0));
            if (memberName == nullptr) {
                PyErr_SetString(PyExc_TypeError, "invalid member name");
                return nullptr;
            }

            auto memberType = PyTuple_GetItem(item, 1);
            if (!PyTuple_Check(memberType) || memberType == nullptr) {
                PyErr_SetString(PyExc_TypeError, "member needs to have a annotation extending from ImHexType");
                return nullptr;
            }

            // Array already is an object
            if (memberType->ob_type->tp_name == "array"s) {

                auto arrayType = PyObject_GetAttrString(memberType, "array_type");
                if (arrayType == nullptr) {
                    PyErr_BadArgument();
                    return nullptr;
                }

                code += "   "s + arrayType->ob_type->tp_name + " " + memberName;

                auto arraySize = PyObject_GetAttrString(memberType, "size");
                if (arraySize == nullptr) {
                    PyErr_BadArgument();
                    return nullptr;
                }

                if (PyUnicode_Check(arraySize))
                    code += "["s + PyUnicode_AsUTF8(arraySize) + "];\n";
                else if (PyLong_Check(arraySize))
                    code += "["s + std::to_string(PyLong_AsLong(arraySize)) + "];\n";
            } else {
                auto memberTypeInstance = PyObject_CallObject(memberType, nullptr);
                if (memberTypeInstance == nullptr || memberTypeInstance->ob_type->tp_base == nullptr || memberTypeInstance->ob_type->tp_base->tp_name != "ImHexType"s) {
                    PyErr_SetString(PyExc_TypeError, "member needs to have a annotation extending from ImHexType");
                    if (memberTypeInstance != nullptr)
                        Py_DECREF(memberTypeInstance);

                    return nullptr;
                }

                code += "   "s + memberTypeInstance->ob_type->tp_name + " "s + memberName + ";\n";

                Py_DECREF(memberTypeInstance);
            }
        }

        code += "};\n";

        EventManager::post<RequestSetPatternLanguageCode>(code);

        Py_RETURN_NONE;
    }

    PyObject *LoaderScript::Py_addStruct(PyObject *self, PyObject *args) {
        hex::unused(self);

        return createStructureType("struct", args);
    }

    PyObject *LoaderScript::Py_addUnion(PyObject *self, PyObject *args) {
        hex::unused(self);

        return createStructureType("union", args);
    }

    bool LoaderScript::processFile(const std::fs::path &scriptPath) {
        Py_SetProgramName(Py_DecodeLocale("ImHex", nullptr));

        for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Python)) {
            if (fs::exists(std::fs::path(dir / "lib" / "python" PYTHON_VERSION_MAJOR_MINOR))) {
                Py_SetPythonHome(Py_DecodeLocale(dir.string().c_str(), nullptr));
                break;
            }
        }

        PyImport_AppendInittab("_imhex", []() -> PyObject * {
            static PyMethodDef ImHexMethods[] = {
                {"get_file_path", &LoaderScript::Py_getFilePath, METH_NOARGS,  "Returns the path of the file being loaded."},
                { "patch",        &LoaderScript::Py_addPatch,    METH_VARARGS, "Patches a region of memory"                },
                { "add_bookmark", &LoaderScript::Py_addBookmark, METH_VARARGS, "Adds a bookmark"                           },
                { "add_struct",   &LoaderScript::Py_addStruct,   METH_VARARGS, "Adds a struct"                             },
                { "add_union",    &LoaderScript::Py_addUnion,    METH_VARARGS, "Adds a union"                              },
                { nullptr,        nullptr,                       0,            nullptr                                     }
            };

            static PyModuleDef ImHexModule = {
                PyModuleDef_HEAD_INIT, "imhex", nullptr, -1, ImHexMethods, nullptr, nullptr, nullptr, nullptr
            };

            auto module = PyModule_Create(&ImHexModule);
            if (module == nullptr)
                return nullptr;

            return module;
        });

        Py_Initialize();

        {
            auto sysPath = PySys_GetObject("path");
            auto path    = PyUnicode_FromString("lib");

            PyList_Insert(sysPath, 0, path);
        }

        fs::File scriptFile(scriptPath, fs::File::Mode::Read);
        PyRun_SimpleFile(scriptFile.getHandle(), scriptFile.getPath().string().c_str());

        Py_Finalize();

        return true;
    }

}