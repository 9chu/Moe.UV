/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#include <Moe.UV/RunLoop.hpp>

#include <chrono>
#include <thread>

#include <Moe.Core/Exception.hpp>
#include <Moe.Core/Logging.hpp>

using namespace std;
using namespace moe;
using namespace UV;

static thread_local RunLoop* t_pRunLoop = nullptr;

RunLoop* RunLoop::GetCurrent()noexcept
{
    return t_pRunLoop;
}

Time::Tick RunLoop::Now()noexcept
{
    auto self = GetCurrent();
    if (!self)
        return ::uv_hrtime();
    return ::uv_now(&self->m_stLoop);
}

::uv_loop_t* RunLoop::GetCurrentUVLoop()
{
    auto self = GetCurrent();
    if (!self)
        MOE_THROW(InvalidCallException, "Bad execution context");
    return &self->m_stLoop;
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

RunLoop::RunLoop(ObjectPool& pool)
    : m_stObjectPool(pool)
{
    if (t_pRunLoop)
        MOE_THROW(InvalidCallException, "RunLoop is already existed");

    MOE_UV_CHECK(::uv_loop_init(&m_stLoop));

    t_pRunLoop = this;
}

RunLoop::~RunLoop()
{
    static const int kMaxLoopTimeout = 5000;

    m_bClosing = true;

    // 关闭所有句柄
    auto start = ::uv_now(&m_stLoop);
    while (::uv_loop_alive(&m_stLoop))
    {
        ForceCloseAllHandle();
        ::uv_run(&m_stLoop, UV_RUN_ONCE);

        this_thread::yield();

        auto now = ::uv_now(&m_stLoop);
        if (now - start > kMaxLoopTimeout)
            break;  // 超时N秒
    }

    // 关闭Loop句柄
    int ret = ::uv_loop_close(&m_stLoop);
    if (ret != 0)
    {
        MOE_UV_LOG_ERROR(ret);
        assert(ret == 0);

        // 必须主动关闭所有句柄才能正常退出
        ::terminate();
    }

    t_pRunLoop = nullptr;
}

void RunLoop::Run()
{
    ::uv_run(&m_stLoop, UV_RUN_DEFAULT);
}

void RunLoop::Stop()noexcept
{
    ::uv_stop(&m_stLoop);
}

void RunLoop::RunOnce(bool wait)
{
    ::uv_run(&m_stLoop, wait ? UV_RUN_ONCE : UV_RUN_NOWAIT);
}

void RunLoop::ForceCloseAllHandle()noexcept
{
    ::uv_walk(&m_stLoop, UVClosingHandleWalker, this);
}

void RunLoop::UpdateTime()noexcept
{
    ::uv_update_time(&m_stLoop);
}
