/*
 * ================================================================================
 * PYFASTY REGISTRY SYSTEM - COMPLETE IMPLEMENTATION
 * ================================================================================
 * 
 * OVERVIEW:
 * This file implements the PyFasty Registry system - a dynamic, thread-safe
 * object that provides auto-creation and intelligent initialization for attributes.
 * 
 * KEY FEATURES:
 * 1. SMART AUTO-INITIALIZATION: pyfasty.registry.counter += 1 auto-creates counter=0
 * 2. OPERATION-AWARE DEFAULTS: Different defaults based on operation (0 for +=, 1 for *=)
 * 3. EVENT-SAFE READ-ONLY MODE: Prevents auto-creation during event condition evaluation
 * 4. THREAD-SAFE MEMORY POOL: Optimized dictionary allocation/deallocation
 * 5. HIERARCHICAL STRUCTURE: Unlimited nested attribute access (registry.a.b.c.d...)
 * 
 * CRITICAL COMPONENTS:
 * - registry_getattro(): Main entry point for attribute access (THE MAGIC HAPPENS HERE)
 * - registry_inplace_operation(): Generic arithmetic handler with smart defaults
 * - registry_getattr_recursive(): Handles READ-ONLY mode for event system integration
 * - Dictionary pool system: High-performance memory management
 * 
 * USAGE EXAMPLES:
 * pyfasty.registry.counter += 1     # Creates counter=0, then adds 1 -> counter=1
 * pyfasty.registry.stats *= 2       # Creates stats=1, then multiplies -> stats=2
 * pyfasty.registry.data.users = 5   # Creates nested structure automatically
 * 
 * This implementation is the core of PyFasty's "just works" philosophy.
 * ================================================================================
 */

#include "../pyfasty.h"
#include "../thread/pyfasty_threading.h"
#include <math.h>

/*
 * ================================================================================
 * SECTION 1: TYPE CHECKING HELPERS
 * Fast inline functions for common type checks (used 50+ times in file)
 * ================================================================================
 */

/* Check if object is primitive: int, float, bool, string, None */
static inline int is_primitive_value(PyObject *obj) {
    return (PyLong_Check(obj) || PyFloat_Check(obj) || PyBool_Check(obj) || 
            PyUnicode_Check(obj) || obj == Py_None);
}

/* Check if object is numeric: int or float only */
static inline int is_numeric_value(PyObject *obj) {
    return (PyLong_Check(obj) || PyFloat_Check(obj));
}

/* Extract numeric value safely with fallback default */
static inline double extract_numeric_value(PyObject *obj, double default_val) {
    if (obj == Py_None) return default_val;
    if (PyLong_Check(obj)) return (double)PyLong_AsLong(obj);
    if (PyFloat_Check(obj)) return PyFloat_AsDouble(obj);
    return default_val;
}

/* Create optimal Python number: int if no decimals, float otherwise */
static inline PyObject *create_optimal_number(double value) {
    if (value == floor(value)) {
        return PyLong_FromLong((long)value);  /* Int for whole numbers */
    } else {
        return PyFloat_FromDouble(value);     /* Float for decimals */
    }
}

/*
 * ================================================================================
 * SECTION 2: CACHE & MEMORY HELPERS  
 * Ultra-safe helpers to prevent memory leaks and simplify repetitive patterns
 * ================================================================================
 */

/* Store in cache with automatic cleanup on failure */
static inline int safe_cache_store(PyObject *cache, PyObject *key, PyObject *value) {
    if (PyDict_SetItem(cache, key, value) < 0) {
        Py_DECREF(value);  /* Cleanup on failure */
        return -1;
    }
    return 0;
}

/* Save original value before overwriting (preserves history for events) */
static inline void maybe_save_original_value(PyFastyBaseObject *obj) {
    if (obj->value != Py_None && !is_numeric_value(obj->value)) {
        PyObject *orig_key = PyUnicode_FromString(PYFASTY_ORIGINAL_VALUE_KEY);
        if (orig_key) {
            if (!PyDict_Contains(obj->data, orig_key)) {
                PyDict_SetItem(obj->data, orig_key, obj->value);
            }
            Py_DECREF(orig_key);
        }
    }
}

/* Wrap primitive values in Registry objects for consistent attribute access */
static inline PyObject *wrap_primitive_if_needed(PyObject *result, PyTypeObject *type, 
                                                PyFastyObjectType obj_type, int depth) {
    if (is_primitive_value(result)) {
        PyObject *new_obj = pyfasty_base_create(type, obj_type, depth, result);
        if (new_obj == NULL) {
            Py_INCREF(result);
            return result;  /* Fallback: return original on failure */
        }
        return new_obj;
    }
    Py_INCREF(result);
    return result;
}

/*
 * ================================================================================
 * SECTION 3: THREAD-SAFE DICTIONARY POOL
 * Memory optimization: reuse PyDict objects instead of alloc/free every time
 * Critical for performance with high-frequency attribute access
 * ================================================================================
 */

/* Global pool state: thread-safe via mutex */
PyFasty_ObjectPool g_dict_pool = {NULL, 0, 0};
static PyFasty_Mutex g_dict_pool_mutex;
static int g_dict_pool_mutex_initialized = 0;

/* Initialize pool: pre-allocate dictionaries for fast reuse */
int pyfasty_dict_pool_init(int size) {
    if (g_dict_pool.objects != NULL) {
        return 0;  /* Already initialized */
    }
    
    /* ROLLBACK : approche moins invasive - init comme avant mais avec fallback si échec */
    if (!g_dict_pool_mutex_initialized) {
        if (PyFasty_MutexInit(&g_dict_pool_mutex) != 0) {
            /* Si le mutex fail, on désactive le pool et utilise dict direct */
            g_dict_pool_mutex_initialized = -1; /* Flag "disabled" */
            return 0; /* Continuer sans pool */
        }
        g_dict_pool_mutex_initialized = 1;
    }
    
    /* Double-checking pattern for thread-safety */
    PyFasty_MutexLock(&g_dict_pool_mutex);
    
    if (g_dict_pool.objects != NULL) {
        PyFasty_MutexUnlock(&g_dict_pool_mutex);
        return 0;
    }
    
    /* Allocate array + pre-create dictionaries */
    g_dict_pool.objects = (PyObject **)malloc(size * sizeof(PyObject *));
    if (!g_dict_pool.objects) {
        PyFasty_MutexUnlock(&g_dict_pool_mutex);
        return -1;
    }
    
    g_dict_pool.size = size;
    g_dict_pool.used = 0;
    
    /* Pre-allocate PyDict_New() for performance */
    for (int i = 0; i < size; i++) {
        g_dict_pool.objects[i] = PyDict_New();
        if (g_dict_pool.objects[i] == NULL) {
            /* Partial cleanup on failure */
            for (int j = 0; j < i; j++) {
                Py_DECREF(g_dict_pool.objects[j]);
            }
            free(g_dict_pool.objects);
            g_dict_pool.objects = NULL;
            PyFasty_MutexUnlock(&g_dict_pool_mutex);
            return -1;
        }
    }
    
    PyFasty_MutexUnlock(&g_dict_pool_mutex);
    return 0;
}

/* Cleanup: free all dictionaries and destroy mutex */
void pyfasty_dict_pool_finalize(void) {
    if (!g_dict_pool_mutex_initialized) {
        return;  /* Mutex was never initialized */
    }
    
    PyFasty_MutexLock(&g_dict_pool_mutex);
    
    if (g_dict_pool.objects == NULL) {
        PyFasty_MutexUnlock(&g_dict_pool_mutex);
        return;
    }
    
    /* Free all dictionaries */
    for (int i = 0; i < g_dict_pool.size; i++) {
        if (g_dict_pool.objects[i]) {
            Py_DECREF(g_dict_pool.objects[i]);
        }
    }
    
    free(g_dict_pool.objects);
    g_dict_pool.objects = NULL;
    g_dict_pool.size = 0;
    g_dict_pool.used = 0;
    
    PyFasty_MutexUnlock(&g_dict_pool_mutex);
    
    /* Destroy mutex */
    PyFasty_MutexDestroy(&g_dict_pool_mutex);
    g_dict_pool_mutex_initialized = 0;
}

/* Get dictionary from pool (or create new if pool empty/uninitialized) */
PyObject *pyfasty_dict_pool_get(void) {
    PyObject *dict = NULL;
    
    /* Check if mutex is initialized or disabled */
    if (!g_dict_pool_mutex_initialized || g_dict_pool_mutex_initialized == -1) {
        return PyDict_New();  /* Create dictionary directly */
    }
    
    PyFasty_MutexLock(&g_dict_pool_mutex);
    
    /* Thread-safe pool access */
    if (g_dict_pool.objects && g_dict_pool.used < g_dict_pool.size) {
        dict = g_dict_pool.objects[g_dict_pool.used];
        g_dict_pool.objects[g_dict_pool.used] = NULL;
        g_dict_pool.used++;
        
        /* Ensure dictionary is empty */
        if (dict != NULL) {
            PyDict_Clear(dict);
        }
    } else {
        /* Pool empty or uninitialized, create new dictionary */
        dict = PyDict_New();
    }
    
    PyFasty_MutexUnlock(&g_dict_pool_mutex);
    return dict;
}

/* Return dictionary to pool (or free if pool full) */
void pyfasty_dict_pool_return(PyObject *dict) {
    if (!dict || !PyDict_Check(dict)) {
        return;
    }
    
    /* Clear dictionary contents */
    PyDict_Clear(dict);
    
    /* Check if mutex is initialized or disabled */
    if (!g_dict_pool_mutex_initialized || g_dict_pool_mutex_initialized == -1) {
        Py_DECREF(dict);
        return;
    }
    
    PyFasty_MutexLock(&g_dict_pool_mutex);
    
    /* Thread-safe pool return */
    if (g_dict_pool.objects && g_dict_pool.used > 0) {
        g_dict_pool.used--;
        g_dict_pool.objects[g_dict_pool.used] = dict;
    } else {
        /* Pool full or uninitialized, free dictionary */
        Py_DECREF(dict);
    }
    
    PyFasty_MutexUnlock(&g_dict_pool_mutex);
}

