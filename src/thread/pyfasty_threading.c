#include "pyfasty_threading.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __APPLE__
#include <unistd.h>
#endif

/* VARIABLES GLOBALES OPTIMISÉES */

/* Pool de threads par défaut */
PyFasty_ThreadPool *g_default_thread_pool = NULL;

/* Configuration globale */
PyFasty_ErrorHandlerFunc g_global_error_handler = NULL;
PyFasty_ThreadMonitorFunc g_global_monitor = NULL;
int g_threading_debug_level = 0;

/* Compteur global pour les IDs de tâches */
static int g_next_task_id = 1;
static PyFasty_Mutex g_task_id_mutex;

/* Verrou global pour protéger les opérations Python */
#ifdef PYFASTY_WINDOWS
static CRITICAL_SECTION gil_mutex;
#else
static pthread_mutex_t gil_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* VARIABLES DE CONFIGURATION DYNAMIQUE */
int g_pyfasty_thread_pool_max_size = PYFASTY_DEFAULT_THREAD_POOL_MAX_SIZE;
int g_pyfasty_thread_pool_default_size = PYFASTY_DEFAULT_THREAD_POOL_DEFAULT_SIZE;
int g_pyfasty_task_queue_max_size = PYFASTY_DEFAULT_TASK_QUEUE_MAX_SIZE;
int g_pyfasty_max_name_length = PYFASTY_DEFAULT_MAX_NAME_LENGTH;

/* FONCTIONS UTILITAIRES OPTIMISÉES */

/* Fonction pour obtenir le temps actuel */
static inline time_t get_current_time(void) {
    return time(NULL);
}

/* Fonction pour calculer la différence de temps en secondes */
static inline double time_diff_seconds(time_t start, time_t end) {
    return difftime(end, start);
}

/* Fonction pour obtenir un ID de tâche unique - cross-platform */
static int get_next_task_id(void) {
#ifdef PYFASTY_WINDOWS
    /* Windows utilise InterlockedIncrement */
    return InterlockedIncrement(&g_next_task_id);
#else
    /* Unix utilise __sync_fetch_and_add */
    return __sync_fetch_and_add(&g_next_task_id, 1);
#endif
}

/* Fonction pour appeler le gestionnaire d'erreurs global */
static void call_error_handler(const char* message, int code) {
    if (g_global_error_handler) {
        g_global_error_handler(message, code);
    }
}

/* Fonction pour appeler le monitor global */
static void call_monitor(int thread_id, const char* status) {
    if (g_global_monitor) {
        g_global_monitor(thread_id, status);
    }
}

/* IMPLÉMENTATION DES FONCTIONS DE MUTEX OPTIMISÉES */

int PyFasty_MutexInit(PyFasty_Mutex *mutex) {
    if (!mutex) return -1;
    
    memset(mutex, 0, sizeof(PyFasty_Mutex));
    
#ifdef PYFASTY_MINIMAL_THREADING
    /* MODE MINIMAL : Pas d'initialisation de mutex réel - mode dégradé */
    mutex->lock_count = 0;
    mutex->thread_id = 0;
    mutex->name = NULL; /* Pas d'allocation en mode minimal */
    mutex->name_allocated = 0;
    return 0; /* Simuler le succès */
#else
#ifdef PYFASTY_WINDOWS
    InitializeCriticalSection(&mutex->mutex);
    return 0;
#else
    int result = pthread_mutex_init(&mutex->mutex, NULL);
    if (result == 0) {
        mutex->lock_count = 0;
        mutex->thread_id = 0;
        mutex->name = NULL; /* Pas d'allocation par défaut */
        mutex->name_allocated = 0;
    }
    return result;
#endif
#endif
}

int PyFasty_MutexInitNamed(PyFasty_Mutex *mutex, const char *name) {
    int result = PyFasty_MutexInit(mutex);
    if (result == 0 && name) {
        strncpy(mutex->name, name, sizeof(mutex->name) - 1);
        mutex->name[sizeof(mutex->name) - 1] = '\0';
    }
    return result;
}

int PyFasty_MutexDestroy(PyFasty_Mutex *mutex) {
    if (!mutex) return -1;
    
#ifdef PYFASTY_WINDOWS
    DeleteCriticalSection(&mutex->mutex);
    return 0;
#else
    return pthread_mutex_destroy(&mutex->mutex);
#endif
}

int PyFasty_MutexLock(PyFasty_Mutex *mutex) {
    if (!mutex) return -1;
    
#ifdef PYFASTY_WINDOWS
    EnterCriticalSection(&mutex->mutex);
    mutex->lock_count++;
    mutex->thread_id = GetCurrentThreadId();
    return 0;
#else
    int result = pthread_mutex_lock(&mutex->mutex);
    if (result == 0) {
        mutex->lock_count++;
        mutex->thread_id = PyFasty_ThreadGetId();
    }
    return result;
#endif
}

int PyFasty_MutexTryLock(PyFasty_Mutex *mutex) {
    if (!mutex) return -1;
    
#ifdef PYFASTY_WINDOWS
    BOOL result = TryEnterCriticalSection(&mutex->mutex);
    if (result) {
        mutex->lock_count++;
        mutex->thread_id = GetCurrentThreadId();
        return 0;
    }
    return -1;
#else
    int result = pthread_mutex_trylock(&mutex->mutex);
    if (result == 0) {
        mutex->lock_count++;
        mutex->thread_id = PyFasty_ThreadGetId();
    }
    return result;
#endif
}

