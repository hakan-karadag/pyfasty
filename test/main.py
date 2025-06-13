"""
PyFasty Basic Usage Example

This demonstrates the core features of PyFasty: registry and config containers.
"""

import pyfasty as pf
from pyfasty import console, registry, config, executor, event

def main():
    registry.test_as = "test"
    console(registry.test_as)
    pf.registry.test_as_2 = "test2"
    console(pf.registry.test_as_2)

    from test_registry import class_test_registry
    class_test_registry.registry_test_pyfasty()

    from test_config import class_test_config
    class_test_config.config_test_pyfasty()

    from test_console import class_test_console
    class_test_console.console_test_pyfasty()

    from test_executor import class_test_executor
    class_test_executor.executor_test_pyfasty()
    
    from test_event import class_test_event
    class_test_event.event_test_pyfasty()

if __name__ == "__main__":
    main()