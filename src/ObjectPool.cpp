/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#include <Moe.UV/ObjectPool.hpp>

using namespace std;
using namespace moe;
using namespace UV;

thread_local static ObjectPool* t_pObjectPool = nullptr;

void UV::details::ObjectPoolDeleter<void>::operator()(void* p)noexcept
{
    auto pool = ObjectPool::GetCurrent();
    assert(pool);
    pool->RawFree(p);
}

ObjectPool* ObjectPool::GetCurrent()noexcept
{
    return t_pObjectPool;
}

ObjectPool::BufferPtr ObjectPool::Alloc(size_t sz)
{
    auto pool = GetCurrent();
    if (!pool)
        MOE_THROW(InvalidCallException, "Bad execution context");
    return BufferPtr(pool->RawAlloc(sz));
}

ObjectPool::ObjectPool()
{
    if (t_pObjectPool)
        MOE_THROW(InvalidCallException, "ObjectPool is already existed");
    t_pObjectPool = this;
}

ObjectPool::~ObjectPool()
{
    t_pObjectPool = nullptr;
}
