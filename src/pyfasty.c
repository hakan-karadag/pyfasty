#include "pyfasty.h"
#include "thread/pyfasty_threading.h"
#include <string.h>  /* Pour strdup */

/* INCLUDES POUR PROTECTION ATOMIQUE */
#ifdef PYFASTY_WINDOWS
#include <windows.h>  /* Pour Sleep() */
#else
#include <unistd.h>   /* Pour usleep() */
#endif

/* VARIABLES GLOBALES OPTIMISÉES */

/* Exception globale */
PyObject *PyFastyError;

/* Pointeur de fonction pour la mise à jour globale de la config */
PyFasty_SetGlobalConfigFunc PyFasty_SetGlobalConfig = NULL;

/* VARIABLES DE CONFIGURATION DYNAMIQUE */
int g_pyfasty_default_pool_size = PYFASTY_DEFAULT_POOL_SIZE_VALUE;
int g_pyfasty_max_recursion_depth = PYFASTY_MAX_RECURSION_DEPTH_VALUE;
int g_pyfasty_attr_cache_size = PYFASTY_ATTR_CACHE_SIZE_VALUE;

/* FONCTIONS DE CONFIGURATION GLOBALE */

int PyFasty_SetDefaultPoolSize(int size) {
    if (size > 0 && size <= 10000) {
        g_pyfasty_default_pool_size = size;
        return 0;
    }
    return -1;
}

int PyFasty_SetMaxRecursionDepth(int depth) {
    if (depth > 0 && depth <= 1000) {
        g_pyfasty_max_recursion_depth = depth;
        return 0;
    }
    return -1;
}

int PyFasty_GetDefaultPoolSize(void) {
    return g_pyfasty_default_pool_size;
}

int PyFasty_GetMaxRecursionDepth(void) {
    return g_pyfasty_max_recursion_depth;
}

int PyFasty_SetAttrCacheSize(int size) {
    if (size > 0 && size <= 64) {
        g_pyfasty_attr_cache_size = size;
        return 0;
    }
    return -1;
}

int PyFasty_GetAttrCacheSize(void) {
    return g_pyfasty_attr_cache_size;
}

/* CACHE OPTIMISÉ POUR LES ÉVÉNEMENTS */

/* Variables statiques pour le cache des événements - optimisées */
static PyObject *g_pyfasty_module = NULL;
static PyObject *g_evaluate_all_func = NULL;
static int g_events_cache_initialized = 0;

/* FONCTIONS UTILITAIRES OPTIMISÉES */

/* Fonction optimisée pour valider un chemin d'accès */
static inline int is_valid_path(const char *path) {
    return path && *path && strlen(path) < (size_t)(g_pyfasty_max_recursion_depth * 10);
}

/* Fonction optimisée pour créer un nom d'attribut Unicode */
static inline PyObject *create_attr_name(const char *token) {
    return PyUnicode_FromString(token);
}

/* IMPLÉMENTATION OPTIMISÉE DE L'ACCÈS PAR CHEMIN */

PyObject* pyfasty_object_get_by_path(PyObject *self, const char *path, 
                                    PyTypeObject *obj_type, 
                                    PyFastyGetAttrFunc get_attr_func) {
    /* Validation optimisée des paramètres */
    if (!self) {
        PyErr_SetString(PyExc_ValueError, "Self object cannot be NULL");
        return NULL;
    }
    
    /* Si le chemin est vide, retourner self */
    if (!is_valid_path(path)) {
        if (!path || !*path) {
            Py_INCREF(self);
            return self;
        }
        PyErr_SetString(PyExc_ValueError, "Invalid or too long path");
        return NULL;
    }
    
    /* Copier le chemin pour manipulation par strtok */
    char *path_copy = strdup(path);
    if (!path_copy) {
        PyErr_NoMemory();
        return NULL;
    }
    
    /* Commencer par l'objet courant */
    PyObject *current = self;
    Py_INCREF(current);
    
    /* Analyser le chemin segment par segment */
    char *saveptr = NULL;
    char *token = PYFASTY_STRTOK_R(path_copy, ".", &saveptr);
    
    /* OPTIMISATION : Compteur de profondeur dynamique pour éviter les boucles infinies */
    int depth = 0;
    
    while (token && depth < g_pyfasty_max_recursion_depth) {
        /* Créer un objet Python pour le nom d'attribut */
        PyObject *attr_name = create_attr_name(token);
        if (!attr_name) {
            Py_DECREF(current);
            free(path_copy);
            return NULL;
        }
        
        /* Accéder à l'attribut - utiliser la fonction appropriée selon le type */
        PyObject *next = NULL;
        
        if (obj_type && PyObject_TypeCheck(current, obj_type) && get_attr_func) {
            /* Pour les objets du type spécifié, utiliser la fonction optimisée */
            next = get_attr_func(current, attr_name);
        } else {
            /* Pour les autres types, accès standard */
            next = PyObject_GetAttr(current, attr_name);
        }
        
        /* Libérer le nom d'attribut */
        Py_DECREF(attr_name);
        
        /* Vérifier les erreurs d'accès */
        if (!next) {
            Py_DECREF(current);
            free(path_copy);
            return NULL;
        }
        
        /* Avancer au segment suivant */
        Py_DECREF(current);
        current = next;
        
        /* Passer au prochain segment */
        token = PYFASTY_STRTOK_R(NULL, ".", &saveptr);
        depth++;
    }
    
    /* Vérifier si on a atteint la limite de profondeur */
    if (depth >= g_pyfasty_max_recursion_depth) {
        PyErr_SetString(PyExc_RecursionError, "Maximum recursion depth reached");
        Py_DECREF(current);
        free(path_copy);
        return NULL;
    }
    
    /* Nettoyer la mémoire */
    free(path_copy);
    
    /* Le résultat final est dans current */
    return current;
}