/* Enum pour les opérations numériques - CORRIGÉ pour inclure TOUS les opérateurs */
typedef enum {
    OP_NONE = 0,    /* Pas d'op en cours */
    OP_IADD,        /* += : init à 0 */
    OP_ISUB,        /* -= : init à 0 */
    OP_IMUL,        /* *= : init à 1 */
    OP_IDIV,        /* /= : init à 1 */
    OP_IMOD,        /* %= : init à 0 */
    OP_IPOW,        /* **= : init à 1 */
    OP_IAND,        /* &= : init à 0 */
    OP_IOR,         /* |= : init à 0 */
    OP_IXOR,        /* ^= : init à 0 */
    OP_ILSHIFT,     /* <<= : init à 0 */
    OP_IRSHIFT,     /* >>= : init à 0 */
    OP_IMATMUL      /* @= : init à 1 */
} RegistryOperation;

/* Forward declarations pour interdépendances */
static PyTypeObject PyFastyRegistryType;
static PyTypeObject PyFastyConfigType;

/* Structure Registry : hérite PyFastyBaseObject + état opération */
typedef struct {
    PyFastyBaseObject base;             /* data, cache, value, depth */
    RegistryOperation current_op;       /* État pour auto-init */
} PyFastyRegistryObject;

/* === FONCTIONS DE CRÉATION === */
/* Factory générique : alloc + init data/cache depuis pool + traitement value */
PyObject* pyfasty_base_create(PyTypeObject *type, PyFastyObjectType obj_type, 
                             int depth, PyObject *value) {
    PyFastyBaseObject *obj = (PyFastyBaseObject *)type->tp_alloc(type, 0);
    if (obj == NULL) {
        return NULL;
    }
    
    /* Pool de dicts pour éviter alloc/dealloc répétées */
    obj->data = pyfasty_dict_pool_get();
    if (obj->data == NULL) {
        Py_DECREF(obj);
        return NULL;
    }
    
    obj->cache = pyfasty_dict_pool_get();
    if (obj->cache == NULL) {
        pyfasty_dict_pool_return(obj->data);
        Py_DECREF(obj);
        return NULL;
    }
    
    obj->depth = depth;
    
    /* Si value=dict : expansion dans data pour accès attributs */
    if (value != NULL && PyDict_Check(value)) {
        /* Parcours dict et conversion récursive des sous-dicts */
        PyObject *key, *val;
        Py_ssize_t pos = 0;
        
        while (PyDict_Next(value, &pos, &key, &val)) {
            /* Clé string = attribut accessible */
            if (PyUnicode_Check(key)) {
                /* Sub-dict = récursion registry/config */
                if (PyDict_Check(val)) {
                    PyObject *sub_obj = pyfasty_base_create(type, obj_type, depth + 1, val);
                    if (sub_obj == NULL) {
                        goto error;
                    }
                    
                    /* Stockage pour access par attribut */
                    if (PyDict_SetItem(obj->data, key, sub_obj) < 0) {
                        Py_DECREF(sub_obj);
                        goto error;
                    }
                    
                    Py_DECREF(sub_obj);
                } else {
                    /* Valeur primitive directe */
                    if (PyDict_SetItem(obj->data, key, val) < 0) {
                        goto error;
                    }
                }
            }
        }
        
        /* Stockage dict complet comme value */
        obj->value = value;
        Py_INCREF(value);
    } else if (value != NULL) {
        obj->value = value;
        Py_INCREF(value);
    } else {
        obj->value = Py_None;
        Py_INCREF(Py_None);
    }
    
    return (PyObject *)obj;

error:
    pyfasty_dict_pool_return(obj->data);
    pyfasty_dict_pool_return(obj->cache);
    Py_DECREF(obj);
    return NULL;
}

/* Forward declaration pour les fonctions d'événement */
extern PyObject *PyObject_CallMethod(PyObject *obj, const char *name, const char *format, ...);

/* Optimized fast-path getattr function */
PyObject* pyfasty_base_getattr_recursive(PyObject *self, PyObject *name,
                                       PyTypeObject *type, PyFastyObjectType obj_type) {
    PyFastyBaseObject *base = (PyFastyBaseObject *)self;
    PyObject *result = NULL;
    
    /* Fast path: special names with dunder */
    const char *name_str = PyUnicode_AsUTF8(name);
    if (name_str[0] == '_') {
        result = PyObject_GenericGetAttr(self, name);
        return result;
    }
    
    /* Special case for get method on dictionaries */
    if (strcmp(name_str, "get") == 0 && base->value != Py_None && PyDict_Check(base->value)) {
        result = PyObject_GetAttrString(base->value, "get");
        if (result != NULL) {
            return result;
        }
        PyErr_Clear();
    }
    
    /* Fast path: check cache first for better performance */
    result = PyDict_GetItem(base->cache, name);
    if (result != NULL) {
        Py_INCREF(result);
        return result;
    }
    
    /* Fast path: try to get from data dictionary */
    result = PyDict_GetItem(base->data, name);
    if (result != NULL) {
        /* Optimized path for dictionaries */
        if (PyDict_Check(result)) {
            PyObject *new_obj = pyfasty_base_create(type, obj_type, base->depth + 1, result);
            if (new_obj == NULL) {
                return NULL;
            }
            
            /* Cache the result avec helper sécurisé */
            if (safe_cache_store(base->cache, name, new_obj) < 0) {
                return NULL;
            }
            
            result = new_obj;
            return result;
        }
        
        /* Si le résultat est un entier, un flottant, un booléen ou une chaîne,
           l'envelopper dans un objet registry pour permettre des accès imbriqués */
        if (is_primitive_value(result)) {
            PyObject *new_obj = pyfasty_base_create(type, obj_type, base->depth + 1, result);
            if (new_obj == NULL) {
                return NULL;
            }
            
            /* Cache the result avec helper sécurisé */
            if (safe_cache_store(base->cache, name, new_obj) < 0) {
                Py_INCREF(result);  /* Fallback to direct value if cache fails */
                return result;
            }
            
            result = new_obj;
            return result;
        }
        
        Py_INCREF(result);
        
        /* Cache the result */
        PyDict_SetItem(base->cache, name, result);
        
        return result;
    }
    
    /* Si la valeur est un registry, on peut accéder à ses attributs */
    if (base->value != Py_None && (PyObject_TypeCheck(base->value, &PyFastyRegistryType) ||
                                  PyObject_TypeCheck(base->value, &PyFastyConfigType))) {
        PyFastyBaseObject *base_val = (PyFastyBaseObject *)base->value;
        result = PyDict_GetItem(base_val->data, name);
        if (result != NULL) {
            Py_INCREF(result);
            return result;
        }
    }
    
    /* Create a new object for auto-creation */
    PyObject *new_obj = pyfasty_base_create(type, obj_type, base->depth + 1, NULL);
    if (new_obj == NULL) {
        return NULL;
    }
    
    /* Store in both data and cache for faster access */
    if (PyDict_SetItem(base->data, name, new_obj) < 0) {
        Py_DECREF(new_obj);
        return NULL;
    }
    
    PyDict_SetItem(base->cache, name, new_obj);
    
    return new_obj;
}

/* Optimized implementation of pyfasty_base_setattr_recursive */
int pyfasty_base_setattr_recursive(PyObject *self, PyObject *name, PyObject *value,
                                 PyTypeObject *type, PyFastyObjectType obj_type) {
    PyFastyBaseObject *base = (PyFastyBaseObject *)self;
    
    /* Fast path: special name with dunder, use default behavior */
    const char *name_str = PyUnicode_AsUTF8(name);
    if (name_str[0] == '_') {
        return PyObject_GenericSetAttr(self, name, value);
    }
    
    /* Check if this is a key attribute that should remain a registry (Registry-specific) */
    PyObject *existing = PyDict_GetItem(base->data, name);
    int is_special_attr = 0;
    
    if (obj_type == PYFASTY_REGISTRY_TYPE) {
        /* Tous les attributs au niveau 0 sont spéciaux, pas seulement "test" */
        is_special_attr = (base->depth == 0);
    }
    
    /* Clear cache for consistency */
    if (PyDict_Contains(base->cache, name)) {
        PyDict_DelItem(base->cache, name);
    }
    
    /* Pour les attributs spéciaux ou pour tous les objets au niveau 0,
       on garde toujours l'objet Registry et on stocke la valeur à l'intérieur */
    if (is_special_attr || base->depth == 0) {
        if (existing == NULL || !PyObject_TypeCheck(existing, type)) {
            /* Create a new registry to replace or initialize the attribute */
            PyObject *new_obj = pyfasty_base_create(type, obj_type, base->depth + 1, value);
            if (new_obj == NULL) {
                return -1;
            }
            
            /* Set the registry as the attribute value */
            int result = PyDict_SetItem(base->data, name, new_obj);
            Py_DECREF(new_obj);
            
            return result;
        } else {
            /* AMÉLIORATION: Ne sauvegarder __original_value__ que si nécessaire avec helper */
            PyFastyBaseObject *obj = (PyFastyBaseObject *)existing;
            maybe_save_original_value(obj);
            
            /* If it's already a registry, just update its value */
            Py_XDECREF(obj->value);
            Py_INCREF(value);
            obj->value = value;
            
            return 0;
        }
    }
    
    /* Pour tous les attributs imbriqués, stockons également la valeur directe */
    if (existing != NULL && PyObject_TypeCheck(existing, type)) {
        /* AMÉLIORATION: Ne sauvegarder __original_value__ que si nécessaire avec helper */
        PyFastyBaseObject *obj = (PyFastyBaseObject *)existing;
        maybe_save_original_value(obj);
        
        /* Si l'attribut existe déjà et est un Registry, mettre à jour sa valeur */
        Py_XDECREF(obj->value);
        Py_INCREF(value);
        obj->value = value;
        
        return 0;
    }
    
    /* Config-specific behavior */
    if (obj_type == PYFASTY_CONFIG_TYPE) {
        /* Store the direct value for all configs (different from registry) */
        Py_XDECREF(base->value);
        Py_INCREF(value);
        base->value = value;
    } else if (obj_type == PYFASTY_REGISTRY_TYPE && base->depth == 0) {
        /* For Registry, store direct value if it's a top-level attribute */
        Py_XDECREF(base->value);
        Py_INCREF(value);
        base->value = value;
    }
    
    /* Set the value */
    int result;
    
    /* Déterminer si la valeur est un dict pour sous-objet ou une valeur primitive */
    if (PyDict_Check(value)) {
        PyObject *new_obj = pyfasty_base_create(type, obj_type, base->depth + 1, value);
        if (new_obj == NULL) {
            return -1;
        }
        
        /* Stocker le sous-objet */
        result = PyDict_SetItem(base->data, name, new_obj);
        Py_DECREF(new_obj);
    } else if (PyLong_Check(value) || PyFloat_Check(value) || PyBool_Check(value) || 
               PyUnicode_Check(value) || value == Py_None) {
        /* Pour les types primitifs, on les enveloppe dans un Registry pour pouvoir 
           accéder à leurs "attributs" virtuels */
        PyObject *new_obj = pyfasty_base_create(type, obj_type, base->depth + 1, value);
        if (new_obj == NULL) {
            return -1;
        }
        
        /* Stocker le sous-objet */
        result = PyDict_SetItem(base->data, name, new_obj);
        Py_DECREF(new_obj);
    } else {
        /* Set the value directly for other object types */
        result = PyDict_SetItem(base->data, name, value);
    }
    
    return result;
}

