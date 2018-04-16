cmake_minimum_required(VERSION 3.2)
project(c4tpl)

include(./cmake/c4Project.cmake)
c4_declare_project(c4tpl)

set(C4TPL_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(C4TPL_EXT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern)

add_subdirectory(src)

if(C4TPL_BUILD_TESTS)
    enable_testing() # this must be done here (and not inside the test dir)
                     # so that the test targets are available at the top level
    add_subdirectory(test)
endif()

if(C4TPL_BUILD_BENCHMARKS)
    add_subdirectory(bm)
endif()
