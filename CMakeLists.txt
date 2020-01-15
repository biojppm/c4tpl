cmake_minimum_required(VERSION 3.2)
project(c4tpl)

include(./ext/c4core/cmake/c4Project.cmake)
c4_declare_project(c4tpl)

set(C4TPL_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(C4TPL_EXT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extx)

set(src
    c4/tpl/c4tpl.natvis
    c4/tpl/common.hpp
    c4/tpl/engine.hpp
    c4/tpl/mgr.hpp
    c4/tpl/pool.hpp
    c4/tpl/rope.hpp
    c4/tpl/token_container.cpp
    c4/tpl/token_container.hpp
    c4/tpl/token.cpp
    c4/tpl/token.hpp
)

c4_require_subproject(c4core SUBDIRECTORY ${C4TPL_EXT_DIR}/c4core)
c4_require_subproject(rapidyaml SUBDIRECTORY ${C4TPL_EXT_DIR}/rapidyaml)

c4_add_library(c4tpl
    SOURCES ${src} SOURCE_ROOT ${C4TPL_SRC_DIR}
    LIBS ryml c4core
    INC_DIRS
       $<BUILD_INTERFACE:${C4TPL_SRC_DIR}> $<INSTALL_INTERFACE:include>
)

c4_install_target(c4tpl)
c4_install_exports()

if(C4TPL_BUILD_TESTS)
    enable_testing() # this must be done here (and not inside the test dir)
                     # so that the test targets are available at the top level
    add_subdirectory(test)
endif()

if(C4TPL_BUILD_BENCHMARKS)
    add_subdirectory(bm)
endif()
