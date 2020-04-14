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

c4_add_install_include_test(c4tpl "c4tpl::")
c4_add_install_link_test(c4tpl "c4tpl::" "
#include <c4/tpl/engine.hpp>
#include <c4/yml/parse.hpp>
#include <vector>
#include <initializer_list>

using csubstr = c4::csubstr;

struct tpl_results { const char* name; csubstr props_yml, result; };
using tpl_cases = std::initializer_list<tpl_results>;

void do_engine_test(csubstr tpl, tpl_cases cases)
{
    using namespace c4;

    std::vector<char> parsed_yml_buf;
    std::vector<char> result_buf;

    c4::tpl::Engine eng;
    c4::tpl::Rope parsed_rope;
    eng.parse(tpl, &parsed_rope);

    c4::yml::Tree tree;
    c4::tpl::Rope rope;

    for(auto const& c : cases)
    {
        tree.clear();
        parsed_yml_buf.assign(c.props_yml.begin(), c.props_yml.end());
        c4::yml::parse(to_substr(parsed_yml_buf), &tree);
        eng.render(tree, &rope);
        csubstr ret = rope.chain_all_resize(&result_buf);
        C4_CHECK(ret == c.result);
    }
}

int main(int argc, char **argv)
{
    do_engine_test(\"foo is {{foo}}\",
                   tpl_cases{
                       {\"0\", \"{foo: 1}\", \"foo is 1\"},
                       {\"1\", \"{foo: 2}\", \"foo is 2\"},
                       {\"2\", \"{foo: 10}\", \"foo is 10\"},
                   });
    return 0;
}

")
