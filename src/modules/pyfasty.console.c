#include "../pyfasty.h"
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

/* Structure pour le module console */
typedef struct {
    PyObject_HEAD
    PyObject *config;         /* Configuration (dictionnaire) */
    PyObject *last_message;   /* Dernier message généré (pour tests) */
    PyObject *log_history;    /* Historique des logs (liste, pour référence future) */
} PyFastyConsoleObject;

/* Types de messages de log */
typedef enum {
    LOG_DEFAULT = 0,
    LOG_INFO,
    LOG_SUCCESS,
    LOG_WARNING,
    LOG_ERROR,
    LOG_DEBUG,
    LOG_CRITICAL,
    LOG_FATAL
} LogType;

/* GÉNÉRALISATION : Configuration des types de log */
typedef struct {
    const char *name;
    const char *display_name;
    const char *default_color;
} LogTypeInfo;

static const LogTypeInfo LOG_TYPE_INFOS[] = {
    {"default", "", ""},
    {"info", "INFO", "\033[38;5;75m"},
    {"success", "SUCCESS", "\033[38;5;82m"},
    {"warning", "WARNING", "\033[38;5;220m"},
    {"error", "ERROR", "\033[38;5;196m"},
    {"debug", "DEBUG", "\033[38;5;198m"},
    {"critical", "CRITICAL", "\033[38;5;57m"},
    {"fatal", "FATAL", "\033[48;5;196m\033[38;5;255m"}
};

/* GÉNÉRALISATION : Configuration des couleurs par défaut */
typedef struct {
    const char *name;
    const char *value;
} DefaultColor;

static const DefaultColor DEFAULT_COLORS[] = {
    {"gray", "\033[38;5;245m"},
    {"reset", "\033[0m"},
    {NULL, NULL}  /* Sentinel */
};

/* GÉNÉRALISATION : Configuration par défaut */
static const char *DEFAULT_FORMAT = "<!gray><%Y>-<%m>-<%d> <%H>:<%M>:<%S>.<%F:4> | <!reset><!type><%TYPE> <%FILE&%FUNC><!reset><!gray> | <!reset><%MESSAGE>";
static const char *DEFAULT_LOG_FILENAME = "log.txt";
static const char *DEFAULT_LOG_FILEMODE = "a";

/* Cache pour gérer les modes d'ouverture de fichiers */
#define MAX_CACHE_FILES 10
static struct {
    char filename[256];
    int opened_with_w;  /* 1 si déjà ouvert avec 'w', 0 sinon */
} file_mode_cache[MAX_CACHE_FILES];
static int cache_initialized = 0;

/* Forward declarations */
static PyObject *console_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
static int console_init(PyFastyConsoleObject *self, PyObject *args, PyObject *kwds);
static void console_dealloc(PyFastyConsoleObject *self);
static PyObject *console_getattro(PyFastyConsoleObject *self, PyObject *name);
static int console_setattro(PyFastyConsoleObject *self, PyObject *name, PyObject *value);
static PyObject *console_call(PyObject *self, PyObject *args, PyObject *kwds);
static PyObject *console_log(PyObject *self, PyObject *args, LogType type);

/* Méthodes spécifiques pour chaque niveau de log */
static PyObject *console_info(PyObject *self, PyObject *args);
static PyObject *console_success(PyObject *self, PyObject *args);
static PyObject *console_warning(PyObject *self, PyObject *args);
static PyObject *console_error(PyObject *self, PyObject *args);
static PyObject *console_debug(PyObject *self, PyObject *args);
static PyObject *console_critical(PyObject *self, PyObject *args);
static PyObject *console_fatal(PyObject *self, PyObject *args);

/* Méthodes du module */
static PyMethodDef console_methods[] = {
    {"info", console_info, METH_VARARGS, "Log an info message"},
    {"success", console_success, METH_VARARGS, "Log a success message"},
    {"warning", console_warning, METH_VARARGS, "Log a warning message"},
    {"error", console_error, METH_VARARGS, "Log an error message"},
    {"debug", console_debug, METH_VARARGS, "Log a debug message"},
    {"critical", console_critical, METH_VARARGS, "Log a critical message"},
    {"fatal", console_fatal, METH_VARARGS, "Log a fatal message"},
    {NULL, NULL, 0, NULL}  /* Sentinel */
};

/* Définition du type PyFastyConsole */
static PyTypeObject PyFastyConsoleType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pyfasty._pyfasty.Console",
    .tp_doc = "Console for logging messages with different levels",
    .tp_basicsize = sizeof(PyFastyConsoleObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = console_new,
    .tp_init = (initproc)console_init,
    .tp_dealloc = (destructor)console_dealloc,
    .tp_getattro = (getattrofunc)console_getattro,
    .tp_setattro = (setattrofunc)console_setattro,
    .tp_call = (ternaryfunc)console_call,
    .tp_methods = console_methods,
};

