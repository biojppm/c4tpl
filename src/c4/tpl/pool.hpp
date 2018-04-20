#ifndef _C4_POOL_HPP_
#define _C4_POOL_HPP_

#include "c4/allocator.hpp"

namespace c4 {

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

template< class I, class Allocator >
struct pool_linear
{
public:

    using index_type = I;
    using allocator_type = Allocator;

public:

    void *m_mem;        ///< the memory buffer
    I     m_obj_size;   ///< the size of each object
    I     m_obj_align;  ///< the alignment of the objects
    I     m_num_objs;   ///< the current number of objects

    /** first: the capacity, expressed in number of objects
     * second: the allocator */
    tight_pair< I, Allocator > m_capacity_allocator;

public:

    C4_NO_COPY_OR_MOVE(pool_linear);

    pool_linear()
        :
        m_mem{nullptr},
        m_obj_size{0},
        m_obj_align{0},
        m_num_objs{0},
        m_capacity_alloc{0, {}}
    {
    }

    template< class ...AllocatorArgs >
    pool_linear(varargs_t, AllocatorArgs && ...args)
        :
        pool_linear()
    {
        m_capacity_allocator.second = {std::forward<Args>(args)...};
    }

    template< class ...AllocatorArgs >
    pool_linear(I obj_size, I obj_align, I capacity, varargs_t, AllocatorArgs && ...args)
        :
        pool_linear(varargs, std::forward<Args>(args)...)
    {
        m_obj_size = obj_size;
        m_obj_align = obj_align;
        reserve(capacity);
    }

    ~pool_linear()
    {
        free();
    }

public:

    I size() const { return m_num_objs; }
    I size_bytes() const { return m_num_objs * m_obj_size; }

    I capacity() const { return m_capacity_allocator.first; }
    I capacity_bytes() const { return m_capacity_allocator.first * m_obj_size; }

    Allocator const& allocator() const { return m_capacity_allocator.second; }

public:

    void free()
    {
        if(m_mem)
        {
            m_capacity_allocator.second.deallocate(m_mem, m_num_objs * m_obj_size);
        }
    }

    void reserve(I num_objs)
    {
        C4_ERROR_IF(num_objs >= m_capacity && m_size > 0, "cannot relocate objects in pool");
        C4_ASSERT(m_mem == nullptr);
        m_capacity_allocator.first = num_objs;
        m_mem = m_capacity_allocator.second.allocate(num_objs * m_obj_size, m_obj_align);
        m_size = 0;
    }

public:

    I claim(I n=1)
    {
        C4_ASSERT(n >= 1);
        C4_ERROR_IF(m_num_objs+n >= m_capacity, "cannot relocate objects in pool");
        I id = m_num_objs;
        m_num_objs += n;
        return id;
    }

    void release(I id, I n=1)
    {
        C4_ASSERT(n >= 1);
        // no-op unless id is the most recent (ie, we're still at the
        // end), in which case the size is decreased
        if(id+n == m_num_objs)
        {
            C4_ASSERT(m_num_objs >= n);
            m_num_objs -= n;
        }
    }

    void *get(I id) const
    {
        C4_ASSERT(id <= m_num_objs);
        return (((char*)m_mem) + id * m_obj_size);
    }
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#ifdef C4_WORK_IN_PROGRESS
template< class I, I PageSize=256 >
struct pool_linear_paged
{
    static_assert((PageSize & (PageSize - 1)) == 0, "PageSize must be a power of two");

public:

    struct Page
    {
        void * mem;
        I      numpg; ///< number of pages allocated in this block
                      ///< (the following numpg pages are allocated together
                      ///< with this block, and their numpg is set to 0)
    };

    Page *    m_pages;
    I         m_size;
    I         m_num_pages;
    MemoryResource *m_mr;

private:

    enum : I
    {
        /** id mask: all the bits up to PageSize. Use to extract the position
         * of an index within a page. */
        id_mask = I(PageSize) - I(1),
        /** page msb: the number of bits complementary to PageSize. Use to
         * extract the page of an index. */
        page_msb = msb11< I, PageSize >::value,
    };

