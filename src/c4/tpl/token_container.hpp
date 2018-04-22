#ifndef _C4_TPL_TOKEN_CONTAINER_HPP_
#define _C4_TPL_TOKEN_CONTAINER_HPP_

#include <vector>
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
    inline virtual csubstr const& stoken() const override { return s_stoken(); } \
    inline virtual csubstr const& etoken() const override { return s_etoken(); } \
    inline virtual csubstr const& marker() const override { return s_marker(); } \
    inline static csubstr const& s_stoken() { static const csubstr s(stok); return s; } \
    inline static csubstr const& s_etoken() { static const csubstr s(etok); return s; } \
    inline static csubstr const& s_marker() { static const csubstr s(mrk); return s; } \

#define C4TPL_REGISTER_TOKEN(mgr, cls) mgr.register_token_type<cls>()

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
using TokenPoolType = pool_linear_paged<256, size_t, allocator_mr<char> >;
constexpr const size_t TokenTypesMax = 32;

class TokenContainer : public ObjMgr< TokenBase, TokenPoolType, TokenTypesMax >
{
    using base_type = ObjMgr< TokenBase, TokenPoolType, TokenTypesMax >;

public:

    std::vector< csubstr >    m_token_starts;
    std::vector< size_t >     m_token_seq;

    using ObjMgr::ObjMgr;

    ~TokenContainer();

    template< class T >
    void register_token_type()
    {
        C4_REGISTER_MANAGED(*this, T);
        m_token_starts.emplace_back(T::s_stoken());
    }

public:

    size_t next_token(csubstr *rem, TplLocation *loc);

public:

    template< class TkC, class TkB >
    struct token_iterator_impl
    {
        TkC *this_;
        size_t id;

        using value_type = TkB;

        token_iterator_impl(TkC* t, size_t i_) : this_(t), id(i_) {}

        token_iterator_impl operator++ () { ++id; return *this; }
        token_iterator_impl operator-- () { ++id; return *this; }

        value_type* operator-> () { return  this_->get(id); }
        value_type& operator*  () { return *this_->get(id); }

        bool operator!= (token_iterator_impl that) const { C4_ASSERT(this_ == that.this_); return id != that.id; }
        bool operator== (token_iterator_impl that) const { C4_ASSERT(this_ == that.this_); return id == that.id; }
    };

    using iterator = token_iterator_impl< TokenContainer, TokenBase >;
    using const_iterator = token_iterator_impl< const TokenContainer, const TokenBase >;

    iterator begin() { return iterator(this, 0); }
    iterator end  () { return iterator(this, m_token_seq.size()); }

    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end  () const { return const_iterator(this, m_token_seq.size()); }

};

} // namespace tpl
} // namespace c4

#ifdef __GNUC__
#   pragma GCC diagnostic pop
#endif

#endif /* _C4_TPL_TOKEN_CONTAINER_HPP_ */
