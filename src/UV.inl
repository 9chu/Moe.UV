#include <Moe.UV/RunLoop.hpp>
#include <Moe.Core/Logging.hpp>

#include <uv.h>

namespace
{
    ::uv_loop_t* GetCurrentUVLoop()
    {
        auto loop = moe::UV::RunLoop::GetCurrent();
        if (!loop)
            MOE_THROW(moe::InvalidCallException, "Runloop required");
        return loop->GetHandle();
    }

    template <typename T, typename P>
    T* GetSelf(P* handle)
    {
        auto* base = static_cast<moe::UV::AsyncHandle*>(handle->data);
        if (!base)
            return nullptr;
        return static_cast<T*>(base);
    }

    template <typename T>
    moe::UniquePooledObject<::uv_handle_t> CastHandle(moe::UniquePooledObject<T>&& rhs)noexcept
    {
        moe::UniquePooledObject<::uv_handle_t> ret;
        ret.reset(reinterpret_cast<::uv_handle_t*>(rhs.release()));
        return ret;
    }
}

#define MOE_UV_GET_SELF(T) \
    auto self = GetSelf<T>(handle); \
    do \
    { \
        if (!self) \
        { \
            assert(::uv_is_closing(reinterpret_cast<::uv_handle_s*>(handle))); \
            return; \
        } \
    } while (false)

#define MOE_UV_GET_HANDLE(T) \
    if (IsClosing()) \
        MOE_THROW(moe::InvalidCallException, "Handle is already disposed"); \
    auto handle = reinterpret_cast<T*>(GetHandle())

#define MOE_UV_GET_HANDLE_NOTHROW(T) \
    auto handle = reinterpret_cast<T*>(GetHandle())

#ifndef NDEBUG
#define MOE_UV_NEW(T, ...) \
    UniquePooledObject<T> object; \
    do { \
        auto loop = moe::UV::RunLoop::GetCurrent(); \
        if (!loop) \
            MOE_THROW(moe::InvalidCallException, "RunLoop is not created"); \
        auto p = loop->GetObjectPool().Alloc(sizeof(T), ObjectPool::AllocContext(__FILE__, __LINE__)); \
        object.reset(new(p.get()) T(__VA_ARGS__)); \
        p.release(); \
    } while (false)
#else
#define MOE_UV_NEW(T, ...) \
    UniquePooledObject<T> object; \
    do { \
        auto loop = moe::UV::RunLoop::GetCurrent(); \
        if (!loop) \
            MOE_THROW(moe::InvalidCallException, "RunLoop is not created"); \
        auto p = loop->GetObjectPool().Alloc(sizeof(T)); \
        object.reset(new(p.get()) T(__VA_ARGS__)); \
        p.release(); \
    } while (false)
#endif

#ifndef NDEBUG
#define MOE_UV_ALLOC(sz) \
    UniquePooledObject<void> buffer; \
    do { \
        auto loop = moe::UV::RunLoop::GetCurrent(); \
        if (!loop) \
            MOE_THROW(InvalidCallException, "RunLoop is not created"); \
        buffer = loop->GetObjectPool().Alloc((sz), ObjectPool::AllocContext(__FILE__, __LINE__)); \
    } while (false)
#else
#define MOE_UV_ALLOC(sz) \
    UniquePooledObject<void> buffer; \
    do { \
        auto loop = moe::UV::RunLoop::GetCurrent(); \
        if (!loop) \
            MOE_THROW(InvalidCallException, "RunLoop is not created"); \
        buffer = loop->GetObjectPool().Alloc(sz); \
    } while (false)
#endif

#define MOE_UV_THROW(status) \
    do { \
        if ((status) == UV_ECANCELED) \
            MOE_THROW(moe::OperationCancelledException, "Operation is cancelled"); \
        const char* err = ::uv_strerror((status)); \
        MOE_THROW(moe::ApiException, "libuv error {0}: {1}", status, err); \
    } while (false)

#define MOE_UV_LOG_ERROR(status) \
    do { \
        if ((status) < 0) { \
            const char* err = ::uv_strerror((status)); \
            MOE_LOG_ERROR("libuv error {0}: {1}", (status), err); \
        } \
    } while (false)

#define MOE_UV_CHECK(status) \
    do { \
        auto ret = (status); \
        if (ret < 0) \
            MOE_UV_THROW(ret); \
    } while (false)

#define MOE_UV_CATCH_ALL_BEGIN \
    try {

#define MOE_UV_CATCH_ALL_END \
    } catch (const moe::ExceptionBase& ex) { \
        MOE_LOG_ERROR("Uncaught exception, desc: {0}", ex); \
    } \
    catch (const std::exception& ex) { \
        MOE_LOG_ERROR("Uncaught exception, desc: {0}", ex.what()); \
    } \
    catch (...) { \
        MOE_LOG_ERROR("Uncaught unknown exception"); \
    }

#if 0
template <typename T>
class RunLoopAllocator
{
public:
    typedef T value_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

    template <typename U>
    struct rebind
    {
        typedef RunLoopAllocator<U> other;
    };

public:
    RunLoopAllocator()noexcept {}
    ~RunLoopAllocator()noexcept {}

    RunLoopAllocator(const RunLoopAllocator&)noexcept {}

    template <class U>
    RunLoopAllocator(const RunLoopAllocator<U>&)noexcept {}

    pointer address(reference value)const { return &value; }
    const_pointer address(const_reference value)const { return &value; }

    size_type max_size()const noexcept { return std::numeric_limits<std::size_t>::max() / sizeof(T); }

    pointer allocate(size_type num, const void* hint=nullptr)
    {
        MOE_UNUSED(hint);
        MOE_UV_ALLOC(num * sizeof(T));
        return static_cast<T*>(buffer.release());
    }

    void deallocate(pointer p, size_type num)
    {
        MOE_UNUSED(num);
        UniquePooledObject<void> object;
        object.reset(p);
    }

    void construct(pointer p, const T& value) { new((void*)p) T(value); }
    void destroy(pointer p) { p->~T(); }
};
#endif
