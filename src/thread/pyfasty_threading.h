#ifndef PYFASTY_THREADING_H
#define PYFASTY_THREADING_H

#include <Python.h>

/* DÉTECTION DE PLATEFORME OPTIMISÉE + MODE MINIMAL POUR DEBUGGING */
#if defined(_WIN32) || defined(_WIN64)
    #define PYFASTY_WINDOWS
    #include <windows.h>
    #include <process.h>
#else
    #define PYFASTY_UNIX
    #include <pthread.h>
    /* MODE MINIMAL LINUX/MACOS pour éviter segfaults d'initialisation */
    #define PYFASTY_MINIMAL_THREADING
#endif

/* CONFIGURATION 100% DYNAMIQUE */
/* Variables globales configurables - plus de valeurs en dur */
extern int g_pyfasty_thread_pool_max_size;
extern int g_pyfasty_thread_pool_default_size;
extern int g_pyfasty_task_queue_max_size;
extern int g_pyfasty_max_name_length;

/* Valeurs par défaut si non configurées */
#define PYFASTY_DEFAULT_THREAD_POOL_MAX_SIZE 32
#define PYFASTY_DEFAULT_THREAD_POOL_DEFAULT_SIZE 4
#define PYFASTY_DEFAULT_TASK_QUEUE_MAX_SIZE 1000
#define PYFASTY_DEFAULT_MAX_NAME_LENGTH 64

/* MACROS UTILITAIRES CROSS-PLATFORM */
#ifdef PYFASTY_WINDOWS
    #define PYFASTY_THREAD_FUNC_RETURN unsigned
    #define PYFASTY_THREAD_FUNC_CALL __stdcall
    #define PYFASTY_INFINITE_WAIT INFINITE
#else
    #define PYFASTY_THREAD_FUNC_RETURN void*
    #define PYFASTY_THREAD_FUNC_CALL
    #define PYFASTY_INFINITE_WAIT ((unsigned long)-1)
#endif

/* FORWARD DECLARATIONS */
typedef struct PyFasty_Mutex PyFasty_Mutex;
typedef struct PyFasty_Cond PyFasty_Cond;
typedef struct PyFasty_Thread PyFasty_Thread;
typedef struct PyFasty_ThreadTask PyFasty_ThreadTask;
typedef struct PyFasty_ThreadPool PyFasty_ThreadPool;
typedef struct PyFasty_PythonTask PyFasty_PythonTask;

/* TYPES DE FONCTIONS OPTIMISÉS */

/* Type de callback pour les tâches - avec support d'erreur */
typedef void* (*PyFasty_ThreadTaskFunc)(void* arg);

/* Type de callback pour le monitoring des threads */
typedef void (*PyFasty_ThreadMonitorFunc)(int thread_id, const char* status);

/* Type de callback pour la gestion d'erreurs */
typedef void (*PyFasty_ErrorHandlerFunc)(const char* error_msg, int error_code);

/* STRUCTURES CROSS-PLATFORM OPTIMISÉES */

/* Mutex cross-platform avec métadonnées - taille dynamique */
struct PyFasty_Mutex {
#ifdef PYFASTY_WINDOWS
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
    /* NOUVEAU : Métadonnées pour le debugging */
    int lock_count;           /* Nombre de verrous acquis */
    int thread_id;           /* ID du thread propriétaire */
    char *name;              /* DYNAMIQUE : Nom du mutex alloué dynamiquement */
    int name_allocated;      /* Flag pour indiquer si le nom doit être libéré */
};

/* Condition variable cross-platform */
struct PyFasty_Cond {
#ifdef PYFASTY_WINDOWS
    CONDITION_VARIABLE cond;
#else
    pthread_cond_t cond;
#endif
    /* NOUVEAU : Statistiques */
    int wait_count;          /* Nombre de threads en attente */
    int signal_count;        /* Nombre de signaux envoyés */
};

/* Thread handle cross-platform avec informations étendues - taille dynamique */
struct PyFasty_Thread {
#ifdef PYFASTY_WINDOWS
    HANDLE handle;
    unsigned int id;
#else
    pthread_t thread;
#endif
    /* NOUVEAU : Métadonnées du thread */
    int thread_index;        /* Index dans le pool */
    int is_running;          /* État du thread */
    char *name;              /* DYNAMIQUE : Nom du thread alloué dynamiquement */
    int name_allocated;      /* Flag pour indiquer si le nom doit être libéré */
    time_t start_time;       /* Heure de démarrage */
    int tasks_completed;     /* Nombre de tâches terminées */
};

