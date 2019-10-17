cmake_minimum_required(VERSION 3.12)

#
# Project
#
set(PROJECT_LIB capy_amqp_cpp)
project(${PROJECT_LIB} VERSION 0.5.4)

#
# Options
#
option(BUILD_TESTING "Enable creation of Eigen tests." OFF)

#
# Global setttings
#
set(VERSION_FILE "version.txt")
set(CMAKE_CXX_STANDARD 17)
# googletes, etc
set (EXTERNAL_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/external)
set (EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)
set (EXTERNAL_INCLUDE_DIR ${EXTERNAL_INSTALL_LOCATION}/include)

#
# Generators
#
file (WRITE "${VERSION_FILE}" "${PROJECT_NAME} ${PROJECT_VERSION}")

#
# Packages
#
find_package(PkgConfig QUIET)

if(NOT PKG_CONFIG_FOUND)
    message(FATAL_ERROR "pkg-config not found!" )
    message(FATAL_ERROR "sudo apt-get install pkg-config, or brew install pkg-config")
endif()

find_package (OpenSSL)
pkg_check_modules(libuv REQUIRED libuv)

#
# Programms
#
find_program(CCACHE_FOUND ccache)
find_program(MAKE_EXE NAMES gmake nmake make)


#
# CCache settings
#
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)


#
# Compiler settings
#
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

    message(STATUS "Using Clang" )

elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

    message(STATUS "Using gcc" )

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -W -Wall -Wextra -Wno-ignored-qualifiers -Wvariadic-macros -pthread -static-libgcc -static-libstdc++ ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

    if (${CMAKE_SYSTEM_NAME} STREQUAL Linux)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DX86_LINUX")
    endif()

elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")

    message( FATAL_ERROR "The project does not support Intel compiler" )

elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")

    message( FATAL_ERROR "The project does not support MSVC compiler" )

endif()

if (MSVC)
    add_definitions (/W4)
elseif (CMAKE_COMPILER_IS_GNUCXX)
    add_definitions (-Wall -pedantic)
endif ()

#
# Package manager
#
include(ExternalProject)

#
# External project dependencies
#
set(CAPY_WORKSPACE_DEPENDECIES)
set(CAPY_WORKSPACE_LIBRARIES ${libuv_LIBRARIES})

#
# Google Test
#
find_package(GTest)
if (NOT GTEST_FOUND)
    ExternalProject_Add(googletest
            GIT_REPOSITORY https://github.com/google/googletest
            CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
            )
    set(CAPY_WORKSPACE_DEPENDECIES googletest)
endif ()

#
# AMQP-CPP
#
pkg_check_modules(amqpcpp amqpcpp)
if (amqpcpp_FOUND)

    message(STATUS "amqpcpp: ${amqpcpp_LIBRARIES}, ${amqpcpp_LIBRARY_DIRS} ${amqpcpp_INCLUDE_DIRS}")

    include_directories(
            ${amqpcpp_INCLUDE_DIRS}
    )

    link_directories(
            ${amqpcpp_LIBRARY_DIRS}
    )

    set(CAPY_WORKSPACE_LIBRARIES ${CAPY_WORKSPACE_LIBRARIES};${amqpcpp_LIBRARIES})

else()

    ExternalProject_Add(AMQP_CPP
            GIT_REPOSITORY https://github.com/dnevera/AMQP-CPP
            CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
            -DAMQP-CPP_LINUX_TCP=ON
            -DAMQP-CPP_BUILD_SHARED=OFF
            -DAMQP-CPP_BUILD_EXAMPLES=OFF
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}
            )

    set(CAPY_WORKSPACE_DEPENDECIES ${CAPY_WORKSPACE_DEPENDECIES};AMQP_CPP)
    set(CAPY_WORKSPACE_LIBRARIES ${CAPY_WORKSPACE_LIBRARIES};amqpcpp)

endif()

#
# Capy threads pool dispatcher
#
pkg_check_modules(capy_dispatchq capy-dispatchq)

if (capy_dispatchq_FOUND)

    message(STATUS "capy-dispatchq: ${capy_dispatchq_LIBRARIES}, ${capy_dispatchq_LIBRARY_DIRS} ${capy_dispatchq_INCLUDE_DIRS}")

    include_directories(
            ${capy_dispatchq_INCLUDE_DIRS}
    )

    link_directories(
            ${capy_dispatchq_LIBRARY_DIRS}
    )

    set(CAPY_WORKSPACE_LIBRARIES ${CAPY_WORKSPACE_LIBRARIES};${capy_dispatchq_LIBRARIES})

else()

    ExternalProject_Add(capy-dispatchq
            GIT_REPOSITORY https://github.com/aithea/capy-dispatchq
            CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DBUILD_TESTING=OFF
            )

    set(CAPY_WORKSPACE_DEPENDECIES ${CAPY_WORKSPACE_DEPENDECIES};capy-dispatchq)
    set(CAPY_WORKSPACE_LIBRARIES ${CAPY_WORKSPACE_LIBRARIES};capy_dispatchq)

endif()

pkg_check_modules(capy_common_cpp capy-common-cpp)

if (capy_common_cpp_FOUND)

    message(STATUS "capy-common-cpp: ${capy_common_cpp_LIBRARIES}, ${capy_common_cpp_LIBRARY_DIRS} ${capy_common_cpp_INCLUDE_DIRS}")

    include_directories(
            ${capy_common_cpp_INCLUDE_DIRS}
    )

    link_directories(
            ${capy_common_cpp_LIBRARY_DIRS}
    )

    set(CAPY_WORKSPACE_LIBRARIES ${CAPY_WORKSPACE_LIBRARIES};${capy_common_cpp_LIBRARIES})

else()

    ExternalProject_Add(capy-common-cpp
            GIT_REPOSITORY https://github.com/aithea/capy-common-cpp
            CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DBUILD_TESTING=OFF
            )

    set(CAPY_WORKSPACE_DEPENDECIES ${CAPY_WORKSPACE_DEPENDECIES};capy-common-cpp)
    set(CAPY_WORKSPACE_LIBRARIES ${CAPY_WORKSPACE_LIBRARIES}; ${EXTERNAL_INSTALL_LOCATION}/lib/libcapy_common_cpp.a)

endif()

#
# Details
#
add_subdirectory(lib)
if(BUILD_TESTING)
    add_subdirectory(tests)
    enable_testing ()
endif()

#
# first we can indicate the documentation build as an option and set it to ON by default
#
option(BUILD_DOC "Build documentation" ON)

#
# check if Doxygen is installed
#
find_package(Doxygen)
if (DOXYGEN_FOUND)
    # set input and output files
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    # request to configure the file
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    message("Doxygen build started")

    # note the option ALL which allows to build the docs together with the application
    add_custom_target( doc_doxygen ALL
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM )
else (DOXYGEN_FOUND)
    message("Doxygen need to be installed to generate the doxygen documentation")
endif (DOXYGEN_FOUND)


# Install package
#
if (NOT CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR  lib)
endif ()
if (NOT CMAKE_INSTALL_INCLUDEDIR)
    set(CMAKE_INSTALL_INCLUDEDIR  include)
endif ()

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")

configure_file(${CMAKE_SOURCE_DIR}/capy-amqp-cpp.pc.in ${CMAKE_CURRENT_BINARY_DIR}/capy-amqp-cpp.pc @ONLY)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/capy-amqp-cpp.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
        )

        message(STATUS "CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS})