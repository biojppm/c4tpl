#include "c4/tpl/token_container.hpp"
#include "c4/tpl/token.hpp"

namespace c4 {
namespace tpl {

TokenContainer::~TokenContainer()
{
    for(TokenBase &tk : *this)
    {
        tk.~TokenBase();
    }
}

size_t TokenContainer::next_token(csubstr *rem, TplLocation *loc)
{
    auto result = rem->first_of_any(m_token_starts.begin(), m_token_starts.end());
    if( ! result) return NONE;
    TokenBase *t = this->create(result.pos);
    m_token_seq.push_back(t->m_id);
    *rem = rem->sub(result.pos);
    loc->m_rope_pos.i += result.pos;
    return t->m_id;
}

} // namespace tpl
} // namespace c4