#include "../pyfasty.h"

/* Structure pour la config */
typedef struct {
    PyFastyBaseObject base;    /* Structure de base commune */
    PyObject *observers;       /* Liste des observateurs pour les changements (pour event) */
} PyFastyConfigObject;

/* Forward declarations */
static PyObject *config_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
static int config_init(PyFastyConfigObject *self, PyObject *args, PyObject *kwds);
static void config_dealloc(PyFastyConfigObject *self);
static PyObject *config_getattro(PyFastyConfigObject *self, PyObject *name);
static int config_setattro(PyFastyConfigObject *self, PyObject *name, PyObject *value);
static PyObject *config_str(PyFastyConfigObject *self);
static PyObject *config_repr(PyFastyConfigObject *self);
static PyObject *config_get_item(PyObject *self, PyObject *key);
static int config_set_item(PyObject *self, PyObject *key, PyObject *value);
static PyObject *config_get_method(PyObject *self, PyObject *args);
static PyObject *config_get_path_method(PyObject *self, PyObject *args);
static int config_contains(PyObject *self, PyObject *key);
static PyObject *config_float(PyObject *obj);
static PyObject *config_int(PyObject *obj);
static PyObject *config_richcompare(PyObject *self, PyObject *other, int op);

/* Mapping methods */
static PyMappingMethods config_as_mapping = {
    0,                      /* mp_length */
    config_get_item,        /* mp_subscript */
    config_set_item,        /* mp_ass_subscript */
};

/* Number methods */
static PyNumberMethods config_as_number = {
    0,                      /* nb_add */
    0,                      /* nb_subtract */
    0,                      /* nb_multiply */
    0,                      /* nb_remainder */
    0,                      /* nb_divmod */
    0,                      /* nb_power */
    0,                      /* nb_negative */
    0,                      /* nb_positive */
    0,                      /* nb_absolute */
    0,                      /* nb_bool */
    0,                      /* nb_invert */
    0,                      /* nb_lshift */
    0,                      /* nb_rshift */
    0,                      /* nb_and */
    0,                      /* nb_xor */
    0,                      /* nb_or */
    config_int,             /* nb_int */
    0,                      /* nb_reserved */
    config_float,           /* nb_float */
};

/* Methods de l'objet */
static PyMethodDef config_methods[] = {
    {"get", (PyCFunction)config_get_method, METH_VARARGS, "Get config item with default value"},
    {"get_path", (PyCFunction)config_get_path_method, METH_VARARGS, "Get config item with a dot-notated path (e.g. 'a.b.c')"},
    {NULL, NULL, 0, NULL}  /* Sentinel */
};

static PyTypeObject PyFastyConfigType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pyfasty._pyfasty.Config",
    .tp_doc = "Config object for application configuration",
    .tp_basicsize = sizeof(PyFastyConfigObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = config_new,
    .tp_init = (initproc)config_init,
    .tp_dealloc = (destructor)config_dealloc,
    .tp_getattro = (getattrofunc)config_getattro,
    .tp_setattro = (setattrofunc)config_setattro,
    .tp_str = (reprfunc)config_str,
    .tp_repr = (reprfunc)config_repr,
    .tp_as_mapping = &config_as_mapping,
    .tp_as_number = &config_as_number,
    .tp_methods = config_methods,
    .tp_richcompare = config_richcompare,
};

/* Helper function to create a new config */
static PyObject *config_create(int depth, PyObject *value) {
    PyFastyConfigObject *config = (PyFastyConfigObject *)pyfasty_base_create(
        &PyFastyConfigType, PYFASTY_CONFIG_TYPE, depth, value);
    
    if (config == NULL) {
        return NULL;
    }
    
    /* Ajouter la liste des observateurs */
    config->observers = PyList_New(0);
    if (config->observers == NULL) {
        Py_DECREF(config);
        return NULL;
    }
    
    return (PyObject *)config;
}

