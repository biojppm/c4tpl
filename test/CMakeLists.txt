c4_setup_testing(C4TPL ON)

function(c4tpl_add_test name)
    c4_add_executable(C4TPL c4tpl-test-${name}
        SOURCES ${ARGN} main.cpp
        INC_DIRS ${CMAKE_CURRENT_LIST_DIR}
        LIBS c4tpl gtest
        FOLDER c4tpl-test)
    c4_add_test(C4TPL c4tpl-test-${name} ON)
endfunction(c4tpl_add_test)

c4tpl_add_test(rope test_rope.cpp)
c4tpl_add_test(pool test_pool.cpp)
c4tpl_add_test(mgr test_mgr.cpp)
c4tpl_add_test(engine test_engine.cpp)
