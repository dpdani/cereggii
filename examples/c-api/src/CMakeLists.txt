cmake_minimum_required(VERSION 3.18)
project(c_api)

set(PY_BUILD_CMAKE_MODULE_NAME "c_api")

# find Python.h et al.
execute_process(COMMAND python -c "import sysconfig; print(sysconfig.get_path('include'), end='')" OUTPUT_VARIABLE Python3_INCLUDE_DIR)
find_package(Python3 REQUIRED COMPONENTS Development.Module)

# find cereggii/*.h
execute_process(COMMAND python -c "import cereggii; from pathlib import Path; print(Path(cereggii.__file__).parent, end='')" OUTPUT_VARIABLE CEREGGII_INSTALLATION_DIR)
message("CEREGGII_INSTALLATION_DIR=${CEREGGII_INSTALLATION_DIR}")

execute_process(COMMAND python -c "import cereggii; print(cereggii.__version__, end='')" OUTPUT_VARIABLE CEREGGII_VERSION)
message(${CEREGGII_VERSION})

set(CEREGGII_INCLUDE_DIR "${CEREGGII_INSTALLATION_DIR}/include")
if (NOT IS_DIRECTORY ${CEREGGII_INCLUDE_DIR})
    message(FATAL_ERROR "Module cereggii not found in ${CEREGGII_INCLUDE_DIR}. Please add it to the build-system.requires section of your pyproject.toml file.")
endif ()
set(CEREGGII_LIB "${CEREGGII_INSTALLATION_DIR}/libcereggii.${CEREGGII_VERSION}.dylib")
if (NOT EXISTS ${CEREGGII_LIB})
    message(FATAL_ERROR "cereggii shared object not found in ${CEREGGII_LIB}.")
endif ()

# add cereggii to the search path for #include
include_directories(${CEREGGII_INCLUDE_DIR})

link_libraries(${CEREGGII_LIB})


# Add the module to compile
Python3_add_library(_c_api MODULE
        "c_api/c_api.c"
)

# Install the module
install(TARGETS _c_api
        EXCLUDE_FROM_ALL
        COMPONENT python_modules
        DESTINATION ${PY_BUILD_CMAKE_MODULE_NAME})