int PyFasty_MutexUnlock(PyFasty_Mutex *mutex) {
    if (!mutex) return -1;
    
#ifdef PYFASTY_WINDOWS
    mutex->lock_count--;
    mutex->thread_id = 0;
    LeaveCriticalSection(&mutex->mutex);
    return 0;
#else
    int result = pthread_mutex_unlock(&mutex->mutex);
    if (result == 0) {
        mutex->lock_count--;
        mutex->thread_id = 0;
    }
    return result;
#endif
}

int PyFasty_MutexTimedLock(PyFasty_Mutex *mutex, int timeout_ms) {
    if (!mutex) return -1;
    
#ifdef PYFASTY_WINDOWS
    /* Windows CRITICAL_SECTION n'a pas de timeout natif, utiliser trylock avec retry */
    int attempts = timeout_ms / 10; /* Essayer tous les 10ms */
    if (attempts <= 0) attempts = 1;
    
    for (int i = 0; i < attempts; i++) {
        BOOL result = TryEnterCriticalSection(&mutex->mutex);
        if (result) {
            mutex->lock_count++;
            mutex->thread_id = GetCurrentThreadId();
            return 0;
        }
        if (i < attempts - 1) {
            Sleep(10); /* Attendre 10ms */
        }
    }
    return -1;
#elif defined(__APPLE__)
    /* macOS n'a pas pthread_mutex_timedlock, utiliser trylock avec retry */
    int attempts = timeout_ms / 10; /* Essayer tous les 10ms */
    if (attempts <= 0) attempts = 1;
    
    for (int i = 0; i < attempts; i++) {
        int result = pthread_mutex_trylock(&mutex->mutex);
        if (result == 0) {
            mutex->lock_count++;
            mutex->thread_id = PyFasty_ThreadGetId();
            return 0;
        }
        if (i < attempts - 1) {
            usleep(10000); /* Attendre 10ms */
        }
    }
    return -1;
#else
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += timeout_ms / 1000;
    timeout.tv_nsec += (timeout_ms % 1000) * 1000000;
    
    int result = pthread_mutex_timedlock(&mutex->mutex, &timeout);
    if (result == 0) {
        mutex->lock_count++;
        mutex->thread_id = PyFasty_ThreadGetId();
    }
    return result;
#endif
}

/* IMPLÉMENTATION DES FONCTIONS DE CONDITION OPTIMISÉES */

int PyFasty_CondInit(PyFasty_Cond *cond) {
    if (!cond) return -1;
    
    memset(cond, 0, sizeof(PyFasty_Cond));
    
#ifdef PYFASTY_WINDOWS
    InitializeConditionVariable(&cond->cond);
    return 0;
#else
    int result = pthread_cond_init(&cond->cond, NULL);
    if (result == 0) {
        cond->wait_count = 0;
        cond->signal_count = 0;
    }
    return result;
#endif
}

int PyFasty_CondDestroy(PyFasty_Cond *cond) {
    if (!cond) return -1;
    
#ifdef PYFASTY_WINDOWS
    /* Windows n'a pas besoin de libérer les condition variables */
    return 0;
#else
    return pthread_cond_destroy(&cond->cond);
#endif
}

int PyFasty_CondWait(PyFasty_Cond *cond, PyFasty_Mutex *mutex) {
    if (!cond || !mutex) return -1;
    
    cond->wait_count++;
    
#ifdef PYFASTY_WINDOWS
    int result = !SleepConditionVariableCS(&cond->cond, &mutex->mutex, INFINITE);
#else
    int result = pthread_cond_wait(&cond->cond, &mutex->mutex);
#endif
    
    cond->wait_count--;
    return result;
}

int PyFasty_CondTimedWait(PyFasty_Cond *cond, PyFasty_Mutex *mutex, int timeout_ms) {
    if (!cond || !mutex) return -1;
    
    cond->wait_count++;
    
#ifdef PYFASTY_WINDOWS
    int result = !SleepConditionVariableCS(&cond->cond, &mutex->mutex, timeout_ms);
#else
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += timeout_ms / 1000;
    timeout.tv_nsec += (timeout_ms % 1000) * 1000000;
    
    int result = pthread_cond_timedwait(&cond->cond, &mutex->mutex, &timeout);
#endif
    
    cond->wait_count--;
    return result;
}

int PyFasty_CondSignal(PyFasty_Cond *cond) {
    if (!cond) return -1;
    
    cond->signal_count++;
    
#ifdef PYFASTY_WINDOWS
    WakeConditionVariable(&cond->cond);
    return 0;
#else
    return pthread_cond_signal(&cond->cond);
#endif
}

int PyFasty_CondBroadcast(PyFasty_Cond *cond) {
    if (!cond) return -1;
    
    cond->signal_count += cond->wait_count;
    
#ifdef PYFASTY_WINDOWS
    WakeAllConditionVariable(&cond->cond);
    return 0;
#else
    return pthread_cond_broadcast(&cond->cond);
#endif
}