/* Optimized fast-path getattr function */
static PyObject *config_getattr_recursive(PyObject *config_obj, PyObject *name) {
    PyFastyConfigObject *self = (PyFastyConfigObject *)config_obj;
    
    /* Fast path: special names with dunder */
    const char *name_str = PyUnicode_AsUTF8(name);
    if (name_str[0] == '_') {
        return PyObject_GenericGetAttr(config_obj, name);
    }
    
    /* CORRECTION #2 : Mode READ-ONLY pendant évaluation de conditions */
    extern int g_in_condition_evaluation;
    
    /* NOUVEAU : Vérifier d'abord dans data */
    PyObject *result = PyDict_GetItem(self->base.data, name);
    if (result != NULL) {
        /* Si c'est un objet Config wrappé, on le retourne TOUJOURS pour permettre les sous-attributs */
        if (PyObject_TypeCheck(result, &PyFastyConfigType)) {
            /* CORRECTION : Toujours retourner l'objet Config pour permettre .port, .host, etc. */
            Py_INCREF(result);
            return result;
        } else {
            /* Si c'est déjà une valeur primitive stockée directement, la retourner */
            Py_INCREF(result);
            return result;
        }
    }
    
    /* SOLUTION GÉNÉRALISTE : En mode READ-ONLY, NE PAS auto-créer d'attributs inexistants */
    /* Cette approche est complètement dynamique et ne fait aucun hardcoding de noms */
    if (g_in_condition_evaluation) {
        /* En mode évaluation d'événements, retourner None pour TOUS les attributs inexistants */
        /* Cela empêche l'auto-création et les faux positifs, peu importe le nom */
        Py_RETURN_NONE;
    }
    
    /* En mode normal : permettre l'auto-création pour tous les attributs (comportement standard) */
    return pyfasty_base_getattr_recursive(config_obj, name, &PyFastyConfigType, PYFASTY_CONFIG_TYPE);
}

/* Forward declaration pour les fonctions d'événement */
extern PyObject *PyObject_CallMethod(PyObject *obj, const char *name, const char *format, ...);

/* Fast-path function for modifying attributes */
static int config_setattr_recursive(PyObject *config_obj, PyObject *name, PyObject *value) {
    PyFastyConfigObject *self = (PyFastyConfigObject *)config_obj;
    
    /* Clear cache for this name */
    if (PyDict_Contains(self->base.cache, name)) {
        PyDict_DelItem(self->base.cache, name);
    }
    
    /* Handle direct value overrides for dictionary-like objects */
    if (value != NULL && strcmp(PyUnicode_AsUTF8(name), PYFASTY_CONFIG_PRIVATE_VALUE_ATTR) == 0) {
        Py_XDECREF(self->base.value);
        Py_INCREF(value);
        self->base.value = value;
        return 0;
    }
    
    /* Fast path: handle dictionaries specially for nested config objects */
    PyObject *old_value = PyDict_GetItem(self->base.data, name);
    
    /* If value is a dictionary, special handling */
    if (value != NULL && PyDict_Check(value)) {
        PyObject *new_config = config_create(self->base.depth + 1, value);
        if (new_config == NULL) {
            return -1;
        }
        
        /* Set the nested config object */
        int result = PyDict_SetItem(self->base.data, name, new_config);
        Py_DECREF(new_config);
        
        /* Trigger events when dictionaries are replaced or modified */
        pyfasty_trigger_sync_events_with_module(MODULE_CONFIG);
        
        return result;
    } else if (value != NULL) {
        /* For normal values, just set directly */
        int result = PyDict_SetItem(self->base.data, name, value);
        
        /* Trigger events for any attribute modification */
        pyfasty_trigger_sync_events_with_module(MODULE_CONFIG);
        
        return result;
    } else {
        /* Handle deletion case as needed */
        /* (Omit for brevity) */
        return -1;
    }
}

/* Deallocation function */
static void config_dealloc(PyFastyConfigObject *self) {
    /* Libérer les observateurs */
    Py_XDECREF(self->observers);
    
    /* Retourner les dictionnaires au pool et libérer la valeur */
    pyfasty_dict_pool_return(self->base.data);
    pyfasty_dict_pool_return(self->base.cache);
    Py_XDECREF(self->base.value);
    
    /* Appel à PyObject_Del avec le bon type */
    PyObject_Del(self);
}

/* New function */
static PyObject *config_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PyObject *value = NULL;
    
    if (PyTuple_Size(args) > 0) {
        value = PyTuple_GetItem(args, 0);
    }
    
    PyFastyConfigObject *self = (PyFastyConfigObject *)pyfasty_base_create(
        type, PYFASTY_CONFIG_TYPE, 0, value);
    
    if (self != NULL) {
        /* Initialize observers */
        self->observers = PyList_New(0);
        if (self->observers == NULL) {
            Py_DECREF(self);
            return NULL;
        }
    }
    
    return (PyObject *)self;
}

/* Init function */
static int config_init(PyFastyConfigObject *self, PyObject *args, PyObject *kwds) {
    PyObject *value = NULL;
    
    if (!PyArg_ParseTuple(args, "|O", &value)) {
        return -1;
    }
    
    /* L'initialisation des autres champs est gérée par pyfasty_base_create */
    self->observers = PyList_New(0);
    if (self->observers == NULL) {
        return -1;
    }
    
    return 0;
}

