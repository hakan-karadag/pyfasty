#include "../pyfasty.h"
#include <string.h>

/* Structure pour l'objet proxy d'exécuteur */
typedef struct {
    PyObject_HEAD
    PyObject *executor;  /* Exécuteur sous-jacent */
} PyFastyExecutorProxyObject;

/* Déclarations anticipées */
static PyObject *executor_proxy_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
static int executor_proxy_init(PyFastyExecutorProxyObject *self, PyObject *args, PyObject *kwds);
static void executor_proxy_dealloc(PyFastyExecutorProxyObject *self);
static PyObject *executor_proxy_getattro(PyFastyExecutorProxyObject *self, PyObject *name);
static PyObject *executor_proxy_call(PyObject *self, PyObject *args, PyObject *kwargs);

/* Définition du type */
static PyTypeObject PyFastyExecutorProxyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pyfasty._pyfasty.ExecutorProxy",
    .tp_doc = "Proxy pour exécuteurs avec résolution dynamique d'attributs",
    .tp_basicsize = sizeof(PyFastyExecutorProxyObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = executor_proxy_new,
    .tp_init = (initproc)executor_proxy_init,
    .tp_dealloc = (destructor)executor_proxy_dealloc,
    .tp_getattro = (getattrofunc)executor_proxy_getattro,
    .tp_call = (ternaryfunc)executor_proxy_call,
};

/* FONCTION OPTIMISÉE : Résolution d'attributs intelligente et généraliste */
static PyObject *resolve_attribute_dynamically(const char *name) {
    /* Étape 1: Chercher dans sys.modules */
    PyObject *sys_modules = PySys_GetObject("modules");
    if (sys_modules != NULL) {
        PyObject *name_obj = PyUnicode_FromString(name);
        if (name_obj) {
            PyObject *module = PyDict_GetItem(sys_modules, name_obj);
            Py_DECREF(name_obj);
            
            if (module != NULL) {
                Py_INCREF(module);
                return module;
            }
        }
    }
    
    /* Étape 2: Chercher dans la frame courante */
    PyObject *frame = PyEval_GetFrame();
    if (frame != NULL) {
        /* Vérifier les locales */
        PyObject *locals = PyObject_GetAttrString(frame, "f_locals");
        if (locals != NULL) {
            PyObject *name_obj = PyUnicode_FromString(name);
            if (name_obj) {
                PyObject *value = PyDict_GetItem(locals, name_obj);
                Py_DECREF(name_obj);
                
                if (value != NULL) {
                    Py_INCREF(value);
                    Py_DECREF(locals);
                    return value;
                }
            }
            Py_DECREF(locals);
        }
        
        /* Vérifier les globales */
        PyObject *globals = PyObject_GetAttrString(frame, "f_globals");
        if (globals != NULL) {
            PyObject *name_obj = PyUnicode_FromString(name);
            if (name_obj) {
                PyObject *value = PyDict_GetItem(globals, name_obj);
                Py_DECREF(name_obj);
                Py_DECREF(globals);
                
                if (value != NULL) {
                    Py_INCREF(value);
                    return value;
                }
            } else {
                Py_DECREF(globals);
            }
        }
    }
    
    return NULL;
}

/* DÉTECTION 100% GÉNÉRALISTE : Analyse dynamique des patterns d'erreur */
static int should_raise_attribute_error(const char *name, PyObject *executor_obj) {
    if (!name) return 0;
    
    /* APPROCHE GÉNÉRALISTE : Essayer d'abord l'accès normal */
    if (executor_obj) {
        PyObject *name_obj = PyUnicode_FromString(name);
        if (name_obj) {
            PyObject *attr = PyObject_GetAttr(executor_obj, name_obj);
            Py_DECREF(name_obj);
            
            if (attr) {
                /* L'attribut existe réellement */
                Py_DECREF(attr);
                return 0;
            } else {
                /* L'attribut n'existe pas - laisser l'erreur Python standard se propager */
                PyErr_Clear();
                return 1;
            }
        }
    }
    
    return 0;
}

/* Implémentation de executor_proxy_new */
static PyObject *executor_proxy_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PyFastyExecutorProxyObject *self = (PyFastyExecutorProxyObject *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->executor = NULL;
    }
    return (PyObject *)self;
}

/* Implémentation de executor_proxy_init */
static int executor_proxy_init(PyFastyExecutorProxyObject *self, PyObject *args, PyObject *kwds) {
    PyObject *executor;
    static char *kwlist[] = {"executor", NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &executor)) {
        return -1;
    }
    
    Py_XINCREF(executor);
    Py_XDECREF(self->executor);
    self->executor = executor;
    
    return 0;
}