/* IMPLÉMENTATION DES FONCTIONS DE THREAD OPTIMISÉES */

#ifdef PYFASTY_WINDOWS
/* Structure pour passer les arguments au thread Windows */
typedef struct {
    void *(*start_routine)(void*);
    void *arg;
    PyFasty_Thread *thread_info;
} ThreadData;

/* Fonction de thread Windows optimisée */
static unsigned __stdcall win_thread_func(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    void *(*start_routine)(void*) = data->start_routine;
    void *routine_arg = data->arg;
    PyFasty_Thread *thread_info = data->thread_info;
    
    if (thread_info) {
        thread_info->is_running = 1;
        thread_info->start_time = get_current_time();
        call_monitor(thread_info->thread_index, "STARTED");
    }
    
    free(data);
    
    void *result = start_routine(routine_arg);
    
    if (thread_info) {
        thread_info->is_running = 0;
        call_monitor(thread_info->thread_index, "FINISHED");
    }
    
    return (unsigned)(uintptr_t)result;
}
#endif

int PyFasty_ThreadCreate(PyFasty_Thread *thread, void *(*start_routine)(void*), void *arg) {
    return PyFasty_ThreadCreateNamed(thread, start_routine, arg, "worker");
}

int PyFasty_ThreadCreateNamed(PyFasty_Thread *thread, void *(*start_routine)(void*), 
                             void *arg, const char *name) {
    if (!thread || !start_routine) return -1;
    
    memset(thread, 0, sizeof(PyFasty_Thread));
    thread->is_running = 0;
    thread->tasks_completed = 0;
    
    if (name) {
        strncpy(thread->name, name, sizeof(thread->name) - 1);
        thread->name[sizeof(thread->name) - 1] = '\0';
    }
    
#ifdef PYFASTY_WINDOWS
    ThreadData *data = (ThreadData *)malloc(sizeof(ThreadData));
    if (!data) {
        return -1;
    }
    data->start_routine = start_routine;
    data->arg = arg;
    data->thread_info = thread;
    
    thread->handle = (HANDLE)_beginthreadex(NULL, 0, win_thread_func, data, 0, &thread->id);
    return (thread->handle == NULL) ? -1 : 0;
#else
    thread->is_running = 1;
    thread->start_time = get_current_time();
    
    int result = pthread_create(&thread->thread, NULL, start_routine, arg);
    if (result != 0) {
        thread->is_running = 0;
    }
    return result;
#endif
}

int PyFasty_ThreadJoin(PyFasty_Thread *thread) {
    if (!thread) return -1;
    
#ifdef PYFASTY_WINDOWS
    DWORD result = WaitForSingleObject(thread->handle, INFINITE);
    CloseHandle(thread->handle);
    return (result == WAIT_OBJECT_0) ? 0 : -1;
#else
    return pthread_join(thread->thread, NULL);
#endif
}

int PyFasty_ThreadJoinTimeout(PyFasty_Thread *thread, int timeout_ms) {
    if (!thread) return -1;
    
#ifdef PYFASTY_WINDOWS
    DWORD result = WaitForSingleObject(thread->handle, timeout_ms);
    if (result == WAIT_OBJECT_0) {
        CloseHandle(thread->handle);
        return 0;
    }
    return -1;
#else
    /* Linux n'a pas de pthread_timedjoin_np standard, utiliser une approche alternative */
    return PyFasty_ThreadJoin(thread); /* Fallback vers join normal */
#endif
}

int PyFasty_ThreadDetach(PyFasty_Thread *thread) {
    if (!thread) return -1;
    
#ifdef PYFASTY_WINDOWS
    CloseHandle(thread->handle);
    return 0;
#else
    return pthread_detach(thread->thread);
#endif
}

int PyFasty_ThreadCancel(PyFasty_Thread *thread) {
    if (!thread) return -1;
    
#ifdef PYFASTY_WINDOWS
    return TerminateThread(thread->handle, 0) ? 0 : -1;
#else
    return pthread_cancel(thread->thread);
#endif
}

void PyFasty_ThreadYield(void) {
#ifdef PYFASTY_WINDOWS
    SwitchToThread();
#else
    sched_yield();
#endif
}

int PyFasty_ThreadGetId(void) {
#ifdef PYFASTY_WINDOWS
    return GetCurrentThreadId();
#elif defined(__APPLE__)
    /* Sur macOS, pthread_t est un pointeur, on utilise mach_thread_self() */
    return (int)(uintptr_t)pthread_self() & 0x7FFFFFFF; /* Mask pour éviter les IDs négatifs */
#else
    return (int)pthread_self();
#endif
}

/* FONCTION DE TRAVAIL OPTIMISÉE POUR LES THREADS DU POOL */