/* Getattro function */
static PyObject *config_getattro(PyFastyConfigObject *self, PyObject *name) {
    /* Tracer l'accès au module config pour la détection de dépendances */
    pyfasty_trace_module_access(MODULE_CONFIG);
    
    /* Fast path: utiliser la fonction récursive optimisée */
    PyObject *result = config_getattr_recursive((PyObject *)self, name);
    return result;
}

/* Setattro function */
static int config_setattro(PyFastyConfigObject *self, PyObject *name, PyObject *value) {
    /* Fast path: noms spéciaux avec dunder */
    const char *name_str = PyUnicode_AsUTF8(name);
    if (name_str[0] == '_') {
        return PyObject_GenericSetAttr((PyObject *)self, name, value);
    }

    /* Cas spécial pour "value" */
    if (strcmp(name_str, PYFASTY_CONFIG_PUBLIC_VALUE_ATTR) == 0) {
        Py_XDECREF(self->base.value);
        Py_INCREF(value);
        self->base.value = value;
        
        /* Déclencher les événements après modification de la valeur directe */
        pyfasty_trigger_sync_events_with_module(MODULE_CONFIG);
        return 0;
    }
    
    /* Utiliser config_setattr_recursive pour le reste */
    int result = pyfasty_base_setattr_recursive((PyObject *)self, name, value, 
                                             &PyFastyConfigType, PYFASTY_CONFIG_TYPE);
    
    /* Déclencher les événements si la modification est réussie */
    if (result >= 0) {
        /* Déclencher uniquement les événements liés à la config */
        pyfasty_trigger_sync_events_with_module(MODULE_CONFIG);
    }
    
    return result;
}

/* String representation */
static PyObject *config_str(PyFastyConfigObject *self) {
    /* If we have a direct value, use that for string representation */
    if (self->base.value != Py_None) {
        return PyObject_Str(self->base.value);
    }
    
    /* Otherwise use the data dictionary */
    return PyObject_Str(self->base.data);
}

/* String representation */
static PyObject *config_repr(PyFastyConfigObject *self) {
    /* If we have a direct value, use that for string representation */
    if (self->base.value != Py_None) {
        return PyObject_Repr(self->base.value);
    }
    
    /* Otherwise use the data dictionary */
    return PyObject_Repr(self->base.data);
}

/* FONCTION GÉNÉRIQUE POUR CONVERSION NUMÉRIQUE - Évite la duplication */
static PyObject *config_convert_to_number(PyObject *obj, PyObject *(*converter)(PyObject*)) {
    PyFastyConfigObject *self = (PyFastyConfigObject *)obj;
    
    /* If we have a direct value, convert it */
    if (self->base.value != Py_None) {
        return converter(self->base.value);
    }
    
    /* Otherwise, try to create a number from the string representation */
    PyObject *str = config_str(self);
    if (str == NULL) {
        return NULL;
    }
    
    PyObject *result = converter(str);
    Py_DECREF(str);
    return result;
}

/* Convert to a float - Version simplifiée */
static PyObject *config_float(PyObject *obj) {
    return config_convert_to_number(obj, PyNumber_Float);
}

/* Convert to an integer - Version simplifiée */
static PyObject *config_int(PyObject *obj) {
    return config_convert_to_number(obj, PyNumber_Long);
}

/* FONCTION HELPER POUR VÉRIFIER LES TYPES PRIMITIFS */
static int config_is_primitive_value(PyObject *value) {
    return PyLong_Check(value) || PyFloat_Check(value) || 
           PyBool_Check(value) || PyUnicode_Check(value);
}

/* FONCTION DE COMPARAISON RICHE SIMPLIFIÉE */
static PyObject *config_richcompare(PyObject *self, PyObject *other, int op) {
    PyFastyConfigObject *config = (PyFastyConfigObject *)self;
    
    /* Priorité 1: Valeur directe */
    if (config->base.value != Py_None) {
        return PyObject_RichCompare(config->base.value, other, op);
    }
    
    /* Priorité 2: Valeur unique primitive dans data */
    if (PyDict_Size(config->base.data) == 1) {
        PyObject *key, *value;
        Py_ssize_t pos = 0;
        if (PyDict_Next(config->base.data, &pos, &key, &value)) {
            if (config_is_primitive_value(value)) {
                return PyObject_RichCompare(value, other, op);
            }
            if (PyObject_TypeCheck(value, &PyFastyConfigType)) {
                return config_richcompare(value, other, op);
            }
        }
    }
    
    /* Fallback : comparaison d'identité */
    if (op == Py_EQ) {
        return PyBool_FromLong(self == other);
    } else if (op == Py_NE) {
        return PyBool_FromLong(self != other);
    }
    
    Py_RETURN_NOTIMPLEMENTED;
}

