#if defined(__declspec)
    #undef __declspec
    #define __declspec(x)
#endif
#include <Python.h>

#include <loaders/loader.hpp>

#define FUNCTION_DEFINITION(ret, name, args1, args2) \
    decltype(name) *name##Func = nullptr;   \
    extern "C" ret name args1 {     \
        return name##Func args2;     \
    }
#define INIT_FUNCTION(name) name##Func = hex::script::loader::getExport<decltype(name##Func)>(pythonLibrary, #name)

FUNCTION_DEFINITION(void, PyPreConfig_InitPythonConfig, (PyPreConfig *config), (config))
FUNCTION_DEFINITION(PyStatus, Py_PreInitialize, (const PyPreConfig *src_config), (src_config))
FUNCTION_DEFINITION(int, PyStatus_Exception, (PyStatus err), (err))
FUNCTION_DEFINITION(void, Py_Initialize, (), ())
FUNCTION_DEFINITION(void, Py_Finalize, (), ())
FUNCTION_DEFINITION(PyInterpreterState *, PyInterpreterState_Get, (), ())
FUNCTION_DEFINITION(PyThreadState *, PyEval_SaveThread, (), ())
FUNCTION_DEFINITION(void, PyEval_RestoreThread, (PyThreadState * state), (state))
FUNCTION_DEFINITION(void, PyErr_Fetch, (PyObject **ptype, PyObject **pvalue, PyObject **ptraceback), (ptype, pvalue, ptraceback))
FUNCTION_DEFINITION(void, PyErr_NormalizeException, (PyObject **ptype, PyObject **pvalue, PyObject **ptraceback), (ptype, pvalue, ptraceback))
FUNCTION_DEFINITION(PyObject *, PyUnicode_FromString, (const char *u), (u))
FUNCTION_DEFINITION(PyObject *, PyImport_Import, (PyObject *name), (name))
FUNCTION_DEFINITION(void, _Py_Dealloc, (PyObject *pobj), (pobj))
FUNCTION_DEFINITION(PyObject *, PyModule_GetDict, (PyObject *pobj), (pobj))
FUNCTION_DEFINITION(PyObject *, PyDict_GetItemString, (PyObject *dp, const char *key), (dp, key))
FUNCTION_DEFINITION(int, PyCallable_Check, (PyObject *dp), (dp))
FUNCTION_DEFINITION(PyObject *, PyTuple_New, (Py_ssize_t len), (len))
FUNCTION_DEFINITION(int, PyTuple_SetItem, (PyObject *p, Py_ssize_t pos, PyObject *o), (p, pos, o))
FUNCTION_DEFINITION(PyObject *, PyObject_CallObject, (PyObject *callable, PyObject *args), (callable, args))
FUNCTION_DEFINITION(PyObject *, PyUnicode_Join, (PyObject *separator, PyObject *iterable), (separator, iterable))
FUNCTION_DEFINITION(const char *, PyUnicode_AsUTF8, (PyObject *unicode), (unicode))
FUNCTION_DEFINITION(void, PyErr_Clear, (), ())
FUNCTION_DEFINITION(int, PyModule_AddStringConstant, (PyObject *module, const char *name, const char *value), (module, name, value))
FUNCTION_DEFINITION(PyObject *, PyEval_GetBuiltins, (), ())
FUNCTION_DEFINITION(int, PyDict_SetItemString, (PyObject *dp, const char *key, PyObject *item), (dp, key, item))
FUNCTION_DEFINITION(PyObject *, PyRun_StringFlags, (const char *str, int start, PyObject *globals, PyObject *locals, PyCompilerFlags *flags), (str, start, globals, locals, flags))
FUNCTION_DEFINITION(void, PyThreadState_Clear, (PyThreadState *tstate), (tstate))
FUNCTION_DEFINITION(void, PyThreadState_DeleteCurrent, (), ())
FUNCTION_DEFINITION(PyThreadState *, PyThreadState_New, (PyInterpreterState *interp), (interp))
FUNCTION_DEFINITION(PyObject *, PyImport_AddModule, (const char *name), (name))
FUNCTION_DEFINITION(PyObject *, PyModule_New, (const char *name), (name))
FUNCTION_DEFINITION(PyObject *, PyObject_GetAttrString, (PyObject *pobj, const char *name), (pobj, name))
FUNCTION_DEFINITION(int, PyObject_HasAttrString, (PyObject *pobj, const char *name), (pobj, name))

bool initPythonLoader() {
    void *pythonLibrary = nullptr;
    for (const std::fs::path &path : { std::fs::path(PYTHON_LIBRARY_PATH), std::fs::path(PYTHON_LIBRARY_PATH).filename() }) {
        pythonLibrary = hex::script::loader::loadLibrary(path.c_str());
        if (pythonLibrary != nullptr) {
            break;
        }
    }

    if (pythonLibrary == nullptr)
        return false;

    INIT_FUNCTION(PyPreConfig_InitPythonConfig);
    INIT_FUNCTION(Py_PreInitialize);
    INIT_FUNCTION(Py_Initialize);
    INIT_FUNCTION(Py_Finalize);

    INIT_FUNCTION(PyEval_SaveThread);
    INIT_FUNCTION(PyEval_RestoreThread);
    INIT_FUNCTION(PyEval_GetBuiltins);
    INIT_FUNCTION(PyRun_StringFlags);

    INIT_FUNCTION(PyErr_Fetch);
    INIT_FUNCTION(PyErr_NormalizeException);
    INIT_FUNCTION(PyErr_Clear);
    INIT_FUNCTION(PyStatus_Exception);

    INIT_FUNCTION(PyThreadState_Clear);
    INIT_FUNCTION(PyThreadState_DeleteCurrent);
    INIT_FUNCTION(PyThreadState_New);
    INIT_FUNCTION(PyInterpreterState_Get);

    INIT_FUNCTION(PyUnicode_FromString);
    INIT_FUNCTION(PyUnicode_Join);
    INIT_FUNCTION(PyUnicode_AsUTF8);

    INIT_FUNCTION(PyTuple_New);
    INIT_FUNCTION(PyTuple_SetItem);

    INIT_FUNCTION(PyImport_Import);
    INIT_FUNCTION(PyImport_AddModule);

    INIT_FUNCTION(PyModule_New);
    INIT_FUNCTION(PyModule_AddStringConstant);
    INIT_FUNCTION(PyModule_GetDict);

    INIT_FUNCTION(PyDict_GetItemString);
    INIT_FUNCTION(PyDict_SetItemString);

    INIT_FUNCTION(PyCallable_Check);
    INIT_FUNCTION(PyObject_CallObject);
    INIT_FUNCTION(PyObject_GetAttrString);
    INIT_FUNCTION(PyObject_HasAttrString);


    INIT_FUNCTION(_Py_Dealloc);


    return true;
}