/* STRUCTURE DE TÂCHE OPTIMISÉE - taille dynamique */
struct PyFasty_ThreadTask {
    PyFasty_ThreadTaskFunc func;      /* Fonction à exécuter */
    void *arg;                        /* Argument de la fonction */
    PyObject *callback;               /* Callback Python (optionnel) */
    PyObject *error_callback;         /* Callback d'erreur Python (optionnel) */
    struct PyFasty_ThreadTask *next;  /* Tâche suivante dans la liste */
    
    /* NOUVEAU : Métadonnées de performance */
    int priority;                     /* Priorité de la tâche (0-10) */
    time_t created_time;             /* Heure de création */
    time_t start_time;               /* Heure de début d'exécution */
    time_t end_time;                 /* Heure de fin d'exécution */
    int retry_count;                 /* Nombre de tentatives */
    char *name;                      /* DYNAMIQUE : Nom de la tâche alloué dynamiquement */
    int name_allocated;              /* Flag pour indiquer si le nom doit être libéré */
};

/* STRUCTURE DE POOL OPTIMISÉE AVEC MONITORING - 100% DYNAMIQUE */
struct PyFasty_ThreadPool {
    PyFasty_Thread *threads;                         /* DYNAMIQUE : Tableau alloué dynamiquement */
    int thread_count;                                /* Nombre de threads actifs */
    int max_threads_capacity;                       /* NOUVEAU : Capacité maximale allouée */
    int running;                                     /* État du pool (1=running, 0=shutdown) */
    
    /* File d'attente optimisée */
    PyFasty_ThreadTask *tasks_head;                  /* Tête de la file d'attente */
    PyFasty_ThreadTask *tasks_tail;                  /* Queue de la file d'attente */
    int task_count;                                  /* Nombre de tâches en attente */
    int max_task_count;                             /* NOUVEAU : Maximum de tâches atteint */
    int task_queue_limit;                           /* NOUVEAU : Limite configurable de la file */
    
    /* Synchronisation */
    PyFasty_Mutex queue_mutex;                       /* Mutex pour la file d'attente */
    PyFasty_Cond queue_not_empty;                    /* Condition pour signaler file non vide */
    PyFasty_Cond queue_empty;                        /* Condition pour signaler file vide */
    
    /* NOUVEAU : Statistiques et monitoring */
    int total_tasks_processed;                       /* Total des tâches traitées */
    int active_threads;                             /* Nombre de threads actifs */
    double avg_task_duration;                       /* Durée moyenne des tâches */
    time_t pool_start_time;                         /* Heure de création du pool */
    PyFasty_ThreadMonitorFunc monitor_callback;     /* Callback de monitoring */
    PyFasty_ErrorHandlerFunc error_handler;         /* Gestionnaire d'erreurs */
    
    /* Configuration du pool */
    int auto_scale;                                 /* Auto-ajustement du nombre de threads */
    int min_threads;                               /* Nombre minimum de threads */
    int max_threads;                               /* Nombre maximum de threads */
};

/* STRUCTURE POUR TÂCHE PYTHON OPTIMISÉE - taille dynamique */
struct PyFasty_PythonTask {
    PyObject *callable;      /* Fonction/méthode Python à appeler */
    PyObject *args;          /* Arguments pour l'appel */
    PyObject *kwargs;        /* Arguments nommés pour l'appel */
    PyObject *result;        /* Résultat de l'appel */
    PyObject *error;         /* Exception levée (le cas échéant) */
    int completed;           /* Indique si la tâche est terminée */
    PyFasty_Mutex mutex;     /* Mutex pour protéger cette structure */
    
    /* NOUVEAU : Métadonnées étendues - dynamiques */
    int task_id;            /* Identifiant unique de la tâche */
    time_t created_time;    /* Heure de création */
    time_t start_time;      /* Heure de début d'exécution */
    time_t end_time;        /* Heure de fin d'exécution */
    int thread_id;          /* ID du thread d'exécution */
    char *description;      /* DYNAMIQUE : Description allouée dynamiquement */
    int description_allocated; /* Flag pour indiquer si la description doit être libérée */
};

/* ÉNUMÉRATION POUR LES ÉTATS DE THREAD */
typedef enum {
    PYFASTY_THREAD_IDLE = 0,
    PYFASTY_THREAD_RUNNING = 1,
    PYFASTY_THREAD_WAITING = 2,
    PYFASTY_THREAD_STOPPING = 3,
    PYFASTY_THREAD_STOPPED = 4,
    PYFASTY_THREAD_ERROR = 5
} PyFasty_ThreadState;

