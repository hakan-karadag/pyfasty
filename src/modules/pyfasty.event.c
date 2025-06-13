#include "../pyfasty.h"

/* =================== VARIABLES GLOBALES =================== */

static PyObject *g_event_handlers = NULL;
int g_events_enabled = 0;
static int g_is_evaluating = 0;
static long g_current_timestamp = 0;

/* État du module actuel pour le déclenchement contextualisé */
ModuleType g_current_module = MODULE_UNKNOWN;

/* Protection contre les récursions */
static int g_in_callback_execution = 0;

/* Variable exportée pour les autres modules */
int g_in_condition_evaluation = 0;

/* NOUVEAU : Protection contre les boucles infinies */
static int g_evaluation_depth = 0;
static const int MAX_EVALUATION_DEPTH = 10;

/* Cache des états des handlers */
#define MAX_HANDLERS 256
static struct {
    PyObject *handler_tuple;
    int last_state;                    /* -1: unknown, 0: false, 1: true */
    int is_impossible;                 /* Condition impossible à évaluer */
    long last_evaluation_triggered;   /* Timestamp du dernier déclenchement */
    int is_direct_module;             /* Événement module direct */
} g_handler_states[MAX_HANDLERS];

/* =================== DÉCLARATIONS =================== */

/* Forward declaration of EventDecoratorType */
PyTypeObject EventDecoratorType;

/* Fonctions de cache et états */
static int get_handler_index(PyObject *handler);
static int get_handler_last_state(PyObject *handler);
static void set_handler_last_state(PyObject *handler, int state);
static int has_triggered_this_evaluation(PyObject *handler);
static void mark_triggered_this_evaluation(PyObject *handler);
static void mark_handler_as_direct_module(PyObject *handler, int is_direct);

/* Fonctions d'analyse des conditions */
static int is_handler_impossible(PyObject *handler);
static int is_simple_module_condition(PyObject *condition);
static ModuleType detect_condition_module_type(PyObject *condition);
static ModuleType detect_direct_module_condition(PyObject *condition);
static int evaluate_condition(PyObject *condition);

/* Fonction d'exécution */
static void execute_callback(PyObject *callback);

/* Fonctions publiques */
PyObject *event_clear_handlers(PyObject *self, PyObject *args);

/* =================== GESTION DU CACHE =================== */

/* Obtenir l'index d'un handler dans le cache */
static int get_handler_index(PyObject *handler) {
    for (int i = 0; i < MAX_HANDLERS; i++) {
        if (g_handler_states[i].handler_tuple == handler) {
            return i;
        }
        if (g_handler_states[i].handler_tuple == NULL) {
            /* Slot libre trouvé - initialiser */
            g_handler_states[i].handler_tuple = handler;
            g_handler_states[i].last_state = -1;
            g_handler_states[i].is_impossible = 0;
            g_handler_states[i].last_evaluation_triggered = 0;
            g_handler_states[i].is_direct_module = 0;
            return i;
        }
    }
    return -1; /* Cache plein */
}

/* Vérifier si un handler s'est déjà déclenché dans cette évaluation */
static int has_triggered_this_evaluation(PyObject *handler) {
    int index = get_handler_index(handler);
    if (index >= 0) {
        return (g_handler_states[index].last_evaluation_triggered == g_current_timestamp);
    }
    return 0;
}

/* Marquer un handler comme déclenché dans cette évaluation */
static void mark_triggered_this_evaluation(PyObject *handler) {
    int index = get_handler_index(handler);
    if (index >= 0) {
        g_handler_states[index].last_evaluation_triggered = g_current_timestamp;
    }
}

/* Obtenir l'état précédent d'un handler */
static int get_handler_last_state(PyObject *handler) {
    int index = get_handler_index(handler);
    return (index >= 0) ? g_handler_states[index].last_state : -1;
}

/* Définir l'état d'un handler */
static void set_handler_last_state(PyObject *handler, int state) {
    int index = get_handler_index(handler);
    if (index >= 0) {
        g_handler_states[index].last_state = state;
    }
}

/* Marquer un handler comme module direct */
static void mark_handler_as_direct_module(PyObject *handler, int is_direct) {
    int index = get_handler_index(handler);
    if (index >= 0) {
        g_handler_states[index].is_direct_module = is_direct;
    }
}