/* FONCTION OPTIMISÉE POUR __setattr__ */

static PyObject* pyfasty_module_setattr(PyObject* self, PyObject* args) {
    PyObject* name;
    PyObject* value;
    
    if (!PyArg_ParseTuple(args, "OO", &name, &value))
        return NULL;
    
    /* Vérifier si le nom est "config" */
    const char* name_str = PyUnicode_AsUTF8(name);
    if (!name_str)
        return NULL;
    
    if (strcmp(name_str, "config") == 0) {
        /* OPTIMISATION : Cas spécial pour config avec gestion d'erreur améliorée */
        PyObject* core_module = PyImport_ImportModule("pyfasty._pyfasty");
        if (!core_module) {
            PyErr_SetString(PyExc_ImportError, "Impossible d'importer le module core pyfasty._pyfasty");
            return NULL;
        }
        
        int result = PyObject_SetAttr(core_module, name, value);
        Py_DECREF(core_module);
        
        if (result < 0)
            return NULL;
    } else {
        /* OPTIMISATION : Mettre à jour les globals du module Python avec cache */
        static PyObject* cached_pyfasty_module = NULL;
        static PyObject* cached_module_dict = NULL;
        
        if (!cached_pyfasty_module) {
            cached_pyfasty_module = PyImport_ImportModule("pyfasty");
            if (!cached_pyfasty_module)
                return NULL;
            
            cached_module_dict = PyModule_GetDict(cached_pyfasty_module);
            /* module_dict est une borrowed reference */
        }
        
        if (PyDict_SetItem(cached_module_dict, name, value) < 0) {
            return NULL;
        }
    }
    
    Py_RETURN_NONE;
}

/* FONCTION OPTIMISÉE POUR L'INITIALISATION POST-MODULE */

static PyObject* post_init_pyfasty_module(PyObject* self, PyObject* args) {
    /* OPTIMISATION : Vérification préalable pour éviter la double initialisation */
    static int already_initialized = 0;
    if (already_initialized) {
        Py_RETURN_NONE;
    }
    
    /* Créer une fonction Python à partir de notre fonction C */
    static PyMethodDef setattr_method = {"__setattr__", pyfasty_module_setattr, METH_VARARGS, NULL};
    PyObject* setattr_func = PyCFunction_New(&setattr_method, NULL);
    if (!setattr_func)
        return NULL;
    
    /* Obtenir le module Python 'pyfasty' */
    PyObject* pyfasty_module = PyImport_ImportModule("pyfasty");
    if (!pyfasty_module) {
        Py_DECREF(setattr_func);
        return NULL;
    }
    
    /* OPTIMISATION : Accès direct à sys.modules */
    PyObject* sys_modules = PySys_GetObject("modules");
    if (!sys_modules) {
        Py_DECREF(setattr_func);
        Py_DECREF(pyfasty_module);
        return NULL;
    }
    
    /* Récupérer le module pyfasty de sys.modules */
    PyObject* name = PyUnicode_FromString("pyfasty");
    PyObject* pyfasty_in_sys = PyDict_GetItem(sys_modules, name);
    Py_DECREF(name);
    
    if (pyfasty_in_sys) {
        /* Attacher la fonction __setattr__ au module dans sys.modules */
        PyObject_SetAttrString(pyfasty_in_sys, "__setattr__", setattr_func);
    }
    
    Py_DECREF(setattr_func);
    Py_DECREF(pyfasty_module);
    
    already_initialized = 1;
    Py_RETURN_NONE;
}

