from setuptools import setup, Extension

pyfasty_module = Extension(
    'pyfasty._pyfasty',
    sources=[
        'src/pyfasty.c',
        'src/modules/pyfasty.registry.c',
        'src/modules/pyfasty.config.c',
        'src/modules/pyfasty.console.c',
        'src/modules/pyfasty.executor.c',
        'src/modules/pyfasty.event.c',
        'src/thread/pyfasty_threading.c',
        'src/proxy/pyfasty.executor_proxy.c',
    ],
    include_dirs=['src', 'src/thread', 'src/proxy'],
    define_macros=[
        ('PY_SSIZE_T_CLEAN', None),
    ],
)

setup(
    ext_modules=[pyfasty_module],
    zip_safe=False,
)