/* ÉNUMÉRATION POUR LES PRIORITÉS DE TÂCHES */
typedef enum {
    PYFASTY_PRIORITY_LOW = 0,
    PYFASTY_PRIORITY_NORMAL = 5,
    PYFASTY_PRIORITY_HIGH = 10
} PyFasty_TaskPriority;

/* FONCTIONS DE CONFIGURATION GLOBALE */

/* Configuration des limites système */
int PyFasty_SetThreadPoolMaxSize(int max_size);
int PyFasty_SetThreadPoolDefaultSize(int default_size);
int PyFasty_SetTaskQueueMaxSize(int queue_size);
int PyFasty_SetMaxNameLength(int name_length);

/* Obtenir les valeurs actuelles */
int PyFasty_GetThreadPoolMaxSize(void);
int PyFasty_GetThreadPoolDefaultSize(void);
int PyFasty_GetTaskQueueMaxSize(void);
int PyFasty_GetMaxNameLength(void);

/* FONCTIONS DE MUTEX OPTIMISÉES */

int PyFasty_MutexInit(PyFasty_Mutex *mutex);
int PyFasty_MutexInitNamed(PyFasty_Mutex *mutex, const char *name);
int PyFasty_MutexDestroy(PyFasty_Mutex *mutex);
int PyFasty_MutexLock(PyFasty_Mutex *mutex);
int PyFasty_MutexTryLock(PyFasty_Mutex *mutex);      /* NOUVEAU */
int PyFasty_MutexUnlock(PyFasty_Mutex *mutex);
int PyFasty_MutexTimedLock(PyFasty_Mutex *mutex, int timeout_ms); /* NOUVEAU */

/* FONCTIONS DE CONDITION OPTIMISÉES */

int PyFasty_CondInit(PyFasty_Cond *cond);
int PyFasty_CondDestroy(PyFasty_Cond *cond);
int PyFasty_CondWait(PyFasty_Cond *cond, PyFasty_Mutex *mutex);
int PyFasty_CondTimedWait(PyFasty_Cond *cond, PyFasty_Mutex *mutex, int timeout_ms); /* NOUVEAU */
int PyFasty_CondSignal(PyFasty_Cond *cond);
int PyFasty_CondBroadcast(PyFasty_Cond *cond);

/* FONCTIONS DE THREAD OPTIMISÉES */

int PyFasty_ThreadCreate(PyFasty_Thread *thread, void *(*start_routine)(void*), void *arg);
int PyFasty_ThreadCreateNamed(PyFasty_Thread *thread, void *(*start_routine)(void*), 
                             void *arg, const char *name); /* NOUVEAU */
int PyFasty_ThreadJoin(PyFasty_Thread *thread);
int PyFasty_ThreadJoinTimeout(PyFasty_Thread *thread, int timeout_ms); /* NOUVEAU */
int PyFasty_ThreadDetach(PyFasty_Thread *thread);         /* NOUVEAU */
int PyFasty_ThreadCancel(PyFasty_Thread *thread);         /* NOUVEAU */
void PyFasty_ThreadYield(void);                           /* NOUVEAU */
int PyFasty_ThreadGetId(void);                            /* NOUVEAU */

/* GESTION GLOBALE DU THREADING */

/* Initialisation et finalisation du système */
int PyFasty_ThreadingInit(void);
void PyFasty_ThreadingFinalize(void);

/* Configuration globale */
void PyFasty_ThreadingSetErrorHandler(PyFasty_ErrorHandlerFunc handler);
void PyFasty_ThreadingSetMonitor(PyFasty_ThreadMonitorFunc monitor);

/* GESTION DE POOL DE THREADS OPTIMISÉE - 100% DYNAMIQUE */

/* Création et destruction */
PyFasty_ThreadPool *PyFasty_ThreadPoolCreate(int num_threads);
PyFasty_ThreadPool *PyFasty_ThreadPoolCreateAdvanced(int min_threads, int max_threads, 
                                                     int auto_scale); /* NOUVEAU */
PyFasty_ThreadPool *PyFasty_ThreadPoolCreateCustom(int min_threads, int max_threads, 
                                                   int task_queue_limit, int auto_scale); /* NOUVEAU */
void PyFasty_ThreadPoolDestroy(PyFasty_ThreadPool *pool);