/* GESTION OPTIMISÉE DES ÉVÉNEMENTS */

/* Fonction optimisée pour initialiser le cache des événements */
static int initialize_events_cache(void) {
    if (g_events_cache_initialized) {
        return 1; /* Déjà initialisé */
    }
    
    /* Libérer les anciennes références si nécessaire */
    Py_XDECREF(g_evaluate_all_func);
    g_evaluate_all_func = NULL;
    
    /* Obtenir le module */
    if (g_pyfasty_module == NULL) {
        g_pyfasty_module = PyImport_ImportModule("pyfasty._pyfasty");
        if (g_pyfasty_module == NULL) {
            PyErr_Clear(); /* Ignorer l'erreur pour la compatibilité */
            return 0;
        }
    }
    
    /* Obtenir la fonction evaluate_all */
    g_evaluate_all_func = PyObject_GetAttrString(g_pyfasty_module, "evaluate_all");
    if (g_evaluate_all_func == NULL) {
        PyErr_Clear(); /* Ignorer l'erreur pour la compatibilité */
        return 0;
    }
    
    g_events_cache_initialized = 1;
    return 1;
}

/* Fonction centralisée et optimisée pour déclencher les événements */
int pyfasty_trigger_events(void) {
    /* Protection contre la récursion */
    static int is_triggering = 0;
    if (is_triggering) return 0;
    
    is_triggering = 1;
    
    /* Appeler d'abord les événements synchrones - avec tous les modules */
    pyfasty_trigger_sync_events_with_module(MODULE_ALL);
    
    int success = 0;
    
    /* OPTIMISATION : Initialisation paresseuse avec cache */
    if (!g_events_cache_initialized) {
        if (!initialize_events_cache()) {
            is_triggering = 0;
            return 0;
        }
    }
    
    /* Appeler la fonction si disponible */
    if (g_evaluate_all_func && PyCallable_Check(g_evaluate_all_func)) {
        PyObject *result = PyObject_CallObject(g_evaluate_all_func, NULL);
        if (result) {
            Py_DECREF(result);
            success = 1;
        } else {
            PyErr_Clear(); /* Ignorer l'erreur pour la compatibilité */
        }
    }
    
    is_triggering = 0;
    return success;
}

/* NETTOYAGE OPTIMISÉ */

/* Fonction optimisée pour nettoyer le cache des événements à la finalisation */
void pyfasty_cleanup_events(void) {
    Py_XDECREF(g_evaluate_all_func);
    g_evaluate_all_func = NULL;
    
    Py_XDECREF(g_pyfasty_module);
    g_pyfasty_module = NULL;
    
    g_events_cache_initialized = 0;
    
    /* Nettoyer le cache des conditions d'événements */
    cleanup_condition_cache();
    
    /* Finaliser le pool de dictionnaires */
    pyfasty_dict_pool_finalize();
}

/* DÉFINITION DES MÉTHODES DU MODULE OPTIMISÉES */

