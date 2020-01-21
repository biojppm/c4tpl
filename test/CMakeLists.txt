c4_setup_testing()

function(c4tpl_add_test name)
    c4_add_executable(c4tpl-test-${name}
        SOURCES ${ARGN} main.cpp
        INC_DIRS ${CMAKE_CURRENT_LIST_DIR}
        LIBS c4tpl gtest
        FOLDER test)
    c4_add_test(c4tpl-test-${name})
endfunction(c4tpl_add_test)

c4tpl_add_test(rope test_rope.cpp)
c4tpl_add_test(pool test_pool.cpp)
c4tpl_add_test(mgr test_mgr.cpp)
c4tpl_add_test(engine test_engine.cpp)