/* GÉNÉRALISATION : Helper pour créer la configuration par défaut */
static PyObject *create_default_config(void) {
    PyObject *config = PyDict_New();
    if (config == NULL) {
        return NULL;
    }
    
    /* Configuration des vues */
    PyDict_SetItemString(config, "console_view", Py_True);
    PyDict_SetItemString(config, "debug_view", Py_True);
    
    /* Format par défaut */
    PyObject *format = PyUnicode_FromString(DEFAULT_FORMAT);
    PyDict_SetItemString(config, "format", format);
    Py_DECREF(format);
    
    /* Configuration de sauvegarde des logs */
    PyObject *save_log_dict = PyDict_New();
    if (save_log_dict == NULL) {
        Py_DECREF(config);
        return NULL;
    }
    
    PyDict_SetItemString(save_log_dict, "status", Py_False);
    PyObject *filename = PyUnicode_FromString(DEFAULT_LOG_FILENAME);
    PyObject *filemode = PyUnicode_FromString(DEFAULT_LOG_FILEMODE);
    PyDict_SetItemString(save_log_dict, "filename", filename);
    PyDict_SetItemString(save_log_dict, "filemode", filemode);
    Py_DECREF(filename);
    Py_DECREF(filemode);
    
    /* Dictionnaire de couleurs */
    PyObject *colors_dict = PyDict_New();
    PyObject *type_colors = PyDict_New();
    
    if (colors_dict == NULL || type_colors == NULL) {
        Py_XDECREF(colors_dict);
        Py_XDECREF(type_colors);
        Py_DECREF(save_log_dict);
        Py_DECREF(config);
        return NULL;
    }
    
    /* DYNAMIQUE : Initialiser les couleurs de types depuis le tableau */
    for (int i = 1; i < sizeof(LOG_TYPE_INFOS)/sizeof(LOG_TYPE_INFOS[0]); i++) {
        if (LOG_TYPE_INFOS[i].name && LOG_TYPE_INFOS[i].default_color) {
            PyObject *color = PyUnicode_FromString(LOG_TYPE_INFOS[i].default_color);
            PyDict_SetItemString(type_colors, LOG_TYPE_INFOS[i].name, color);
            Py_DECREF(color);
        }
    }
    
    /* DYNAMIQUE : Initialiser les couleurs génériques depuis le tableau */
    PyDict_SetItemString(colors_dict, "type", type_colors);
    for (int i = 0; DEFAULT_COLORS[i].name != NULL; i++) {
        PyObject *color = PyUnicode_FromString(DEFAULT_COLORS[i].value);
        PyDict_SetItemString(colors_dict, DEFAULT_COLORS[i].name, color);
        Py_DECREF(color);
    }
    
    PyDict_SetItemString(config, "colors", colors_dict);
    PyDict_SetItemString(config, "save_log", save_log_dict);
    
    Py_DECREF(type_colors);
    Py_DECREF(colors_dict);
    Py_DECREF(save_log_dict);
    
    return config;
}

/* Helper pour la création d'un nouveau module console */
static PyObject *console_create(void) {
    PyFastyConsoleObject *console = (PyFastyConsoleObject *)PyFastyConsoleType.tp_alloc(&PyFastyConsoleType, 0);
    if (console == NULL) {
        return NULL;
    }
    
    /* GÉNÉRALISATION : Utiliser la fonction pour créer la config par défaut */
    console->config = create_default_config();
    if (console->config == NULL) {
        Py_DECREF(console);
        return NULL;
    }
    
    /* Initialiser l'historique et dernier message */
    console->last_message = PyUnicode_FromString("");
    if (console->last_message == NULL) {
        Py_DECREF(console->config);
        Py_DECREF(console);
        return NULL;
    }
    
    console->log_history = PyList_New(0);
    if (console->log_history == NULL) {
        Py_DECREF(console->last_message);
        Py_DECREF(console->config);
        Py_DECREF(console);
        return NULL;
    }
    
    return (PyObject *)console;
}

/* GÉNÉRALISATION : Initialiser le cache des modes de fichiers */
static void init_file_mode_cache(void) {
    if (!cache_initialized) {
        for (int i = 0; i < MAX_CACHE_FILES; i++) {
            file_mode_cache[i].filename[0] = '\0';
            file_mode_cache[i].opened_with_w = 0;
        }
        cache_initialized = 1;
    }
}

