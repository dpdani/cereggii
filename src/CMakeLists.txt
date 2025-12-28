cmake_minimum_required(VERSION 3.18)
project(cereggii)

set(PY_BUILD_CMAKE_MODULE_NAME "cereggii")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR=${CMAKE_HOST_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_HOST_SYSTEM_NAME=${CMAKE_HOST_SYSTEM_NAME}")

if (CMAKE_SIZE_OF_VOID_P EQUAL 4)
    message(FATAL_ERROR "32-bit platforms are unsupported")
endif ()

if (NOT MSVC)
    add_compile_options("-Werror=implicit-function-declaration")
endif ()

if (DEFINED ENV{CEREGGII_TSAN})
    add_compile_options("-fsanitize=thread")
    add_link_options("-fsanitize=thread")
    add_compile_options("-g")
    add_compile_options("-fno-omit-frame-pointer")
endif ()

if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(arm|ARM|aarch64)")
    set(IS_ARM TRUE)
    message(STATUS "Detected ARM architecture")
else()
    set(IS_ARM FALSE)
endif()

if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    if (IS_ARM)
        add_compile_options("-march=armv8-a+crc")
    else ()
        add_compile_options("-mcx16")
        add_compile_options("-msse4.2")
    endif ()
    execute_process(COMMAND cat /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size OUTPUT_VARIABLE LEVEL1_DCACHE_LINESIZE)
elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if (NOT IS_ARM)
        add_compile_options("-msse4.2")
    endif ()
    execute_process(COMMAND sysctl -n hw.cachelinesize OUTPUT_VARIABLE LEVEL1_DCACHE_LINESIZE)
elseif (WIN32)
    if (NOT MSVC)
        message(FATAL_ERROR "Windows builds require MSVC compiler.")
    endif ()
    add_compile_options("/std:c11")
    add_compile_options("/experimental:c11atomics")
    if (NOT IS_ARM)
        add_compile_options("/arch:AVX2")
    endif ()
    set(LEVEL1_DCACHE_LINESIZE 64)
else ()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_HOST_SYSTEM_NAME}")
endif ()

configure_file(cereggiiconfig.h.in "${CMAKE_CURRENT_SOURCE_DIR}/include/internal/cereggiiconfig.h")

execute_process(COMMAND python -c "import sysconfig; print(sysconfig.get_path('include'), end='')" OUTPUT_VARIABLE Python3_INCLUDE_DIR)
if (WIN32)
    # don't know why, but removing this line causes an access violation exception on windows+freethreading
    # https://github.com/dpdani/cereggii/actions/runs/20556789700/job/59042260881?pr=104#step:9:11
    execute_process(COMMAND python -c "import sysconfig; from pathlib import Path; print(Path(sysconfig.get_config_var('LIBDIR'))/Path(sysconfig.get_config_var('LIBRARY')).with_suffix('.lib'), end='')" OUTPUT_VARIABLE Python3_LIBRARY)
endif ()
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
        "cereggii/atomic_event.c"
        "cereggii/atomic_int.c"
        "cereggii/atomic_ref.c"
        "cereggii/cereggii.c"
        "cereggii/constants.c"
        "cereggii/thread_handle.c"
)

# Install the module
install(TARGETS _cereggii
        EXCLUDE_FROM_ALL
        COMPONENT python_modules
        DESTINATION ${PY_BUILD_CMAKE_MODULE_NAME})

if (MSVC)
    install(FILES $<TARGET_PDB_FILE:_cereggii>
            EXCLUDE_FROM_ALL
            COMPONENT python_modules
            DESTINATION ${PY_BUILD_CMAKE_MODULE_NAME}
            OPTIONAL)
endif()

# Install stubs to get autocomplete and type hints
install(FILES cereggii/_cereggii.pyi
        EXCLUDE_FROM_ALL
        COMPONENT python_modules
        DESTINATION ${PY_BUILD_CMAKE_MODULE_NAME})
