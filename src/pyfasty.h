#ifndef PYFASTY_H
#define PYFASTY_H

#define PY_SSIZE_T_CLEAN
#include <Python.h>

/* CONFIGURATION 100% DYNAMIQUE : Variables configurables */
extern int g_pyfasty_default_pool_size;
extern int g_pyfasty_max_recursion_depth;
extern int g_pyfasty_attr_cache_size;

/* Valeurs par défaut si non configurées */
#define PYFASTY_DEFAULT_POOL_SIZE_VALUE 200
#define PYFASTY_MAX_RECURSION_DEPTH_VALUE 100
#define PYFASTY_ATTR_CACHE_SIZE_VALUE 8

/* CONSTANTES POUR LES CLÉS INTERNES - plus de hardcoding */
#define PYFASTY_ORIGINAL_VALUE_KEY "__original_value__"
#define PYFASTY_INTERNAL_VALUE_KEY "__value__"

/* CONSTANTES POUR LES NOMS D'ATTRIBUTS PUBLICS CONFIG */
#define PYFASTY_CONFIG_PRIVATE_VALUE_ATTR "_value"
#define PYFASTY_CONFIG_PUBLIC_VALUE_ATTR "value"

/* VALEURS PAR DÉFAUT POUR LES OPÉRATIONS ARITHMÉTIQUES */
#define PYFASTY_DEFAULT_ADD_VALUE 0
#define PYFASTY_DEFAULT_MUL_VALUE 1
#define PYFASTY_DEFAULT_DIV_VALUE 0.0

/* Macro pour la portabilité de strtok entre Windows et Unix */
#ifdef _WIN32
  #define PYFASTY_STRTOK_R strtok_s
#else
  #define PYFASTY_STRTOK_R strtok_r
#endif

/* Version du module */
#define PYFASTY_VERSION "0.1.0b2"

/* FORWARD DECLARATIONS : Éviter les dépendances circulaires */
typedef struct PyFastyBaseObject PyFastyBaseObject;
typedef struct PyFasty_ObjectPool PyFasty_ObjectPool;
typedef struct EventDecoratorObject EventDecoratorObject;

/* Exception globale */
extern PyObject *PyFastyError;

/* TYPES ET ÉNUMÉRATIONS OPTIMISÉES */

/* Types d'objets pour identification - avec masques binaires pour combinaisons */
typedef enum {
    PYFASTY_REGISTRY_TYPE = 1 << 0,  /* 1 */
    PYFASTY_CONFIG_TYPE = 1 << 1,    /* 2 */
    PYFASTY_CONSOLE_TYPE = 1 << 2,   /* 4 */
    PYFASTY_EXECUTOR_TYPE = 1 << 3   /* 8 */
} PyFastyObjectType;

/* Types de modules avec masques binaires pour optimisation */
typedef enum {
    MODULE_UNKNOWN = 0,
    MODULE_CONFIG = 1 << 0,          /* 1 */
    MODULE_REGISTRY = 1 << 1,        /* 2 */
    MODULE_CONSOLE = 1 << 2,         /* 4 */
    MODULE_EXECUTOR_SYNC = 1 << 3,   /* 8 */
    MODULE_EXECUTOR_ASYNC = 1 << 4,  /* 16 */
    MODULE_ALL = (1 << 5) - 1        /* 31 - tous les modules */
} ModuleType;

/* STRUCTURES OPTIMISÉES */

/* Structure de base commune pour Registry et Config - alignement optimisé */
struct PyFastyBaseObject {
    PyObject_HEAD
    PyObject *data;            /* Dictionnaire contenant les valeurs */
    PyObject *cache;           /* Dictionnaire pour le cache d'accès */
    PyObject *value;           /* Valeur directe */
    int depth;                 /* Profondeur pour optimisation */
    char _padding[4];          /* Alignement mémoire explicite */
};