/* Forward declarations */
static PyObject *registry_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
static int registry_init(PyFastyRegistryObject *self, PyObject *args, PyObject *kwds);
static void registry_dealloc(PyFastyRegistryObject *self);
static PyObject *registry_getattro(PyFastyRegistryObject *self, PyObject *name);
static int registry_setattro(PyFastyRegistryObject *self, PyObject *name, PyObject *value);
static PyObject *registry_str(PyFastyRegistryObject *self);
static PyObject *registry_repr(PyFastyRegistryObject *self);
static PyObject *registry_inplace_add(PyObject *self, PyObject *other);
static PyObject *registry_add(PyObject *self, PyObject *other);
static PyObject *registry_radd(PyObject *self, PyObject *other);
static PyObject *registry_format(PyObject *self, PyObject *format_spec);
static PyObject *registry_inplace_subtract(PyObject *self, PyObject *other);
static PyObject *registry_inplace_multiply(PyObject *self, PyObject *other);
static PyObject *registry_inplace_divide(PyObject *self, PyObject *other);
static PyObject *registry_inplace_remainder(PyObject *self, PyObject *other);
static PyObject *registry_inplace_power(PyObject *self, PyObject *other);
static PyObject *registry_inplace_and(PyObject *self, PyObject *other);
static PyObject *registry_inplace_or(PyObject *self, PyObject *other);
static PyObject *registry_inplace_floor_divide(PyObject *self, PyObject *other);
static PyObject *registry_inplace_xor(PyObject *self, PyObject *other);
static PyObject *registry_inplace_lshift(PyObject *self, PyObject *other);
static PyObject *registry_inplace_rshift(PyObject *self, PyObject *other);
static PyObject *registry_inplace_matrix_multiply(PyObject *self, PyObject *other);

/* Déclarations des adaptateurs */
#define INPLACE_ADAPTER_DECL(name) \
    static PyObject *registry_inplace_##name##_adapter(PyObject *self, PyObject *other)

/* Déclaration spéciale pour power qui prend 3 paramètres */
static PyObject *registry_inplace_power_adapter(PyObject *self, PyObject *other, PyObject *modulus);

INPLACE_ADAPTER_DECL(add);
INPLACE_ADAPTER_DECL(subtract);
INPLACE_ADAPTER_DECL(multiply);
INPLACE_ADAPTER_DECL(divide);
INPLACE_ADAPTER_DECL(remainder);
/* power is declared separately above */
INPLACE_ADAPTER_DECL(and);
INPLACE_ADAPTER_DECL(or);
INPLACE_ADAPTER_DECL(floor_divide);
INPLACE_ADAPTER_DECL(xor);
INPLACE_ADAPTER_DECL(lshift);
INPLACE_ADAPTER_DECL(rshift);
INPLACE_ADAPTER_DECL(matrix_multiply);

/* Déclarations des fonctions */
static PyObject *registry_get_item(PyObject *self, PyObject *key);
static int registry_set_item(PyObject *self, PyObject *key, PyObject *value);
static int registry_contains(PyObject *self, PyObject *key);
static PyObject *registry_get_method(PyObject *self, PyObject *args);
static PyObject *registry_get_path_method(PyObject *self, PyObject *args);
static PyObject *registry_bitwise_and_method(PyObject *self, PyObject *args);
static PyObject *registry_bitwise_or_method(PyObject *self, PyObject *args);
static PyObject *registry_floor_divide_method(PyObject *self, PyObject *args);
static PyObject *registry_xor_method(PyObject *self, PyObject *args);
static PyObject *registry_lshift_method(PyObject *self, PyObject *args);
static PyObject *registry_rshift_method(PyObject *self, PyObject *args);
static PyObject *registry_matrix_multiply_method(PyObject *self, PyObject *args);

/* Implémentations des adaptateurs */
#define INPLACE_ADAPTER_DEF(name) \
    static PyObject *registry_inplace_##name##_adapter(PyObject *self, PyObject *other) { \
        return registry_inplace_##name(self, other); \
    }

/* Adaptateur spécial pour power qui prend 3 paramètres (ternaryfunc) */
static PyObject *registry_inplace_power_adapter(PyObject *self, PyObject *other, PyObject *modulus) {
    return registry_inplace_power(self, other);
}

INPLACE_ADAPTER_DEF(add)
INPLACE_ADAPTER_DEF(subtract)
INPLACE_ADAPTER_DEF(multiply)
INPLACE_ADAPTER_DEF(divide)
INPLACE_ADAPTER_DEF(remainder)
/* power is defined separately above */
INPLACE_ADAPTER_DEF(and)
INPLACE_ADAPTER_DEF(or)
INPLACE_ADAPTER_DEF(floor_divide)
INPLACE_ADAPTER_DEF(xor)
INPLACE_ADAPTER_DEF(lshift)
INPLACE_ADAPTER_DEF(rshift)
INPLACE_ADAPTER_DEF(matrix_multiply)

/* Rich comparison methods */
static PyObject *registry_richcompare(PyObject *self, PyObject *other, int op) {
    PyFastyRegistryObject *registry = (PyFastyRegistryObject *)self;
    
    /* Protection contre la récursion infinie lors des comparaisons */
    static int in_comparison = 0;
    PyObject *result;
    
    /* Gestion simple si déjà dans une comparaison */
    if (in_comparison) {
        in_comparison = 0;
        
        /* Comparaison d'identité directe pour éviter la récursion */
        if (op == Py_EQ)
            return PyBool_FromLong(self == other);
        else if (op == Py_NE)
            return PyBool_FromLong(self != other);
        else
            Py_RETURN_NOTIMPLEMENTED;
    }
    
    /* Indiquer que nous sommes dans une comparaison */
    in_comparison = 1;
    
    /* Si la valeur est None ou un dictionnaire vide, utiliser 0 pour comparer */
    if (registry->base.value == Py_None || 
        (PyDict_Check(registry->base.value) && PyDict_Size(registry->base.value) == 0)) {
        PyObject *zero = PyLong_FromLong(0);
        result = PyObject_RichCompare(zero, other, op);
        Py_DECREF(zero);
        in_comparison = 0;
        return result;
    }
    
    /* Si nous avons une valeur directe, essayer de la comparer */
    if (registry->base.value != Py_None) {
        result = PyObject_RichCompare(registry->base.value, other, op);
        if (result != Py_NotImplemented) {
            in_comparison = 0;
            return result;
        }
        Py_DECREF(result);
    }

    /* Fallback à la comparaison d'identité */
    in_comparison = 0;
    if (op == Py_EQ)
        return PyBool_FromLong(self == other);
    else if (op == Py_NE)
        return PyBool_FromLong(self != other);
    else
        Py_RETURN_NOTIMPLEMENTED;
}

/* Number methods */
static PyNumberMethods registry_as_number = {
    registry_add,            /* nb_add */
    0,                       /* nb_subtract */
    0,                       /* nb_multiply */
    0,                       /* nb_remainder */
    0,                       /* nb_divmod */
    0,                       /* nb_power */
    0,                       /* nb_negative */
    0,                       /* nb_positive */
    0,                       /* nb_absolute */
    0,                       /* nb_bool */
    0,                       /* nb_invert */
    0,                       /* nb_lshift */
    0,                       /* nb_rshift */
    0,                       /* nb_and */
    0,                       /* nb_xor */
    0,                       /* nb_or */
    0,                       /* nb_int */
    0,                       /* nb_reserved */
    0,                       /* nb_float */
    registry_inplace_add_adapter,    /* nb_inplace_add */
    registry_inplace_subtract_adapter, /* nb_inplace_subtract */
    registry_inplace_multiply_adapter, /* nb_inplace_multiply */
    registry_inplace_remainder_adapter, /* nb_inplace_remainder */
    registry_inplace_power_adapter, /* nb_inplace_power */
    registry_inplace_lshift_adapter, /* nb_inplace_lshift */
    registry_inplace_rshift_adapter, /* nb_inplace_rshift */
    registry_inplace_and_adapter,  /* nb_inplace_and */
    registry_inplace_xor_adapter,  /* nb_inplace_xor */
    registry_inplace_or_adapter,   /* nb_inplace_or */
    0,                       /* nb_floor_divide */
    0,                       /* nb_true_divide */
    registry_inplace_floor_divide_adapter, /* nb_inplace_floor_divide */
    registry_inplace_divide_adapter, /* nb_inplace_true_divide */
    0,                       /* nb_index */
    registry_inplace_matrix_multiply_adapter, /* nb_matrix_multiply */
    registry_inplace_matrix_multiply_adapter, /* nb_inplace_matrix_multiply */
};

/* Mapping methods */
static PyMappingMethods registry_as_mapping = {
    0,                       /* mp_length */
    registry_get_item,       /* mp_subscript */
    registry_set_item,       /* mp_ass_subscript */
};

/* Methods de l'objet */
static PyMethodDef registry_methods[] = {
    {"get", (PyCFunction)registry_get_method, METH_VARARGS, "Get item with default value"},
    {"get_path", (PyCFunction)registry_get_path_method, METH_VARARGS, "Get item with a dot-notated path (e.g. 'a.b.c')"},
    {"bitwise_and", (PyCFunction)registry_bitwise_and_method, METH_VARARGS, "Perform bitwise AND operation (alternative to &=)"},
    {"bitwise_or", (PyCFunction)registry_bitwise_or_method, METH_VARARGS, "Perform bitwise OR operation (alternative to |=)"},
    {"floor_divide", (PyCFunction)registry_floor_divide_method, METH_VARARGS, "Perform floor division operation (alternative to //=)"},
    {"xor", (PyCFunction)registry_xor_method, METH_VARARGS, "Perform XOR operation (alternative to ^=)"},
    {"lshift", (PyCFunction)registry_lshift_method, METH_VARARGS, "Perform left shift operation (alternative to <<=)"},
    {"rshift", (PyCFunction)registry_rshift_method, METH_VARARGS, "Perform right shift operation (alternative to >>=)"},
    {"matrix_multiply", (PyCFunction)registry_matrix_multiply_method, METH_VARARGS, "Perform matrix multiplication (alternative to @=)"},
    {NULL, NULL, 0, NULL}  /* Sentinel */
};

