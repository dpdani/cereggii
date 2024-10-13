cmake_minimum_required(VERSION 3.18)
project(cereggii)

set(PY_BUILD_CMAKE_MODULE_NAME "cereggii")


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options("-mcx16")
add_compile_options("-msse4.2")

execute_process(COMMAND python -c "import sysconfig; print(sysconfig.get_path('include'), end='')" OUTPUT_VARIABLE Python3_INCLUDE_DIR)
find_package(Python3 REQUIRED COMPONENTS Development.Module)


include_directories("include/")

# Add the module to compile
Python3_add_library(_cereggii MODULE
        "cereggii/atomic_dict/accessor_storage.c"
        "cereggii/atomic_dict/atomic_dict.c"
        "cereggii/atomic_dict/blocks.c"
        "cereggii/atomic_dict/delete.c"
        "cereggii/atomic_dict/insert.c"
        "cereggii/atomic_dict/iter.c"
        "cereggii/atomic_dict/lookup.c"
        "cereggii/atomic_dict/meta.c"
        "cereggii/atomic_dict/migrate.c"
        "cereggii/atomic_dict/node_ops.c"
        "cereggii/atomic_dict/node_sizes_table.c"
        "cereggii/atomic_dict/robin_hood.c"
        "cereggii/atomic_int/atomic_int.c"
        "cereggii/atomic_int/handle.c"
        "cereggii/atomic_event.c"
        "cereggii/atomic_ops.c"
        "cereggii/atomic_ref.c"
        "cereggii/cereggii.c"
        "cereggii/constants.c"
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