/* GÉNÉRALISATION : Vérifier si un fichier a déjà été ouvert en mode 'w' */
static const char* check_file_mode(const char* filename, const char* filemode) {
    if (!cache_initialized) {
        init_file_mode_cache();
    }
    
    if (strcmp(filemode, "w") == 0) {
        for (int i = 0; i < MAX_CACHE_FILES; i++) {
            if (strcmp(file_mode_cache[i].filename, filename) == 0) {
                return "a";  /* Fichier déjà ouvert, utiliser 'a' au lieu de 'w' */
            }
            
            if (file_mode_cache[i].filename[0] == '\0') {
                strncpy(file_mode_cache[i].filename, filename, sizeof(file_mode_cache[i].filename) - 1);
                file_mode_cache[i].filename[sizeof(file_mode_cache[i].filename) - 1] = '\0';
                file_mode_cache[i].opened_with_w = 1;
                return filemode;  /* Utiliser 'w' car c'est la première ouverture */
            }
        }
    }
    
    return filemode;
}

/* GÉNÉRALISATION : Helper pour obtenir le nom du type de log */
static const char *get_log_type_name(LogType type) {
    if (type >= 0 && type < sizeof(LOG_TYPE_INFOS)/sizeof(LOG_TYPE_INFOS[0])) {
        return LOG_TYPE_INFOS[type].display_name;
    }
    return "";
}

/* Helper pour obtenir une couleur depuis la configuration */
static const char *get_color_from_config(PyObject *config, const char *color_name, LogType type) {
    static char default_color[] = "";
    
    if (config == NULL || !PyDict_Check(config)) {
        return default_color;
    }
    
    PyObject *colors_dict = PyDict_GetItemString(config, "colors");
    if (colors_dict == NULL || !PyDict_Check(colors_dict)) {
        return default_color;
    }
    
    /* Cas spécial pour "type" qui utilise le type de log actuel */
    if (strcmp(color_name, "type") == 0) {
        PyObject *type_dict = PyDict_GetItemString(colors_dict, "type");
        if (type_dict == NULL || !PyDict_Check(type_dict)) {
            return default_color;
        }
        
        /* DYNAMIQUE : Utiliser le tableau des types au lieu de hardcoder */
        if (type >= 0 && type < sizeof(LOG_TYPE_INFOS)/sizeof(LOG_TYPE_INFOS[0])) {
            const char *type_name = LOG_TYPE_INFOS[type].name;
            if (type_name && *type_name) {
                PyObject *color_obj = PyDict_GetItemString(type_dict, type_name);
                if (color_obj != NULL && PyUnicode_Check(color_obj)) {
                    return PyUnicode_AsUTF8(color_obj);
                }
            }
        }
    } else {
        /* Couleurs directes */
        PyObject *color_obj = PyDict_GetItemString(colors_dict, color_name);
        if (color_obj != NULL && PyUnicode_Check(color_obj)) {
            return PyUnicode_AsUTF8(color_obj);
        }
    }
    
    return default_color;
}