/* Structure pour le pool d'objets - optimisée avec métadonnées */
struct PyFasty_ObjectPool {
    PyObject **objects;        /* Tableau d'objets disponibles */
    int size;                  /* Taille totale du pool */
    int used;                  /* Nombre d'objets actuellement utilisés */
    int peak_usage;           /* Usage maximum atteint (statistiques) */
    int hit_count;            /* Nombre de hits du cache */
    int miss_count;           /* Nombre de misses du cache */
};

/* Structure générique pour les décorateurs d'événements */
struct EventDecoratorObject {
    PyObject_HEAD
    PyObject *condition;
};

/* TYPES DE FONCTIONS OPTIMISÉES */

/* Définition du type de fonction pour l'accès aux attributs */
typedef PyObject* (*PyFastyGetAttrFunc)(PyObject*, PyObject*);

/* Type pour les fonctions d'exécution de callbacks d'événements */
typedef void (*EventCallbackExecutor)(PyObject *callback);

/* Fonction d'aide pour la config */
typedef int (*PyFasty_SetGlobalConfigFunc)(PyObject *module, PyObject *dict);

/* VARIABLES GLOBALES ORGANISÉES */

/* Pool global de dictionnaires */
extern PyFasty_ObjectPool g_dict_pool;

/* Pointeur de fonction pour la mise à jour globale de la config */
extern PyFasty_SetGlobalConfigFunc PyFasty_SetGlobalConfig;

/* Variables d'état des événements */
extern PyObject *g_event_sync_handlers;
extern int g_events_enabled;
extern int g_in_condition_evaluation;

/* Variables de configuration dynamique */
extern int g_pyfasty_default_pool_size;
extern int g_pyfasty_max_recursion_depth;

/* FONCTIONS DE CONFIGURATION GLOBALE */

/* Configuration des paramètres système */
int PyFasty_SetDefaultPoolSize(int size);
int PyFasty_SetMaxRecursionDepth(int depth);
int PyFasty_SetAttrCacheSize(int size);

/* Obtenir les valeurs actuelles */
int PyFasty_GetDefaultPoolSize(void);
int PyFasty_GetMaxRecursionDepth(void);
int PyFasty_GetAttrCacheSize(void);

/* FONCTIONS DE GESTION DE POOL OPTIMISÉES */

/* Initialisation et finalisation */
int pyfasty_dict_pool_init(int size);
void pyfasty_dict_pool_finalize(void);

/* Opérations principales avec statistiques intégrées */
PyObject *pyfasty_dict_pool_get(void);
void pyfasty_dict_pool_return(PyObject *dict);

/* NOUVEAU : Fonctions de monitoring du pool */
double pyfasty_dict_pool_hit_ratio(void);
int pyfasty_dict_pool_peak_usage(void);
void pyfasty_dict_pool_reset_stats(void);

/* FONCTIONS DE BASE GÉNÉRIQUES ET OPTIMISÉES */

/* Création d'objets avec paramètres optionnels */
PyObject* pyfasty_base_create(PyTypeObject *type, PyFastyObjectType obj_type, 
                             int depth, PyObject *value);

/* Accès aux attributs avec cache intelligent */
PyObject* pyfasty_base_getattr_recursive(PyObject *self, PyObject *name, 
                                       PyTypeObject *type, PyFastyObjectType obj_type);

/* Modification d'attributs avec validation */
int pyfasty_base_setattr_recursive(PyObject *self, PyObject *name, PyObject *value,
                                 PyTypeObject *type, PyFastyObjectType obj_type);

/* Accès par chemin avec optimisation */
PyObject* pyfasty_object_get_by_path(PyObject *self, const char *path, 
                                    PyTypeObject *obj_type, 
                                    PyFastyGetAttrFunc get_attr_func);

/* FONCTIONS COMMUNES POUR LES EXÉCUTEURS */

PyObject *executor_common_resolve_path(PyObject *path_list);
PyObject *executor_common_path_to_string(PyObject *path_list);
PyObject *executor_common_extend_path(PyObject *current_path, PyObject *name);

/* NOUVEAU : Fonction pour accéder au dernier résultat d'exécution */
PyObject *get_last_executor_result(void);

/* INTERFACE D'ÉVÉNEMENTS UNIFIÉE */

