#ifndef _C4_TPL_MGR_HPP_
#define _C4_TPL_MGR_HPP_

#include <stddef.h>
#include <c4/allocator.hpp>
#include <c4/memory_util.hpp>

#include "c4/tpl/pool.hpp"

namespace c4 {
namespace tpl {

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#define C4_DECLARE_MANAGED(cls, base, idx)                          \
public:                                                             \
                                                                    \
    using base_type = base;                                         \
                                                                    \
    static idx _s_type_id;                                          \
    static idx _s_set_type_id(idx id)                               \
    {                                                               \
        C4_ASSERT_MSG(_s_type_id == (idx)-1, "type id was already set"); \
        _s_type_id = id;                                            \
        return _s_type_id;                                          \
    }                                                               \
                                                                    \
    static cls* _s_create(void *mem)                                \
    {                                                               \
        new (mem) cls();                                            \
        return (cls*) mem;                                          \
    }                                                               \
    static base* _s_create_base(void *mem)                          \
    {                                                               \
        return _s_create(mem);                                      \
    }                                                               \
    static void _s_destroy(void *mem)                               \
    {                                                               \
        cls *ptr = (cls*)mem;                                       \
        ptr->~cls();                                                \
    }                                                               \
                                                                    \
private:                                                            \
                                                                    \
    idx m_id;                                                       \
                                                                    \
public:                                                             \
                                                                    \
    inline idx id() const { return m_id; }                          \
    static inline idx s_type_id() { return _s_type_id; }            \
    static inline const char* s_type_name() { return #cls; }        \
                                                                    \
private:                                                            \


#define C4_DEFINE_MANAGED(cls, idx) \
idx cls::_s_type_id = (idx)-1


#define C4_REGISTER_MANAGED(mgr, cls) \
(mgr).register_type<cls>()


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

template< class B, class Pool >
struct ObjPool : public Pool
{
    using pfn_create = B* (*)(void *mem);
    using pfn_destroy = void (*)(void *mem);
    using I = typename Pool::index_type;

    using Pool::Pool;

    I           m_type_id;
    csubstr     m_type_name;
    pfn_create  m_type_create;
    pfn_destroy m_type_destroy;

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/** A manager of objects with a common base type. */
template< class B, class Pool, size_t NumPoolsMax >
struct ObjMgr
{
    using pool_type = ObjPool<B, Pool>;
    using I = typename pool_type::I;

    struct name_id
    {
        csubstr name;
        I id;
    };

public:

    pool_collection< ObjPool<B, Pool>, NumPoolsMax > m_pools;
    I m_size;

    name_id m_type_ids; ///< @todo

public:

    ObjMgr() : m_pools(), m_size(), m_type_ids()
    {
    }

    ~ObjMgr()
    {
        clear();
        free();
    }

    ObjMgr(ObjMgr const&) = delete;
    ObjMgr(ObjMgr     &&) = delete;

    ObjMgr& operator= (ObjMgr const&) = delete;
    ObjMgr& operator= (ObjMgr     &&) = delete;

public:

    void clear()
    {
        m_pools.clear();
    }

    void free()
    {
        m_pools.free();
    }

    bool empty() const { return m_size == 0; }
    I size() const { return m_size; }

public:

    template< class T >
    I register_type()
    {
        static_assert(std::is_base_of< B, T >::value, "B must be base of T");
        I type_id = m_pools.add_pool();
        T::_s_set_type_id(type_id);
        pool_type *p = m_pools.get_pool(type_id);
        p->m_type_id = type_id;
        p->m_type_name = to_csubstr(T::s_type_name());
        p->m_type_create = &T::_s_create_base;
        p->m_type_destroy = &T::_s_destroy;
        return type_id;
    }

public:

    pool_type * get_pool(I pool_id)
    {
        return m_pools.get_pool(pool_id);
    }

    template< class T >
    pool_type * get_pool()
    {
        I type_id = T::s_type_id();
        return m_pools.get_pool(type_id);
    }

    template< class T >
    pool_type const* get_pool() const
    {
        I type_id = T::s_type_id();
        return m_pools.get_pool(type_id);
    }

    pool_type * get_pool(csubstr type_name)
    {
        for(auto &p : m_pools)
        {
            if(p.m_type_name == type_name)
            {
                return &p;
            }
        }
        return nullptr;
    }

    pool_type const* get_pool(csubstr type_name) const
    {
        for(auto &p : m_pools)
        {
            if(p.m_type_name == type_name)
            {
                return &p;
            }
        }
        return nullptr;
    }

public:

    template< class T, class... CtorArgs >
    T * create_as(CtorArgs&& ...args)
    {
        I type_id = T::s_type_id();
        I id = m_pools.claim(type_id);
        pool_type *p = get_pool(type_id);
        T* tptr = new (m_pools.get(id)) T(std::forward< CtorArgs >(args)...);
        tptr->m_id = id;
        ++m_size;
        return tptr;
    }

    B * create(I type_id)
    {
        I id = m_pools.claim(type_id);
        pool_type *p = get_pool(type_id);
        B* tptr = p->m_type_create(m_pools.get(id));
        tptr->m_id = id;
        ++m_size;
        return tptr;
    }

    B * create(csubstr type_name)
    {
        pool_type *p = this->get_pool(type_name);
        I id = m_pools.claim(p->m_type_id);
        B* tptr = p->m_type_create(m_pools.get(id));
        tptr->m_id = id;
        ++m_size;
        return tptr;
    }

    void release(I id)
    {
        m_pools.release(id);
        --m_size;
    }

public:

    B * get(I id) const
    {
        B *ptr = (B*) m_pools.get(id);
        return ptr;
    }

    template< class T >
    T * get_as(I id) const
    {
        T *ptr = static_cast< T* >((B*) m_pools.get(id));
        return ptr;
    }
};

} // namespace tpl
} // namespace c4

#endif /* _C4_TPL_MGR_HPP_ */