    static inline I _page(I id) { return id >> page_msb; }
    static inline I _pos (I id) { return id &  id_mask; }

public:

    linear_arena_paged(allocator_mr<T> const& a={})
        :
        m_pages(nullptr),
        m_size(0),
        m_num_pages(0),
        m_alloc(a)
    {
    }
    ~linear_arena_paged()
    {
        clear();
        free();
    }

    linear_arena_paged(linear_arena_paged const& that) = delete;
    linear_arena_paged(linear_arena_paged     && that) = delete;

    linear_arena_paged& operator= (linear_arena_paged const& that) = delete;
    linear_arena_paged& operator= (linear_arena_paged     && that) = delete;

public:

    I next_num_pages(size_t cap)
    {
        I rem = (cap % PageSize);
        cap += rem ? PageSize - rem : 0;
        return cap / PageSize;
    }

    void reserve(size_t cap) override
    {
        if(cap <= capacity()) return;
        I np = next_num_pages(cap);
        C4_ASSERT(np > m_num_pages);

        // allocate pages arr
        auto * pgs = (Page*) m_alloc.allocate(np * sizeof(Page));//, m_pages);
        memcpy(pgs, m_pages, m_num_pages * sizeof(Page));
        m_alloc.deallocate((char*)m_pages, np * sizeof(Page));
        m_pages = pgs;

        // allocate page mem
        I more_pages = np - m_num_pages;
        auto* mem = m_alloc.allocate(more_pages * PageSize * sizeof(T));//, last);
        // the first page owns the mem (by setting numpg to the number of pages in this mem block)
        m_pages[m_num_pages]->mem = mem;
        m_pages[m_num_pages]->numpg = more_pages;
        // remaining pages only have their pointers set (and numpg is set to 0)
        for(I i = m_num_pages+1; i < np; ++i)
        {
            m_pages[i]->mem = mem + i * PageSize;
            m_pages[i]->numpg = 0;
        }
        m_num_pages = np;
    }

    void free() override final
    {
        if(m_num_pages == 0) return;
        for(I i = 0; i < m_num_pages; ++i)
        {
            Page *p = m_pages + i;
            if(p->numpg == 0) continue;
            m_alloc.deallocate(p->mem, p->numpg * PageSize * sizeof(T));
            i += p->numpg;
            C4_ASSERT(i <= m_num_pages);
        }
        m_alloc.free(m_pages, m_num_pages * sizeof(Page));
        m_num_pages = 0;
    }

    void clear() override final
    {
        for(I i = 0; i < m_size; ++i)
        {
            T *ptr = get(i);
            ptr->~T();
        }
        m_size = 0;
    }

    I claim() override final
    {
        if(m_size == capacity())
        {
            reserve(m_size + 1); // adds a single page
        }
        return m_size++;
    }

public:

    void* get(I id) const
    {
        void *mem = , pos = ;
        T *mem = (char*) m_pages[_page(id)].mem + _pos(id);
        mem += pos * sizeof(T);
        return (T*) mem;
    }

    inline I size() const { return m_size; }
    inline I capacity() const { return m_num_pages * PageSize; }

};
#endif C4_WORK_IN_PROGRESS


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

C4_BEGIN_NAMESPACE(detail)
template< class Pool, class CollectionImpl >
struct _pool_collection_crtp
{
#define _c4this  static_cast< CollectionImpl      *>(this)
#define _c4cthis static_cast< CollectionImpl const*>(this)


    I claim(I pool, I n=1)
    {
        C4_ASSERT(pool <= _c4cthis->size());
        I pos = _c4this->get_pool(pool)->claim(n);
        I id = encode_id(pool, pos);
        return id;
    }

    void release(I id, I n=1)
    {
        C4_ASSERT(n >= 1);
        I pool = decode_pool(id);
        I pos = decode_pos(id);
        Pool *p = _c4cthis->get_pool(pool);
        p->release(pos, n);
    }

    C4_ALWAYS_INLINE void *get(I id) const
    {
        I pool = decode_pool(id);
        I pos = decode_pos(id);
        Pool *p = _c4cthis->get_pool(pool);
        return p->get(pos);
    }

public:

