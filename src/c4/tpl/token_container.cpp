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

} // namespace tpl
} // namespace c4