/* Helper pour formater un message de log */
static PyObject *format_log_message(PyObject *format_str, const char *message, LogType type, const char *file, const char *func, PyObject *config, int use_colors) {
    if (format_str == NULL || !PyUnicode_Check(format_str)) {
        return PyUnicode_FromString(message);  /* Format par défaut */
    }
    
    /* Cache de date/heure pour éviter les appels répétés à strftime */
    static time_t last_time = 0;
    static char cached_hour[3] = {0};
    static char cached_min[3] = {0};
    static char cached_sec[3] = {0};
    static char cached_day[3] = {0};
    static char cached_month[3] = {0};
    static char cached_year[5] = {0};
    
    /* Obtenir la date et l'heure actuelles */
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    
    /* Format de date standard - utiliser le cache si possible */
    char hour[3], min[3], sec[3], day[3], month[3], year[5];
    
    /* Mise à jour du cache si la seconde a changé */
    if (t != last_time) {
        strftime(cached_hour, sizeof(cached_hour), "%H", tm_info);
        strftime(cached_min, sizeof(cached_min), "%M", tm_info);
        strftime(cached_sec, sizeof(cached_sec), "%S", tm_info);
        strftime(cached_day, sizeof(cached_day), "%d", tm_info);
        strftime(cached_month, sizeof(cached_month), "%m", tm_info);
        strftime(cached_year, sizeof(cached_year), "%Y", tm_info);
        last_time = t;
    }
    
    /* Utiliser les valeurs mises en cache */
    strcpy(hour, cached_hour);
    strcpy(min, cached_min);
    strcpy(sec, cached_sec);
    strcpy(day, cached_day);
    strcpy(month, cached_month);
    strcpy(year, cached_year);
    
    /* Obtenir les millisecondes en utilisant clock() */
    clock_t ticks = clock();
    unsigned long ms = (ticks * 1000) / CLOCKS_PER_SEC % 1000;
    char ms_buffer[10]; /* Pour gérer différentes précisions */
    
    /* Obtenir le type de log */
    const char *type_str = get_log_type_name(type);
    
    /* Construire un format amélioré en remplaçant les tokens */
    const char *format_c = PyUnicode_AsUTF8(format_str);
    char buffer[2048] = "";  /* Buffer plus grand pour les codes de couleur */
    char *pos = buffer;
    
    while (*format_c) {
        /* Vérifier les balises de couleur */
        if (format_c[0] == '<' && format_c[1] == '!') {
            format_c += 2;  /* Skip "<!" */
            char color_name[32] = {0};
            int i = 0;
            
            /* Extraire le nom de la couleur */
            while (*format_c && *format_c != '>' && i < sizeof(color_name) - 1) {
                color_name[i++] = *format_c++;
            }
            
            if (*format_c == '>') {
                format_c++;  /* Skip ">" */
                /* Obtenir et appliquer la couleur seulement si use_colors est activé */
                if (use_colors) {
                    const char *color_code = get_color_from_config(config, color_name, type);
                    pos += sprintf(pos, "%s", color_code);
                }
            }
        }
        /* Vérifier les tokens de format */
        else if (format_c[0] == '<' && format_c[1] == '%') {
            /* Nouveau format avec % */
            if (strncmp(format_c, "<%Y>", 4) == 0) {
                pos += sprintf(pos, "%s", year);
                format_c += 4;
            } else if (strncmp(format_c, "<%m>", 4) == 0) {
                pos += sprintf(pos, "%s", month);
                format_c += 4;
            } else if (strncmp(format_c, "<%d>", 4) == 0) {
                pos += sprintf(pos, "%s", day);
                format_c += 4;
            } else if (strncmp(format_c, "<%H>", 4) == 0) {
                pos += sprintf(pos, "%s", hour);
                format_c += 4;
            } else if (strncmp(format_c, "<%M>", 4) == 0) {
                pos += sprintf(pos, "%s", min);
                format_c += 4;
            } else if (strncmp(format_c, "<%S>", 4) == 0) {
                pos += sprintf(pos, "%s", sec);
                format_c += 4;
            } else if (strncmp(format_c, "<%F:", 4) == 0) {
                /* Format millisecondes avec précision */
                format_c += 4; /* Skip "<%F:" */
                int precision = 0;
                
                /* Lire la précision */
                while (isdigit(*format_c)) {
                    precision = precision * 10 + (*format_c - '0');
                    format_c++;
                }
                
                if (*format_c == '>') {
                    format_c++; /* Skip ">" */
                    
                    /* Format avec la précision spécifiée */
                    if (precision > 0) {
                        if (precision > 9) precision = 9; /* Limite raisonnable */
                        snprintf(ms_buffer, sizeof(ms_buffer), "%0*lu", precision, ms % (unsigned long)pow(10, precision));
                        pos += sprintf(pos, "%s", ms_buffer);
                    }
                } else {
                    /* Format incorrect, copier tel quel */
                    memcpy(pos, "<%F:", 4);
                    pos += 4;
                }
            } else if (strncmp(format_c, "<%TYPE>", 7) == 0) {
                if (type_str && *type_str) {
                    pos += sprintf(pos, "[%s]", type_str);
                } else {
                    pos += sprintf(pos, "");
                }
                format_c += 7;
            } else if (strncmp(format_c, "<%FUNC>", 7) == 0) {
                if (func != NULL) {
                    pos += sprintf(pos, "[%s]", func);
                } else {
                    pos += sprintf(pos, "");
                }
                format_c += 7;
            } else if (strncmp(format_c, "<%FILE>", 7) == 0) {
                pos += sprintf(pos, "[%s]", file);
                format_c += 7;
            } else if (strncmp(format_c, "<%FILE&%FUNC>", 13) == 0 || strncmp(format_c, "<%FUNC&%FILE>", 13) == 0) {
                if (func != NULL) {
                    pos += sprintf(pos, "[%s:%s]", file, func);
                } else {
                    pos += sprintf(pos, "[%s]", file);
                }
                format_c += 13;
            } else if (strncmp(format_c, "<%MESSAGE>", 10) == 0) {
                /* For messages with newlines, we need to handle them specially */
                const char *msg_ptr = message;
                while (*msg_ptr) {
                    if (*msg_ptr == '\n') {
                        *pos++ = '\n';
                    } else {
                        *pos++ = *msg_ptr;
                    }
                    msg_ptr++;
                }
                format_c += 10;
            } else {
                /* Anciennes versions des tokens sans % si encore utilisées */
                if (strncmp(format_c, "<HH>", 4) == 0) {
                    pos += sprintf(pos, "%s", hour);
                    format_c += 4;
                } else if (strncmp(format_c, "<MM>", 4) == 0) {
                    pos += sprintf(pos, "%s", min);
                    format_c += 4;
                } else if (strncmp(format_c, "<SS>", 4) == 0) {
                    pos += sprintf(pos, "%s", sec);
                    format_c += 4;
                } else if (strncmp(format_c, "<MS>", 4) == 0) {
                    sprintf(ms_buffer, "%03lu", ms);
                    pos += sprintf(pos, "%s", ms_buffer);
                    format_c += 4;
                } else if (strncmp(format_c, "<DD>", 4) == 0) {
                    pos += sprintf(pos, "%s", day);
                    format_c += 4;
                } else if (strncmp(format_c, "<MO>", 4) == 0) {
                    pos += sprintf(pos, "%s", month);
                    format_c += 4;
                } else if (strncmp(format_c, "<YYYY>", 6) == 0) {
                    pos += sprintf(pos, "%s", year);
                    format_c += 6;
                } else if (strncmp(format_c, "<TYPE>", 6) == 0) {
                    if (type_str && *type_str) {
                        pos += sprintf(pos, "[%s]", type_str);
                    } else {
                        pos += sprintf(pos, "");
                    }
                    format_c += 6;
                } else if (strncmp(format_c, "<FUNC>", 6) == 0) {
                    if (func != NULL) {
                        pos += sprintf(pos, "[%s]", func);
                    } else {
                        pos += sprintf(pos, "");
                    }
                    format_c += 6;
                } else if (strncmp(format_c, "<FILE>", 6) == 0) {
                    pos += sprintf(pos, "[%s]", file);
                    format_c += 6;
                } else if (strncmp(format_c, "<FILE&FUNC>", 11) == 0 || strncmp(format_c, "<FUNC&FILE>", 11) == 0) {
                    if (func != NULL) {
                        pos += sprintf(pos, "[%s:%s]", file, func);
                    } else {
                        pos += sprintf(pos, "[%s]", file);
                    }
                    format_c += 11;
                } else if (strncmp(format_c, "<MESSAGE>", 9) == 0) {
                    /* For messages with newlines, we need to handle them specially */
                    const char *msg_ptr = message;
                    while (*msg_ptr) {
                        if (*msg_ptr == '\n') {
                            *pos++ = '\n';
                        } else {
                            *pos++ = *msg_ptr;
                        }
                        msg_ptr++;
                    }
                    format_c += 9;
                } else {
                    /* Si ce n'est pas un token reconnu, ajouter simplement le caractère */
                    *pos++ = *format_c++;
                }
            }
        } else {
            /* Copier caractère par caractère */
            *pos++ = *format_c++;
        }
    }
    *pos = '\0';  /* Terminer la chaîne */
    
    /* Créer l'objet Python pour le résultat formaté */
    return PyUnicode_FromString(buffer);
}

