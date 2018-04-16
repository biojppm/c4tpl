set(src
    c4/tpl/common.hpp
    c4/tpl/engine.hpp
    c4/tpl/mgr.hpp
    c4/tpl/rope.hpp
    c4/tpl/token_container.cpp
    c4/tpl/token_container.hpp
    c4/tpl/token.cpp
    c4/tpl/token.hpp
)

c4_require_module(c4tpl
    rapidyaml SUBDIRECTORY ${C4TPL_EXT_DIR}/rapidyaml)

c4_add_library(C4TPL c4tpl
    SOURCES ${src}
    LIBS ryml
    INC_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)
