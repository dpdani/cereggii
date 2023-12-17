cmake_minimum_required(VERSION 3.18)
project(cereggii)

set(PY_BUILD_CMAKE_MODULE_NAME "cereggii")

include_directories("include/")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options("-march=native")

# Find the Python development files
set(python_base "$ENV{HOME}/.pyenv/versions/nogil-3.9.10-1-debug")
message("CI=" $ENV{CI})
if ("$ENV{CI}" STREQUAL "true")
    set(python_base "$ENV{HOME}/nogil-3.9.10")
endif ()
set(Python3_INCLUDE_DIR "${python_base}/include/python3.9/")

find_package(Python3 REQUIRED COMPONENTS Development.Module)

# Add the module to compile
Python3_add_library(_cereggii MODULE
        "cereggii/atomic_dict/atomic_dict.c"
        "cereggii/atomic_dict/blocks.c"
        "cereggii/atomic_dict/delete.c"
        "cereggii/atomic_dict/insert.c"
        "cereggii/atomic_dict/lookup.c"
        "cereggii/atomic_dict/meta.c"
        "cereggii/atomic_dict/migrate.c"
        "cereggii/atomic_dict/node_ops.c"
        "cereggii/atomic_dict/node_sizes_table.c"
        "cereggii/atomic_dict/reservation_buffer.c"
        "cereggii/atomic_int/atomic_int.c"
        "cereggii/atomic_int/handle.c"
        "cereggii/atomic_ref.c"
        "cereggii/cereggii.c"
)

# Install the module
install(TARGETS _cereggii
        EXCLUDE_FROM_ALL
        COMPONENT python_modules
        DESTINATION ${PY_BUILD_CMAKE_MODULE_NAME})

# Install stubs to get autocomplete and type hints
install(FILES cereggii/_cereggii.pyi
        EXCLUDE_FROM_ALL
        COMPONENT python_modules
        DESTINATION ${PY_BUILD_CMAKE_MODULE_NAME})