/* =================== ANALYSE DES CONDITIONS =================== */

/* Vérifier si un handler est impossible à évaluer */
static int is_handler_impossible(PyObject *handler) {
    PyObject *condition = PyTuple_GetItem(handler, 0);
    
    /* Vérifier dans le cache */
    int index = get_handler_index(handler);
    if (index >= 0 && g_handler_states[index].is_impossible != -1) {
        return g_handler_states[index].is_impossible;
    }
    
    int is_impossible = 0;
    
    /* Mode read-only pour éviter les effets de bord */
    int old_in_condition = g_in_condition_evaluation;
    g_in_condition_evaluation = 1;
    
    PyObject *result = PyObject_CallFunction(condition, NULL);
    g_in_condition_evaluation = old_in_condition;
    
    if (result == NULL) {
        /* Exception = condition impossible */
        PyErr_Clear();
        is_impossible = 1;
    } else {
        /* Vérifier si c'est un objet Config vide */
        PyObject *type_name = PyObject_GetAttrString((PyObject*)Py_TYPE(result), "__name__");
        if (type_name && PyUnicode_Check(type_name)) {
            const char *name = PyUnicode_AsUTF8(type_name);
            if (name && strcmp(name, "Config") == 0) {
                PyObject *str_repr = PyObject_Str(result);
                if (str_repr && PyUnicode_Check(str_repr)) {
                    const char *repr_str = PyUnicode_AsUTF8(str_repr);
                    if (repr_str && (strcmp(repr_str, "{}") == 0 || strlen(repr_str) <= 2)) {
                        is_impossible = 1;
                    }
                }
                Py_XDECREF(str_repr);
            }
        }
        Py_XDECREF(type_name);
        
        /* Détecter les conditions avec "and False" */
        if (!is_impossible && PyCallable_Check(condition) && PyFunction_Check(condition)) {
            PyObject *code_obj = PyFunction_GetCode(condition);
            if (code_obj) {
                PyObject *co_consts = PyObject_GetAttrString(code_obj, "co_consts");
                if (co_consts && PyTuple_Check(co_consts)) {
                    Py_ssize_t consts_len = PyTuple_Size(co_consts);
                    for (Py_ssize_t j = 0; j < consts_len; j++) {
                        PyObject *const_val = PyTuple_GetItem(co_consts, j);
                        if (const_val == Py_False) {
                            if (result == Py_False || PyObject_IsTrue(result) == 0) {
                                is_impossible = 1;
                            }
                            break;
                        }
                    }
                    Py_DECREF(co_consts);
                }
            }
        }
        
        Py_DECREF(result);
    }
    
    /* Sauvegarder dans le cache */
    if (index >= 0) {
        g_handler_states[index].is_impossible = is_impossible;
    }
    
    return is_impossible;
}

/* Détecter le type de module d'une condition */
static ModuleType detect_condition_module_type(PyObject *condition) {
    if (!PyCallable_Check(condition) || !PyFunction_Check(condition)) {
        return MODULE_UNKNOWN;
    }
    
    PyObject *code_obj = PyFunction_GetCode(condition);
    if (!code_obj) {
        return MODULE_UNKNOWN;
    }
    
    PyObject *co_names = PyObject_GetAttrString(code_obj, "co_names");
    if (!co_names || !PyTuple_Check(co_names)) {
        Py_XDECREF(co_names);
        return MODULE_UNKNOWN;
    }
    
    /* Analyser les noms pour détecter les modules */
    Py_ssize_t names_len = PyTuple_Size(co_names);
    int has_config = 0;
    int has_registry = 0;
    int has_pyfasty = 0;
    
    /* Détecter pyfasty d'abord */
    for (Py_ssize_t i = 0; i < names_len; i++) {
        PyObject *name = PyTuple_GetItem(co_names, i);
        if (name && PyUnicode_Check(name)) {
            const char *name_str = PyUnicode_AsUTF8(name);
            if (name_str && strcmp(name_str, "pyfasty") == 0) {
                has_pyfasty = 1;
                break;
            }
        }
    }
    
    /* Si pyfasty présent, chercher config et registry */
    if (has_pyfasty) {
        for (Py_ssize_t i = 0; i < names_len; i++) {
            PyObject *name = PyTuple_GetItem(co_names, i);
            if (name && PyUnicode_Check(name)) {
                const char *name_str = PyUnicode_AsUTF8(name);
                if (name_str) {
                    if (strcmp(name_str, "config") == 0) {
                        has_config = 1;
                    } else if (strcmp(name_str, "registry") == 0) {
                        has_registry = 1;
                    }
                }
            }
        }
    }
    
    /* Déterminer le type de module */
    ModuleType detected_module = MODULE_UNKNOWN;
    if (has_config && has_registry) {
        detected_module = MODULE_ALL; /* Condition mixte */
    } else if (has_config) {
        detected_module = MODULE_CONFIG;
    } else if (has_registry) {
        detected_module = MODULE_REGISTRY;
    }
    
    Py_DECREF(co_names);
    return detected_module;
}

/* Détecter les conditions simples (accès direct sans comparaison) */
static int is_simple_module_condition(PyObject *condition) {
    if (!PyCallable_Check(condition) || !PyFunction_Check(condition)) {
        return 0;
    }
    
    PyObject *code_obj = PyFunction_GetCode(condition);
    if (!code_obj) {
        return 0;
    }
    
    PyObject *co_names = PyObject_GetAttrString(code_obj, "co_names");
    if (!co_names || !PyTuple_Check(co_names)) {
        Py_XDECREF(co_names);
        return 0;
    }
    
    Py_ssize_t names_len = PyTuple_Size(co_names);
    int is_simple = 0;
    
    /* Candidat condition simple : 3 noms avec pyfasty et config/registry */
    if (names_len == 3) {
        PyObject *name1 = PyTuple_GetItem(co_names, 0);
        PyObject *name2 = PyTuple_GetItem(co_names, 1);
        
        if (name1 && PyUnicode_Check(name1) && name2 && PyUnicode_Check(name2)) {
            const char *name1_str = PyUnicode_AsUTF8(name1);
            const char *name2_str = PyUnicode_AsUTF8(name2);
            
            if (name1_str && name2_str && strcmp(name1_str, "pyfasty") == 0) {
                if (strcmp(name2_str, "config") == 0 || strcmp(name2_str, "registry") == 0) {
                    /* Vérifier qu'il n'y a pas d'opérateurs de comparaison */
                    PyObject *co_code = PyObject_GetAttrString(code_obj, "co_code");
                    if (co_code && PyBytes_Check(co_code)) {
                        const char *bytecode = PyBytes_AsString(co_code);
                        Py_ssize_t code_len = PyBytes_Size(co_code);
                        
                        int has_comparison = 0;
                        for (Py_ssize_t i = 0; i < code_len; i++) {
                            unsigned char opcode = (unsigned char)bytecode[i];
                            /* Opcodes de comparaison : COMPARE_OP (107), CONTAINS_OP (118), IS_OP (117) */
                            if (opcode == 107 || opcode == 118 || opcode == 117) {
                                has_comparison = 1;
                                break;
                            }
                        }
                        
                        /* Condition simple = aucune comparaison */
                        if (!has_comparison) {
                            is_simple = 1;
                        }
                    }
                    Py_XDECREF(co_code);
                }
            }
        }
    }
    
    /* Vérifier que la condition simple n'accède pas à une propriété inexistante */
    if (is_simple) {
        PyObject *result = PyObject_CallFunction(condition, NULL);
        if (result != NULL) {
            PyObject *type_name = PyObject_GetAttrString((PyObject*)Py_TYPE(result), "__name__");
            if (type_name && PyUnicode_Check(type_name)) {
                const char *name = PyUnicode_AsUTF8(type_name);
                if (name && strcmp(name, "Config") == 0) {
                    PyObject *str_repr = PyObject_Str(result);
                    if (str_repr && PyUnicode_Check(str_repr)) {
                        const char *repr_str = PyUnicode_AsUTF8(str_repr);
                        if (repr_str && (strcmp(repr_str, "{}") == 0 || strlen(repr_str) <= 2)) {
                            is_simple = 0; /* Objet Config vide = pas simple */
                        }
                    }
                    Py_XDECREF(str_repr);
                }
            }
            Py_XDECREF(type_name);
            Py_DECREF(result);
        } else {
            PyErr_Clear();
            is_simple = 0; /* Exception = pas simple */
        }
    }
    
    Py_DECREF(co_names);
    return is_simple;
}

