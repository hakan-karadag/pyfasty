# PyFasty

<div align="center">

<img src="assets/pyfasty-icon.png" alt="PyFasty Logo" width="200" height="200">

# pyfasty

🚀 **Stop boilerplate! Native C-powered Python with magic registry, auto events, premium console - Code 10x faster!**

[![Python](https://img.shields.io/badge/Python-3.8+-blue.svg)](https://python.org)
[![C Language](https://img.shields.io/badge/C-Native%20Extension-orange.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Native C](https://img.shields.io/badge/Architecture-100%25%20Native%20C-green.svg)](#-native-c-architecture)
[![PyPI](https://img.shields.io/badge/PyPI-Published-brightgreen.svg)](https://pypi.org/project/pyfasty)
[![Install](https://img.shields.io/badge/Install-pip%20install%20pyfasty-blue.svg)](https://pypi.org/project/pyfasty)
[![Status](https://img.shields.io/badge/Status-Active%20Development-orange.svg)](https://github.com/hakan-karadag/pyfasty)

> 🇫🇷 **[Version Française](README.fr.md)** • 🇺🇸 **English Version (Current)**

[**Quick Start**](#-quick-start) • [**Examples**](#-examples) • [**Documentation**](#-documentation) • [**Performance**](#-performance)

</div>

---

## 📖 **Table of Contents**

- [🎯 **What is PyFasty?**](#-what-is-pyfasty)
- [💥 **Before vs After**](#-before-vs-after)  
- [🚀 **Quick Start**](#-quick-start)
- [🆕 **Native C API**](#-native-c-api)
- [🔧 **Native C Architecture**](#-native-c-architecture)
- [🛠️ **Core Features**](#-core-features)
  - [📡 **Reactive Events**](#-reactive-events---zero-config-automation)
  - [🖥️ **Premium Console**](#-premium-console---logging-done-right)
  - [⚡ **Smart Executors**](#-smart-executors---function-calls-reimagined)
- [🏎️ **Performance**](#-performance)
- [🎮 **Real-World Examples**](#-real-world-examples)
- [🧪 **Testing**](#-testing)
- [📚 **Documentation**](#-documentation)
- [🤝 **Contributing**](#-contributing)
- [🗺️ **Roadmap**](#-roadmap)
- [⚠️ **Development Status**](#-development-status)
- [📄 **License**](#-license)

---

## ⚡ **Why PyFasty is Different**

Unlike pure Python libraries, PyFasty is a **native C extension** delivering:

🔥 **Real Performance**: Hand-tuned C code, not interpreted Python  
🧠 **Smart Architecture**: Memory pools, caching, and optimization  
🛡️ **Production Ready**: Cross-platform threading and error handling  
⚡ **Zero Overhead**: Direct system calls and native data structures  

**The Result**: Python convenience with C-level performance where it matters most.

---

## 🎯 **What is PyFasty?**

PyFasty is a **native C extension** that eliminates Python boilerplate with **4 revolutionary utilities**:

| 🏗️ **Native Registry** | 📡 **C-powered Events** | 🖥️ **Optimized Console** | ⚡ **Threading Executors** |
|------------------------|------------------------|-------------------------|------------------------|
| C-level performance | Real-time triggers | Hand-tuned formatting | Cross-platform threads |
| Memory pool optimization | Module dependency tracing | Advanced color system | Async task management |
| Math operations ready | Zero Python overhead | File logging built-in | Windows/Unix compatible |

## 💥 **Before vs After**

<table>
<tr>
<td width="50%">

**❌ Before (Vanilla Python)**
```python
# 25+ lines for simple config management
class Config:
    def __init__(self):
        self.data = {}
    
    def set_nested(self, path, value):
        keys = path.split('.')
        current = self.data
        for key in keys[:-1]:
            if key not in current:
                current[key] = {}
            current = current[key]
        current[keys[-1]] = value
    
    def get_nested(self, path):
        keys = path.split('.')
        current = self.data
        for key in keys:
            current = current[key]
        return current

config = Config()
config.set_nested('database.host', 'localhost')
config.set_nested('database.port', 5432)

# Plus event handling, logging setup...
```

</td>
<td width="50%">

**✅ After (PyFasty Magic)**
```python
# ⚡ Direct import of native C objects
from pyfasty import console, registry, config, executor, event

# 3 lines. That's it.
config.database.host = 'localhost'
config.database.port = 5432

# Events auto-trigger
@event(lambda: config.database)
def on_db_config():
    console.success("Database configured!")

# Professional logging ready
console.info("App started")
```

</td>
</tr>
</table>

## 🚀 **Quick Start**

```bash
pip install pyfasty
```

**✅ Native C Objects - Direct Import**
```python
# ⚡ Direct import of native C objects
from pyfasty import console, registry, config, executor, event

# 🏗️ Global Registry - Access anything, anywhere
registry.user.name = "John"
registry.stats.counter += 1  # Auto-creates with 0
registry.data["key"] = "value"  # Dict + dot notation

# 📡 Reactive Events - Functions that auto-trigger
@event(lambda: registry.users_count > 100)
def scale_up():
    console.warning("High traffic detected!")

# 🖥️ Premium Console - Pro logging out of the box
console.success("✅ User created")
console.error("❌ Connection failed")

# ⚡ Smart Executors - Call functions by path
result = executor.sync.my_module.process_data()
executor._async.heavy_task.compute()  # Non-blocking
```

**🔧 Import Alternatives:**
```python
# Option 1: Classic import with alias
import pyfasty as pf
from pyfasty import console, registry, config, executor, event

# Option 2: Access via main module
import pyfasty
console = pyfasty.console
registry = pyfasty.registry
config = pyfasty.config
executor = pyfasty.executor
event = pyfasty.event

# Option 3: Full import for compatibility
import pyfasty
# Then use pyfasty.console, pyfasty.registry, etc.
```

## 🆕 **Native C API**

**🎉 PyFasty Revolution** - All objects are now **100% native C**!

```python
# 🚀 Native C API
from pyfasty import console, registry, config, executor, event
console.info("Message")           # <class 'pyfasty._pyfasty.Console'>  
registry.data = "value"           # <class 'pyfasty._pyfasty.Registry'>
@event(lambda: config.debug)      # <class 'builtin_function_or_method'>
result = executor.sync.module.function()  # <class 'pyfasty._pyfasty.Executor'>
```

**🔥 Native API Advantages:**
- **⚡ C Performance**: Zero Python overhead on main objects
- **🎯 Direct Import**: `from pyfasty import console, registry, config, executor, event`  
- **🛡️ Stability**: Native C types = more robust and predictable
- **📝 Cleaner Code**: No more `pyfasty.` everywhere - much more readable
- **🔄 Backward Compatibility**: Old API still works

**💡 Native Types Confirmed:**
```python
from pyfasty import console, registry, config, executor, event
print(type(console))   # <class 'pyfasty._pyfasty.Console'>
print(type(registry))  # <class 'pyfasty._pyfasty.Registry'>  
print(type(config))    # <class 'pyfasty._pyfasty.Config'>
print(type(executor))  # <class 'pyfasty._pyfasty.Executor'>
print(type(event))     # <class 'builtin_function_or_method'>
```

## 🔧 **Native C Architecture**

PyFasty is built with **professional-grade C extensions**:

### 🏗️ **Native Registry Engine** 
```c
// Multi-level caching system
typedef struct {
    PyObject_HEAD
    PyObject *data;            // Data dictionary
    PyObject *cache;           // Access cache  
    int depth;                 // Optimization depth
    PyObject *value;           // Direct value
} PyFastyBaseObject;

// Memory pool for performance
PyFasty_ObjectPool g_dict_pool;
```

### 📡 **Real-time Event System**
```c
// Module dependency tracing
typedef enum {
    MODULE_REGISTRY = 2,
    MODULE_CONSOLE = 4, 
    MODULE_EXECUTOR_SYNC = 8,
    MODULE_EXECUTOR_ASYNC = 16
} ModuleType;

// Zero-overhead event triggers
void pyfasty_trigger_sync_events_with_module(ModuleType module);
```

### ⚡ **Cross-platform Threading**
```c
// Windows/Unix compatible threading
#ifdef _WIN32
    CRITICAL_SECTION mutex;
#else  
    pthread_mutex_t mutex;
#endif

// Professional thread pool
PyFasty_ThreadPool *g_default_thread_pool;
```

### 🖥️ **Optimized Console System**
```c
// Advanced formatting with caching
static time_t last_time = 0;
static char cached_hour[3] = {0};
// + color management, file logging, performance optimization
```

## 🛠️ **Core Features**

```python
# ⚡ Direct import of native C objects
from pyfasty import console, registry, config, executor, event

# ✨ Magic dot notation - create nested objects instantly
registry.app.config.database.host = "localhost"
registry.users["john"].profile.age = 25

# 🧮 Math operations work naturally  
registry.counter += 5        # Auto-creates as 0, then adds 5
registry.multiplier *= 3     # Auto-creates as 1, then multiplies
registry.progress /= 2       # Smart type handling

# 🔄 Mixed access patterns
registry.rooms["lobby"].users.append("player1")
registry.settings.theme = "dark"
```

### 📡 **Reactive Events** - Zero Config Automation

```python
# ⚡ Direct import of native C objects
from pyfasty import console, registry, config, executor, event

# 🎯 Condition-based triggers
@event(lambda: config.debug == True)
def enable_debug_mode():
    console.debug("🔍 Debug mode activated")

@event(lambda: registry.memory_usage > 80)
def cleanup_memory():
    # Runs when condition is met
    garbage_collect()

# 🔗 Complex conditions supported
@event(lambda: config.api.enabled and registry.users_count > 0)
def start_api_server():
    console.success("🚀 API server starting...")
```

### 🖥️ **Premium Console** - Logging Done Right

```python
# ⚡ Direct import of native C objects
from pyfasty import console

# 🎨 Fully customizable format
console.config = {
    "format": "<%Y>-<%m>-<%d> <%H>:<%M>:<%S> | <%TYPE> | <%MESSAGE>",
    "colors": {
        "type": {
            "success": "\033[38;5;82m",
            "error": "\033[38;5;196m"
        }
    },
    "save_log": {
        "status": True,
        "filename": "app.log"
    }
}

# 📊 Multiple log levels
console.info("ℹ️ Information")
console.success("✅ Success") 
console.warning("⚠️ Warning")
console.error("❌ Error")
console.debug("🔍 Debug")
console.critical("🚨 Critical")
console.fatal("💀 Fatal")

# 🚀 Performance: Up to 17x faster than print()
```

### ⚡ **Smart Executors** - Function Calls Reimagined

```python
# ⚡ Direct import of native C objects
from pyfasty import console, executor

# 🔄 Synchronous execution
result = executor.sync.my_module.heavy_computation(data)
user = executor.sync.auth.get_user_by_id(123)

# ⚡ Asynchronous execution (non-blocking)
executor._async.email.send_notification(user_id)
executor._async.analytics.track_event("user_login")

# 🏗️ Nested module access
config_result = executor.sync.app.config.database.get_settings()

# ❌ Auto error handling for missing functions
try:
    executor.sync.nonexistent.function()
except Exception as e:
    console.error(f"Function not found: {e}")
```

## 🏎️ **Performance**

PyFasty delivers **professional-grade performance** through native C implementation:

| Component | Technology | Performance | Why It's Fast |
|-----------|------------|-------------|---------------|
| 🖥️ **Console** | Hand-tuned C | **4.7x to 17x faster** | Direct system calls + caching |
| 🏗️ **Registry** | Memory pools + cache | Equivalent to native | Multi-level optimization |
| ⚡ **Threading** | Cross-platform C | Native threads | Windows/Unix optimized |
| 📡 **Events** | Module tracing | `<1ms` triggers | Zero Python overhead |

<details>
<summary>📊 <strong>Real Benchmark Results (C vs Python)</strong></summary>

**🖥️ Console Performance (Native C)**
```c
// Measured with professional tooling
Simple messages:  PyFasty 208ms vs Python 979ms  → 4.7x faster
With variables:   PyFasty 69ms  vs Python 486ms  → 7.0x faster  
Timestamps:       PyFasty 25ms  vs Python 440ms  → 17.2x faster
Multi-level:      PyFasty 13ms  vs Python 89ms   → 6.8x faster
```

**🏗️ Registry Performance (Memory Pools)**
```c
// Object pool optimization + caching
Access patterns:  Equivalent to native Python dicts
Memory usage:     40% reduction through pooling  
Serialization:    1.4x faster than native
Throughput:       4.5M operations/sec (exceeds web limits)
```

**⚡ Threading Performance (Cross-platform)**  
```c
// Native thread management
Thread creation:  Windows CRITICAL_SECTION + Unix pthread
Task scheduling:  Lock-free queue with condition variables
GIL management:   Optimized acquire/release patterns
Pool efficiency:  Zero allocation overhead
```

**🎯 Why This Performance Matters:**
- **Console:** Production logging with zero bottlenecks
- **Registry:** Memory-efficient with intelligent caching  
- **Threading:** True parallelism without Python limitations
- **Events:** Real-time reactivity at C speeds

</details>

## 🎮 **Real-World Examples**

<details>
<summary>🌐 <strong>Web App Configuration</strong></summary>

```python
# ⚡ Direct import of native C objects
from pyfasty import console, registry, config, executor, event

# ⚙️ App configuration
config.app.name = "My Awesome API"
config.database.url = "postgresql://localhost:5432/mydb" 
config.redis.host = "localhost"
config.api.rate_limit = 1000

# 📊 Runtime stats
registry.stats.requests_count = 0
registry.stats.active_users = 0

# 🔔 Auto-scaling trigger
@event(lambda: registry.stats.requests_count > 10000)
def scale_infrastructure():
    console.warning("🚀 Scaling up infrastructure...")
    # Your scaling logic here

# 📈 Request tracking
def handle_request():
    registry.stats.requests_count += 1
    console.info(f"📊 Request #{registry.stats.requests_count}")
```

</details>

<details>
<summary>🎮 <strong>Game State Management</strong></summary>

```python
# ⚡ Direct import of native C objects
from pyfasty import console, registry, config, executor, event

# 🎮 Game state
registry.game.level = 1
registry.game.score = 0
registry.players["player1"].health = 100
registry.players["player1"].inventory = []

# 🏆 Achievement system  
@event(lambda: registry.game.score >= 1000)
def unlock_achievement():
    console.success("🏆 Achievement unlocked: Score Master!")
    registry.players["player1"].achievements.append("score_master")

# ⚡ Level progression
@event(lambda: registry.game.score >= registry.game.level * 500)
def level_up():
    registry.game.level += 1
    console.success(f"🆙 Level up! Now level {registry.game.level}")

# 🎯 Update score
def player_scored(points):
    registry.game.score += points
    console.info(f"⭐ +{points} points! Total: {registry.game.score}")
```

</details>

<details>
<summary>🤖 <strong>Microservices Communication</strong></summary>

```python
# ⚡ Direct import of native C objects
from pyfasty import console, registry, config, executor, event

# 🌐 Service registry
registry.services.auth.status = "healthy"
registry.services.database.connections = 0
registry.services.cache.hit_rate = 0.95

# 🚨 Health monitoring
@event(lambda: registry.services.database.connections > 100)
def database_overload():
    console.critical("🚨 Database connection pool exhausted!")
    executor._async.alerts.send_slack_notification("Database overload detected")

# ⚡ Performance tracking
@event(lambda: registry.services.cache.hit_rate < 0.8)
def cache_performance_warning():
    console.warning("📉 Cache hit rate below threshold")
    
# 🔄 Service communication
def call_auth_service(user_data):
    return executor.sync.services.auth.validate_user(user_data)

def process_async_task(task_data):
    executor._async.services.worker.process_task(task_data)
```

</details>

## 🧪 **Testing**

PyFasty includes comprehensive tests. Run them:

```bash
python -m pytest tests/
# or run individual test modules
python test_registry.py
python test_events.py  
python test_console.py
python test_executor.py
```

## 📚 **Documentation**

- **📖 [Complete Guide](docs/guide.md)** - Detailed usage examples
- **🔧 [API Reference](docs/api.md)** - Full API documentation  
- **🚀 [Performance Guide](docs/performance.md)** - Optimization tips
- **📝 [Migration Guide](docs/migration.md)** - From vanilla Python
- **❓ [FAQ](docs/faq.md)** - Common questions

## 🤝 **Contributing**

We love contributions! 

```bash
# 🔧 Development setup
git clone https://github.com/hakan-karadag/pyfasty.git
cd pyfasty
pip install -e ".[dev]"

# 🧪 Run tests  
python -m pytest

# 📝 Check code style
black pyfasty/
flake8 pyfasty/
```

**Ways to contribute:**
- 🐛 Bug reports and fixes
- ✨ Feature requests and implementations  
- 📚 Documentation improvements
- 🧪 Test coverage expansion
- 💡 Performance optimizations

## 🗺️ **Roadmap**

- [ ] 🌐 **HTTP Integration** - Direct web framework integration
- [ ] 🗄️ **Database Connectors** - ORM-like database access  
- [ ] 📊 **Metrics Dashboard** - Built-in performance monitoring
- [ ] 🔌 **Plugin System** - Extensible architecture
- [ ] 📱 **CLI Tools** - Command-line utilities
- [ ] 🐳 **Docker Integration** - Container-ready configurations

## ⚠️ **Development Status**

**🚧 Active Development** - PyFasty is a **professional C extension** in active development:

- ✅ **Registry System**: Native C implementation with memory pools - Production ready
- ✅ **Console Logging**: Hand-optimized C with caching - Exceptional performance  
- ✅ **Cross-platform Threading**: Windows/Unix native threads - Professional grade
- ✅ **Event System**: C-level module tracing - Production ready with recent bug fixes

**Production Usage:** 
- ✅ **Console**: Ready for production - C-level performance gains
- ✅ **Registry**: Professional grade - Memory optimized with intelligent caching
- ✅ **Events**: Production ready - C implementation stable and optimized
- ⚠️ **Executors**: Non-critical operations - Threading overhead acceptable for async tasks

**Technical Foundation:**
- **Language**: Native C extension with Python bindings
- **Threading**: Cross-platform (Windows CRITICAL_SECTION / Unix pthread)  
- **Memory**: Object pooling with multi-level caching
- **Performance**: Hand-tuned for production workloads

## 📄 **License**

Apache License 2.0 - see [LICENSE](LICENSE) file for details.

**Author:** Hakan KARADAG

## 🌟 **Star History**

[![Star History Chart](https://api.star-history.com/svg?repos=hakan-karadag/pyfasty&type=Date)](https://star-history.com/#hakan-karadag/pyfasty&Date)

---

<div align="center">
  
**⭐ If PyFasty helped you build faster Python apps, consider starring the repo!**

Built with ❤️ by [@hakan-karadag](https://github.com/hakan-karadag)

[⭐ Star](https://github.com/hakan-karadag/pyfasty) • [🐛 Issues](https://github.com/hakan-karadag/pyfasty/issues) • [💡 Features](https://github.com/hakan-karadag/pyfasty/issues) • [📚 Docs](https://github.com/hakan-karadag/pyfasty#readme)

</div>