cmake_minimum_required(VERSION 3.18)
project(cereggii)

set(PY_BUILD_CMAKE_MODULE_NAME "cereggii")

include_directories("include/")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options("-march=native")

# Find the Python development files
set(python_base "$ENV{HOME}/.pyenv/versions/nogil-3.9.10-1")
message("CI=" $ENV{CI})
if ("$ENV{CI}" STREQUAL "true")
    set(python_base "$ENV{HOME}/nogil-3.9.10")
endif ()
set(Python3_INCLUDE_DIR "${python_base}/include/python3.9/")

find_package(Python3 REQUIRED COMPONENTS Development.Module)

# Add the module to compile
Python3_add_library(_cereggii MODULE
        "cereggii/cereggii.c"
        "cereggii/atomic_dict/atomic_dict_node_ops.c"
        "cereggii/atomic_dict/node_sizes_table.c"
        "cereggii/atomic_dict/atomic_dict.c"
)

# Install the module
install(TARGETS _cereggii
        EXCLUDE_FROM_ALL
        COMPONENT python_modules
        DESTINATION ${PY_BUILD_CMAKE_MODULE_NAME})

# Install stubs to get autocomplete and type hints
# install(FILES _add_module.pyi
#         EXCLUDE_FROM_ALL
#         COMPONENT python_modules
#         DESTINATION ${PY_BUILD_CMAKE_MODULE_NAME})
