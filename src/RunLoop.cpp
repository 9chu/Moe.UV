/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#include <Moe.UV/RunLoop.hpp>

#include <chrono>
#include <thread>

#include "UV.inl"

using namespace std;
using namespace moe;
using namespace UV;

thread_local static RunLoop* t_pRunLoop = nullptr;

RunLoop* RunLoop::GetCurrent()noexcept
{
    return t_pRunLoop;
}

Time::Tick RunLoop::Now()noexcept
{
    auto self = GetCurrent();
    if (!self)
        return ::uv_hrtime();
    return ::uv_now(self->GetHandle());
}

void RunLoop::UVClosingHandleWalker(::uv_handle_t* handle, void* arg)noexcept
{
    RunLoop* self = static_cast<RunLoop*>(arg);
    MOE_UNUSED(self);

    if (!::uv_is_closing(handle))
    {
        if (handle->data != nullptr)
            ::uv_close(handle, AsyncHandle::OnUVClose);
        else
        {
            assert(false);
            ::uv_close(handle, nullptr);
        }
    }
}

RunLoop::RunLoop(ObjectPool& pool, bool useDefaultLoop)
    : m_stObjectPool(pool)
{
    if (t_pRunLoop)
        MOE_THROW(InvalidCallException, "RunLoop is already existed");

    if (!useDefaultLoop)
    {
        // 此处不能使用MOE_UV_NEW
#ifndef NDEBUG
        auto p = GetObjectPool().Alloc(sizeof(uv_loop_t), ObjectPool::AllocContext(__FILE__, __LINE__));
#else
        auto p = GetObjectPool().Alloc(sizeof(uv_loop_t));
#endif
        m_pHandle.reset(new(p.get()) uv_loop_t());
        p.release();

        MOE_UV_CHECK(::uv_loop_init(GetHandle()));
    }

    t_pRunLoop = this;
}

RunLoop::~RunLoop()
{
    static const int kMaxLoopTimeout = 5000;

    m_bClosing = true;

    // 关闭所有句柄
    auto start = ::uv_now(GetHandle());
    while (::uv_loop_alive(GetHandle()))
    {
        ForceCloseAllHandle();
        ::uv_run(GetHandle(), UV_RUN_ONCE);

        this_thread::yield();

        auto now = ::uv_now(GetHandle());
        if (now - start > kMaxLoopTimeout)
            break;  // 超时N秒
    }

    if (m_pHandle)
    {
        // 关闭Loop句柄
        int ret = ::uv_loop_close(GetHandle());
        if (ret != 0)
        {
            MOE_UV_LOG_ERROR(ret);
            assert(ret == 0);

            // 必须主动关闭所有句柄才能正常退出
            ::terminate();
        }
    }

    t_pRunLoop = nullptr;
}

::uv_loop_s* RunLoop::GetHandle()const noexcept
{
    auto ret = m_pHandle.get();
    if (!ret)
        return ::uv_default_loop();
    return ret;
}

void RunLoop::Run()
{
    ::uv_run(GetHandle(), UV_RUN_DEFAULT);
}

void RunLoop::Stop()noexcept
{
    ::uv_stop(GetHandle());
}

void RunLoop::RunOnce(bool wait)
{
    ::uv_run(GetHandle(), wait ? UV_RUN_ONCE : UV_RUN_NOWAIT);
}

void RunLoop::ForceCloseAllHandle()noexcept
{
    ::uv_walk(GetHandle(), UVClosingHandleWalker, this);
}

void RunLoop::UpdateTime()noexcept
{
    ::uv_update_time(GetHandle());
}