static PyTypeObject PyFastyRegistryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pyfasty._pyfasty.Registry",
    .tp_doc = "Registry object for dynamic attribute access",
    .tp_basicsize = sizeof(PyFastyRegistryObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = registry_new,
    .tp_init = (initproc)registry_init,
    .tp_dealloc = (destructor)registry_dealloc,
    .tp_getattro = (getattrofunc)registry_getattro,
    .tp_setattro = (setattrofunc)registry_setattro,
    .tp_str = (reprfunc)registry_str,
    .tp_repr = (reprfunc)registry_repr,
    .tp_as_number = &registry_as_number,
    .tp_as_mapping = &registry_as_mapping,
    .tp_methods = registry_methods,
    .tp_richcompare = registry_richcompare,
};

/* Helper function to create a new registry */
static PyObject *registry_create(int depth, PyObject *value) {
    PyFastyRegistryObject *registry = (PyFastyRegistryObject *)pyfasty_base_create(
        &PyFastyRegistryType, PYFASTY_REGISTRY_TYPE, depth, value);
    
    if (registry != NULL) {
        /* Initialize operation state */
        registry->current_op = OP_NONE;
    }
    
    return (PyObject *)registry;
}

/* Optimized fast-path getattr function */
static PyObject* registry_getattr_recursive(PyObject *self, PyObject *name) {
    /* CRITICAL: Reference to global event evaluation state (from event_sync.c) */
    extern int g_in_condition_evaluation;
    
    /* STEP 1: Check if attribute already exists (fast path) */
    PyFastyBaseObject *base = (PyFastyBaseObject *)self;
    PyObject *existing_result = PyDict_GetItem(base->data, name);
    if (existing_result != NULL) {
        /* Attribute exists - return it normally (la conversion sera gérée par __str__ si nécessaire) */
        /* Sinon, retourner l'objet normalement */
        Py_INCREF(existing_result);
        return existing_result;
    }
    
    /* STEP 2: READ-ONLY MODE - prevent auto-creation during event evaluation */
    /* This prevents false positives when event conditions check for attributes */
    if (g_in_condition_evaluation) {
        /* In event evaluation mode: return None for ALL non-existent attributes */
        /* This stops auto-creation and prevents side effects during condition checks */
        Py_RETURN_NONE;
    }
    
    /* STEP 3: Normal mode - allow auto-creation for standard attribute access */
    return pyfasty_base_getattr_recursive(self, name, &PyFastyRegistryType, PYFASTY_REGISTRY_TYPE);
}

/* Recursive setattr function */
static int registry_setattr_recursive(PyObject *obj, PyObject *name, PyObject *value) {
    /* CASE 1: Registry object - use internal data dictionary */
    if (Py_TYPE(obj) == &PyFastyRegistryType) {
        PyFastyRegistryObject *self = (PyFastyRegistryObject *)obj;
        PyObject *internal_dict = self->base.data;
        if (internal_dict == NULL) {
            PyErr_SetString(PyExc_AttributeError, "Internal dictionary not initialized");
            return -1;
        }
        
        /* Update or add element to internal dictionary */
        if (PyDict_SetItem(internal_dict, name, value) < 0) {
            return -1;
        }
        
        return 0;
    }
    /* CASE 2: Plain dictionary - direct dictionary access */
    else if (PyDict_Check(obj)) {
        if (PyDict_SetItem(obj, name, value) < 0) {
            return -1;
        }
        
        return 0;
    }
    /* CASE 3: List object - interpret name as index */
    else if (PyList_Check(obj)) {
        long index = PyLong_AsLong(name);
        if (index == -1 && PyErr_Occurred()) {
            PyErr_Clear();
            /* Try to convert string to number */
            if (PyUnicode_Check(name)) {
                const char *str = PyUnicode_AsUTF8(name);
                if (str) {
                    char *endptr;
                    index = strtol(str, &endptr, 10);
                    if (*endptr != '\0') {
                        PyErr_Format(PyExc_IndexError, "Invalid list index: '%s'", str);
                        return -1;
                    }
                }
            } else {
                PyErr_SetString(PyExc_TypeError, "List index must be integer or string");
                return -1;
            }
        }
        
        /* Validate index bounds */
        Py_ssize_t size = PyList_Size(obj);
        if (index < 0 || index >= size) {
            PyErr_Format(PyExc_IndexError, "List index out of bounds: %ld", index);
            return -1;
        }
        
        /* Replace element at index */
        Py_INCREF(value);
        return PyList_SetItem(obj, index, value);
    }
    /* CASE 4: Generic object - use default setattr */
    else {
        return PyObject_SetAttr(obj, name, value);
    }
}

/* Fonction publique pour la modification d'un élément */
PyObject *registry_setitem(PyObject *self, PyObject *args) {
    PyObject *key, *value;
    
    if (!PyArg_ParseTuple(args, "OO", &key, &value)) {
        return NULL;
    }
    
    if (registry_setattr_recursive(self, key, value) < 0) {
        return NULL;
    }
    
    /* Déclencher les événements synchrones, en spécifiant que l'événement vient du module registry */
    pyfasty_trigger_sync_events_with_module(MODULE_REGISTRY);
    
    Py_RETURN_NONE;
}

/* Deallocation function */
static void registry_dealloc(PyFastyRegistryObject *self) {
    /* Retourner les dictionnaires au pool et libérer la valeur */
    pyfasty_dict_pool_return(self->base.data);
    pyfasty_dict_pool_return(self->base.cache);
    Py_XDECREF(self->base.value);
    
    /* Appel à PyObject_Del avec le bon type */
    PyObject_Del(self);
}

/* New function */
static PyObject *registry_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PyObject *value = NULL;
    
    if (PyTuple_Size(args) > 0) {
        value = PyTuple_GetItem(args, 0);
    }
    
    PyFastyRegistryObject *self = (PyFastyRegistryObject *)pyfasty_base_create(
        type, PYFASTY_REGISTRY_TYPE, 0, value);
    
    if (self != NULL) {
        /* Initialize registry-specific fields */
        self->current_op = OP_NONE;
    }
    
    return (PyObject *)self;
}

/* Init function */
static int registry_init(PyFastyRegistryObject *self, PyObject *args, PyObject *kwds) {
    PyObject *value = NULL;
    
    if (!PyArg_ParseTuple(args, "|O", &value)) {
        return -1;
    }
    
    /* Initialize operation state */
    self->current_op = OP_NONE;
    
    return 0;
}

/* Helper function pour sauvegarder la valeur originale - REMPLACÉ par helper inline */
static void registry_save_original_value(PyFastyRegistryObject *registry) {
    maybe_save_original_value((PyFastyBaseObject *)registry);
}

/* Getattro function */
static PyObject *registry_getattro(PyFastyRegistryObject *self, PyObject *name) {
    /* Tracer l'accès au module registry pour la détection de dépendances */
    pyfasty_trace_module_access(MODULE_REGISTRY);
    
    const char *name_str = PyUnicode_AsUTF8(name);
    
    /* Si c'est un nom spécial avec _, utiliser la voie normale */
    if (name_str[0] == '_') {
        return PyObject_GenericGetAttr((PyObject *)self, name);
    }

    /* Nouvelle logique pour les opérations en place */
    if (self->current_op != OP_NONE) {
        PyObject *attribute_obj = PyDict_GetItem(self->base.data, name);

        if (attribute_obj != NULL) {
            // L'attribut existe dans self->base.data
            if (PyObject_TypeCheck(attribute_obj, &PyFastyRegistryType)) {
                Py_INCREF(attribute_obj);
                return attribute_obj;
            } else {
                // L'attribut existe mais n'est pas un Registry, l'envelopper.
                PyObject *wrapped_registry = registry_create(self->base.depth + 1, attribute_obj);
                if (wrapped_registry == NULL) {
                    return NULL;
                }
                if (PyDict_SetItem(self->base.data, name, wrapped_registry) < 0) {
                    Py_DECREF(wrapped_registry);
                    return NULL;
                }
                PyDict_SetItem(self->base.cache, name, wrapped_registry); 
                return wrapped_registry; 
            }
        } else {
            // L'attribut n'existe pas, le créer avec une valeur par défaut basée sur l'opération
            PyObject *initial_value_for_new_attr = NULL;
            switch (self->current_op) {
                case OP_IMUL: 
                case OP_IPOW: 
                case OP_IMATMUL:
                    initial_value_for_new_attr = PyLong_FromLong(PYFASTY_DEFAULT_MUL_VALUE);
                    break;
                case OP_IDIV: 
                    initial_value_for_new_attr = PyFloat_FromDouble(PYFASTY_DEFAULT_DIV_VALUE); 
                    break;
                case OP_IADD:
                case OP_ISUB:
                case OP_IMOD:
                case OP_IAND:
                case OP_IOR:
                case OP_IXOR:
                case OP_ILSHIFT:
                case OP_IRSHIFT:
                default:
                    initial_value_for_new_attr = PyLong_FromLong(PYFASTY_DEFAULT_ADD_VALUE);
                    break;
            }

            if (initial_value_for_new_attr == NULL) {
                return NULL; 
            }

            PyObject *new_registry_attr = registry_create(self->base.depth + 1, initial_value_for_new_attr);
            Py_DECREF(initial_value_for_new_attr); 

            if (new_registry_attr == NULL) {
                return NULL;
            }

            if (PyDict_SetItem(self->base.data, name, new_registry_attr) < 0) {
                Py_DECREF(new_registry_attr);
                return NULL;
            }
            PyDict_SetItem(self->base.cache, name, new_registry_attr);
            return new_registry_attr; 
    }
    } else {
        // Comportement standard pour les accès non liés à une opération en place
        // Si la valeur directe est une primitive, la sauvegarder avant de la transformer potentiellement
    if (self->base.value != Py_None && is_primitive_value(self->base.value)) {
        
            PyObject *existing_attr_in_data = PyDict_GetItem(self->base.data, name);
            if (!existing_attr_in_data || !PyObject_TypeCheck(existing_attr_in_data, &PyFastyRegistryType)) {
                registry_save_original_value(self);
            }
        }

    PyObject *result = registry_getattr_recursive((PyObject *)self, name);
    
    if (!result && PyErr_Occurred() && PyErr_ExceptionMatches(PyExc_AttributeError)) {
        PyErr_Clear();
        result = registry_create(self->base.depth + 1, NULL);
        if (result) {
                if (PyDict_SetItem(self->base.data, name, result) < 0) {
                    Py_DECREF(result);
                    return NULL; 
                }
            PyDict_SetItem(self->base.cache, name, result);
        }
    }
    return result;
    }
}