/* CORRECTION : Fonction centrale pour gérer les logs avec option simple */
static PyObject *console_log_internal(PyFastyConsoleObject *self, const char *message, LogType type, const char *file, const char *func, int use_simple_format) {
    /* Mode silencieux pendant l'exécution des callbacks d'événements */
    if (pyfasty_is_in_callback_execution()) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    
    /* Mode silencieux pendant l'évaluation des conditions d'événements */
    if (g_in_condition_evaluation) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    
    /* CORRECTION : Si format simple demandé, juste afficher le message brut */
    if (use_simple_format) {
        /* Sauvegarder le dernier message */
        PyObject *simple_message = PyUnicode_FromString(message);
        if (simple_message == NULL) {
            return NULL;
        }
        
        Py_XDECREF(self->last_message);
        self->last_message = simple_message;
        Py_INCREF(self->last_message);
        
        /* Ajouter à l'historique des logs */
        if (PyList_Append(self->log_history, simple_message) < 0) {
            return NULL;
        }
        
        /* Afficher directement le message comme print() */
        PySys_WriteStdout("%s\n", message);
        
        return simple_message;
    }
    
    /* Vérifier si la console est activée */
    PyObject *console_view = PyDict_GetItemString(self->config, "console_view");
    if (console_view == NULL || !PyObject_IsTrue(console_view)) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    
    /* Pour les messages debug, vérifier debug_view */
    if (type == LOG_DEBUG) {
        PyObject *debug_view = PyDict_GetItemString(self->config, "debug_view");
        if (debug_view == NULL || !PyObject_IsTrue(debug_view)) {
            Py_INCREF(Py_None);
            return Py_None;
        }
    }
    
    /* Récupérer le format */
    PyObject *format_str = PyDict_GetItemString(self->config, "format");
    
    /* Formater le message */
    PyObject *formatted_message = format_log_message(format_str, message, type, file, func, self->config, 1);
    if (formatted_message == NULL) {
        return NULL;
    }
    
    /* Sauvegarder le dernier message */
    Py_XDECREF(self->last_message);
    self->last_message = formatted_message;
    Py_INCREF(self->last_message);
    
    /* Ajouter à l'historique des logs */
    if (PyList_Append(self->log_history, formatted_message) < 0) {
        Py_DECREF(formatted_message);
        return NULL;
    }
    
    /* Afficher le message (utiliser PySys_WriteStdout pour éviter la récursion) */
    PyObject *str_obj = PyObject_Str(formatted_message);
    if (str_obj != NULL) {
        const char *str = PyUnicode_AsUTF8(str_obj);
        if (str != NULL) {
            /* Ne pas ajouter de \n automatiquement pour préserver le formatage */
            PySys_WriteStdout("%s", str);
            
            /* Si le message ne se termine pas par un saut de ligne, en ajouter un */
            size_t len = strlen(str);
            if (len > 0 && str[len-1] != '\n') {
                PySys_WriteStdout("\n");
            }
            
            /* Écrire dans le fichier de log si activé */
            PyObject *save_log = PyDict_GetItemString(self->config, "save_log");
            if (save_log != NULL && PyDict_Check(save_log)) {
                PyObject *status = PyDict_GetItemString(save_log, "status");
                if (status != NULL && PyObject_IsTrue(status)) {
                    /* Récupérer le nom du fichier et le mode */
                    PyObject *filename_obj = PyDict_GetItemString(save_log, "filename");
                    PyObject *filemode_obj = PyDict_GetItemString(save_log, "filemode");
                    
                    const char *filename = DEFAULT_LOG_FILENAME;
                    const char *filemode = DEFAULT_LOG_FILEMODE;
                    
                    if (filename_obj != NULL && PyUnicode_Check(filename_obj)) {
                        filename = PyUnicode_AsUTF8(filename_obj);
                    }
                    
                    if (filemode_obj != NULL && PyUnicode_Check(filemode_obj)) {
                        filemode = PyUnicode_AsUTF8(filemode_obj);
                        /* Vérifier que le mode est valide ('a' ou 'w') */
                        if (strcmp(filemode, "a") != 0 && strcmp(filemode, "w") != 0) {
                            filemode = DEFAULT_LOG_FILEMODE;
                        }
                    }
                    
                    /* Ajuster le mode d'ouverture si nécessaire */
                    filemode = check_file_mode(filename, filemode);
                    
                    /* Ouvrir le fichier et écrire le message */
                    FILE *log_file = fopen(filename, filemode);
                    if (log_file != NULL) {
                        /* Format sans couleurs pour le fichier log */
                        PyObject *log_formatted_message = format_log_message(format_str, message, type, file, func, self->config, 0);
                        if (log_formatted_message != NULL) {
                            const char *log_str = PyUnicode_AsUTF8(log_formatted_message);
                            if (log_str != NULL) {
                                fprintf(log_file, "%s", log_str);
                                size_t len = strlen(log_str);
                                if (len > 0 && log_str[len-1] != '\n') {
                                    fprintf(log_file, "\n");
                                }
                            }
                            Py_DECREF(log_formatted_message);
                        }
                        fclose(log_file);
                    }
                }
            }
        }
        Py_DECREF(str_obj);
    }
    
    return formatted_message;
}