/* Détecter les modules directs (lambda: pyfasty.module) */
static ModuleType detect_direct_module_condition(PyObject *condition) {
    if (!PyCallable_Check(condition) || !PyFunction_Check(condition)) {
        return MODULE_UNKNOWN;
    }
    
    PyObject *code_obj = PyFunction_GetCode(condition);
    if (!code_obj) {
        return MODULE_UNKNOWN;
    }
    
    PyObject *co_names = PyObject_GetAttrString(code_obj, "co_names");
    if (!co_names || !PyTuple_Check(co_names)) {
        Py_XDECREF(co_names);
        return MODULE_UNKNOWN;
    }
    
    /* Accès direct = exactement 2 noms : pyfasty.module */
    Py_ssize_t names_len = PyTuple_Size(co_names);
    ModuleType detected_module = MODULE_UNKNOWN;
    
    if (names_len == 2) {
        PyObject *name1 = PyTuple_GetItem(co_names, 0);
        PyObject *name2 = PyTuple_GetItem(co_names, 1);
        
        if (name1 && PyUnicode_Check(name1) && name2 && PyUnicode_Check(name2)) {
            const char *name1_str = PyUnicode_AsUTF8(name1);
            const char *name2_str = PyUnicode_AsUTF8(name2);
            
            if (name1_str && name2_str && strcmp(name1_str, "pyfasty") == 0) {
                if (strcmp(name2_str, "config") == 0) {
                    detected_module = MODULE_CONFIG;
                } else if (strcmp(name2_str, "sync_executor") == 0) {
                    detected_module = MODULE_EXECUTOR_SYNC;
                } else if (strcmp(name2_str, "async_executor") == 0) {
                    detected_module = MODULE_EXECUTOR_ASYNC;
                } else if (strcmp(name2_str, "console") == 0) {
                    detected_module = MODULE_CONSOLE;
                } else if (strcmp(name2_str, "registry") == 0) {
                    detected_module = MODULE_REGISTRY;
                }
            }
        }
    }
    
    Py_DECREF(co_names);
    return detected_module;
}

/* Détecter les conditions avec constantes toujours vraies (déclenchements multiples) */
static int has_always_true_constants(PyObject *condition) {
    /* CORRECTION CRITIQUE : Cette fonction était défectueuse !
     * 
     * Une condition comme :
     * lambda: pyfasty.registry.event_test_5_sync == "8458DD-NCHDDD-11ADD2" and 8770 == 8770 and True
     * 
     * N'EST PAS "toujours vraie" car elle dépend de la valeur de event_test_5_sync !
     * 
     * Le fait qu'elle contienne "8770 == 8770" et "True" ne la rend pas toujours vraie.
     * 
     * SOLUTION : Retourner toujours 0 (false) pour forcer le comportement normal
     * de déclenchement sur changement False→True uniquement.
     */
    return 0;  /* Aucune condition n'est considérée comme "toujours vraie" */
}