/* Setattro function */
static int registry_setattro(PyFastyRegistryObject *self, PyObject *name, PyObject *value) {
    /* Fast path: noms spéciaux avec dunder */
    const char *name_str = PyUnicode_AsUTF8(name);
    if (name_str[0] == '_') {
        return PyObject_GenericSetAttr((PyObject *)self, name, value);
    }
    
    /* AMÉLIORATION CRITIQUE: Détecter les opérations en place */
    if (value != NULL && PyObject_TypeCheck(value, &PyFastyRegistryType)) {
        /* Vérifier si l'attribut existe déjà */
        PyObject *existing = PyDict_GetItem(self->base.data, name);
        if (existing != NULL && existing == value) {
            /* C'est le même objet ! (résultat d'une opération en place)
               Ne rien faire, la valeur a déjà été modifiée par l'opération arithmétique */
            pyfasty_trigger_sync_events_with_module(MODULE_REGISTRY);
            return 0;
        }
    }
    
    /* Traitement des opérations en place basé sur current_op plutôt que des noms spécifiques */
    if (self->current_op != OP_NONE) {
        /* Stocker la valeur dans le dictionnaire avec une clé générique __value__ */
        PyObject *key = PyUnicode_FromString(PYFASTY_INTERNAL_VALUE_KEY);
        if (key) {
            PyDict_SetItem(self->base.data, key, value);
            Py_DECREF(key);
        }
        
        /* Mettre à jour la valeur directe pour que les accès soient corrects */
        Py_XDECREF(self->base.value);
        Py_INCREF(value);
        self->base.value = value;
        
        /* Réinitialiser l'opération en cours */
        self->current_op = OP_NONE;
        
        /* Retourner succès */
        return 0;
    }
    
    /* AMÉLIORATION: Ne pas sauvegarder __original_value__ pour les primitives numériques */
    /* Vérifier si l'objet actuel est un attribut (test.test) et non un niveau racine (test) */
    if (self->base.depth > 0 && PyUnicode_Check(self->base.value)) {
        /* Si c'est un attribut de chaîne de texte, ne pas remplacer sa valeur directe
           mais simplement stocker le nouvel attribut dans le dictionnaire data */
        PyObject *existing = PyDict_GetItem(self->base.data, name);
        if (existing) {
            /* Si l'attribut existe déjà et est un Registry, mettre à jour sa valeur */
            if (PyObject_TypeCheck(existing, &PyFastyRegistryType)) {
                PyFastyBaseObject *obj = (PyFastyBaseObject *)existing;
                
                /* Mettre à jour la valeur directe de l'attribut */
                Py_XDECREF(obj->value);
                Py_INCREF(value);
                obj->value = value;
                
                return 0;
            }
        }
        
        /* Créer un nouveau Registry pour l'attribut */
        PyObject *new_obj = registry_create(self->base.depth + 1, value);
        if (new_obj == NULL) {
            return -1;
        }
        
        /* Stocker dans le dictionnaire de données mais NE PAS mettre à jour base.value */
        int result = PyDict_SetItem(self->base.data, name, new_obj);
        Py_DECREF(new_obj);
        
        /* Déclencher les événements synchrones après modification d'attributs */
        if (result >= 0) {
            pyfasty_trigger_sync_events_with_module(MODULE_REGISTRY);
        }
        
        return result;
    }
    
    /* AMÉLIORATION: Sauvegarder __original_value__ seulement si ce n'est pas une valeur numérique */
    if (self->base.value != Py_None && 
        !is_numeric_value(self->base.value)) {
        /* Avant de remplacer la valeur, sauvegarder la valeur originale */
        registry_save_original_value(self);
    }
    
    /* Pour le niveau racine ou les types non-chaîne, stocker la valeur directe */
    Py_XDECREF(self->base.value);
    Py_INCREF(value);
    self->base.value = value;
    
    /* Utiliser pyfasty_base_setattr_recursive pour le reste */
    int result = pyfasty_base_setattr_recursive((PyObject *)self, name, value, 
                                              &PyFastyRegistryType, PYFASTY_REGISTRY_TYPE);
    
    /* Déclencher les événements synchrones après modification d'attributs */
    if (result >= 0) {
        /* Déclencher uniquement les événements liés au registry */
        pyfasty_trigger_sync_events_with_module(MODULE_REGISTRY);
    }
    
    return result;
}

/* Sq_ass_item function (indexed assignment) */
static int registry_ass_item(PyFastyRegistryObject *self, Py_ssize_t i, PyObject *value) {
    /* Convertir l'index en objet Python */
    PyObject *index = PyLong_FromSsize_t(i);
    if (index == NULL) {
        return -1;
    }
    
    /* Utiliser registry_setattr_recursive pour le reste */
    int result = registry_setattr_recursive((PyObject *)self, index, value);
    Py_DECREF(index);
    
    /* Déclencher les événements synchrones après modification */
    if (result >= 0) {
        /* Déclencher uniquement les événements liés au registry */
        pyfasty_trigger_sync_events_with_module(MODULE_REGISTRY);
    }
    
    return result;
}

/* Sq_ass_subscript function (subscript assignment) */
static int registry_ass_subscript(PyFastyRegistryObject *self, PyObject *key, PyObject *value) {
    /* Cas spécial pour None: effacer */
    if (value == NULL) {
        /* Obtenir le dictionnaire interne */
        PyObject *internal_dict = self->base.data;
        if (internal_dict == NULL) {
            PyErr_SetString(PyExc_AttributeError, "Internal dictionary not initialized");
            return -1;
        }
        
        /* Supprimer l'élément */
        if (PyDict_DelItem(internal_dict, key) < 0) {
            return -1;
        }
        
        /* Déclencher les événements synchrones */
        pyfasty_trigger_sync_events_with_module(MODULE_REGISTRY);
        
        return 0;
    }
    
    /* Utiliser registry_setattr_recursive pour le reste */
    int result = registry_setattr_recursive((PyObject *)self, key, value);
    
    /* Déclencher les événements synchrones après modification */
    if (result >= 0) {
        /* Déclencher uniquement les événements liés au registry */
        pyfasty_trigger_sync_events_with_module(MODULE_REGISTRY);
    }
    
    return result;
}

/* Helper pour obtenir la représentation en chaîne d'un Registry - Version simplifiée et thread-safe */
static PyObject *registry_get_str_repr(PyFastyRegistryObject *self, PyObject *(*converter)(PyObject*)) {
    /* Protection simple contre la récursion infinie avec un compteur statique */
    static int recursion_depth = 0;
    
    /* Protection contre la récursion infinie */
    if (recursion_depth > 10) {
        return converter(self->base.data);
    }
    
    recursion_depth++;
    PyObject *result = NULL;
    
    /* Priorité 1: Si la valeur est un dictionnaire non-vide, l'afficher */
    if (self->base.value != Py_None && PyDict_Check(self->base.value)) {
        Py_ssize_t dict_size = PyDict_Size(self->base.value);
        if (dict_size > 0) {
            /* Vérifier si c'est un dictionnaire "pollué" avec seulement __original_value__ */
            if (dict_size == 1) {
                PyObject *orig_key = PyUnicode_FromString(PYFASTY_ORIGINAL_VALUE_KEY);
                if (orig_key && PyDict_Contains(self->base.value, orig_key)) {
                    /* Dictionnaire pollué, utiliser la valeur originale */
                    PyObject *orig_value = PyDict_GetItem(self->base.value, orig_key);
                    Py_DECREF(orig_key);
                    if (orig_value != NULL) {
                        result = converter(orig_value);
                        goto cleanup;
                    }
                } else {
                    Py_XDECREF(orig_key);
                }
            }
            /* Dictionnaire normal, l'afficher */
            result = converter(self->base.value);
            goto cleanup;
        }
    }
    
    /* Priorité 2: Valeurs primitives */
    if (self->base.value != Py_None) {
        if (PyLong_Check(self->base.value) || PyFloat_Check(self->base.value) || 
            PyBool_Check(self->base.value) || PyUnicode_Check(self->base.value)) {
            result = converter(self->base.value);
            goto cleanup;
        }
        
        /* Si la valeur est un autre Registry, suivre sa valeur */
        if (PyObject_TypeCheck(self->base.value, &PyFastyRegistryType)) {
            PyFastyRegistryObject *nested_registry = (PyFastyRegistryObject *)self->base.value;
            
            /* Éviter la récursion infinie */
            if (nested_registry != self && recursion_depth < 10) {
                /* Appeler récursivement pour obtenir la représentation */
                result = registry_get_str_repr(nested_registry, converter);
                goto cleanup;
            }
        }
    }
    
    /* Priorité 3: Essayer __value__ du dictionnaire de données */
    PyObject *internal_value_obj = PyDict_GetItemString(self->base.data, PYFASTY_INTERNAL_VALUE_KEY);
    if (internal_value_obj != NULL) { 
        result = converter(internal_value_obj);
        if (result != NULL) {
            goto cleanup;
        }
        PyErr_Clear();
    }
    
    /* Priorité 4: Si base.value était None, retourner None */
    if (self->base.value == Py_None) {
        result = converter(Py_None);
        goto cleanup;
            }
            
    /* Priorité 5: Fallback au dictionnaire de données */
    result = converter(self->base.data);

cleanup:
    recursion_depth--;
    return result;
                }

/* String representation */
static PyObject *registry_str(PyFastyRegistryObject *self) {
    return registry_get_str_repr(self, PyObject_Str);
}

/* String representation */
static PyObject *registry_repr(PyFastyRegistryObject *self) {
    return registry_get_str_repr(self, PyObject_Repr);
}