static PyMethodDef PyFasty_Methods[] = {
    {"_post_init_module", post_init_pyfasty_module, METH_NOARGS, 
     "Initialize the Python module with a custom __setattr__"},
     
    /* ÉVÉNEMENTS UNIFIÉS */
    {"event", event_decorator, METH_VARARGS, 
     "Décorateur unifié pour les événements PyFasty"},
    {"event_enable", event_enable, METH_NOARGS, 
     "Active le système d'événements"},
    {"event_disable", event_disable, METH_NOARGS, 
     "Désactive le système d'événements"},
    {"event_evaluate_all", event_evaluate_all, METH_NOARGS, 
     "Évalue les conditions et déclenche les événements"},
    {"event_clear_handlers", event_clear_handlers, METH_NOARGS, 
     "Vide la liste des handlers d'événements"},
     
    /* ALIASES DE COMPATIBILITÉ */
    {"event_sync", event_decorator, METH_VARARGS, 
     "Alias de compatibilité pour event"},
    {"event_sync_enable", event_enable, METH_NOARGS, 
     "Alias de compatibilité pour event_enable"},
    {"event_sync_disable", event_disable, METH_NOARGS, 
     "Alias de compatibilité pour event_disable"},
    {"event_sync_evaluate_all", event_evaluate_all, METH_NOARGS, 
     "Alias de compatibilité pour event_evaluate_all"},
    {"evaluate_all", event_evaluate_all, METH_NOARGS, 
     "Alias pour event_evaluate_all"},
    {"evaluate_executor_sync_only", event_sync_evaluate_executor_only, METH_NOARGS, 
     "Évalue seulement les événements EXECUTOR_SYNC"},
    
    {"event_async", event_decorator, METH_VARARGS, 
     "Alias de compatibilité pour event"},
    {"event_async_enable", event_enable, METH_NOARGS, 
     "Alias de compatibilité pour event_enable"},
    {"event_async_disable", event_disable, METH_NOARGS, 
     "Alias de compatibilité pour event_disable"},
    {"event_async_evaluate_all", event_evaluate_all, METH_NOARGS, 
     "Alias de compatibilité pour event_evaluate_all"},
    
    /* CONFIGURATION DYNAMIQUE */
    {"set_default_pool_size", (PyCFunction)PyFasty_SetDefaultPoolSize, METH_O,
     "Configurer la taille par défaut du pool"},
    {"set_max_recursion_depth", (PyCFunction)PyFasty_SetMaxRecursionDepth, METH_O,
     "Configurer la profondeur maximale de récursion"},
    {"get_default_pool_size", (PyCFunction)PyFasty_GetDefaultPoolSize, METH_NOARGS,
     "Obtenir la taille par défaut du pool"},
    {"get_max_recursion_depth", (PyCFunction)PyFasty_GetMaxRecursionDepth, METH_NOARGS,
     "Obtenir la profondeur maximale de récursion"},
    {"set_attr_cache_size", (PyCFunction)PyFasty_SetAttrCacheSize, METH_O,
     "Configurer la taille maximale du cache des attributs"},
    {"get_attr_cache_size", (PyCFunction)PyFasty_GetAttrCacheSize, METH_NOARGS,
     "Obtenir la taille maximale du cache des attributs"},
    
    {NULL, NULL, 0, NULL}
};

/* DÉFINITION DU MODULE OPTIMISÉE */

static struct PyModuleDef pyfasty_module = {
    PyModuleDef_HEAD_INIT,
    "pyfasty._pyfasty",
    "Fast Python extension modules - Optimized version",
    -1,
    PyFasty_Methods,
    NULL,                                    /* m_slots */
    NULL,                                    /* m_traverse */
    NULL,                                    /* m_clear */
    (freefunc)pyfasty_cleanup_events        /* m_free - Nettoyer les ressources */
};

/* FONCTION D'INITIALISATION OPTIMISÉE */

/* Protection atomique pour éviter les race conditions d'initialisation */
static volatile int g_pyfasty_init_in_progress = 0;
static volatile int g_pyfasty_init_done = 0;
static PyObject* g_pyfasty_cached_module = NULL;