static void *thread_worker(void *arg) {
    PyFasty_ThreadPool *pool = (PyFasty_ThreadPool *)arg;
    PyFasty_ThreadTask *task;
    PyGILState_STATE gstate;
    
    if (!pool) return NULL;
    
    /* Trouver l'index de ce thread dans le pool */
    int thread_index = -1;
    for (int i = 0; i < pool->thread_count; i++) {
        if (pool->threads[i].is_running) {
            thread_index = i;
            break;
        }
    }
    
    call_monitor(thread_index, "WORKER_STARTED");
    
    while (1) {
        /* Attendre une tâche */
        PyFasty_MutexLock(&pool->queue_mutex);
        
        while (pool->task_count == 0 && pool->running) {
            pool->threads[thread_index].is_running = 0; /* En attente */
            PyFasty_CondWait(&pool->queue_not_empty, &pool->queue_mutex);
            pool->threads[thread_index].is_running = 1; /* Réveillé */
        }
        
        /* Vérifier si le pool est en cours d'arrêt */
        if (!pool->running && pool->task_count == 0) {
            PyFasty_MutexUnlock(&pool->queue_mutex);
            break;
        }
        
        /* Récupérer une tâche */
        task = pool->tasks_head;
        if (task) {
            pool->tasks_head = task->next;
            if (pool->tasks_head == NULL) {
                pool->tasks_tail = NULL;
            }
            pool->task_count--;
            
            /* Signaler si la file est vide */
            if (pool->task_count == 0) {
                PyFasty_CondSignal(&pool->queue_empty);
            }
        }
        
        PyFasty_MutexUnlock(&pool->queue_mutex);
        
        if (task) {
            /* OPTIMISATION : Mise à jour des métadonnées de performance */
            task->start_time = get_current_time();
            call_monitor(thread_index, "EXECUTING_TASK");
            
            /* Acquérir le GIL si la tâche implique Python */
            if (task->callback || task->error_callback) {
                gstate = PyGILState_Ensure();
            }
            
            /* Exécuter la tâche */
            void *result = NULL;
            PyObject *py_result = NULL;
            int has_error = 0;
            
            if (task->func) {
                result = task->func(task->arg);
            }
            
            /* Mise à jour des statistiques */
            task->end_time = get_current_time();
            pool->threads[thread_index].tasks_completed++;
            pool->total_tasks_processed++;
            
            /* Calculer la durée moyenne des tâches */
            double task_duration = time_diff_seconds(task->start_time, task->end_time);
            pool->avg_task_duration = (pool->avg_task_duration * (pool->total_tasks_processed - 1) + 
                                     task_duration) / pool->total_tasks_processed;
            
            /* Appeler le callback si présent */
            if (task->callback && PyCallable_Check(task->callback)) {
                PyObject *arg = (result != NULL) ? PyLong_FromVoidPtr(result) : Py_None;
                if (arg != Py_None) {
                    py_result = PyObject_CallFunctionObjArgs(task->callback, arg, NULL);
                    if (py_result == NULL) {
                        has_error = 1;
                        PyErr_Print();
                    }
                    Py_DECREF(arg);
                } else {
                    Py_INCREF(Py_None);
                    py_result = PyObject_CallFunctionObjArgs(task->callback, Py_None, NULL);
                    if (py_result == NULL) {
                        has_error = 1;
                        PyErr_Print();
                    }
                }
                
                Py_XDECREF(py_result);
            }
            
            /* Gérer les erreurs */
            if (has_error && task->error_callback && PyCallable_Check(task->error_callback)) {
                PyObject *type, *value, *traceback;
                PyErr_Fetch(&type, &value, &traceback);
                PyErr_NormalizeException(&type, &value, &traceback);
                
                py_result = PyObject_CallFunctionObjArgs(task->error_callback, value, NULL);
                Py_XDECREF(py_result);
                
                Py_XDECREF(type);
                Py_XDECREF(value);
                Py_XDECREF(traceback);
            }
            
            /* Relâcher le GIL */
            if (task->callback || task->error_callback) {
                PyGILState_Release(gstate);
            }
            
            /* Libérer la tâche */
            Py_XDECREF(task->callback);
            Py_XDECREF(task->error_callback);
            free(task);
            
            call_monitor(thread_index, "TASK_COMPLETED");
        }
    }
    
    call_monitor(thread_index, "WORKER_STOPPED");
    return NULL;
}

/* FONCTIONS D'INITIALISATION OPTIMISÉES */

int PyFasty_ThreadingInit(void) {
#ifdef PYFASTY_MINIMAL_THREADING
    /* MODE MINIMAL LINUX/MACOS : Initialisation ultra-simple pour éviter segfaults */
    g_default_thread_pool = NULL;
    /* NE PAS initialiser de mutex complexes - mode dégradé */
    return 0; /* Succès en mode minimal */
#else
    /* MODE NORMAL WINDOWS : Initialisation complète */
    /* Initialiser le verrou global */
#ifdef PYFASTY_WINDOWS
    InitializeCriticalSection(&gil_mutex);
#endif

    /* Initialiser le mutex pour les IDs de tâches */
    if (PyFasty_MutexInitNamed(&g_task_id_mutex, "task_id") != 0) {
        return -1;
    }

    /* Initialiser le support des threads Python (deprecated mais nécessaire pour compatibilité) */
#if PY_VERSION_HEX < 0x030900A4
    PyEval_InitThreads();
#endif
    
    /* NE PAS créer le pool de threads maintenant - le reporter à la première utilisation */
    /* Cela évite les problèmes de threading pendant l'import du module */
    g_default_thread_pool = NULL;
    
    return 0; /* Succès */
#endif
}