/* Fast implementation of __getitem__ */
static PyObject *config_get_item(PyObject *self, PyObject *key) {
    return pyfasty_common_getitem(self, key, &PyFastyConfigType, PYFASTY_CONFIG_TYPE, config_create);
}

/* Implementation of __setitem__ */
static int config_set_item(PyObject *self, PyObject *key, PyObject *value) {
    return pyfasty_common_setitem(self, key, value, &PyFastyConfigType, PYFASTY_CONFIG_TYPE, config_create);
}

/* Implementation of get method */
static PyObject *config_get_method(PyObject *self, PyObject *args) {
    return pyfasty_common_getmethod(self, args, PYFASTY_CONFIG_TYPE);
}

/* Nouvelle fonction: accès optimisé par chemin composé */
static PyObject *config_get_by_path(PyFastyConfigObject *self, const char *path) {
    return pyfasty_object_get_by_path((PyObject*)self, path, 
                                    &PyFastyConfigType, 
                                    (PyFastyGetAttrFunc)config_getattr_recursive);
}

/* Méthode Python get_path */
static PyObject *config_get_path_method(PyObject *self, PyObject *args) {
    const char *path;
    PyObject *default_value = Py_None;
    
    /* Analyser les arguments */
    if (!PyArg_ParseTuple(args, "s|O:get_path", &path, &default_value))
        return NULL;
    
    /* Appeler la fonction interne d'accès par chemin */
    PyObject *result = config_get_by_path((PyFastyConfigObject *)self, path);
    
    /* En cas d'erreur, retourner la valeur par défaut */
    if (!result) {
        PyErr_Clear();
        Py_INCREF(default_value);
        return default_value;
    }
    
    return result;
}

/* Implementation of contains check */
static int config_contains(PyObject *self, PyObject *key) {
    return pyfasty_common_contains(self, key);
}

/* Nouvelle fonction pour mettre à jour la config depuis un dictionnaire */
static int config_update_from_dict(PyFastyConfigObject *self, PyObject *dict) {
    if (!PyDict_Check(dict)) {
        PyErr_SetString(PyExc_TypeError, "Configuration must be a dictionary");
        return -1;
    }
    
    /* Parcourir le dictionnaire et mettre à jour les valeurs */
    PyObject *key, *val;
    Py_ssize_t pos = 0;
    
    while (PyDict_Next(dict, &pos, &key, &val)) {
        /* Vérifier que la clé est une chaîne */
        if (!PyUnicode_Check(key)) {
            continue;  /* Ignorer les clés non-string */
        }
        
        /* Mettre à jour l'attribut */
        if (config_setattro((PyObject *)self, key, val) < 0) {
            return -1;
        }
    }
    
    return 0;
}

/* Fonction pour gérer le remplacement global de la config */
static int config_set_global(PyObject *module, PyObject *dict) {
    /* Récupérer l'objet config global */
    PyObject *global_config = PyObject_GetAttrString(module, "config");
    if (global_config == NULL) {
        return -1;
    }
    
    int result;
    if (PyDict_Check(dict)) {
        /* Si c'est un dictionnaire, mettre à jour la config */
        result = config_update_from_dict((PyFastyConfigObject *)global_config, dict);
    } else {
        /* Sinon remplacer complètement (comportement standard) */
        result = PyObject_SetAttrString(module, "config", dict);
    }
    
    Py_DECREF(global_config);
    return result;
}

/* Globals */
static PyObject *g_config = NULL;

/* Initialization function */
int PyFasty_Config_Init(PyObject *module) {
    if (PyType_Ready(&PyFastyConfigType) < 0) {
        return -1;
    }
    
    /* Ajouter le type au module */
    Py_INCREF(&PyFastyConfigType);
    if (PyModule_AddObject(module, "Config", (PyObject *)&PyFastyConfigType) < 0) {
        Py_DECREF(&PyFastyConfigType);
        return -1;
    }
    
    /* Créer l'instance singleton et l'ajouter au module */
    g_config = config_create(0, NULL);
    if (g_config == NULL) {
        return -1;
    }
    
    if (PyModule_AddObject(module, "config", g_config) < 0) {
        Py_DECREF(g_config);
        g_config = NULL;
        return -1;
    }
    
    /* Exporter la fonction de mise à jour globale */
    PyFasty_SetGlobalConfig = config_set_global;
    
    return 0;
}