/* Implementation of contains check - Version générique pour Registry et Config */
int pyfasty_common_contains(PyObject *self, PyObject *key) {
    PyFastyBaseObject *base = (PyFastyBaseObject *)self;
    
    /* If we have a direct value and it's a dict, check there first */
    if (base->value != Py_None && PyDict_Check(base->value)) {
        if (PyDict_Contains(base->value, key)) {
            return 1;
        }
    }
    
    /* Check the data dictionary */
    return PyDict_Contains(base->data, key);
}

/* Implementation of contains check - Version spécifique Registry */
static int registry_contains(PyObject *self, PyObject *key) {
    return pyfasty_common_contains(self, key);
}

/* Version générique de get_item pour Registry et Config */
PyObject *pyfasty_common_getitem(PyObject *self, PyObject *key, PyTypeObject *type, 
                                PyFastyObjectType obj_type, 
                                PyObject *(*create_func)(int, PyObject*)) {
    PyFastyBaseObject *base = (PyFastyBaseObject *)self;
    PyObject *result = NULL;
    
    /* If we have a direct value and it's a dict or supports __getitem__ */
    if (base->value != Py_None) {
        if (PyDict_Check(base->value)) {
            result = PyDict_GetItem(base->value, key);
            if (result != NULL) {
                /* Si le résultat est un dict, on le convertit en Registry/Config */
                if (PyDict_Check(result)) {
                    PyObject *new_obj = create_func(base->depth + 1, result);
                    if (new_obj == NULL) {
                        return NULL;
                    }
                    
                    /* Stocker dans le dictionnaire direct */
                    if (PyDict_SetItem(base->value, key, new_obj) < 0) {
                        Py_DECREF(new_obj);
                        return NULL;
                    }
                    
                    return new_obj;
                }
                
                /* IMPORTANT: Si le résultat est une valeur primitive (entier, chaîne, etc.), 
                   l'envelopper dans un Registry/Config pour un comportement cohérent avec l'accès par attribut */
                return wrap_primitive_if_needed(result, type, obj_type, base->depth + 1);
            }
        } else if (PyObject_HasAttrString(base->value, "__getitem__")) {
            result = PyObject_GetItem(base->value, key);
            if (result != NULL) {
                /* Même ici, envelopper les valeurs primitives si nécessaire avec helper */
                return wrap_primitive_if_needed(result, type, obj_type, base->depth + 1);
            }
            PyErr_Clear();
        } else if (PyLong_Check(base->value) || PyFloat_Check(base->value) || 
                   PyBool_Check(base->value) || PyUnicode_Check(base->value)) {
            /* CRUCIAL: Pour les accès mixtes comme pyfasty.registry["test_3"]["test"], 
               nous devons nous assurer que même si base->value est un type primitif comme un entier,
               on crée quand même un Registry pour l'accès aux attributs/clés suivants */
            PyObject *new_obj = create_func(base->depth + 1, NULL);
            if (new_obj == NULL) {
                PyErr_SetObject(PyExc_KeyError, key);
                return NULL;
            }
            
            /* Stocker la clé comme attribut dans ce nouveau Registry */
            PyObject *key_str = NULL;
            if (PyUnicode_Check(key)) {
                key_str = key;
                Py_INCREF(key);
            } else {
                key_str = PyObject_Str(key);
            }
            
            if (key_str) {
                int result = pyfasty_base_setattr_recursive(new_obj, key_str, Py_None, 
                                                          type, obj_type);
                Py_DECREF(key_str);
                
                if (result < 0) {
                    Py_DECREF(new_obj);
                    PyErr_SetObject(PyExc_KeyError, key);
                    return NULL;
                }
            }
            
            return new_obj;
        }
    }
    
    /* Check the data dictionary as fallback */
    result = PyDict_GetItem(base->data, key);
    if (result == NULL) {
        /* Si aucun élément trouvé, mais que nous avons une valeur directe qui est un entier ou autre primitive,
           alors nous créons un nouvel objet Registry pour permettre l'accès par attributs dynamique */
        if (base->value != Py_None && (PyLong_Check(base->value) || PyFloat_Check(base->value) || 
            PyBool_Check(base->value) || PyUnicode_Check(base->value))) {
            PyObject *new_obj = create_func(base->depth + 1, NULL);
            if (new_obj == NULL) {
                PyErr_SetObject(PyExc_KeyError, key);
                return NULL;
            }
            
            /* Stocker dans le dictionnaire data pour les futurs accès */
            if (PyDict_SetItem(base->data, key, new_obj) < 0) {
                Py_DECREF(new_obj);
                PyErr_SetObject(PyExc_KeyError, key);
                return NULL;
            }
            
            return new_obj;
        }
        
        PyErr_SetObject(PyExc_KeyError, key);
        return NULL;
    }
    
    /* Si le résultat est un dict, on le convertit en Registry/Config */
    if (PyDict_Check(result)) {
        PyObject *new_obj = create_func(base->depth + 1, result);
        if (new_obj == NULL) {
            return NULL;
        }
        
        /* Stocker dans le dictionnaire data */
        if (PyDict_SetItem(base->data, key, new_obj) < 0) {
            Py_DECREF(new_obj);
            Py_INCREF(result);
            return result;
        }
        
        return new_obj;
    }
    
    /* Si c'est une valeur primitive, l'envelopper avec helper optimisé */
    return wrap_primitive_if_needed(result, type, obj_type, base->depth + 1);
}

/* Version générique de set_item pour Registry et Config */
int pyfasty_common_setitem(PyObject *self, PyObject *key, PyObject *value, PyTypeObject *type, 
                          PyFastyObjectType obj_type, 
                          PyObject *(*create_func)(int, PyObject*)) {
    PyFastyBaseObject *base = (PyFastyBaseObject *)self;
    int result;
    
    /* Si on a une valeur directe et c'est un dict */
    if (base->value != Py_None && PyDict_Check(base->value)) {
        if (value == NULL) {
            /* Cas de suppression */
            result = PyDict_DelItem(base->value, key);
            return result;
        } else {
            /* Cas d'affectation */
            /* Si value est un dict, on le convertit d'abord en Registry/Config */
            if (PyDict_Check(value)) {
                PyObject *new_obj = create_func(base->depth + 1, value);
                if (new_obj == NULL) {
                    return -1;
                }
                
                /* Affecter le nouveau Registry/Config */
                result = PyDict_SetItem(base->value, key, new_obj);
                Py_DECREF(new_obj);
            } else {
                result = PyDict_SetItem(base->value, key, value);
            }
            
            return result;
        }
    }
    
    /* Sinon, utiliser le dictionnaire de données */
    if (value == NULL) {
        result = PyDict_DelItem(base->data, key);
    } else {
        /* Vider le cache pour cohérence */
        if (PyDict_Contains(base->cache, key)) {
            PyDict_DelItem(base->cache, key);
        }
        
        /* Si value est un dict, on le convertit d'abord en Registry/Config */
        if (PyDict_Check(value)) {
            PyObject *new_obj = create_func(base->depth + 1, value);
            if (new_obj == NULL) {
                return -1;
            }
            
            /* Affecter le nouveau Registry/Config */
            result = PyDict_SetItem(base->data, key, new_obj);
            Py_DECREF(new_obj);
        } else {
            result = PyDict_SetItem(base->data, key, value);
        }
    }
    
    return result;
}

/* Implementation of registry_set_item - version Registry spécifique */
static int registry_set_item(PyObject *self, PyObject *key, PyObject *value) {
    /* Ensurer un comportement cohérent avec l'accès par attribut */
    if (value != NULL) {
        /* Si value est une valeur primitive, l'envelopper dans un Registry pour permettre l'accès aux attributs */
        if (PyLong_Check(value) || PyFloat_Check(value) || PyBool_Check(value) || 
            PyUnicode_Check(value) || value == Py_None) {
            /* Créer un objet Registry encapsulant la valeur primitive */
            PyObject *reg_obj = registry_create(0, value);
            if (reg_obj != NULL) {
                /* Utiliser le Registry au lieu de la valeur brute */
                int result = pyfasty_common_setitem(self, key, reg_obj, 
                                                  &PyFastyRegistryType, PYFASTY_REGISTRY_TYPE, registry_create);
                Py_DECREF(reg_obj);
                
                /* Déclencher les événements après modification */
                if (result >= 0) {
                    pyfasty_trigger_sync_events_with_module(MODULE_REGISTRY);
                }
                
                return result;
            }
        }
    }
    
    /* Pour les autres cas, utiliser l'implémentation commune */
    int result = pyfasty_common_setitem(self, key, value, &PyFastyRegistryType, 
                                      PYFASTY_REGISTRY_TYPE, registry_create);
    
    /* Déclencher les événements synchrones après modification d'éléments par clé */
    if (result >= 0) {
        pyfasty_trigger_sync_events_with_module(MODULE_REGISTRY);
    }
    
    return result;
}

/* Get item function - version Registry spécifique */
static PyObject *registry_get_item(PyObject *self, PyObject *key) {
    PyFastyRegistryObject *registry = (PyFastyRegistryObject *)self;
    
    /* Cas spécial: si la valeur directe est un entier ou une autre valeur primitive,
       nous devons quand même retourner un Registry pour permettre l'accès aux attributs/clés */
    if (registry->base.value != Py_None) {
        if (PyLong_Check(registry->base.value) || PyFloat_Check(registry->base.value) || 
            PyBool_Check(registry->base.value) || PyUnicode_Check(registry->base.value)) {
            
            /* D'abord vérifier si cette clé existe déjà dans le dictionnaire de données */
            PyObject *existing = PyDict_GetItem(registry->base.data, key);
            if (existing != NULL) {
                Py_INCREF(existing);
                return existing;
            }
            
            /* Pour les accès comme pyfasty.registry["test_3"]["test"], alors que test_3 est un entier,
               nous créons un Registry qui contient l'attribut 'test' */
            PyObject *new_obj = registry_create(registry->base.depth + 1, registry->base.value);
            if (new_obj != NULL) {
                /* Créer dynamiquement un attribut avec le nom de la clé */
                PyObject *str_key = PyObject_Str(key);
                if (str_key != NULL) {
                    /* Ajouter l'attribut/clé au Registry pour les accès futurs */
                    PyFastyBaseObject *reg_base = (PyFastyBaseObject *)new_obj;
                    /* IMPORTANT: Stocker également l'attribut dans le registry original */
                    PyDict_SetItem(registry->base.data, key, new_obj);
                    
                    /* Si ce registry a déjà un attribut avec ce nom, le copier dans le nouveau */
                    PyObject *attr_value = PyDict_GetItem(registry->base.data, str_key);
                    if (attr_value != NULL) {
                        PyDict_SetItem(reg_base->data, str_key, attr_value);
                    } else {
                        PyDict_SetItem(reg_base->data, str_key, Py_None);
                    }
                    Py_DECREF(str_key);
                }
                
                return new_obj;
            }
        }
    }
    
    /* Pour les autres cas, utiliser l'implémentation commune */
    return pyfasty_common_getitem(self, key, &PyFastyRegistryType, PYFASTY_REGISTRY_TYPE, registry_create);
}

