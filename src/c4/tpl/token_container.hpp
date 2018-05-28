#ifndef _C4_TPL_TOKEN_CONTAINER_HPP_
#define _C4_TPL_TOKEN_CONTAINER_HPP_

#include <vector>
#include <c4/std/vector.hpp>
#include "c4/tpl/rope.hpp"
#include "c4/tpl/mgr.hpp"

#ifdef __GNUC__
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wtype-limits"
#endif

namespace c4 {
namespace tpl {

class TokenBase;
class TokenContainer;

class Rope;

struct TplLocation
{
    Rope *         m_rope;
    Rope::rope_pos m_rope_pos;
    //! @todo:
    //size_t         m_offset;
    //size_t         m_line;
    //size_t         m_column;
};

void register_known_tokens(TokenContainer &c);

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** allow user-registered tokens */
#define C4TPL_DECLARE_TOKEN(cls, stok, etok, mrk)                       \
\
    C4_DECLARE_MANAGED(cls, TokenBase, size_t);\
\
public:\
                                                                        \
    /** start token */                                                  \
    inline virtual csubstr stoken() const override { return s_stoken(); } \
    /** end token */                                                    \
    inline virtual csubstr etoken() const override { return s_etoken(); } \
    inline virtual csubstr marker() const override { return s_marker(); } \
    inline static  csubstr s_stoken() { static const csubstr s(stok); return s; } \
    inline static  csubstr s_etoken() { static const csubstr s(etok); return s; } \
    inline static  csubstr s_marker() { static const csubstr s(mrk); return s; } \

#define C4TPL_REGISTER_TOKEN(mgr, cls) mgr.register_token_type<cls>()

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
using TokenPoolType = pool_linear_paged<256, size_t, allocator_mr<char>>;
constexpr const size_t TokenTypesMax = 32;

class TokenContainer : public ObjMgr<TokenBase, TokenPoolType, TokenTypesMax>
{
    using base_type = ObjMgr<TokenBase, TokenPoolType, TokenTypesMax>;
    using pool_type = typename base_type::pool_type;

public:

    std::vector<csubstr>    m_token_starts;
    std::vector<size_t>     m_token_seq;

    using ObjMgr::ObjMgr;

    template< class T >
    void register_token_type()
    {
        C4_REGISTER_MANAGED(*this, T);
        m_token_starts.emplace_back(T::s_stoken());
    }

public:

    size_t next_token(csubstr *rem, TplLocation *loc);

};

} // namespace tpl
} // namespace c4

#ifdef __GNUC__
#   pragma GCC diagnostic pop
#endif

#endif /* _C4_TPL_TOKEN_CONTAINER_HPP_ */
