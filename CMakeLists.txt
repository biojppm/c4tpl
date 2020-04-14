cmake_minimum_required(VERSION 3.2)
project(c4tpl)

include(./ext/c4core/cmake/c4Project.cmake)
c4_declare_project(c4tpl)

c4_require_subproject(c4core SUBDIRECTORY ${C4TPL_EXT_DIR}/c4core)
c4_require_subproject(ryml   SUBDIRECTORY ${C4TPL_EXT_DIR}/rapidyaml)

c4_add_library(c4tpl
    SOURCE_ROOT ${C4TPL_SRC_DIR}
    SOURCES
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
    LIBS ryml c4core
    INC_DIRS
       $<BUILD_INTERFACE:${C4TPL_SRC_DIR}> $<INSTALL_INTERFACE:include>
)

c4_install_target(c4tpl)
c4_install_exports(DEPENDENCIES ryml c4core)

c4_add_dev_targets()