void PyFasty_ThreadingFinalize(void) {
    if (g_default_thread_pool) {
        PyFasty_ThreadPoolDestroy(g_default_thread_pool);
        g_default_thread_pool = NULL;
    }
    
    /* Nettoyer le mutex des IDs de tâches */
    PyFasty_MutexDestroy(&g_task_id_mutex);
    
#ifdef PYFASTY_WINDOWS
    DeleteCriticalSection(&gil_mutex);
#endif

    /* Reset des variables globales */
    g_global_error_handler = NULL;
    g_global_monitor = NULL;
    g_threading_debug_level = 0;
}

/* FONCTIONS DE CONFIGURATION GLOBALE */

void PyFasty_ThreadingSetErrorHandler(PyFasty_ErrorHandlerFunc handler) {
    g_global_error_handler = handler;
}

void PyFasty_ThreadingSetMonitor(PyFasty_ThreadMonitorFunc monitor) {
    g_global_monitor = monitor;
}

/* FONCTIONS DE GESTION DE POOL OPTIMISÉES */

PyFasty_ThreadPool *PyFasty_ThreadPoolCreate(int num_threads) {
    return PyFasty_ThreadPoolCreateAdvanced(num_threads, num_threads, 0);
}

PyFasty_ThreadPool *PyFasty_ThreadPoolCreateAdvanced(int min_threads, int max_threads, int auto_scale) {
    if (min_threads <= 0 || max_threads <= 0 || min_threads > max_threads || 
        max_threads > g_pyfasty_thread_pool_max_size) {
        call_error_handler("Invalid thread pool parameters", -1);
        return NULL;
    }
    
    PyFasty_ThreadPool *pool = (PyFasty_ThreadPool *)malloc(sizeof(PyFasty_ThreadPool));
    if (!pool) {
        call_error_handler("Failed to allocate thread pool", -1);
        return NULL;
    }
    
    /* Initialiser les membres */
    memset(pool, 0, sizeof(PyFasty_ThreadPool));
    pool->thread_count = min_threads;
    pool->min_threads = min_threads;
    pool->max_threads = max_threads;
    pool->auto_scale = auto_scale;
    pool->running = 1;
    pool->pool_start_time = get_current_time();
    
    PyFasty_MutexInitNamed(&pool->queue_mutex, "pool_queue");
    PyFasty_CondInit(&pool->queue_not_empty);
    PyFasty_CondInit(&pool->queue_empty);
    
    /* Créer les threads */
    for (int i = 0; i < min_threads; i++) {
        pool->threads[i].thread_index = i;
        char thread_name[32];
        snprintf(thread_name, sizeof(thread_name), "worker-%d", i);
        
        if (PyFasty_ThreadCreateNamed(&pool->threads[i], thread_worker, pool, thread_name) != 0) {
            /* Erreur: détruire le pool et retourner NULL */
            pool->thread_count = i; /* Nombre de threads créés avec succès */
            PyFasty_ThreadPoolDestroy(pool);
            call_error_handler("Failed to create worker thread", i);
            return NULL;
        }
        pool->active_threads++;
    }
    
    return pool;
}

void PyFasty_ThreadPoolDestroy(PyFasty_ThreadPool *pool) {
    if (!pool) {
        return;
    }
    
    call_monitor(-1, "POOL_DESTROYING");
    
    /* Marquer comme en cours d'arrêt */
    PyFasty_MutexLock(&pool->queue_mutex);
    pool->running = 0;
    PyFasty_CondBroadcast(&pool->queue_not_empty);
    PyFasty_MutexUnlock(&pool->queue_mutex);
    
    /* Attendre que tous les threads se terminent */
    for (int i = 0; i < pool->thread_count; i++) {
        PyFasty_ThreadJoin(&pool->threads[i]);
    }
    
    /* Libérer les tâches restantes */
    PyFasty_ThreadTask *task = pool->tasks_head;
    while (task) {
        PyFasty_ThreadTask *next = task->next;
        Py_XDECREF(task->callback);
        Py_XDECREF(task->error_callback);
        free(task);
        task = next;
    }
    
    /* Détruire les mutex et conditions */
    PyFasty_MutexDestroy(&pool->queue_mutex);
    PyFasty_CondDestroy(&pool->queue_not_empty);
    PyFasty_CondDestroy(&pool->queue_empty);
    
    call_monitor(-1, "POOL_DESTROYED");
    
    /* Libérer le pool */
    free(pool);
}

/* AJOUT DE TÂCHE OPTIMISÉ */

int PyFasty_ThreadPoolAddTask(PyFasty_ThreadPool *pool, 
                             PyFasty_ThreadTaskFunc func, 
                             void *arg,
                             PyObject *callback,
                             PyObject *error_callback) {
    return PyFasty_ThreadPoolAddTaskPriority(pool, func, arg, callback, error_callback, 
                                            PYFASTY_PRIORITY_NORMAL, "task");
}

int PyFasty_ThreadPoolAddTaskPriority(PyFasty_ThreadPool *pool, 
                                     PyFasty_ThreadTaskFunc func, 
                                     void *arg,
                                     PyObject *callback,
                                     PyObject *error_callback,
                                     int priority,
                                     const char *name) {
    if (!pool || !func) {
        return 0;
    }
    
    /* Vérifier la limite de la file d'attente */
    if (pool->task_count >= g_pyfasty_task_queue_max_size) {
        call_error_handler("Task queue is full", pool->task_count);
        return 0;
    }
    
    /* Préparer la tâche hors de la section critique */
    PyFasty_ThreadTask *task = (PyFasty_ThreadTask *)malloc(sizeof(PyFasty_ThreadTask));
    if (!task) {
        call_error_handler("Failed to allocate task", -1);
        return 0;
    }
    
    memset(task, 0, sizeof(PyFasty_ThreadTask));
    task->func = func;
    task->arg = arg;
    task->next = NULL;
    task->priority = priority;
    task->created_time = get_current_time();
    task->retry_count = 0;
    
    if (name) {
        strncpy(task->name, name, sizeof(task->name) - 1);
        task->name[sizeof(task->name) - 1] = '\0';
    }
    
    /* Références aux callbacks Python */
    if (callback) {
        Py_INCREF(callback);
        task->callback = callback;
    } else {
        task->callback = NULL;
    }
    
    if (error_callback) {
        Py_INCREF(error_callback);
        task->error_callback = error_callback;
    } else {
        task->error_callback = NULL;
    }
    
    /* Section critique minimale pour l'insertion */
    PyFasty_MutexLock(&pool->queue_mutex);
    
    /* Insertion avec priorité (simple: haute priorité en tête) */
    if (priority >= PYFASTY_PRIORITY_HIGH && pool->tasks_head) {
        /* Insérer en tête pour haute priorité */
        task->next = pool->tasks_head;
        pool->tasks_head = task;
        if (pool->tasks_tail == NULL) {
            pool->tasks_tail = task;
        }
    } else {
        /* Ajouter à la fin pour priorité normale ou basse */
        if (pool->tasks_head == NULL) {
            pool->tasks_head = task;
            pool->tasks_tail = task;
        } else {
            pool->tasks_tail->next = task;
            pool->tasks_tail = task;
        }
    }
    
    pool->task_count++;
    
    /* Mise à jour des statistiques */
    if (pool->task_count > pool->max_task_count) {
        pool->max_task_count = pool->task_count;
    }
    
    PyFasty_CondSignal(&pool->queue_not_empty);
    PyFasty_MutexUnlock(&pool->queue_mutex);
    
    return 1;
}

/* FONCTIONS DE CONTRÔLE DU POOL */

void PyFasty_ThreadPoolWaitAll(PyFasty_ThreadPool *pool) {
    if (!pool) {
        return;
    }
    
    PyFasty_MutexLock(&pool->queue_mutex);
    
    while (pool->task_count > 0) {
        PyFasty_CondWait(&pool->queue_empty, &pool->queue_mutex);
    }
    
    PyFasty_MutexUnlock(&pool->queue_mutex);
}

int PyFasty_ThreadPoolWaitAllTimeout(PyFasty_ThreadPool *pool, int timeout_ms) {
    if (!pool) {
        return -1;
    }
    
    PyFasty_MutexLock(&pool->queue_mutex);
    
    int result = 0;
    while (pool->task_count > 0) {
        if (PyFasty_CondTimedWait(&pool->queue_empty, &pool->queue_mutex, timeout_ms) != 0) {
            result = -1; /* Timeout */
            break;
        }
    }
    
    PyFasty_MutexUnlock(&pool->queue_mutex);
    return result;
}

void PyFasty_ThreadPoolPause(PyFasty_ThreadPool *pool) {
    if (pool) {
        PyFasty_MutexLock(&pool->queue_mutex);
        pool->running = 0;
        PyFasty_MutexUnlock(&pool->queue_mutex);
    }
}

void PyFasty_ThreadPoolResume(PyFasty_ThreadPool *pool) {
    if (pool) {
        PyFasty_MutexLock(&pool->queue_mutex);
        pool->running = 1;
        PyFasty_CondBroadcast(&pool->queue_not_empty);
        PyFasty_MutexUnlock(&pool->queue_mutex);
    }
}

void PyFasty_ThreadPoolClear(PyFasty_ThreadPool *pool) {
    if (!pool) return;
    
    PyFasty_MutexLock(&pool->queue_mutex);
    
    /* Libérer toutes les tâches en attente */
    PyFasty_ThreadTask *task = pool->tasks_head;
    while (task) {
        PyFasty_ThreadTask *next = task->next;
        Py_XDECREF(task->callback);
        Py_XDECREF(task->error_callback);
        free(task);
        task = next;
    }
    
    pool->tasks_head = NULL;
    pool->tasks_tail = NULL;
    pool->task_count = 0;
    
    PyFasty_CondSignal(&pool->queue_empty);
    PyFasty_MutexUnlock(&pool->queue_mutex);
}

/* FONCTIONS DE MONITORING ET STATISTIQUES */

int PyFasty_ThreadPoolGetActiveThreads(PyFasty_ThreadPool *pool) {
    return pool ? pool->active_threads : 0;
}

int PyFasty_ThreadPoolGetQueueSize(PyFasty_ThreadPool *pool) {
    return pool ? pool->task_count : 0;
}

