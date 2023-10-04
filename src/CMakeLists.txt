cmake_minimum_required(VERSION 3.18)
project(cereggii)

set(PY_BUILD_CMAKE_MODULE_NAME "cereggii")

# Find the Python development files
set(Python3_INCLUDE_DIR "$ENV{HOME}/.pyenv/versions/nogil-3.9.10-1/include/python3.9/")
find_package(Python3 REQUIRED COMPONENTS Development.Module)

# Add the module to compile
Python3_add_library(_atomic_dict MODULE "cereggii/atomic_dict/atomic_dict.c")

# Install the module
install(TARGETS _atomic_dict
        EXCLUDE_FROM_ALL
        COMPONENT python_modules
        DESTINATION ${PY_BUILD_CMAKE_MODULE_NAME})

# Install stubs to get autocomplete and type hints
# install(FILES _add_module.pyi
#         EXCLUDE_FROM_ALL
#         COMPONENT python_modules
#         DESTINATION ${PY_BUILD_CMAKE_MODULE_NAME})
