from setuptools import setup, Extension, find_packages

# Lire le README pour la description longue
with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

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
    name='pyfasty',
    version='0.1.0b1',
    description='🚀 Native C-powered Python utilities: magic registry, auto events, premium console - Code 10x faster!',
    long_description=long_description,
    long_description_content_type="text/markdown",
    author='Hakan KARADAG',
    author_email='',
    url='https://github.com/hakan-karadag/pyfasty',
    project_urls={
        'Homepage': 'https://github.com/hakan-karadag/pyfasty',
        'Documentation': 'https://github.com/hakan-karadag/pyfasty#readme',
        'Repository': 'https://github.com/hakan-karadag/pyfasty',
        'Source': 'https://github.com/hakan-karadag/pyfasty',
        'Bug Reports': 'https://github.com/hakan-karadag/pyfasty/issues',
        'Bug Tracker': 'https://github.com/hakan-karadag/pyfasty/issues',
        'Changelog': 'https://github.com/hakan-karadag/pyfasty/releases',
    },
    ext_modules=[pyfasty_module],
    packages=find_packages(),
    python_requires='>=3.8',
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: Apache Software License",
        "Operating System :: OS Independent",
        "Programming Language :: C",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Topic :: Software Development :: Libraries :: Python Modules",
        "Topic :: Software Development :: Libraries",
        "Topic :: System :: Logging",
        "Topic :: Utilities",
        "Topic :: Internet :: WWW/HTTP :: Dynamic Content",
    ],
    keywords='python c-extension performance registry console logging events threading native fast utilities',
    license='Apache-2.0',
    zip_safe=False,
    include_package_data=True,
)