/* Évaluer une condition */
static int evaluate_condition(PyObject *condition) {
    if (condition == Py_None) {
        return 1;
    }
    
    if (!PyCallable_Check(condition)) {
        return 0;
    }
    
    PyObject *result = PyObject_CallFunction(condition, NULL);
    if (result != NULL) {
        int is_true = PyObject_IsTrue(result);
        
        /* Vérifier si c'est un objet Config vide */
        if (is_true == 1) {
            PyObject *type_name = PyObject_GetAttrString((PyObject*)Py_TYPE(result), "__name__");
            if (type_name && PyUnicode_Check(type_name)) {
                const char *name = PyUnicode_AsUTF8(type_name);
                if (name && strcmp(name, "Config") == 0) {
                    PyObject *str_repr = PyObject_Str(result);
                    if (str_repr && PyUnicode_Check(str_repr)) {
                        const char *repr_str = PyUnicode_AsUTF8(str_repr);
                        if (repr_str && (strcmp(repr_str, "{}") == 0 || strlen(repr_str) <= 2)) {
                            is_true = 0; /* Objet Config vide = false */
                        }
                    }
                    Py_XDECREF(str_repr);
                }
            }
            Py_XDECREF(type_name);
        }
        
        /* Objets PyFasty non-vides sont toujours true */
        if (is_true == 0 && result != Py_None) {
            PyObject *type_name = PyObject_GetAttrString((PyObject*)Py_TYPE(result), "__name__");
            if (type_name && PyUnicode_Check(type_name)) {
                const char *name = PyUnicode_AsUTF8(type_name);
                if (name && (strstr(name, "ExecutorProxy") || strstr(name, "Console") || 
                            strstr(name, "Registry"))) {
                    is_true = 1;
                }
            }
            Py_XDECREF(type_name);
        }
        
        Py_DECREF(result);
        return is_true;
    } else {
        PyErr_Clear();
        return 0;
    }
}

/* =================== EXÉCUTION =================== */

/* Exécuter un callback */
static void execute_callback(PyObject *callback) {
    g_in_callback_execution = 1;
    
    PyObject *result = PyObject_CallFunction(callback, NULL);
    if (result == NULL) {
        PyErr_Clear();
    } else {
        Py_DECREF(result);
    }
    
    g_in_callback_execution = 0;
}

/* Fonction simple pour tracer l'accès aux modules */
void pyfasty_trace_module_access(ModuleType module) {
    /* Version simplifiée - juste pour compatibilité */
}

/* =================== FONCTION PRINCIPALE =================== */

/* Fonction principale d'évaluation des événements */
PyObject *event_evaluate_all(PyObject *self, PyObject *args) {
    /* PROTECTION ANTI-BOUCLE INFINIE */
    if (g_is_evaluating || !g_events_enabled || !g_event_handlers) {
        return Py_BuildValue("");
    }
    
    /* Éviter les évaluations trop profondes */
    if (g_evaluation_depth >= MAX_EVALUATION_DEPTH) {
        PyErr_SetString(PyExc_RecursionError, "Maximum event evaluation depth reached - infinite loop detected");
        return NULL;
    }
    
    g_is_evaluating = 1;
    g_evaluation_depth++;
    g_current_timestamp++; /* Nouvel timestamp pour chaque évaluation */
    
    Py_ssize_t len = PyList_Size(g_event_handlers);
    
    for (Py_ssize_t i = 0; i < len; i++) {
        PyObject *handler = PyList_GetItem(g_event_handlers, i);
        if (!handler || !PyTuple_Check(handler) || PyTuple_Size(handler) != 2) {
            continue;
        }
        
        PyObject *condition = PyTuple_GetItem(handler, 0);
        PyObject *callback = PyTuple_GetItem(handler, 1);
        
        /* Protection contre les déclenchements multiples dans la même évaluation */
        if (has_triggered_this_evaluation(handler)) {
            continue;
        }
        
        /* RÈGLE 1: Modules directs = déclenchements multiples */
        ModuleType direct_module = detect_direct_module_condition(condition);
        if (direct_module != MODULE_UNKNOWN) {
            mark_handler_as_direct_module(handler, 1);
            
            if (g_current_module == direct_module) {
                execute_callback(callback);
                mark_triggered_this_evaluation(handler);
            }
            continue;
        }
        
        /* RÈGLE 2: Conditions simples = déclenchement sur changement False→True */
        if (is_simple_module_condition(condition)) {
            ModuleType condition_module = detect_condition_module_type(condition);
            
            if (g_current_module == condition_module) {
                int current_state = evaluate_condition(condition) ? 1 : 0;
                int last_state = get_handler_last_state(handler);
                
                /* Déclenchement uniquement sur changement False→True */
                if (current_state == 1 && last_state != 1) {
                    execute_callback(callback);
                    mark_triggered_this_evaluation(handler);
                }
                
                set_handler_last_state(handler, current_state);
            }
            continue;
        }
        
        /* RÈGLE 3: Conditions impossibles = jamais */
        if (is_handler_impossible(handler)) {
            continue;
        }
        
        /* RÈGLE 4: Conditions avec comparaison */
        ModuleType condition_module = detect_condition_module_type(condition);
        
        int should_evaluate = 0;
        if (condition_module == MODULE_ALL) {
            /* Conditions mixtes: se déclenchent avec config ou registry */
            should_evaluate = (g_current_module == MODULE_CONFIG || g_current_module == MODULE_REGISTRY);
        } else {
            /* Conditions normales: se déclenchent avec leur module spécifique */
            should_evaluate = (g_current_module == condition_module);
        }
        
        if (should_evaluate) {
            int current_state = evaluate_condition(condition) ? 1 : 0;
            int last_state = get_handler_last_state(handler);
            
            if (current_state == 1) {
                /* Détecter les conditions avec constantes toujours vraies pour Test 5 */
                int has_always_true = has_always_true_constants(condition);
                
                if (has_always_true) {
                    /* Conditions toujours vraies: déclenchement à chaque évaluation vraie */
                    execute_callback(callback);
                    /* NE PAS marquer comme déclenché pour permettre déclenchements multiples */
                } else {
                    /* Conditions normales : déclenchement unique False→True */
                    if (last_state != 1 && !has_triggered_this_evaluation(handler)) {
                        execute_callback(callback);
                        mark_triggered_this_evaluation(handler);
                    }
                    /* Sauvegarder l'état pour les conditions normales */
                    set_handler_last_state(handler, current_state);
                }
            } else {
                /* Condition false - sauvegarder l'état */
                set_handler_last_state(handler, current_state);
            }
        }
    }
    
    g_is_evaluating = 0;
    g_evaluation_depth--;  /* DÉCRÉMENTER le compteur de profondeur */
    Py_RETURN_NONE;
}