PyMODINIT_FUNC PyInit__pyfasty(void) {
    PyObject *m;
    
    /* PROTECTION ATOMIQUE : Vérifier si init déjà en cours ou terminée */
    if (g_pyfasty_init_done && g_pyfasty_cached_module) {
        Py_INCREF(g_pyfasty_cached_module);
        return g_pyfasty_cached_module;
    }
    
    /* Éviter les doubles initialisations concurrentes */
    if (g_pyfasty_init_in_progress) {
        /* Attendre un peu et retry */
        for (int i = 0; i < 100 && g_pyfasty_init_in_progress; i++) {
            /* Micro-sleep pour laisser le temps à l'autre thread de finir */
#ifdef PYFASTY_WINDOWS
            Sleep(1);
#else
            usleep(1000);
#endif
        }
        /* Si init terminée entre temps, retourner le module */
        if (g_pyfasty_init_done && g_pyfasty_cached_module) {
            Py_INCREF(g_pyfasty_cached_module);
            return g_pyfasty_cached_module;
        }
    }
    
    /* Marquer le début de l'initialisation */
    g_pyfasty_init_in_progress = 1;

    /* Créer le module */
    m = PyModule_Create(&pyfasty_module);
    if (m == NULL) {
        g_pyfasty_init_in_progress = 0;
        return NULL;
    }

    /* OPTIMISATION : Créer l'exception avec gestion d'erreur améliorée */
    PyFastyError = PyErr_NewException("pyfasty.PyFastyError", NULL, NULL);
    Py_XINCREF(PyFastyError);
    if (PyModule_AddObject(m, "PyFastyError", PyFastyError) < 0) {
        Py_XDECREF(PyFastyError);
        Py_CLEAR(PyFastyError);
        Py_DECREF(m);
        return NULL;
    }
    
    /* INITIALISATION ULTRA-DEFENSIVE pour Linux/macOS */
    /* Initialiser les modules un par un avec gestion d'erreur robuste */
    
    /* 1. Registry - Le plus critique */
    if (PyFasty_Registry_Init && PyFasty_Registry_Init(m) < 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to initialize Registry module");
        goto fail;
    }
    
    /* 2. Config - Nécessaire pour les autres */
    if (PyFasty_Config_Init && PyFasty_Config_Init(m) < 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to initialize Config module");
        goto fail;
    }
    
    /* 3. Console - Log minimal */
    if (PyFasty_Console_Init && PyFasty_Console_Init(m) < 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to initialize Console module");
        goto fail;
    }
    
    /* 4. Event - Système d'événements simple */
    if (PyFasty_Event_Init && PyFasty_Event_Init(m) < 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to initialize Event module");
        goto fail;
    }
    
    /* 5. ExecutorProxy - Plus simple que Executor complet */
    if (PyFasty_ExecutorProxy_Init && PyFasty_ExecutorProxy_Init(m) < 0) {
        /* FALLBACK : Ne pas échouer si ExecutorProxy ne marche pas */
        PyErr_Clear();
    }
    
    /* 6. Executor - Le plus complexe, en dernier avec fallback */
    if (PyFasty_Executor_Init && PyFasty_Executor_Init(m) < 0) {
        /* FALLBACK : Ne pas échouer si Executor ne marche pas */
        PyErr_Clear();
    }

    /* INIT ADAPTATIVE SELON PLATEFORME : Plus conservatrice pour Linux/macOS */
#if defined(__linux__) || defined(__APPLE__)
    /* Linux/macOS : Mode ultra-conservateur - pas de pool pour éviter les crashes */
    /* Le pool sera initialisé à la demande si nécessaire */
#else
    /* Windows : Mode normal avec pool */
    if (pyfasty_dict_pool_init(g_pyfasty_default_pool_size) < 0) {
        /* Si l'init du pool échoue, continuer sans pool (mode fallback) */
        PyErr_Clear(); /* Ne pas propager l'erreur */
    }
#endif

    /* CORRECTION : Activer événements DE MANIÈRE CONDITIONNELLE pour éviter crash init */
    /* Ne plus activer automatiquement - sera fait à la première utilisation */
    /* event_enable(NULL, NULL); -- DÉSACTIVÉ pour éviter threading pendant init */
    
    /* NOUVEAUTÉ : Exposer les objets principaux en natif C */
    /* Ces objets seront accessibles directement via pyfasty.console, pyfasty.registry, etc. */
    
    /* Ajouter console */
    PyObject *console_obj = PyObject_GetAttrString(m, "console");
    if (console_obj) {
        if (PyModule_AddObject(m, "console", console_obj) < 0) {
            Py_DECREF(console_obj);
            goto fail;
        }
        /* console_obj est maintenant owned par le module, pas besoin de Py_DECREF */
    }
    
    /* Ajouter registry */
    PyObject *registry_obj = PyObject_GetAttrString(m, "registry");
    if (registry_obj) {
        if (PyModule_AddObject(m, "registry", registry_obj) < 0) {
            Py_DECREF(registry_obj);
            goto fail;
        }
    }
    
    /* Ajouter config */
    PyObject *config_obj = PyObject_GetAttrString(m, "config");
    if (config_obj) {
        if (PyModule_AddObject(m, "config", config_obj) < 0) {
            Py_DECREF(config_obj);
            goto fail;
        }
    }
    
    /* Ajouter executor */
    PyObject *executor_obj = PyObject_GetAttrString(m, "executor");
    if (executor_obj) {
        if (PyModule_AddObject(m, "executor", executor_obj) < 0) {
            Py_DECREF(executor_obj);
            goto fail;
        }
    }
    
    /* Ajouter event (fonction) */
    PyObject *event_obj = PyObject_GetAttrString(m, "event");
    if (event_obj) {
        if (PyModule_AddObject(m, "event", event_obj) < 0) {
            Py_DECREF(event_obj);
            goto fail;
        }
    }
    
    /* OPTIMISATION : Ajouter des informations de version et métadonnées */
    if (PyModule_AddStringConstant(m, "__version__", PYFASTY_VERSION) < 0)
        goto fail;
    if (PyModule_AddStringConstant(m, "__author__", "PyFasty Development Team") < 0)
        goto fail;

    /* OPTIMISATION : Créer et ajouter la liste __all__ de manière plus efficace */
    static const char* all_items[] = {
        /* NOUVEAUTÉ : Objets principaux en natif */
        "console", "registry", "config", "executor", "event",
        /* Objets existants */
        "sync_executor", "async_executor",
        "event_sync", "event_async", "__version__", "PyFastyError", 
        "set_default_pool_size", "set_max_recursion_depth", 
        "get_default_pool_size", "get_max_recursion_depth", "set_attr_cache_size",
        "get_attr_cache_size", NULL
    };
    
    PyObject *all_list = PyList_New(0);
    if (all_list == NULL) goto fail;
    
    for (int i = 0; all_items[i] != NULL; i++) {
        PyObject *item = PyUnicode_FromString(all_items[i]);
        if (item == NULL || PyList_Append(all_list, item) < 0) {
            Py_XDECREF(item);
            Py_DECREF(all_list);
            goto fail;
        }
        Py_DECREF(item);
    }
    
    /* Stocker __all__ dans un dictionnaire qui sera utilisé plus tard */
    PyObject* module_setup_dict = PyDict_New();
    if (module_setup_dict == NULL) {
        Py_DECREF(all_list);
        goto fail;
    }
    PyDict_SetItemString(module_setup_dict, "__all__", all_list);
    Py_DECREF(all_list);
    
    /* Ajouter ce dictionnaire au module C */
    if (PyModule_AddObject(m, "_module_setup", module_setup_dict) < 0) {
        Py_DECREF(module_setup_dict);
        goto fail;
    }
    
    /* OPTIMISATION : Installation optimisée de __setattr__ et __all__ */
    PyObject* sys_modules = PySys_GetObject("modules");
    if (sys_modules == NULL) goto fail;

    PyObject* pyfasty_name = PyUnicode_FromString("pyfasty");
    if (pyfasty_name == NULL) goto fail;

    PyObject* pyfasty_module_obj = PyDict_GetItem(sys_modules, pyfasty_name);
    Py_DECREF(pyfasty_name);

    if (pyfasty_module_obj != NULL) {
        /* Vérifier si l'initialisation a déjà été faite */
        PyObject* init_done = PyObject_GetAttrString(pyfasty_module_obj, "_init_done");
        if (init_done == NULL) {
            PyErr_Clear(); /* Ignorer l'erreur si l'attribut n'existe pas */
            
            /* Marquer le module comme initialisé */
            if (PyObject_SetAttrString(pyfasty_module_obj, "_init_done", Py_True) < 0)
                goto fail;
            
            /* Installer __setattr__ */
            PyObject* post_init_func = PyObject_GetAttrString(m, "_post_init_module");
            if (post_init_func != NULL) {
                PyObject* call_result = PyObject_CallObject(post_init_func, NULL);
                Py_DECREF(post_init_func);
                if (call_result == NULL) goto fail;
                Py_DECREF(call_result);
            }
            
            /* Installer __all__ */
            PyObject* module_setup = PyObject_GetAttrString(m, "_module_setup");
            if (module_setup != NULL) {
                PyObject* all_list_obj = PyDict_GetItemString(module_setup, "__all__");
                if (all_list_obj != NULL) {
                    Py_INCREF(all_list_obj);
                    if (PyObject_SetAttrString(pyfasty_module_obj, "__all__", all_list_obj) < 0) {
                        Py_DECREF(all_list_obj);
                        Py_DECREF(module_setup);
                        goto fail;
                    }
                    Py_DECREF(all_list_obj);
                }
                Py_DECREF(module_setup);
            }
        } else {
            Py_DECREF(init_done);
        }
    }

    /* FINALISATION ATOMIQUE : Marquer init terminée et cacher le module */
    g_pyfasty_cached_module = m;
    Py_INCREF(m); /* Garder une référence pour le cache */
    g_pyfasty_init_done = 1;
    g_pyfasty_init_in_progress = 0;
    
    return m;

fail:
    /* NETTOYAGE ATOMIQUE : Reset des flags en cas d'échec */
    g_pyfasty_init_in_progress = 0;
    g_pyfasty_init_done = 0;
    g_pyfasty_cached_module = NULL;
    
    Py_XDECREF(PyFastyError);
    Py_CLEAR(PyFastyError);
    Py_DECREF(m);
    return NULL;
}