PyObject *event_decorator(PyObject *self, PyObject *args);
PyObject *event_enable(PyObject *self, PyObject *args);
PyObject *event_disable(PyObject *self, PyObject *args);
PyObject *event_evaluate_all(PyObject *self, PyObject *args);
PyObject *event_clear_handlers(PyObject *self, PyObject *args);

/* FONCTIONS DE COMPATIBILITÉ - Alias vers le module unifié */

/* Aliases pour event_sync (pour compatibilité) */
#define event_sync_decorator event_decorator
#define event_sync_enable event_enable
#define event_sync_disable event_disable
#define event_sync_evaluate_all event_evaluate_all
#define event_sync_clear_handlers event_clear_handlers

/* Aliases pour event_async (pour compatibilité) */
#define event_async_decorator event_decorator
#define event_async_enable event_enable
#define event_async_disable event_disable
#define event_async_evaluate_all event_evaluate_all

/* Fonction spécialisée qui reste pour compatibilité */
PyObject *event_sync_evaluate_executor_only(PyObject *self, PyObject *args);

/* FONCTIONS PARTAGÉES D'ÉVÉNEMENTS */

extern int ensure_event_handlers_exist(void);
extern int init_event_decorator_type(PyTypeObject *decorator_type);
extern int add_event_methods_to_module(PyObject *module, PyMethodDef *methods);
extern PyObject *eventsync_decorator_call(void *self, PyObject *args, PyObject *kwds);
extern PyTypeObject EventSyncDecoratorType;

/* Fonctions génériques utilisées par event_async */
extern PyObject *generic_event_evaluate_all(PyObject *handlers_list, int *enabled_flag, 
                                           EventCallbackExecutor executor);
extern PyObject *create_event_handlers_list(void);
extern PyObject *generic_event_enable(int *enabled_flag, PyObject *self, PyObject *args);
extern PyObject *generic_event_disable(int *enabled_flag, PyObject *self, PyObject *args);
extern PyObject *generic_event_decorator_call(void *self, PyObject *args, PyObject *kwds,
                                              PyObject *handlers_list, int *enabled_flag);
extern PyObject *generic_event_decorator_create(PyTypeObject *decorator_type, PyObject *args,
                                               const char *arg_format);

/* FONCTIONS PARTAGÉES DE REGISTRY/CONFIG */

PyObject *pyfasty_common_getitem(PyObject *self, PyObject *key, PyTypeObject *type, 
                                PyFastyObjectType obj_type, 
                                PyObject *(*create_func)(int, PyObject*));
int pyfasty_common_setitem(PyObject *self, PyObject *key, PyObject *value, PyTypeObject *type, 
                          PyFastyObjectType obj_type, 
                          PyObject *(*create_func)(int, PyObject*));
int pyfasty_common_contains(PyObject *self, PyObject *key);
PyObject *pyfasty_common_getmethod(PyObject *self, PyObject *args, PyFastyObjectType obj_type);

/* GESTION GLOBALE DES ÉVÉNEMENTS */

/* Déclenchement d'événements */
int pyfasty_trigger_events(void);
void pyfasty_trigger_sync_events(void);
void pyfasty_trigger_sync_events_with_module(ModuleType module_type);

/* Nettoyage et monitoring */
void pyfasty_cleanup_events(void);
void cleanup_condition_cache(void);
void pyfasty_trace_module_access(ModuleType module);

/* État des événements */
int pyfasty_is_in_callback_execution(void);
int pyfasty_is_readonly_evaluation(void);

/* INITIALISATION DES SOUS-MODULES */

int PyFasty_Registry_Init(PyObject *module);
int PyFasty_Config_Init(PyObject *module);
int PyFasty_Console_Init(PyObject *module);
int PyFasty_Executor_Init(PyObject *module);
int PyFasty_ExecutorProxy_Init(PyObject *module);
int PyFasty_Event_Init(PyObject *module);

/* FONCTION D'INITIALISATION PRINCIPALE */

PyMODINIT_FUNC PyInit__pyfasty(void);

#endif /* PYFASTY_H */