/* Gestion des tâches */
int PyFasty_ThreadPoolAddTask(PyFasty_ThreadPool *pool, 
                             PyFasty_ThreadTaskFunc func, 
                             void *arg,
                             PyObject *callback,
                             PyObject *error_callback);

int PyFasty_ThreadPoolAddTaskPriority(PyFasty_ThreadPool *pool, 
                                     PyFasty_ThreadTaskFunc func, 
                                     void *arg,
                                     PyObject *callback,
                                     PyObject *error_callback,
                                     int priority,
                                     const char *name); /* NOUVEAU */

/* Contrôle du pool */
void PyFasty_ThreadPoolWaitAll(PyFasty_ThreadPool *pool);
int PyFasty_ThreadPoolWaitAllTimeout(PyFasty_ThreadPool *pool, int timeout_ms); /* NOUVEAU */
void PyFasty_ThreadPoolPause(PyFasty_ThreadPool *pool);     /* NOUVEAU */
void PyFasty_ThreadPoolResume(PyFasty_ThreadPool *pool);    /* NOUVEAU */
void PyFasty_ThreadPoolClear(PyFasty_ThreadPool *pool);     /* NOUVEAU */

/* FONCTIONS DE MONITORING ET STATISTIQUES */

/* Statistiques du pool */
int PyFasty_ThreadPoolGetActiveThreads(PyFasty_ThreadPool *pool);
int PyFasty_ThreadPoolGetQueueSize(PyFasty_ThreadPool *pool);
int PyFasty_ThreadPoolGetTotalTasks(PyFasty_ThreadPool *pool);
double PyFasty_ThreadPoolGetAvgTaskDuration(PyFasty_ThreadPool *pool);
int PyFasty_ThreadPoolGetPeakQueueSize(PyFasty_ThreadPool *pool);

/* Configuration dynamique */
int PyFasty_ThreadPoolSetThreadCount(PyFasty_ThreadPool *pool, int count);
void PyFasty_ThreadPoolSetAutoScale(PyFasty_ThreadPool *pool, int enabled);
void PyFasty_ThreadPoolSetCallbacks(PyFasty_ThreadPool *pool, 
                                   PyFasty_ThreadMonitorFunc monitor,
                                   PyFasty_ErrorHandlerFunc error_handler);

/* GESTION DES TÂCHES PYTHON OPTIMISÉE */

/* Création et destruction */
PyFasty_PythonTask *PyFasty_PythonTaskCreate(PyObject *callable, 
                                           PyObject *args, 
                                           PyObject *kwargs);

PyFasty_PythonTask *PyFasty_PythonTaskCreateAdvanced(PyObject *callable, 
                                                    PyObject *args, 
                                                    PyObject *kwargs,
                                                    const char *description); /* NOUVEAU */

void PyFasty_PythonTaskDestroy(PyFasty_PythonTask *task);

/* Exécution */
void *PyFasty_PythonTaskExecute(void *arg);

/* État et résultats */
int PyFasty_PythonTaskIsCompleted(PyFasty_PythonTask *task);
PyObject *PyFasty_PythonTaskGetResult(PyFasty_PythonTask *task);
PyObject *PyFasty_PythonTaskGetError(PyFasty_PythonTask *task);
double PyFasty_PythonTaskGetDuration(PyFasty_PythonTask *task); /* NOUVEAU */

/* FONCTIONS UTILITAIRES */

/* Validation et vérification */
int PyFasty_IsCallable(PyObject *obj);
int PyFasty_IsValidThread(PyFasty_Thread *thread);
int PyFasty_IsValidPool(PyFasty_ThreadPool *pool);

/* Conversion et utilitaires */
const char *PyFasty_ThreadStateToString(PyFasty_ThreadState state);
const char *PyFasty_PriorityToString(PyFasty_TaskPriority priority);

/* Debugging et logging */
void PyFasty_ThreadingDumpStats(PyFasty_ThreadPool *pool);
void PyFasty_ThreadingSetDebugLevel(int level);

/* VARIABLES GLOBALES */

/* Pool de threads par défaut */
extern PyFasty_ThreadPool *g_default_thread_pool;

/* Configuration globale */
extern PyFasty_ErrorHandlerFunc g_global_error_handler;
extern PyFasty_ThreadMonitorFunc g_global_monitor;
extern int g_threading_debug_level;

/* Variables de configuration dynamique */
extern int g_pyfasty_thread_pool_max_size;
extern int g_pyfasty_thread_pool_default_size;
extern int g_pyfasty_task_queue_max_size;
extern int g_pyfasty_max_name_length;

#endif /* PYFASTY_THREADING_H */