int PyFasty_ThreadPoolGetTotalTasks(PyFasty_ThreadPool *pool) {
    return pool ? pool->total_tasks_processed : 0;
}

double PyFasty_ThreadPoolGetAvgTaskDuration(PyFasty_ThreadPool *pool) {
    return pool ? pool->avg_task_duration : 0.0;
}

int PyFasty_ThreadPoolGetPeakQueueSize(PyFasty_ThreadPool *pool) {
    return pool ? pool->max_task_count : 0;
}

/* CONFIGURATION DYNAMIQUE DU POOL */

int PyFasty_ThreadPoolSetThreadCount(PyFasty_ThreadPool *pool, int count) {
    if (!pool || count <= 0 || count > g_pyfasty_thread_pool_max_size) {
        return -1;
    }
    
    /* Pour l'instant, retourner une erreur car la redimensionnement dynamique est complexe */
    call_error_handler("Dynamic thread count change not implemented yet", count);
    return -1;
}

void PyFasty_ThreadPoolSetAutoScale(PyFasty_ThreadPool *pool, int enabled) {
    if (pool) {
        pool->auto_scale = enabled;
    }
}

void PyFasty_ThreadPoolSetCallbacks(PyFasty_ThreadPool *pool, 
                                   PyFasty_ThreadMonitorFunc monitor,
                                   PyFasty_ErrorHandlerFunc error_handler) {
    if (pool) {
        pool->monitor_callback = monitor;
        pool->error_handler = error_handler;
    }
}

/* GESTION DES TÂCHES PYTHON OPTIMISÉE */

PyFasty_PythonTask *PyFasty_PythonTaskCreate(PyObject *callable, 
                                           PyObject *args, 
                                           PyObject *kwargs) {
    return PyFasty_PythonTaskCreateAdvanced(callable, args, kwargs, "python_task");
}

PyFasty_PythonTask *PyFasty_PythonTaskCreateAdvanced(PyObject *callable, 
                                                    PyObject *args, 
                                                    PyObject *kwargs,
                                                    const char *description) {
    if (!callable || !PyCallable_Check(callable)) {
        return NULL;
    }
    
    PyFasty_PythonTask *task = (PyFasty_PythonTask *)malloc(sizeof(PyFasty_PythonTask));
    if (!task) {
        return NULL;
    }
    
    memset(task, 0, sizeof(PyFasty_PythonTask));
    
    /* Initialiser les membres */
    Py_INCREF(callable);
    task->callable = callable;
    
    if (args) {
        Py_INCREF(args);
        task->args = args;
    } else {
        task->args = PyTuple_New(0);
    }
    
    if (kwargs) {
        Py_INCREF(kwargs);
        task->kwargs = kwargs;
    } else {
        task->kwargs = NULL;
    }
    
    task->result = NULL;
    task->error = NULL;
    task->completed = 0;
    task->task_id = get_next_task_id();
    task->created_time = get_current_time();
    task->thread_id = PyFasty_ThreadGetId();
    
    if (description) {
        strncpy(task->description, description, sizeof(task->description) - 1);
        task->description[sizeof(task->description) - 1] = '\0';
    }
    
    PyFasty_MutexInitNamed(&task->mutex, "python_task");
    
    return task;
}

void PyFasty_PythonTaskDestroy(PyFasty_PythonTask *task) {
    if (!task) {
        return;
    }
    
    Py_XDECREF(task->callable);
    Py_XDECREF(task->args);
    Py_XDECREF(task->kwargs);
    Py_XDECREF(task->result);
    Py_XDECREF(task->error);
    
    PyFasty_MutexDestroy(&task->mutex);
    
    free(task);
}

void *PyFasty_PythonTaskExecute(void *arg) {
    PyFasty_PythonTask *task = (PyFasty_PythonTask *)arg;
    if (!task) {
        return NULL;
    }
    
    /* Acquérir le GIL */
    PyGILState_STATE gstate = PyGILState_Ensure();
    
    /* Marquer le début d'exécution */
    PyFasty_MutexLock(&task->mutex);
    task->start_time = get_current_time();
    task->thread_id = PyFasty_ThreadGetId();
    PyFasty_MutexUnlock(&task->mutex);
    
    /* Appeler la fonction Python */
    PyObject *result = NULL;
    
    if (task->kwargs) {
        result = PyObject_Call(task->callable, task->args, task->kwargs);
    } else {
        result = PyObject_CallObject(task->callable, task->args);
    }
    
    PyFasty_MutexLock(&task->mutex);
    
    task->end_time = get_current_time();
    
    if (result) {
        task->result = result;
        task->error = NULL;
    } else {
        task->result = NULL;
        
        /* Récupérer l'exception */
        PyObject *type, *value, *traceback;
        PyErr_Fetch(&type, &value, &traceback);
        PyErr_NormalizeException(&type, &value, &traceback);
        
        task->error = value;
        Py_XDECREF(type);
        Py_XDECREF(traceback);
    }
    
    task->completed = 1;
    
    PyFasty_MutexUnlock(&task->mutex);
    
    /* Relâcher le GIL */
    PyGILState_Release(gstate);
    
    return task;
}