/* Implementation de console_new */
static PyObject *console_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PyFastyConsoleObject *self = (PyFastyConsoleObject *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->config = PyDict_New();
        if (self->config == NULL) {
            Py_DECREF(self);
            return NULL;
        }
        
        self->last_message = PyUnicode_FromString("");
        if (self->last_message == NULL) {
            Py_DECREF(self->config);
            Py_DECREF(self);
            return NULL;
        }
        
        self->log_history = PyList_New(0);
        if (self->log_history == NULL) {
            Py_DECREF(self->last_message);
            Py_DECREF(self->config);
            Py_DECREF(self);
            return NULL;
        }
    }
    
    return (PyObject *)self;
}

/* Implementation de console_init */
static int console_init(PyFastyConsoleObject *self, PyObject *args, PyObject *kwds) {
    /* GÉNÉRALISATION : Utiliser la fonction pour créer la config par défaut */
    self->config = create_default_config();
    if (self->config == NULL) {
        return -1;
    }
    
    return 0;
}

/* Implementation de console_dealloc */
static void console_dealloc(PyFastyConsoleObject *self) {
    Py_XDECREF(self->config);
    Py_XDECREF(self->last_message);
    Py_XDECREF(self->log_history);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

/* Implementation de console_getattro */
static PyObject *console_getattro(PyFastyConsoleObject *self, PyObject *name) {
    /* Tracer l'accès au module console pour la détection de dépendances */
    pyfasty_trace_module_access(MODULE_CONSOLE);
    
    /* Cas spécial pour config */
    const char *name_str = PyUnicode_AsUTF8(name);
    if (strcmp(name_str, "config") == 0) {
        Py_INCREF(self->config);
        return self->config;
    }
    
    /* Cas standard */
    return PyObject_GenericGetAttr((PyObject *)self, name);
}

/* Implementation de console_setattro */
static int console_setattro(PyFastyConsoleObject *self, PyObject *name, PyObject *value) {
    const char *name_str = PyUnicode_AsUTF8(name);
    
    /* Cas spécial pour config */
    if (strcmp(name_str, "config") == 0) {
        /* Si la nouvelle valeur est un dictionnaire, fusionner avec la config existante */
        if (PyDict_Check(value)) {
            /* Mettre à jour les clés définies dans le nouveau dictionnaire */
            PyObject *key, *val;
            Py_ssize_t pos = 0;
            
            while (PyDict_Next(value, &pos, &key, &val)) {
                if (PyDict_SetItem(self->config, key, val) < 0) {
                    return -1;
                }
            }
            
            /* Déclencher les événements après modification de la config */
            if (!g_in_condition_evaluation) {
                pyfasty_trigger_sync_events_with_module(MODULE_CONSOLE);
            }
            return 0;
        }
        /* Si ce n'est pas un dictionnaire, remplacer entièrement */
        else {
            PyObject *tmp = self->config;
            Py_INCREF(value);
            self->config = value;
            Py_DECREF(tmp);
            
            /* Déclencher les événements après modification de la config */
            if (!g_in_condition_evaluation) {
                pyfasty_trigger_sync_events_with_module(MODULE_CONSOLE);
            }
            return 0;
        }
    }
    
    /* Cas standard */
    int result = PyObject_GenericSetAttr((PyObject *)self, name, value);
    
    /* Déclencher les événements si la modification est réussie */
    if (result >= 0) {
        if (!g_in_condition_evaluation) {
            pyfasty_trigger_sync_events_with_module(MODULE_CONSOLE);
        }
    }
    
    return result;
}

/* CORRECTION : Implementation de console_call avec format simple */
static PyObject *console_call(PyObject *self, PyObject *args, PyObject *kwds) {
    PyFastyConsoleObject *console = (PyFastyConsoleObject *)self;
    PyObject *message;
    
    /* Tracer l'accès au module console pour la détection de dépendances */
    pyfasty_trace_module_access(MODULE_CONSOLE);
    
    /* Ne déclencher les événements console QUE si on n'est PAS en évaluation */
    if (!g_in_condition_evaluation) {
        pyfasty_trigger_sync_events_with_module(MODULE_CONSOLE);
    }
    
    /* Analyser les arguments */
    if (!PyArg_ParseTuple(args, "O", &message)) {
        return NULL;
    }
    
    /* Convertir le message en chaîne */
    PyObject *str_message = PyObject_Str(message);
    const char *msg = PyUnicode_AsUTF8(str_message);
    
    /* CORRECTION : Appel direct avec format simple (comme print) */
    PyObject *result = console_log_internal(console, msg, LOG_DEFAULT, NULL, NULL, 1);
    
    /* Nettoyage */
    Py_DECREF(str_message);
    
    return result;
}

/* Implementation de console_log (générique) */
static PyObject *console_log(PyObject *self, PyObject *args, LogType type) {
    const char *message;
    
    if (!PyArg_ParseTuple(args, "s", &message)) {
        return NULL;
    }
    
    /* Ne déclencher les événements console QUE si on n'est PAS en évaluation */
    if (!g_in_condition_evaluation) {
        pyfasty_trigger_sync_events_with_module(MODULE_CONSOLE);
    }
    
    /* Extraire le fichier et la fonction appelante en utilisant l'introspection Python */
    char file_buffer[256] = "unknown.py";
    char func_buffer[256] = "";
    const char *file = file_buffer;
    const char *func = NULL;  /* NULL si pas de fonction (niveau module) */
    
    /* Obtenir la frame courante */
    PyObject *frame_obj = PyEval_GetFrame();
    if (frame_obj != NULL) {
        PyObject *current_frame = frame_obj;
        Py_INCREF(current_frame);
        
        /* Traverser la pile d'appel pour trouver la frame qui nous intéresse */
        PyObject *back_frame = NULL;
        while (current_frame != NULL) {
            /* Obtenir d'abord les informations de cette frame */
            PyObject *code_obj = NULL;
            if (PyObject_HasAttrString(current_frame, "f_code")) {
                code_obj = PyObject_GetAttrString(current_frame, "f_code");
                
                if (code_obj != NULL) {
                    /* Obtenir le nom du fichier */
                    PyObject *filename_obj = NULL;
                    const char *temp_file = NULL;
                    
                    if (PyObject_HasAttrString(code_obj, "co_filename")) {
                        filename_obj = PyObject_GetAttrString(code_obj, "co_filename");
                        if (filename_obj != NULL && PyUnicode_Check(filename_obj)) {
                            temp_file = PyUnicode_AsUTF8(filename_obj);
                        }
                    }
                    
                    /* Obtenir le nom de la fonction */
                    PyObject *funcname_obj = NULL;
                    const char *temp_func = NULL;
                    
                    if (PyObject_HasAttrString(code_obj, "co_name")) {
                        funcname_obj = PyObject_GetAttrString(code_obj, "co_name");
                        if (funcname_obj != NULL && PyUnicode_Check(funcname_obj)) {
                            temp_func = PyUnicode_AsUTF8(funcname_obj);
                        }
                    }
                    
                    /* Si nous avons trouvé un fichier test_console.py ou main.py, utiliser cette frame */
                    if (temp_file != NULL) {
                        const char *last_slash = strrchr(temp_file, '\\');
                        const char *last_slash2 = strrchr(temp_file, '/');
                        if (last_slash2 > last_slash) last_slash = last_slash2;
                        
                        const char *simple_filename = (last_slash != NULL) ? last_slash + 1 : temp_file;
                        
                        /* Trouver un fichier spécifique, ou tout fichier qui n'est pas dans pyfasty/ */
                        if (strstr(simple_filename, "test_console.py") != NULL ||
                            strstr(simple_filename, "main.py") != NULL ||
                            (strstr(temp_file, "pyfasty/") == NULL && 
                             strstr(temp_file, "pyfasty\\") == NULL)) {
                            
                            /* Extraire et stocker le nom du fichier simple */
                            strncpy(file_buffer, simple_filename, sizeof(file_buffer) - 1);
                            file_buffer[sizeof(file_buffer) - 1] = '\0';
                            file = file_buffer;
                            
                            /* Stocker la fonction si disponible et pas "<module>" */
                            if (temp_func != NULL && strcmp(temp_func, "<module>") != 0) {
                                strncpy(func_buffer, temp_func, sizeof(func_buffer) - 1);
                                func_buffer[sizeof(func_buffer) - 1] = '\0';
                                func = func_buffer;
                            }
                            
                            /* Nous avons trouvé une bonne frame, on peut arrêter la recherche */
                            Py_XDECREF(funcname_obj);
                            Py_XDECREF(filename_obj);
                            Py_DECREF(code_obj);
                            break;
                        }
                    }
                    
                    Py_XDECREF(funcname_obj);
                    Py_XDECREF(filename_obj);
                    Py_DECREF(code_obj);
                }
            }
            
            /* Passer à la frame précédente */
            back_frame = NULL;
            if (PyObject_HasAttrString(current_frame, "f_back")) {
                back_frame = PyObject_GetAttrString(current_frame, "f_back");
                Py_DECREF(current_frame);
                current_frame = back_frame;
            } else {
                Py_DECREF(current_frame);
                current_frame = NULL;
            }
        }
        
        /* Libérer la référence si on n'a pas trouvé de frame d'intérêt */
        if (current_frame != NULL) {
            Py_DECREF(current_frame);
        }
    }
    
    /* CORRECTION : Appeler avec format normal (pas simple) pour les méthodes de log */
    return console_log_internal((PyFastyConsoleObject *)self, message, type, file, func, 0);
}

/* Implementation des fonctions de log spécifiques */
static PyObject *console_info(PyObject *self, PyObject *args) {
    return console_log(self, args, LOG_INFO);
}

static PyObject *console_success(PyObject *self, PyObject *args) {
    return console_log(self, args, LOG_SUCCESS);
}

static PyObject *console_warning(PyObject *self, PyObject *args) {
    return console_log(self, args, LOG_WARNING);
}

static PyObject *console_error(PyObject *self, PyObject *args) {
    return console_log(self, args, LOG_ERROR);
}

static PyObject *console_debug(PyObject *self, PyObject *args) {
    return console_log(self, args, LOG_DEBUG);
}

static PyObject *console_critical(PyObject *self, PyObject *args) {
    return console_log(self, args, LOG_CRITICAL);
}

static PyObject *console_fatal(PyObject *self, PyObject *args) {
    return console_log(self, args, LOG_FATAL);
}

/* Instance globale */
static PyObject *g_console = NULL;

/* Initialisation du module */
int PyFasty_Console_Init(PyObject *module) {
    /* Préparer le type */
    if (PyType_Ready(&PyFastyConsoleType) < 0) {
        return -1;
    }
    
    /* Ajouter le type au module */
    Py_INCREF(&PyFastyConsoleType);
    if (PyModule_AddObject(module, "Console", (PyObject *)&PyFastyConsoleType) < 0) {
        Py_DECREF(&PyFastyConsoleType);
        return -1;
    }
    
    /* Créer et ajouter l'instance globale */
    g_console = console_create();
    if (g_console == NULL) {
        return -1;
    }
    
    if (PyModule_AddObject(module, "console", g_console) < 0) {
        Py_DECREF(g_console);
        g_console = NULL;
        return -1;
    }
    
    return 0;
}
