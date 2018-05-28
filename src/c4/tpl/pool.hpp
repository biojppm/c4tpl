#ifndef _C4_POOL_HPP_
#define _C4_POOL_HPP_

#include "c4/allocator.hpp"

namespace c4 {

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

template<class I, class Allocator>
struct pool_linear
{
    static_assert(std::is_same<char, typename Allocator::value_type>::value, "Allocator must be a raw allocator");

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
    tight_pair<I, Allocator> m_capacity_allocator;

public:

    C4_NO_COPY_OR_MOVE(pool_linear);

    pool_linear()
        :
        m_mem{nullptr},
        m_obj_size{0},
        m_obj_align{0},
        m_num_objs{0},
        m_capacity_allocator{0, {}}
    {
    }

    template<class ...AllocatorArgs>
    pool_linear(varargs_t, AllocatorArgs && ...args)
        :
        pool_linear()
    {
        m_capacity_allocator.second = {std::forward<AllocatorArgs>(args)...};
    }

    template<class ...AllocatorArgs>
    pool_linear(I obj_size, I obj_align, I capacity, varargs_t, AllocatorArgs && ...args)
        :
        pool_linear(varargs, std::forward<AllocatorArgs>(args)...)
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

    I capacity() const { return m_capacity_allocator.first(); }
    I capacity_bytes() const { return m_capacity_allocator.first() * m_obj_size; }

    Allocator const& allocator() const { return m_capacity_allocator.second(); }

public:

    void free()
    {
        C4_ASSERT(m_num_objs == 0);
        if(m_mem)
        {
            m_capacity_allocator.second().deallocate((char*)m_mem, m_num_objs * m_obj_size);
        }
    }

    void reserve(I num_objs)
    {
        C4_ERROR_IF(num_objs>= m_capacity_allocator.first() && m_num_objs > 0, "cannot relocate objects in pool");
        C4_ASSERT(m_mem == nullptr);
        m_capacity_allocator.first = num_objs;
        m_mem = m_capacity_allocator.second().allocate(num_objs * m_obj_size, m_obj_align);
        m_num_objs = 0;
    }

public:

    I claim(I n=1)
    {
        C4_ASSERT(n >= 1);
        C4_ERROR_IF(m_num_objs+n >= m_capacity_allocator.first(), "cannot relocate objects in pool");
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
template<size_t PageSize_, class I, class Allocator>
struct pool_linear_paged
{
    static_assert(PageSize_ > 0, "PageSize must be nonzero");
    static_assert((PageSize_ & (PageSize_ - 1)) == 0, "PageSize must be a power of two");
    static_assert(std::is_same<char, typename Allocator::value_type>::value, "Allocator must be a raw allocator");

    using index_type = I;
    using allocator_type = Allocator;

public:

    struct Page
    {
        void * mem;   ///< the memory for this page
        I      numpg; ///< number of pages allocated in this block
                      ///< (the following numpg pages are allocated together
                      ///< with this block, and their numpg is set to 0)
    };

    Page *    m_pages;      ///< the page buffer
    I         m_obj_size;   ///< the size of each object
    I         m_obj_align;  ///< the alignment of each object
    I         m_num_objs;   ///< the current number of objects

    /** first: the number of pages
     * second: the allocator */
    tight_pair<I, Allocator> m_numpg_allocator;

public:

    enum : I
    {
        PageSize = (I)PageSize_,
        /** id mask: all the bits up to PageSize. Use to extract the position
         * of an index within a page. */
        id_mask = PageSize - I(1),
        /** page lsb: the number of bits complementary to PageSize. Use to
         * extract the page of an index. */
        page_lsb = lsb11<I, PageSize>::value,
    };

    static constexpr inline I _page(I id) { return id >> page_lsb; }
    static constexpr inline I _pos (I id) { return id &  id_mask; }
    static constexpr inline I _id(I pg, I pos) { return (pg << page_lsb) | pos; }

public:

    C4_NO_COPY_OR_MOVE(pool_linear_paged);

    pool_linear_paged()
        :
        m_pages{nullptr},
        m_obj_size{0},
        m_obj_align{0},
        m_num_objs{0},
        m_numpg_allocator{0, {}}
    {
    }

    template<class ...AllocatorArgs>
    pool_linear_paged(varargs_t, AllocatorArgs && ...args)
        :
        pool_linear_paged()
    {
        m_numpg_allocator.second = {std::forward<AllocatorArgs>(args)...};
    }

    pool_linear_paged(I obj_size, I obj_align, I capacity)
        :
        pool_linear_paged()
    {
        m_obj_size = obj_size;
        m_obj_align = obj_align;
        reserve(capacity);
    }

    template<class ...AllocatorArgs>
    pool_linear_paged(I obj_size, I obj_align, I capacity, varargs_t, AllocatorArgs && ...args)
        :
        pool_linear_paged(varargs, std::forward<AllocatorArgs>(args)...)
    {
        m_obj_size = obj_size;
        m_obj_align = obj_align;
        reserve(capacity);
    }

    ~pool_linear_paged()
    {
        free();
    }

public:

    I size() const { return m_num_objs; }
    I size_bytes() const { return m_num_objs * m_obj_size; }

    I capacity() const { return m_numpg_allocator.first() * PageSize; }
    I capacity_bytes() const { return m_numpg_allocator.first() * PageSize * m_obj_size; }

    Allocator const& allocator() const { return m_numpg_allocator.second(); }

    I num_pages() const { return m_numpg_allocator.first(); }

    static constexpr inline I page_size() { return PageSize; }

public:

    void reserve(I cap)
    {
        I np = (cap + PageSize - 1) / PageSize;
        if(np <= m_numpg_allocator.first()) return;

        auto a = m_numpg_allocator.second();
        auto pg_a = a.template rebound<Page>();
        I np_old = m_numpg_allocator.first();
        m_numpg_allocator.first() = np;

        // allocate pages arr
        auto * pgs = pg_a.allocate(np);//, m_pages);
        if(m_pages)
        {
            memcpy(pgs, m_pages, np_old * sizeof(Page));
            pg_a.deallocate(m_pages, np_old);
        }
        m_pages = pgs;

        // allocate page mem
        I more_pages = np - np_old;
        auto* mem = a.allocate(more_pages * PageSize * m_obj_size, m_obj_align);//, last);
        // the first page owns the mem (by setting numpg to the number of pages in this mem block)
        m_pages[np_old].mem = mem;
        m_pages[np_old].numpg = more_pages;
        // remaining pages only have their pointers set (and numpg is set to 0)
        for(I i = np_old+1; i < np; ++i)
        {
            m_pages[i].mem = (void*)((char*)mem + i * PageSize);
            m_pages[i].numpg = 0;
        }
    }

    void free()
    {
        C4_ASSERT(m_num_objs == 0);
        I np = m_numpg_allocator.first();
        auto &a = m_numpg_allocator.second();
        if(np == 0) return;
        for(I i = 0; i < np; ++i)
        {
            Page *p = m_pages + i;
            if(p->numpg == 0) continue;
            a.deallocate((char*)p->mem, p->numpg * PageSize * m_obj_size);
            i += p->numpg;
            C4_ASSERT(i <= np);
        }
        a.template rebound<Page>().deallocate(m_pages, np);
        m_numpg_allocator.first() = 0;
        m_pages = nullptr;
    }

    template<class Destructor>
    void destroy(Destructor fn)
    {
        C4_ASSERT( ! !fn);
        for(I id = 0; id < m_num_objs; ++id)
        {
            void *obj = get(id);
            C4_ASSERT(obj != nullptr);
            fn(get(id));
        }
        m_num_objs = 0;
    }

public:

    I claim(I n=1)
    {
        C4_ASSERT(n >= 1);
        if(m_num_objs + n > capacity())
        {
            reserve(m_num_objs + n); // adds a single page
        }
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
            m_num_objs -= n;
        }
    }

    void* get(I id) const
    {
        C4_ASSERT(id < m_num_objs);
        void *mem = ((char*) m_pages[_page(id)].mem) + _pos(id) * m_obj_size;
        return mem;
    }

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

C4_BEGIN_NAMESPACE(detail)
template<class Pool, class CollectionImpl, class I>
struct _pool_collection_crtp
{
#define _c4this  static_cast<CollectionImpl      *>(this)
#define _c4cthis static_cast<CollectionImpl const*>(this)

    I claim(I pool, I n=1)
    {
        C4_ASSERT(pool <= _c4cthis->num_pools());
        I pos = _c4this->get_pool(pool)->claim(n);
        I id = encode_id(pool, pos);
        return id;
    }

    void release(I id, I n=1)
    {
        C4_ASSERT(n>= 1);
        I pool = decode_pool(id);
        I pos = decode_pos(id);
        Pool *p = _c4cthis->get_pool(pool);
        p->release(pos, n);
    }

    C4_ALWAYS_INLINE void *get(I id) const
    {
        I pool = decode_pool(id);
        I pos = decode_pos(id);
        Pool const* p = _c4cthis->get_pool(pool);
        return p->get(pos);
    }

public:

    C4_CONSTEXPR14 C4_ALWAYS_INLINE I encode_id(I pool_, I pos_) const
    {
        C4_ASSERT(pool_ < _c4cthis->_num_pools_max());
        C4_ASSERT(pos_ <= _c4cthis->_pos_mask());
        return (pool_ << _c4cthis->_pool_shift()) | pos_;
    }

    C4_CONSTEXPR14 C4_ALWAYS_INLINE I decode_pool(I id) const
    {
        return id >> _c4cthis->_pool_shift();
    }

    C4_CONSTEXPR14 C4_ALWAYS_INLINE I decode_pos(I id) const
    {
        return id & _c4cthis->_pos_mask();
    }

#undef _c4this
#undef _c4cthis
};
C4_END_NAMESPACE(detail)


//-----------------------------------------------------------------------------

/** pool collection with a max number of pools fixed at compile time */
template<class Pool, size_t NumPoolsMax>
struct pool_collection
    :
    public detail::_pool_collection_crtp<Pool, pool_collection<Pool, NumPoolsMax>, typename Pool::index_type>
{
    static_assert((sizeof(Pool) >= alignof(Pool)) && (sizeof(Pool) % alignof(Pool) == 0), "array of pools would align to a lower value");
    static_assert(NumPoolsMax > 0, "invalid parameter");

public:

    using I = typename Pool::index_type;

public:

#ifdef NDEBUG
    alignas(alignof(Pool)) char m_pools[NumPoolsMax * sizeof(Pool)];
#else
    union
    {
        alignas(alignof(Pool)) Pool m_pool_buf[NumPoolsMax]; ///< this is only defined in debug builds for easier debugging
        alignas(alignof(Pool)) char m_pools[NumPoolsMax * sizeof(Pool)];
    };
#endif
    I m_num_pools;

public:

    enum : I {
        s_num_pools_max = I(NumPoolsMax),
        s_pool_bits  = msb11<I, NumPoolsMax-1>::value, /// it's the MSB of the max pool id (which is NumPoolsMax-1)
        s_pool_shift = I(8) * I(sizeof(I)) - s_pool_bits, /// reserve the highest bits
        s_pos_mask   = (( ~ I(0)) >> s_pool_bits)
    };

    static constexpr C4_ALWAYS_INLINE I _pool_shift() { return s_pool_shift; }
    static constexpr C4_ALWAYS_INLINE I _pos_mask() { return s_pos_mask; }
    static constexpr C4_ALWAYS_INLINE I _num_pools_max() { return s_num_pools_max; }

public:

    pool_collection() : m_num_pools(0)
    {
    }
    ~pool_collection()
    {
        free();
    }

public:

    static C4_CONSTEXPR14 C4_ALWAYS_INLINE I capacity() { return NumPoolsMax; }
    C4_CONSTEXPR14 C4_ALWAYS_INLINE I num_pools() const { return m_num_pools; }
    bool empty() const { return m_num_pools == 0; }

    C4_CONSTEXPR14 C4_ALWAYS_INLINE Pool      * pools()       { return reinterpret_cast<Pool*>(m_pools); }
    C4_CONSTEXPR14 C4_ALWAYS_INLINE Pool const* pools() const { return reinterpret_cast<Pool*>(m_pools); }

    C4_CONSTEXPR14 C4_ALWAYS_INLINE Pool* get_pool(I pool)
    {
        C4_ASSERT(pool <= m_num_pools);
        return reinterpret_cast<Pool*>(m_pools + pool * sizeof(Pool));
    }

    C4_CONSTEXPR14 C4_ALWAYS_INLINE Pool const* get_pool(I pool) const
    {
        C4_ASSERT(pool <= m_num_pools);
        return reinterpret_cast<Pool const*>(m_pools + pool * sizeof(Pool));
    }

public:

    template<class... PoolArgs>
    I add_pool(PoolArgs && ...args)
    {
        C4_ASSERT(m_num_pools < NumPoolsMax);
        I pool_id = m_num_pools;
        ++m_num_pools;
        Pool *p = get_pool(pool_id);
        new ((void*)p) Pool(std::forward<PoolArgs>(args)...);
        return pool_id;
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

    iterator begin() { return reinterpret_cast<Pool *>(m_pools); }
    iterator end  () { return reinterpret_cast<Pool *>(m_pools) + m_num_pools; }

    const_iterator begin() const { return reinterpret_cast<Pool const*>(m_pools); }
    const_iterator end  () const { return reinterpret_cast<Pool const*>(m_pools) + m_num_pools; }

    Pool& front() { C4_ASSERT(m_num_pools > 0); return *(reinterpret_cast<Pool *>(m_pools)); }
    Pool& back () { C4_ASSERT(m_num_pools > 0); return *(reinterpret_cast<Pool *>(m_pools) + m_num_pools); }

    Pool const& front() const { C4_ASSERT(m_num_pools > 0); return *(reinterpret_cast<Pool const*>(m_pools)); }
    Pool const& back () const { C4_ASSERT(m_num_pools > 0); return *(reinterpret_cast<Pool const*>(m_pools) + m_num_pools); }
};


//-----------------------------------------------------------------------------

/** pool collection with a run time-determined number of pools, allocated from the heap */
template<class Pool>
struct pool_collection<Pool, 0>
    :
    public detail::_pool_collection_crtp<Pool, pool_collection<Pool, 0>, typename Pool::index_type>
{
    using I = typename Pool::index_type;

    Pool *m_pools{nullptr};
    I m_num_pools;
    I m_capacity;

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

template<class Pool, class Obj>
struct pool_iterator_impl
{
    using value_type = Obj;

    using pool_type = Pool;
    using index_type = typename Pool::index_type;

    Pool *pool, *last_valid;
    index_type pos;

    pool_iterator_impl(pool_type *pool_, pool_type *last_valid_, index_type pos_)
        : pool(pool_), last_valid(last_valid_), pos(pos_)
    {
        if( ! pool) return;
        _next_if_invalid();
    }

    void _next_if_invalid()
    {
        while(pos == pool->m_num_objs && pool != last_valid)
        {
            pos = 0;
            ++pool;
        }
    }
    void _inc()
    {
        if( ! pool) return;
        ++pos;
        _next_if_invalid();
    }

    pool_iterator_impl operator++ () { _inc(); return *this; }

    value_type* operator-> () { return  (Obj*)pool->get(pos); }
    value_type& operator*  () { return *(Obj*)pool->get(pos); }

    bool operator!= (pool_iterator_impl that) const { return pool != that.pool || pos != that.pos; }
    bool operator== (pool_iterator_impl that) const { return !this->operator!=(that); }
};

} // namespace c4

#endif /* _C4_POOL_HPP_ */