/* =================== FONCTIONS SPÉCIALISÉES =================== */

/* Fonction spécialisée pour les événements EXECUTOR_SYNC (compatibilité) */
PyObject *event_sync_evaluate_executor_only(PyObject *self, PyObject *args) {
    /* Utiliser la même logique que evaluate_all pour la simplicité */
    return event_evaluate_all(self, args);
}

/* =================== DÉCORATEUR =================== */

/* Fonction __call__ du décorateur */
PyObject *event_decorator_call(EventDecoratorObject *self, PyObject *args, PyObject *kwds) {
    PyObject *func;
    
    if (!PyArg_ParseTuple(args, "O:__call__", &func))
        return NULL;
    
    if (!g_events_enabled || !g_event_handlers) {
        Py_INCREF(func);
        return func;
    }
    
    /* Créer un tuple (condition, callback) */
    PyObject *handler_tuple = Py_BuildValue("(OO)", self->condition, func);
    
    if (handler_tuple) {
        PyList_Append(g_event_handlers, handler_tuple);
        Py_DECREF(handler_tuple);
    }
    
    Py_INCREF(func);
    return func;
}

/* Définition du type EventDecorator */
PyTypeObject EventDecoratorType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pyfasty._pyfasty.EventDecorator",
    .tp_doc = "Décorateur pour les événements PyFasty",
    .tp_basicsize = sizeof(EventDecoratorObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_dealloc = (destructor)PyObject_Del,
    .tp_call = (ternaryfunc)event_decorator_call,
};

/* Fonction pour créer le décorateur */
PyObject *event_decorator(PyObject *self, PyObject *args) {
    PyObject *condition = NULL;
    
    if (!PyArg_ParseTuple(args, "|O:event", &condition)) {
        return NULL;
    }
    
    EventDecoratorObject *decorator = PyObject_New(EventDecoratorObject, &EventDecoratorType);
    if (decorator == NULL)
        return NULL;
    
    if (condition) {
        Py_INCREF(condition);
        decorator->condition = condition;
    } else {
        Py_INCREF(Py_None);
        decorator->condition = Py_None;
    }
    
    return (PyObject *)decorator;
}

/* =================== FONCTIONS DE CONTRÔLE =================== */

/* Fonctions d'activation/désactivation */
PyObject *event_enable(PyObject *self, PyObject *args) {
    g_events_enabled = 1;
    Py_RETURN_NONE;
}