/* Implémentation de executor_proxy_dealloc */
static void executor_proxy_dealloc(PyFastyExecutorProxyObject *self) {
    Py_XDECREF(self->executor);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

/* IMPLÉMENTATION 100% GÉNÉRALISTE ET OPTIMISÉE */
static PyObject *executor_proxy_getattro(PyFastyExecutorProxyObject *self, PyObject *name) {
    const char *name_str = PyUnicode_AsUTF8(name);
    if (!name_str) {
        return NULL;
    }
    
    /* Gestion rapide des attributs spéciaux */
    if (name_str[0] == '_') {
        return PyObject_GenericGetAttr((PyObject *)self, name);
    }
    
    /* ÉTAPE 1 : Résolution dynamique intelligente */
    PyObject *resolved = resolve_attribute_dynamically(name_str);
    if (resolved != NULL) {
        return resolved;
    }
    
    /* ÉTAPE 2 : Délégation avec gestion d'erreur généraliste */
    if (self->executor) {
        PyObject *result = PyObject_GetAttr(self->executor, name);
        if (result) {
            return result;
        }
        
        /* GÉNÉRALISTE : L'erreur Python standard est la meilleure approche */
        /* Laisser l'AttributeError standard se propager - c'est le comportement le plus généraliste */
        return NULL;
    }
    
    /* Pas d'exécuteur configuré */
                PyErr_Format(PyExc_AttributeError, "Executor proxy not configured for attribute '%s'", name_str);
    return NULL;
}

/* Implémentation de executor_proxy_call */
static PyObject *executor_proxy_call(PyObject *self, PyObject *args, PyObject *kwargs) {
    PyFastyExecutorProxyObject *proxy = (PyFastyExecutorProxyObject *)self;
    
    if (!proxy->executor) {
        PyErr_SetString(PyExc_RuntimeError, "Executor proxy not configured");
        return NULL;
    }
    
    /* Appeler l'exécuteur sous-jacent */
    return PyObject_Call(proxy->executor, args, kwargs);
}

/* Variables globales pour les instances */
static PyObject *g_sync_executor_proxy = NULL;
static PyObject *g_async_executor_proxy = NULL;

/* INITIALISATION OPTIMISÉE ET ROBUSTE */
int PyFasty_ExecutorProxy_Init(PyObject *module) {
    /* Préparer le type */
    if (PyType_Ready(&PyFastyExecutorProxyType) < 0) {
        return -1;
    }
    
    /* Ajouter le type au module */
    Py_INCREF(&PyFastyExecutorProxyType);
    if (PyModule_AddObject(module, "ExecutorProxy", (PyObject *)&PyFastyExecutorProxyType) < 0) {
        Py_DECREF(&PyFastyExecutorProxyType);
        return -1;
    }
    
    /* Récupérer les exécuteurs originaux */
    PyObject *sync_executor = PyObject_GetAttrString(module, "sync_executor");
    PyObject *async_executor = PyObject_GetAttrString(module, "async_executor");
    
    if (!sync_executor || !async_executor) {
        Py_XDECREF(sync_executor);
        Py_XDECREF(async_executor);
        return -1;
    }
    
    /* Créer les proxys */
    PyObject *args = Py_BuildValue("(O)", sync_executor);
    if (args) {
        g_sync_executor_proxy = PyObject_Call((PyObject *)&PyFastyExecutorProxyType, args, NULL);
        Py_DECREF(args);
    }
    
    args = Py_BuildValue("(O)", async_executor);
    if (args) {
        g_async_executor_proxy = PyObject_Call((PyObject *)&PyFastyExecutorProxyType, args, NULL);
        Py_DECREF(args);
    }
    
    Py_DECREF(sync_executor);
    Py_DECREF(async_executor);
    
    if (!g_sync_executor_proxy || !g_async_executor_proxy) {
        Py_XDECREF(g_sync_executor_proxy);
        Py_XDECREF(g_async_executor_proxy);
        return -1;
    }
    
    /* Remplacer les exécuteurs dans le module */
    if (PyObject_DelAttrString(module, "sync_executor") < 0 ||
        PyObject_DelAttrString(module, "async_executor") < 0) {
        Py_DECREF(g_sync_executor_proxy);
        Py_DECREF(g_async_executor_proxy);
        return -1;
    }
    
    if (PyModule_AddObject(module, "sync_executor", g_sync_executor_proxy) < 0 ||
        PyModule_AddObject(module, "async_executor", g_async_executor_proxy) < 0) {
        Py_XDECREF(g_sync_executor_proxy);
        Py_XDECREF(g_async_executor_proxy);
        g_sync_executor_proxy = NULL;
        g_async_executor_proxy = NULL;
        return -1;
    }
    
    return 0;
}
