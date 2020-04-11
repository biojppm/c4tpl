#include "c4/tpl/pool.hpp"
#include <gtest/gtest.h>

namespace c4 {

using plin = pool_linear<size_t, Allocator<char,MemRes>>;
using ppag = pool_linear_paged<256, size_t, Allocator<char,MemRes>>;

template<class PoolLinear>
void test_pool_linear_instantiation()
{
    PoolLinear p;

    EXPECT_EQ(p.size(), 0);
    EXPECT_EQ(p.capacity(), 0);
}

template<class PoolLinearPaged>
void test_pool_linear_paged_instantiation()
{
    PoolLinearPaged p;

    EXPECT_EQ(p.size(), 0);
    EXPECT_EQ(p.capacity(), 0);
}

TEST(pool_linear, instantiation)
{
    test_pool_linear_instantiation<plin>();
}

TEST(pool_linear_paged, basic)
{
    test_pool_linear_paged_instantiation<ppag>();
}


//-----------------------------------------------------------------------------

template<class PoolLinearPaged>
void do_test_paged_indices(size_t pg, size_t pos, size_t id)
{
    EXPECT_EQ(PoolLinearPaged::_page(id), pg)    << "pg=" << pg << "  pos=" << pos << "  id=" << id;
    EXPECT_EQ(PoolLinearPaged::_pos(id), pos)    << "pg=" << pg << "  pos=" << pos << "  id=" << id;
    EXPECT_EQ(PoolLinearPaged::_id(pg, pos), id) << "pg=" << pg << "  pos=" << pos << "  id=" << id;
}

TEST(pool_linear_paged, indices)
{
    size_t id = 0;
    for(size_t page = 0; page <= 8; ++page)
    {
        for(size_t pos = 0; pos < ppag::page_size(); ++pos)
        {
            do_test_paged_indices<ppag>(page, pos, id);
            ++id;
        }
    }
}


template<class PoolCollection>
void do_test_pool_collection_indices(PoolCollection &p, size_t pool, size_t pos)
{
    size_t id = p.encode_id(pool, pos);
    EXPECT_EQ(p.decode_pool(id), pool) << "pool=" << pool << "  pos=" << pos << "  id=" << id;
    EXPECT_EQ(p.decode_pos(id), pos)   << "pool=" << pool << "  pos=" << pos << "  id=" << id;

    size_t pool2 = p.decode_pool(id);
    size_t pos2 = p.decode_pos(id);
    EXPECT_EQ(p.encode_id(pool2, pos2), id) << "pool=" << pool << "  pos=" << pos << "  id=" << id;
}

template<class Pool>
void test_pool_collection_indices()
{
    using pcol = pool_collection<Pool, 16>;
    pcol p;

    size_t id = 0;
    for(size_t pool = 0; pool < p.s_num_pools_max; ++pool)
    {
        for(size_t pos = 0; pos < 512; ++pos)
        {
            do_test_pool_collection_indices(p, pool, pos);
            ++id;
        }
    }
}


TEST(pool_linear, collection_indices)
{
    test_pool_collection_indices<plin>();
}


TEST(pool_linear_paged, collection_indices)
{
    test_pool_collection_indices<ppag>();
}


} // namespace c4
