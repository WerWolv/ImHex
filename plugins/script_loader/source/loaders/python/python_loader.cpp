#include <loaders/python/python_loader.hpp>

#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>
#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>
#include <wolv/utils/string.hpp>

#include <hex/helpers/utils.hpp>
#include <romfs/romfs.hpp>

#if defined(__declspec)
    #undef __declspec
    #define __declspec(x)
#endif
#include <Python.h>

bool initPythonLoader();

namespace hex::script::loader {

    static PyInterpreterState *mainThreadState;

    bool PythonLoader::initialize() {
        if (!initPythonLoader())
            return false;

        PyPreConfig preconfig;
        PyPreConfig_InitPythonConfig(&preconfig);

        preconfig.utf8_mode = 1;

        auto status = Py_PreInitialize(&preconfig);
        if (PyStatus_Exception(status)) {
            return false;
        }

        Py_Initialize();
        mainThreadState = PyInterpreterState_Get();
        PyEval_SaveThread();
        return true;
    }

    std::string getCurrentTraceback() {
        PyObject *ptype, *pvalue, *ptraceback;
        PyErr_Fetch(&ptype, &pvalue, &ptraceback);
        PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);

        PyObject *pModuleName = PyUnicode_FromString("traceback");
        PyObject *pModule = PyImport_Import(pModuleName);
        Py_DECREF(pModuleName);

        if (pModule != nullptr) {
            PyObject *pDict = PyModule_GetDict(pModule);
            PyObject *pFunc = PyDict_GetItemString(pDict, "format_exception");

            if (pFunc && PyCallable_Check(pFunc)) {
                PyObject *pArgs = PyTuple_New(3);
                PyTuple_SetItem(pArgs, 0, ptype);
                PyTuple_SetItem(pArgs, 1, pvalue);
                PyTuple_SetItem(pArgs, 2, ptraceback);

                PyObject *pResult = PyObject_CallObject(pFunc, pArgs);
                Py_DECREF(pArgs);

                if (pResult != NULL) {
                    const char *errorMessage = PyUnicode_AsUTF8(PyUnicode_Join(PyUnicode_FromString(""), pResult));
                    Py_DECREF(pResult);
                    Py_DECREF(pModule);
                    return errorMessage;
                }
            }
            Py_DECREF(pModule);
        }

        PyErr_Clear();
        return "";
    }

    void populateModule(PyObject *pyModule, const std::string &sourceCode) {
        PyModule_AddStringConstant(pyModule, "__file__", "");

        PyObject *localDict = PyModule_GetDict(pyModule);
        PyObject *builtins = PyEval_GetBuiltins();

        PyDict_SetItemString(localDict, "__builtins__", builtins);

        PyErr_Clear();
        PyObject *pyValue = PyRun_String(sourceCode.c_str(), Py_file_input, localDict, localDict);
        if (pyValue != nullptr) {
            Py_DECREF(pyValue);
        } else {
            log::error("{}", getCurrentTraceback());
        }
    }

    bool PythonLoader::loadAll() {
        this->clearScripts();

        for (const auto &imhexPath : hex::fs::getDefaultPaths(hex::fs::ImHexPath::Scripts)) {
            auto directoryPath = imhexPath / "custom" / "python";
            if (!wolv::io::fs::exists(directoryPath))
                wolv::io::fs::createDirectories(directoryPath);

            if (!wolv::io::fs::exists(directoryPath))
                continue;

            for (const auto &entry : std::fs::directory_iterator(directoryPath)) {
                if (!entry.is_directory())
                    continue;

                const auto scriptPath = entry.path() / "main.py";
                if (!std::fs::exists(scriptPath))
                    continue;

                auto pathString = wolv::util::toUTF8String(scriptPath);
                wolv::io::File scriptFile(pathString, wolv::io::File::Mode::Read);
                if (!scriptFile.isValid())
                    continue;

                this->addScript(entry.path().stem().string(), [pathString, scriptContent = scriptFile.readString()] {
                    PyThreadState* ts = PyThreadState_New(mainThreadState);
                    PyEval_RestoreThread(ts);

                    ON_SCOPE_EXIT {
                        PyThreadState_Clear(ts);
                        PyThreadState_DeleteCurrent();
                    };
                    PyObject *libraryModule = PyImport_AddModule("imhex");

                    PyModule_AddStringConstant(libraryModule, "__script_loader__", hex::format("{}", (u64)hex::getContainingModule((void*)&getCurrentTraceback)).c_str());
                    populateModule(libraryModule, romfs::get("python/imhex.py").data<const char>());


                    PyObject *mainModule = PyModule_New(pathString.c_str());
                    populateModule(mainModule, scriptContent);

                    Py_DECREF(mainModule);
                });

            }
        }

        return true;
    }

}