    C4_CONSTEXPR14 C4_ALWAYS_INLINE I encode_id(I pool_, I pos_) const
    {
        return (pool_ << _c4cthis->_pool_shift()) | pos_;
    }

    C4_CONSTEXPR14 C4_ALWAYS_INLINE I decode_pool(I id) const
    {
        return (id & _c4cthis->_pool_mask()) >> _c4cthis->_pool_shift();
    }

    C4_CONSTEXPR14 C4_ALWAYS_INLINE I decode_pos(I id) const
    {
        return (id & (~_c4cthis->_type_mask());
    }

};

#undef _c4this
#undef _c4cthis
};
C4_END_NAMESPACE(detail)


/** pool collection with a compile time-fixed number of pools */
template< class Pool, size_t NumPoolsMax >
struct pool_collection : public detail::_pool_collection_crtp< Pool, pool_collection< Pool, NumPoolsMax >
{
    static_assert((sizeof(Pool) >= alignof(Pool)) && (sizeof(Pool) % alignof(Pool) == 0), "array of pools would align to a lower value");

    alignas(alignof(Pool)) char m_pools[NumPoolsMax * sizeof(Pool)];
    I m_num_pools;

public:

    using I = typename Pool::index_type;

    enum : I {
        s_pool_bits  = msb(NumPoolsMax),
        s_pool_shift = 8 * sizeof(I) - s_pool_bits,
        s_pool_mask  = ((I(1) << s_pool_bits) - I(1)) << s_pool_shift,
        s_pos_mask   = (~(I(1) & I(0))) & (~s_pool_mask)
    };

    static constexpr C4_ALWAYS_INLINE _pool_shift() { return s_pool_shift; }
    static constexpr C4_ALWAYS_INLINE _pool_mask() { return s_pool_mask; }
    static constexpr C4_ALWAYS_INLINE _pos_mask() { return s_pos_mask; }

public:

    static C4_CONSTEXPR14 C4_ALWAYS_INLINE I capacity() { return NumPoolsMax; }
    C4_CONSTEXPR14 C4_ALWAYS_INLINE I num_pools() const { return m_size; }

    C4_CONSTEXPR14 C4_ALWAYS_INLINE Pool* get_pool(I pool)
    {
        C4_ASSERT(pool <= m_num_pools);
        return reinterpret_cast< Pool* >(m_pools + pool * sizeof(Pool));
    }

    C4_CONSTEXPR14 C4_ALWAYS_INLINE Pool const* get_pool(I pool) const
    {
        C4_ASSERT(pool <= m_num_pools);
        return reinterpret_cast< Pool const* >(m_pools + pool * sizeof(Pool));
    }

public:

    template< class... PoolArgs >
    I add_pool(PoolArgs && ...args)
    {
        C4_ASSERT(m_num_pools < NumPoolsMax);
        I pool_id = m_num_pools;
        ++m_num_pools;
        Pool *p = get_pool(pool_id);
        new ((void*)p) Pool(std::forward< PoolArgs >(args)...);
        return pool_id;
    }

    void clear()
    {
        for(I i = 0; i < m_num_pools; ++i)
        {
            Pool *p = get_pool(i);
            p->~Pool();
        }
        m_num_pools = 0;
    }


    void free()
    {
        for(I i = 0; i < m_num_pools; ++i)
        {
            Pool *p = get_pool(i);
            p->~Pool();
        }
        m_num_pools = 0;
    }

public:

    using iterator = Pool*;
    using const_iterator = Pool const*;

    iterator begin() { return m_pools; }
    iterator end  () { return m_pools + m_num_pools; }

    const_iterator begin() const { return m_pools; }
    const_iterator end  () const { return m_pools + m_num_pools; }
};


#ifdef C4_WORK_IN_PROGRESS
/** pool collection with a run time-determined number of pools */
template< class Pool >
struct pool_collection< Pool, 0 > : public detail::_pool_collection_crtp< Pool, pool_collection< Pool, 0 >
{
    using I = typename Pool::index_type;

    Pool *m_pools{nullptr};
    I m_num_pools;
    I m_capacity;

};
#endif // C4_WORK_IN_PROGRESS

} // namespace c4

#endif /* _C4_POOL_HPP_ */
