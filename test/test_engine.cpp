#include "c4/tpl/engine.hpp"
#include "c4/yml/parse.hpp"

//#include "../../../test_case.hpp"

#include <gtest/gtest.h>

namespace c4 {

inline void PrintTo(const substr& s, ::std::ostream* os) { *os << s; }
inline void PrintTo(const csubstr& s, ::std::ostream* os) { *os << s; }

namespace tpl {

struct tpl_results { const char* name; csubstr props_yml, result; };
using tpl_cases = std::initializer_list< tpl_results >;

void do_engine_test(csubstr tpl, csubstr parsed_tpl, tpl_cases cases)
{
    std::vector< char > parsed_tpl_buf;
    std::vector< char > parsed_yml_buf;
    std::vector< char > result_buf;

    c4::tpl::Engine eng;
    c4::tpl::Rope parsed_rope;
    eng.parse(tpl, &parsed_rope);
    csubstr ret = parsed_rope.chain_all_resize(&parsed_tpl_buf);
    (void)parsed_tpl;//EXPECT_EQ(ret, parsed_tpl);

    c4::yml::Tree tree;
    c4::tpl::Rope rope;

    for(auto const& c : cases)
    {
        SCOPED_TRACE(c.name);
        tree.clear();
        parsed_yml_buf.assign(c.props_yml.begin(), c.props_yml.end());
        c4::yml::parse(to_substr(parsed_yml_buf), &tree);
        //print_tree(tree);
        eng.render(tree, &rope);
        ret = rope.chain_all_resize(&result_buf);
        EXPECT_EQ(ret, c.result);
    }
}

//-----------------------------------------------------------------------------

TEST(expr, basic)
{
    do_engine_test("foo is {{foo}}",
                   "foo is <<<expr>>>",
                   tpl_cases{
                       {"0", "{foo: 1}", "foo is 1"},
                       {"1", "{foo: 2}", "foo is 2"},
                       {"2", "{foo: 10}", "foo is 10"},
                   });
}

TEST(expr, basic2)
{
    do_engine_test("foo is {{foo}}, bar is {{bar}}",
                   "foo is <<<expr>>>, bar is <<<expr>>>",
                   tpl_cases{
                       {"0", "{foo: 1, bar: 20}", "foo is 1, bar is 20"},
                       {"1", "{foo: 2, bar: 30}", "foo is 2, bar is 30"},
                       {"2", "{foo: 3, bar: 40}", "foo is 3, bar is 40"},
                   });
}

//-----------------------------------------------------------------------------
TEST(if, simple)
{
    do_engine_test("{% if foo %}bar{% endif %}",
                   "<<<if>>>",
                   tpl_cases{
                       {"0", "{}", ""},
                       {"1", "{foo: 2}", "bar"},
                   });
}

TEST(if, simple_empty)
{
    do_engine_test("{% if foo %}{% endif %}",
                   "<<<if>>>",
                   tpl_cases{
                       {"0", "{}", ""},
                       {"1", "{foo: 2}", ""},
                   });
}

//-----------------------------------------------------------------------------

TEST(if_else, simple)
{
    do_engine_test("{% if foo %}foo{% else %}bar{% endif %}",
                   "<<<if>>>",
                   tpl_cases{
                       {"0", "{}", "bar"},
                       {"1", "{foo: 2}", "foo"},
                   });
}

TEST(if_elif, simple)
{
    do_engine_test("{% if foo %}foo{% elif bar %}bar{% endif %}",
                   "<<<if>>>",
                   tpl_cases{
                       {"0", "{}", ""},
                       {"1", "{foo: 2}", "foo"},
                       {"2", "{bar: 2}", "bar"},
                       {"3", "{foo: 2, bar: 2}", "foo"},
                   });
}

TEST(if_elif_else, simple)
{
    do_engine_test("{% if foo %}foo{% elif bar %}bar{% else %}baz{% endif %}",
                   "<<<if>>>",
                   tpl_cases{
                       {"0", "{}", "baz"},
                       {"1", "{foo: 2}", "foo"},
                       {"2", "{bar: 2}", "bar"},
                       {"3", "{baz: 2}", "baz"},
                       {"4", "{foo: 2, bar: 2}", "foo"},
                       {"5", "{bar: 2, baz: 2}", "bar"},
                   });
}

//-----------------------------------------------------------------------------

TEST(if, with_vars)
{
    do_engine_test("{% if foo %}foo is active! val={{foo}}{% endif %}",
                   "<<<if>>>",
                   tpl_cases{
                       {"case 0", "{}", ""},
                       {"case 1", "{foo: 1}", "foo is active! val=1"},
                       {"case 2", "{foo: 2}", "foo is active! val=2"},
                       {"case 3", "{foo: 3}", "foo is active! val=3"},
                   });
}

TEST(if_else, with_vars)
{
    do_engine_test("{% if foo %}foo is active! foo='{{foo}}' bar='{{bar}}'{% else %}actually, no. foo='{{foo}}' bar='{{bar}}'{% endif %}",
                   "<<<if>>>",
                   tpl_cases{
                       {"case 0", "{}", "actually, no. foo='' bar=''"},
                       {"case 1", "{bar: 1}", "actually, no. foo='' bar='1'"},
                       {"case 2", "{bar: 2}", "actually, no. foo='' bar='2'"},
                       {"case 3", "{foo: 3}", "foo is active! foo='3' bar=''"},
                       {"case 4", "{foo: 4, bar: 4}", "foo is active! foo='4' bar='4'"},
                   });
}

TEST(if_elif_else, with_vars)
{
    do_engine_test("{% if foo %}foo is active! foo='{{foo}}' bar='{{bar}}'{% elif bar %}only bar is active: foo='{{foo}}' bar='{{bar}}'{% else %}actually, no. foo='{{foo}}' bar='{{bar}}'{% endif %}",
                   "<<<if>>>",
                   tpl_cases{
                       {"case 0", "{}", "actually, no. foo='' bar=''"},
                       {"case 1", "{bar: 1}", "only bar is active: foo='' bar='1'"},
                       {"case 2", "{bar: 2}", "only bar is active: foo='' bar='2'"},
                       {"case 3", "{foo: 3}", "foo is active! foo='3' bar=''"},
                       {"case 4", "{foo: 4, bar: 4}", "foo is active! foo='4' bar='4'"},
                   });
}

//-----------------------------------------------------------------------------

TEST(if, with_nested_if)
{
    do_engine_test(R"({% if foo %}foo is active! foo='{{foo}}'{% if bar %}. bar is active! bar='{{bar}}'{% endif %}.{% endif %})",
                   "<<<if>>>",
                   tpl_cases{
                       {"case 0", "{}", ""},
                       {"case 10", "{foo: 1}", "foo is active! foo='1'."},
                       {"case 20", "{foo: 2}", "foo is active! foo='2'."},
                       {"case 01", "{bar: 1}", ""},
                       {"case 02", "{bar: 2}", ""},
                       {"case 11", "{foo: 1, bar: 1}", "foo is active! foo='1'. bar is active! bar='1'."},
                       {"case 12", "{foo: 1, bar: 2}", "foo is active! foo='1'. bar is active! bar='2'."},
                       {"case 21", "{foo: 2, bar: 1}", "foo is active! foo='2'. bar is active! bar='1'."},
                       {"case 22", "{foo: 2, bar: 2}", "foo is active! foo='2'. bar is active! bar='2'."},
                   });
}

TEST(if, with_nested_if_else)
{
    do_engine_test(R"({% if foo %}foo is active! foo='{{foo}}'{% if bar %}. bar is active! bar='{{bar}}'{% else %}. No bar..{% endif %}.{% endif %})",
                   "<<<if>>>",
                   tpl_cases{
                       {"case 0", "{}", ""},
                       {"case 10", "{foo: 1}", "foo is active! foo='1'. No bar..."},
                       {"case 20", "{foo: 2}", "foo is active! foo='2'. No bar..."},
                       {"case 01", "{bar: 1}", ""},
                       {"case 02", "{bar: 2}", ""},
                       {"case 11", "{foo: 1, bar: 1}", "foo is active! foo='1'. bar is active! bar='1'."},
                       {"case 12", "{foo: 1, bar: 2}", "foo is active! foo='1'. bar is active! bar='2'."},
                       {"case 21", "{foo: 2, bar: 1}", "foo is active! foo='2'. bar is active! bar='1'."},
                       {"case 22", "{foo: 2, bar: 2}", "foo is active! foo='2'. bar is active! bar='2'."},
                   });
}

TEST(if, with_nested_if_elif_else)
{
    do_engine_test(R"({% if foo %}foo is active! foo='{{foo}}'{% if bar %}. bar is active! bar='{{bar}}'{% elif a %}. aaaa{% else %}. No bar nor a..{% endif %}.{% endif %})",
                   "<<<if>>>",
                   tpl_cases{
                       {"case 0", "{}", ""},
                       {"case 10" , "{foo: 1}", "foo is active! foo='1'. No bar nor a..."},
                       {"case 20" , "{foo: 2}", "foo is active! foo='2'. No bar nor a..."},
                       {"case 10a", "{foo: 1, a: 1}", "foo is active! foo='1'. aaaa."},
                       {"case 20a", "{foo: 2, a: 1}", "foo is active! foo='2'. aaaa."},
                       {"case 01", "{bar: 1}", ""},
                       {"case 02", "{bar: 2}", ""},
                       {"case 11", "{foo: 1, bar: 1}", "foo is active! foo='1'. bar is active! bar='1'."},
                       {"case 12", "{foo: 1, bar: 2}", "foo is active! foo='1'. bar is active! bar='2'."},
                       {"case 21", "{foo: 2, bar: 1}", "foo is active! foo='2'. bar is active! bar='1'."},
                       {"case 22", "{foo: 2, bar: 2}", "foo is active! foo='2'. bar is active! bar='2'."},
                       {"case 11a", "{foo: 1, a: 1}", "foo is active! foo='1'. aaaa."},
                       {"case 12a", "{foo: 1, a: 2}", "foo is active! foo='1'. aaaa."},
                       {"case 21a", "{foo: 2, a: 1}", "foo is active! foo='2'. aaaa."},
                       {"case 22a", "{foo: 2, a: 2}", "foo is active! foo='2'. aaaa."},
                   });
}

TEST(if, with_double_nested_if)
{
    do_engine_test(R"({% if foo %}foo is active! foo='{{foo}}'.{% if bar %} bar is active! bar='{{bar}}'.{% if baz %} baz is active! baz='{{baz}}'.{% endif %}{% endif %}{% endif %})",
                   "<<<if>>>",
                   tpl_cases{
                       {"case 000", "{}", ""},
                       {"case 100", "{foo: 1}", "foo is active! foo='1'."},
                       {"case 200", "{foo: 2}", "foo is active! foo='2'."},
                       {"case 010", "{bar: 1}", ""},
                       {"case 020", "{bar: 2}", ""},
                       {"case 011", "{baz: 1}", ""},
                       {"case 022", "{baz: 2}", ""},
                       {"case 110", "{foo: 1, bar: 1}", "foo is active! foo='1'. bar is active! bar='1'."},
                       {"case 120", "{foo: 1, bar: 2}", "foo is active! foo='1'. bar is active! bar='2'."},
                       {"case 210", "{foo: 2, bar: 1}", "foo is active! foo='2'. bar is active! bar='1'."},
                       {"case 220", "{foo: 2, bar: 2}", "foo is active! foo='2'. bar is active! bar='2'."},
                       {"case 111", "{foo: 1, bar: 1, baz: 1}", "foo is active! foo='1'. bar is active! bar='1'. baz is active! baz='1'."},
                       {"case 121", "{foo: 1, bar: 2, baz: 1}", "foo is active! foo='1'. bar is active! bar='2'. baz is active! baz='1'."},
                       {"case 212", "{foo: 2, bar: 1, baz: 2}", "foo is active! foo='2'. bar is active! bar='1'. baz is active! baz='2'."},
                       {"case 222", "{foo: 2, bar: 2, baz: 2}", "foo is active! foo='2'. bar is active! bar='2'. baz is active! baz='2'."},
                   });
}

TEST(if_elif_else, all_with_nested_if)
{
    do_engine_test(R"({% if foo %}foo is active! foo='{{foo}}'{% if v %}. And v!{% endif %}.{% elif bar %}bar is active! bar='{{bar}}'{% if v %}. And v!{% endif %}.{% elif baz %}baz is active! baz='{{baz}}'{% if v %}. And v!{% endif %}.{% else %}no var is active.{% if v %} But v!{% endif %}{% endif %})",
                   "<<<if>>>",
                   tpl_cases{
                       {"case 000f", "{    }", "no var is active."},
                       {"case 000t", "{v: 1}", "no var is active. But v!"},
                       {"case 100f", "{foo: 1      }", "foo is active! foo='1'."},
                       {"case 100t", "{foo: 1, v: 1}", "foo is active! foo='1'. And v!."},
                       {"case 200f", "{foo: 2      }", "foo is active! foo='2'."},
                       {"case 200t", "{foo: 2, v: 1}", "foo is active! foo='2'. And v!."},
                       {"case 010f", "{bar: 1      }", "bar is active! bar='1'."},
                       {"case 010t", "{bar: 1, v: 1}", "bar is active! bar='1'. And v!."},
                       {"case 011f", "{baz: 1      }", "baz is active! baz='1'."},
                       {"case 011t", "{baz: 1, v: 1}", "baz is active! baz='1'. And v!."},
                       {"case 110f", "{foo: 1, bar: 1      }", "foo is active! foo='1'."},
                       {"case 110t", "{foo: 1, bar: 1, v: 1}", "foo is active! foo='1'. And v!."},
                       {"case 120f", "{foo: 1, bar: 2      }", "foo is active! foo='1'."},
                       {"case 120t", "{foo: 1, bar: 2, v: 1}", "foo is active! foo='1'. And v!."},
                       {"case 210f", "{foo: 2, bar: 1      }", "foo is active! foo='2'."},
                       {"case 210t", "{foo: 2, bar: 1, v: 1}", "foo is active! foo='2'. And v!."},
                       {"case 220f", "{foo: 2, bar: 2      }", "foo is active! foo='2'."},
                       {"case 220t", "{foo: 2, bar: 2, v: 1}", "foo is active! foo='2'. And v!."},
                       {"case 010f", "{        bar: 1      }", "bar is active! bar='1'."},
                       {"case 010t", "{        bar: 1, v: 1}", "bar is active! bar='1'. And v!."},
                       {"case 020f", "{        bar: 2      }", "bar is active! bar='2'."},
                       {"case 020t", "{        bar: 2, v: 1}", "bar is active! bar='2'. And v!."},
                       {"case 001f", "{        baz: 1      }", "baz is active! baz='1'."},
                       {"case 001t", "{        baz: 1, v: 1}", "baz is active! baz='1'. And v!."},
                       {"case 002f", "{        baz: 2      }", "baz is active! baz='2'."},
                       {"case 002t", "{        baz: 2, v: 1}", "baz is active! baz='2'. And v!."},
                       {"case 000f", "{                    }", "no var is active."},
                       {"case 000t", "{                v: 1}", "no var is active. But v!"},
                       {"case 000f", "{                    }", "no var is active."},
                       {"case 000t", "{                v: 1}", "no var is active. But v!"},
                       {"case 000f", "{                    }", "no var is active."},
                       {"case 000t", "{                v: 1}", "no var is active. But v!"},
                       {"case 000f", "{                    }", "no var is active."},
                       {"case 000t", "{                v: 1}", "no var is active. But v!"},
                   });
}

//-----------------------------------------------------------------------------
TEST(for, simple_no_vars)
{
    do_engine_test("{% for v in var %}this block will repeat{% endfor %}",
                   "<<<for>>>",
                   tpl_cases{
                       {"case 0", "{}", ""},
                       {"case 1", "{var: [0]}", "this block will repeat"},
                       {"case 2", "{var: [0, 1]}", "this block will repeatthis block will repeat"},
                   });
}

TEST(for, simple_with_vars)
{
    do_engine_test("{% for v in var %}this block will repeat v={{v}}. {% endfor %}",
                   "<<<for>>>",
                   tpl_cases{
                       {"case 0", "{}", ""},
                       {"case 1", "{var: [0]}", "this block will repeat v=0. "},
                       {"case 2", "{var: [0, 1]}", "this block will repeat v=0. this block will repeat v=1. "},
                       {"case 3", "{var: [0, 1, 2, 3, 4, 5]}", "this block will repeat v=0. this block will repeat v=1. this block will repeat v=2. this block will repeat v=3. this block will repeat v=4. this block will repeat v=5. "},
                   });
}


//-----------------------------------------------------------------------------
TEST(engine, basic)
{
    do_engine_test(R"(
foo={{foo}}
bar={{bar}}
baz={{baz}}
seq[0]={{seq[0]}}
seq[1]={{seq[1]}}
seq[2]={{seq[2]}}
nested.foo={{nested.foo}}
nested.very.bar={{nested.very.bar}}
nested.very.deeply.baz={{nested.very.deeply.baz}}
foo={{ foo }}
bar={{ bar }}
baz={{ baz }}
seq[0]={{ seq[0] }}
seq[1]={{ seq[1] }}
seq[2]={{ seq[2] }}
nested.foo={{ nested.foo }}
nested.very.bar={{ nested.very.bar }}
nested.very.deeply.baz={{ nested.very.deeply.baz }}
# an if here
{% if foo %}
foo is active! val={{foo}}
{% endif %}
{% if nothing %}
nothing is active!
{% elif bar %}
bar is active! val={{bar}}
{% endif %}
{% if zzz %}
zzz is active!
{% elif zzz %}
zzz is active!
{% elif seq %}
seq is not empty!
{% endif %}
{% if zzz %}
zzz is active!
{% elif c0 in seq %}
c0 is in seq!
{% endif %}
# a for here
{% for s in seq %}
   - {{s}}
{% endfor %}
)",
                   R"(
foo=<<<expr>>>
bar=<<<expr>>>
baz=<<<expr>>>
seq[0]=<<<expr>>>
seq[1]=<<<expr>>>
seq[2]=<<<expr>>>
nested.foo=<<<expr>>>
nested.very.bar=<<<expr>>>
nested.very.deeply.baz=<<<expr>>>
foo=<<<expr>>>
bar=<<<expr>>>
baz=<<<expr>>>
seq[0]=<<<expr>>>
seq[1]=<<<expr>>>
seq[2]=<<<expr>>>
nested.foo=<<<expr>>>
nested.very.bar=<<<expr>>>
nested.very.deeply.baz=<<<expr>>>
# an if here
<<<if>>>
<<<if>>>
<<<if>>>
<<<if>>>
# a for here
<<<for>>>
)",
                   tpl_cases{
                       {
                           "case 0",
                           R"({
foo: 0,
bar: 1,
seq: [c0, c1, c2],
nested: {
  foo: 10,
  very: {
    bar: 100,
    deeply: {
      baz: 1000
    }
  }
}})",
                            R"(
foo=0
bar=1
baz=
seq[0]=c0
seq[1]=c1
seq[2]=c2
nested.foo=10
nested.very.bar=100
nested.very.deeply.baz=1000
foo=0
bar=1
baz=
seq[0]=c0
seq[1]=c1
seq[2]=c2
nested.foo=10
nested.very.bar=100
nested.very.deeply.baz=1000
# an if here
foo is active! val=0
bar is active! val=1
seq is not empty!
c0 is in seq!
# a for here
   - c0
   - c1
   - c2
)"

                               },

                   }
        );
}

} // namespace tpl
} // namespace c4