/* Version générique de get_method pour Registry et Config */
PyObject *pyfasty_common_getmethod(PyObject *self, PyObject *args, PyFastyObjectType obj_type) {
    PyFastyBaseObject *base = (PyFastyBaseObject *)self;
    PyObject *key, *default_value = Py_None, *result;
    
    if (!PyArg_ParseTuple(args, "O|O:get", &key, &default_value))
        return NULL;
    
    /* If we have a direct value and it's a dict, check there first */
    if (base->value != Py_None && PyDict_Check(base->value)) {
        result = PyDict_GetItem(base->value, key);
        if (result != NULL) {
            Py_INCREF(result);
            return result;
        }
    }
    
    /* Check the data dictionary */
    result = PyDict_GetItem(base->data, key);
    if (result == NULL) {
        Py_INCREF(default_value);
        return default_value;
    }
    
    Py_INCREF(result);
    return result;
}

/* Implementation of get method - version Registry spécifique */
static PyObject *registry_get_method(PyObject *self, PyObject *args) {
    return pyfasty_common_getmethod(self, args, PYFASTY_REGISTRY_TYPE);
}

/* Nettoyage __original_value__ si présent */
static void registry_clean_original_value(PyFastyRegistryObject *registry) {
    PyObject *orig_key = PyUnicode_FromString(PYFASTY_ORIGINAL_VALUE_KEY);
    if (orig_key) {
        if (PyDict_Contains(registry->base.data, orig_key)) {
            PyDict_DelItem(registry->base.data, orig_key);
        }
        Py_DECREF(orig_key);
    }
}

/* Storage unified : base.value + __value__ dans data */
static int registry_store_value(PyFastyRegistryObject *registry, PyObject *new_value) {
            if (new_value == NULL) {
        return -1;
            }
            
            /* Update direct value */
    Py_XDECREF(registry->base.value);
    Py_INCREF(new_value);
            registry->base.value = new_value;
            
    /* Store dans data sous clé __value__ */
    PyObject *key = PyUnicode_FromString(PYFASTY_INTERNAL_VALUE_KEY);
    if (key) {
        int result = PyDict_SetItem(registry->base.data, key, new_value);
        Py_DECREF(key);
        return result;
    }
    
    return 0;
        }

/* === OPÉRATIONS ARITHMÉTIQUES IN-PLACE - VERSION OPTIMISÉE === */
/* Fonction générique pour toutes les opérations arithmétiques (factorisation sécurisée) */
typedef enum {
    ARITH_ADD, ARITH_SUB, ARITH_MUL, ARITH_DIV, ARITH_MOD, ARITH_POW, ARITH_AND, ARITH_OR, ARITH_FDIV,
    ARITH_XOR, ARITH_LSHIFT, ARITH_RSHIFT, ARITH_MATMUL
} ArithmeticType;

/* Table de mapping pour cohérence entre ArithmeticType et RegistryOperation */
static const RegistryOperation arith_to_registry_op[] = {
    [ARITH_ADD] = OP_IADD,
    [ARITH_SUB] = OP_ISUB,
    [ARITH_MUL] = OP_IMUL,
    [ARITH_DIV] = OP_IDIV,
    [ARITH_MOD] = OP_IMOD,
    [ARITH_POW] = OP_IPOW,
    [ARITH_AND] = OP_IAND,
    [ARITH_OR] = OP_IOR,
    [ARITH_FDIV] = OP_IDIV,     /* Floor division utilise même init que division */
    [ARITH_XOR] = OP_IXOR,
    [ARITH_LSHIFT] = OP_ILSHIFT,
    [ARITH_RSHIFT] = OP_IRSHIFT,
    [ARITH_MATMUL] = OP_IMATMUL
};

/* Helper pour définir l'opération en cours de manière cohérente */
static inline void registry_set_current_operation(PyFastyRegistryObject *registry, ArithmeticType arith_op) {
    if (arith_op >= 0 && arith_op < sizeof(arith_to_registry_op)/sizeof(arith_to_registry_op[0])) {
        registry->current_op = arith_to_registry_op[arith_op];
            } else {
        registry->current_op = OP_NONE;  /* Sécurité en cas d'opération invalide */
    }
}

static PyObject *registry_inplace_operation(PyObject *self, PyObject *other, ArithmeticType op_type) {
    PyFastyRegistryObject *registry = (PyFastyRegistryObject *)self;
    
    /* CORRECTION CRITIQUE : Définir current_op de manière cohérente */
    registry_set_current_operation(registry, op_type);
    
    /* Validation des types selon l'opération */
    switch (op_type) {
        case ARITH_MOD:
        case ARITH_AND:
        case ARITH_OR:
        case ARITH_XOR:
        case ARITH_LSHIFT:
        case ARITH_RSHIFT:
            if (!PyLong_Check(other)) {
                PyErr_SetString(PyExc_TypeError, 
                    op_type == ARITH_MOD ? "Opérateur %= nécessite un entier" :
                    op_type == ARITH_AND ? "Opérateur &= nécessite un entier" :
                    op_type == ARITH_OR ? "Opérateur |= nécessite un entier" :
                    op_type == ARITH_XOR ? "Opérateur ^= nécessite un entier" :
                    op_type == ARITH_LSHIFT ? "Opérateur <<= nécessite un entier" :
                                            "Opérateur >>= nécessite un entier");
        return NULL;
    }
            break;
        case ARITH_FDIV:
            if (!is_numeric_value(other)) {
        PyErr_Format(PyExc_TypeError, 
                            "Operateur //= non supporte entre les types %s et %s",
                            Py_TYPE(self)->tp_name, Py_TYPE(other)->tp_name);
            return NULL;
        }
            break;
        case ARITH_MATMUL:
            /* Multiplication matricielle - accepter tous types pour flexibilité */
            break;
        default:
            if (!is_numeric_value(other)) {
                const char *op_names[] = {"+=", "-=", "*=", "/=", "%=", "**=", "&=", "|=", "//=", "^=", "<<=", ">>=", "@="};
                PyErr_Format(PyExc_TypeError, "Operator %s requires a number", 
                           op_names[op_type]);
            return NULL;
        }
    }
    
    /* Protection division par zéro */
    if ((op_type == ARITH_DIV || op_type == ARITH_FDIV) && 
        extract_numeric_value(other, 1.0) == 0.0) {
        PyErr_SetString(PyExc_ZeroDivisionError, 
                       op_type == ARITH_DIV ? "Division par zéro" : "Division entière par zéro");
        return NULL;
    }
    
    if (op_type == ARITH_MOD && PyLong_AsLong(other) == 0) {
                    PyErr_SetString(PyExc_ZeroDivisionError, "Modulo by zero");
        return NULL;
    }
    
    /* Calcul selon l'opération */
    PyObject *new_value = NULL;
    
    if (op_type == ARITH_MOD || op_type == ARITH_AND || op_type == ARITH_OR || op_type == ARITH_FDIV ||
        op_type == ARITH_XOR || op_type == ARITH_LSHIFT || op_type == ARITH_RSHIFT) {
        /* Opérations entières */
        long current_val = 0;
        if (registry->base.value != Py_None && PyLong_Check(registry->base.value)) {
            current_val = PyLong_AsLong(registry->base.value);
        } else if (registry->base.value != Py_None && PyFloat_Check(registry->base.value)) {
            current_val = (long)PyFloat_AsDouble(registry->base.value);
        }
        
        long other_val = PyLong_AsLong(other);
        long result_val;
        
        switch (op_type) {
            case ARITH_MOD:    result_val = current_val % other_val; break;
            case ARITH_AND:    result_val = current_val & other_val; break;
            case ARITH_OR:     result_val = current_val | other_val; break;
            case ARITH_XOR:    result_val = current_val ^ other_val; break;
            case ARITH_LSHIFT: result_val = current_val << other_val; break;
            case ARITH_RSHIFT: result_val = current_val >> other_val; break;
            case ARITH_FDIV:   result_val = current_val / other_val; break;
            default: result_val = 0; break;
    }
    
        new_value = PyLong_FromLong(result_val);
    } else {
        /* Opérations flottantes et matricielles */
        double default_vals[] = {0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0};
        double current_val = extract_numeric_value(registry->base.value, default_vals[op_type]);
        double other_val = extract_numeric_value(other, 0.0);
        double result_val;
        
        switch (op_type) {
            case ARITH_ADD: result_val = current_val + other_val; break;
            case ARITH_SUB: result_val = current_val - other_val; break;
            case ARITH_MUL: result_val = current_val * other_val; break;
            case ARITH_DIV: result_val = current_val / other_val; break;
            case ARITH_POW: result_val = pow(current_val, other_val); break;
            case ARITH_MATMUL: result_val = current_val * other_val; break;  /* Simplification : traiter @ comme * */
            default: result_val = 0.0; break;
        }
        
        if (op_type == ARITH_DIV) {
            new_value = PyFloat_FromDouble(result_val);  /* Division toujours en float */
    } else {
            new_value = create_optimal_number(result_val);
        }
    }
    
    if (new_value == NULL) return NULL;
    
    /* Remplacement atomique + trigger */
    Py_XDECREF(registry->base.value);
    registry->base.value = new_value;
    
    pyfasty_trigger_sync_events_with_module(MODULE_REGISTRY);
    
    Py_INCREF(self);
    return self;
}

/* Fonctions d'interface simplifiées (préservation API) */
static PyObject *registry_inplace_add(PyObject *self, PyObject *other) {
    return registry_inplace_operation(self, other, ARITH_ADD);
}