/* FONCTIONS D'ÉTAT ET DE RÉSULTATS DES TÂCHES PYTHON */

int PyFasty_PythonTaskIsCompleted(PyFasty_PythonTask *task) {
    if (!task) return 0;
    
    PyFasty_MutexLock(&task->mutex);
    int completed = task->completed;
    PyFasty_MutexUnlock(&task->mutex);
    
    return completed;
}

PyObject *PyFasty_PythonTaskGetResult(PyFasty_PythonTask *task) {
    if (!task) return NULL;
    
    PyFasty_MutexLock(&task->mutex);
    PyObject *result = task->result;
    if (result) {
        Py_INCREF(result);
    }
    PyFasty_MutexUnlock(&task->mutex);
    
    return result;
}

PyObject *PyFasty_PythonTaskGetError(PyFasty_PythonTask *task) {
    if (!task) return NULL;
    
    PyFasty_MutexLock(&task->mutex);
    PyObject *error = task->error;
    if (error) {
        Py_INCREF(error);
    }
    PyFasty_MutexUnlock(&task->mutex);
    
    return error;
}

double PyFasty_PythonTaskGetDuration(PyFasty_PythonTask *task) {
    if (!task) return 0.0;
    
    PyFasty_MutexLock(&task->mutex);
    double duration = 0.0;
    if (task->completed && task->start_time > 0 && task->end_time > 0) {
        duration = time_diff_seconds(task->start_time, task->end_time);
    }
    PyFasty_MutexUnlock(&task->mutex);
    
    return duration;
}

/* FONCTIONS UTILITAIRES */

int PyFasty_IsCallable(PyObject *obj) {
    return (obj && PyCallable_Check(obj));
}

int PyFasty_IsValidThread(PyFasty_Thread *thread) {
    return (thread && thread->is_running);
}

int PyFasty_IsValidPool(PyFasty_ThreadPool *pool) {
    return (pool && pool->running);
}

const char *PyFasty_ThreadStateToString(PyFasty_ThreadState state) {
    switch (state) {
        case PYFASTY_THREAD_IDLE: return "IDLE";
        case PYFASTY_THREAD_RUNNING: return "RUNNING";
        case PYFASTY_THREAD_WAITING: return "WAITING";
        case PYFASTY_THREAD_STOPPING: return "STOPPING";
        case PYFASTY_THREAD_STOPPED: return "STOPPED";
        case PYFASTY_THREAD_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

const char *PyFasty_PriorityToString(PyFasty_TaskPriority priority) {
    switch (priority) {
        case PYFASTY_PRIORITY_LOW: return "LOW";
        case PYFASTY_PRIORITY_NORMAL: return "NORMAL";
        case PYFASTY_PRIORITY_HIGH: return "HIGH";
        default: return "UNKNOWN";
    }
}

void PyFasty_ThreadingDumpStats(PyFasty_ThreadPool *pool) {
    if (!pool) return;
    
    printf("=== PyFasty Thread Pool Statistics ===\n");
    printf("Pool start time: %ld\n", (long)pool->pool_start_time);
    printf("Active threads: %d/%d\n", pool->active_threads, pool->thread_count);
    printf("Current queue size: %d\n", pool->task_count);
    printf("Peak queue size: %d\n", pool->max_task_count);
    printf("Total tasks processed: %d\n", pool->total_tasks_processed);
    printf("Average task duration: %.3f seconds\n", pool->avg_task_duration);
    printf("Auto-scale enabled: %s\n", pool->auto_scale ? "YES" : "NO");
    printf("Thread range: %d - %d\n", pool->min_threads, pool->max_threads);
    printf("=======================================\n");
}

void PyFasty_ThreadingSetDebugLevel(int level) {
    g_threading_debug_level = level;
}

/* FONCTIONS DE CONFIGURATION GLOBALE */

int PyFasty_SetThreadPoolMaxSize(int max_size) {
    if (max_size > 0 && max_size <= 1000) {
        g_pyfasty_thread_pool_max_size = max_size;
        return 0;
    }
    return -1;
}

int PyFasty_SetThreadPoolDefaultSize(int default_size) {
    if (default_size > 0 && default_size <= g_pyfasty_thread_pool_max_size) {
        g_pyfasty_thread_pool_default_size = default_size;
        return 0;
    }
    return -1;
}

int PyFasty_SetTaskQueueMaxSize(int queue_size) {
    if (queue_size > 0 && queue_size <= 100000) {
        g_pyfasty_task_queue_max_size = queue_size;
        return 0;
    }
    return -1;
}

int PyFasty_SetMaxNameLength(int name_length) {
    if (name_length > 0 && name_length <= 1000) {
        g_pyfasty_max_name_length = name_length;
        return 0;
    }
    return -1;
}

int PyFasty_GetThreadPoolMaxSize(void) {
    return g_pyfasty_thread_pool_max_size;
}

int PyFasty_GetThreadPoolDefaultSize(void) {
    return g_pyfasty_thread_pool_default_size;
}

int PyFasty_GetTaskQueueMaxSize(void) {
    return g_pyfasty_task_queue_max_size;
}

int PyFasty_GetMaxNameLength(void) {
    return g_pyfasty_max_name_length;
}