PyObject *event_disable(PyObject *self, PyObject *args) {
    g_events_enabled = 0;
    Py_RETURN_NONE;
}

/* Fonction de nettoyage pour éviter l'accumulation d'événements */
PyObject *event_clear_handlers(PyObject *self, PyObject *args) {
    if (g_event_handlers) {
        PyList_SetSlice(g_event_handlers, 0, PyList_Size(g_event_handlers), NULL);
    }
    
    /* Nettoyer aussi le cache des états */
    cleanup_condition_cache();
    
    /* Reset timestamp pour éviter les conflits */
    g_current_timestamp = 0;
    
    Py_RETURN_NONE;
}

/* =================== DÉCLENCHEMENT ET ÉVÉNEMENTS =================== */

/* Fonction de déclenchement des événements */
void pyfasty_trigger_events_with_module(ModuleType module_type) {
    if (g_in_callback_execution || g_is_evaluating || !g_events_enabled) {
        return;
    }
    
    g_current_module = module_type;
    event_evaluate_all(NULL, NULL);
    g_current_module = MODULE_UNKNOWN;
}

/* Version compatible */
int pyfasty_trigger_events_internal(void) {
    pyfasty_trigger_events_with_module(MODULE_ALL);
    return 0;
}

/* Fonction publique pour vérifier si on est dans un callback */
int pyfasty_is_in_callback_execution(void) {
    return g_in_callback_execution;
}

/* =================== NETTOYAGE ET UTILITAIRES =================== */

/* Nettoyage du cache */
void cleanup_condition_cache(void) {
    for (int i = 0; i < MAX_HANDLERS; i++) {
        g_handler_states[i].handler_tuple = NULL;
        g_handler_states[i].last_state = -1;
        g_handler_states[i].is_impossible = 0;
        g_handler_states[i].last_evaluation_triggered = 0;
        g_handler_states[i].is_direct_module = 0;
    }
}

/* =================== FONCTIONS POUR COMPATIBILITÉ =================== */

/* Fonctions compatibles avec l'ancienne API event_sync */
void pyfasty_trigger_sync_events_with_module(ModuleType module_type) {
    pyfasty_trigger_events_with_module(module_type);
}

void pyfasty_trigger_sync_events(void) {
    pyfasty_trigger_events_internal();
}

/* =================== INITIALISATION =================== */

/* Initialisation du module */
int PyFasty_Event_Init(PyObject *module) {
    /* Créer la liste des handlers */
    if (g_event_handlers == NULL) {
        g_event_handlers = PyList_New(0);
        if (g_event_handlers == NULL) {
            return -1;
        }
    }
    
    /* Activer les événements par défaut */
    g_events_enabled = 1;
    
    /* Initialiser les caches */
    cleanup_condition_cache();
    
    /* Préparer le type EventDecorator */
    if (PyType_Ready(&EventDecoratorType) < 0) {
        return -1;
    }
    
    /* Ajouter les méthodes au module */
    static PyMethodDef event_methods[] = {
        {"event", event_decorator, METH_VARARGS, "Décorateur pour les événements PyFasty"},
        {"event_enable", event_enable, METH_NOARGS, "Active le système d'événements"},
        {"event_disable", event_disable, METH_NOARGS, "Désactive le système d'événements"},
        {"event_clear_handlers", event_clear_handlers, METH_NOARGS, "Vide la liste des handlers d'événements"},
        {"event_evaluate_all", event_evaluate_all, METH_NOARGS, "Évalue toutes les conditions et déclenche les événements"},
        {"event_sync_evaluate_executor_only", event_sync_evaluate_executor_only, METH_NOARGS, "Fonction spécialisée pour EXECUTOR_SYNC"},
        {"event_sync_evaluate_all", event_evaluate_all, METH_NOARGS, "Alias pour event_evaluate_all (compatibilité)"},
        {NULL, NULL, 0, NULL}
    };
    
    for (PyMethodDef *method = event_methods; method->ml_name != NULL; method++) {
        PyObject *func = PyCFunction_New(method, NULL);
        if (func == NULL || PyModule_AddObject(module, method->ml_name, func) < 0) {
            Py_XDECREF(func);
            return -1;
        }
    }
    
    return 0;
}