/* NOUVELLE FONCTION : Addition/concaténation avec Registry - Support for string concatenation */
static PyObject *registry_add(PyObject *self, PyObject *other) {
    PyFastyRegistryObject *registry = (PyFastyRegistryObject *)self;
    
    /* Si l'autre opérande est un string, on fait une concaténation de string */
    if (PyUnicode_Check(other)) {
        /* Convertir le registry en string et faire la concaténation */
        PyObject *self_str = registry_str(registry);
        if (self_str == NULL) {
            return NULL;
        }
        
        PyObject *result = PyUnicode_Concat(self_str, other);
        Py_DECREF(self_str);
        return result;
    }
    
    /* Si l'autre opérande est aussi un Registry, on concatène les deux strings */
    if (PyObject_TypeCheck(other, &PyFastyRegistryType)) {
        PyObject *self_str = registry_str(registry);
        if (self_str == NULL) {
            return NULL;
        }
        
        PyObject *other_str = registry_str((PyFastyRegistryObject *)other);
        if (other_str == NULL) {
            Py_DECREF(self_str);
            return NULL;
        }
        
        PyObject *result = PyUnicode_Concat(self_str, other_str);
        Py_DECREF(self_str);
        Py_DECREF(other_str);
        return result;
    }
    
    /* Pour les autres types, retourner NotImplemented pour laisser Python essayer autre chose */
    Py_RETURN_NOTIMPLEMENTED;
}

/* NOUVELLE FONCTION : Addition inversée (RADD) - Support for 'string' + registry */
static PyObject *registry_radd(PyObject *self, PyObject *other) {
    PyFastyRegistryObject *registry = (PyFastyRegistryObject *)self;
    
    /* Si l'autre opérande est un string, on fait une concaténation de string */
    if (PyUnicode_Check(other)) {
        /* Convertir le registry en string et faire la concaténation inversée */
        PyObject *self_str = registry_str(registry);
        if (self_str == NULL) {
            return NULL;
        }
        
        PyObject *result = PyUnicode_Concat(other, self_str);
        Py_DECREF(self_str);
        return result;
    }
    
    /* Pour les autres types, retourner NotImplemented */
    Py_RETURN_NOTIMPLEMENTED;
}

static PyObject *registry_inplace_subtract(PyObject *self, PyObject *other) {
    return registry_inplace_operation(self, other, ARITH_SUB);
    }
    
static PyObject *registry_inplace_multiply(PyObject *self, PyObject *other) {
    return registry_inplace_operation(self, other, ARITH_MUL);
}

static PyObject *registry_inplace_divide(PyObject *self, PyObject *other) {
    return registry_inplace_operation(self, other, ARITH_DIV);
}

static PyObject *registry_inplace_remainder(PyObject *self, PyObject *other) {
    return registry_inplace_operation(self, other, ARITH_MOD);
}

static PyObject *registry_inplace_power(PyObject *self, PyObject *other) {
    return registry_inplace_operation(self, other, ARITH_POW);
}

static PyObject *registry_inplace_and(PyObject *self, PyObject *other) {
    return registry_inplace_operation(self, other, ARITH_AND);
}

static PyObject *registry_inplace_or(PyObject *self, PyObject *other) {
    return registry_inplace_operation(self, other, ARITH_OR);
}

static PyObject *registry_inplace_floor_divide(PyObject *self, PyObject *other) {
    return registry_inplace_operation(self, other, ARITH_FDIV);
}

static PyObject *registry_inplace_xor(PyObject *self, PyObject *other) {
    return registry_inplace_operation(self, other, ARITH_XOR);
}

static PyObject *registry_inplace_lshift(PyObject *self, PyObject *other) {
    return registry_inplace_operation(self, other, ARITH_LSHIFT);
}

static PyObject *registry_inplace_rshift(PyObject *self, PyObject *other) {
    return registry_inplace_operation(self, other, ARITH_RSHIFT);
    }
    
static PyObject *registry_inplace_matrix_multiply(PyObject *self, PyObject *other) {
    return registry_inplace_operation(self, other, ARITH_MATMUL);
}

/* Fonction optimisée pour accéder à un attribut via un chemin */
static PyObject *registry_get_by_path(PyFastyRegistryObject *self, const char *path) {
    return pyfasty_object_get_by_path((PyObject*)self, path, 
                                    &PyFastyRegistryType, 
                                    (PyFastyGetAttrFunc)registry_getattr_recursive);
}

/* Méthode Python get_path */
static PyObject *registry_get_path_method(PyObject *self, PyObject *args) {
    const char *path;
    PyObject *default_value = Py_None;
    
    /* Analyser les arguments */
    if (!PyArg_ParseTuple(args, "s|O:get_path", &path, &default_value))
        return NULL;
    
    /* Appeler la fonction interne d'accès par chemin */
    PyObject *result = registry_get_by_path((PyFastyRegistryObject *)self, path);
    
    /* En cas d'erreur, retourner la valeur par défaut */
    if (!result) {
        PyErr_Clear();
        Py_INCREF(default_value);
        return default_value;
    }
    
    return result;
}

/* Fonction helper générique pour les méthodes d'opérations binaires */
static PyObject *registry_binary_op_method(PyObject *self, PyObject *args, 
                                        PyObject *(*op_func)(PyObject*, PyObject*),
                                        const char *name) {
    PyObject *other;
    
    if (!PyArg_ParseTuple(args, "O", &other))
        return NULL;
    
    /* Déléguer à la fonction spécifique */
    return op_func(self, other);
}

/* Méthode pour l'opération &= (AND) */
static PyObject *registry_bitwise_and_method(PyObject *self, PyObject *args) {
    return registry_binary_op_method(self, args, registry_inplace_and, "bitwise_and");
}

/* Méthode pour l'opération |= (OR) */
static PyObject *registry_bitwise_or_method(PyObject *self, PyObject *args) {
    return registry_binary_op_method(self, args, registry_inplace_or, "bitwise_or");
}

/* Méthode pour l'opération //= (division entière) */
static PyObject *registry_floor_divide_method(PyObject *self, PyObject *args) {
    return registry_binary_op_method(self, args, registry_inplace_floor_divide, "floor_divide");
}

/* Méthode pour l'opération ^= (XOR) */
static PyObject *registry_xor_method(PyObject *self, PyObject *args) {
    return registry_binary_op_method(self, args, registry_inplace_xor, "xor");
}

/* Méthode pour l'opération <<= (LSHIFT) */
static PyObject *registry_lshift_method(PyObject *self, PyObject *args) {
    return registry_binary_op_method(self, args, registry_inplace_lshift, "lshift");
}

/* Méthode pour l'opération >>= (RSHIFT) */
static PyObject *registry_rshift_method(PyObject *self, PyObject *args) {
    return registry_binary_op_method(self, args, registry_inplace_rshift, "rshift");
}

/* Méthode pour l'opération @= (MATRIX_MULTIPLY) */
static PyObject *registry_matrix_multiply_method(PyObject *self, PyObject *args) {
    return registry_binary_op_method(self, args, registry_inplace_matrix_multiply, "matrix_multiply");
}

/* Globals */
static PyObject *g_registry = NULL;

/* Fonction helper générique pour les opérations binaires au niveau du module */
static PyObject *pyfasty_binary_op(PyObject *self, PyObject *args, 
                                PyObject *(*op_func)(PyObject*, PyObject*)) {
    PyObject *registry_obj;
    PyObject *value;
    
    if (!PyArg_ParseTuple(args, "OO", &registry_obj, &value))
        return NULL;
    
    /* Vérifier que le premier argument est un Registry */
    if (!PyObject_TypeCheck(registry_obj, &PyFastyRegistryType)) {
        PyErr_SetString(PyExc_TypeError, "First argument must be a Registry object");
        return NULL;
    }
    
    return op_func(registry_obj, value);
}

/* Bitwise AND function */
static PyObject *pyfasty_bitwise_and(PyObject *self, PyObject *args) {
    return pyfasty_binary_op(self, args, registry_inplace_and);
}

/* Bitwise OR function */
static PyObject *pyfasty_bitwise_or(PyObject *self, PyObject *args) {
    return pyfasty_binary_op(self, args, registry_inplace_or);
}

/* Initialization function */
int PyFasty_Registry_Init(PyObject *module) {
    /* STEP 1: Finalize Registry type before use */
    if (PyType_Ready(&PyFastyRegistryType) < 0) {
        return -1;
    }
    
    /* STEP 2: Add Registry type to module for user access */
    Py_INCREF(&PyFastyRegistryType);
    if (PyModule_AddObject(module, "Registry", (PyObject *)&PyFastyRegistryType) < 0) {
        Py_DECREF(&PyFastyRegistryType);
        return -1;
    }
    
    /* STEP 3: Create THE singleton global registry instance */
    /* This is what users access as: pyfasty.registry.counter */
    g_registry = registry_create(0, NULL);
    if (g_registry == NULL) {
        return -1;
    }
    
    /* STEP 4: Expose global registry to Python as 'registry' */
    if (PyModule_AddObject(module, "registry", g_registry) < 0) {
        Py_DECREF(g_registry);
        return -1;
    }
    
    /* STEP 5: Add module-level utility functions */
    static PyMethodDef methods[] = {
        {"bitwise_and", pyfasty_bitwise_and, METH_VARARGS, "Perform bitwise AND operation"},
        {"bitwise_or", pyfasty_bitwise_or, METH_VARARGS, "Perform bitwise OR operation"},
        {NULL, NULL, 0, NULL}  /* Sentinel */
    };
    
    /* Register utility functions to module */
    for (PyMethodDef *def = methods; def->ml_name != NULL; def++) {
        PyObject *func = PyCFunction_New(def, NULL);
        if (func) {
            if (PyModule_AddObject(module, def->ml_name, func) < 0) {
                Py_DECREF(func);
                return -1;
            }
    } else {
            return -1;
        }
    }
    
    return 0;  /* Success - Registry system ready */
}

/* NOUVELLE FONCTION : Support de __format__ pour conversion automatique en string */
static PyObject *registry_format(PyObject *self, PyObject *format_spec) {
    /* Convertir le registry en string et appliquer le format */
    PyObject *str_repr = registry_str((PyFastyRegistryObject *)self);
    if (str_repr == NULL) {
        return NULL;
    }
    
    /* Si pas de format spécifique, retourner directement la représentation string */
    if (format_spec == NULL || PyUnicode_GetLength(format_spec) == 0) {
        return str_repr;
    }
    
    /* Sinon, appliquer le format à la string */
    PyObject *result = PyUnicode_Format(format_spec, str_repr);
    Py_DECREF(str_repr);
    return result;
}
