/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#include <Moe.UV/RunLoop.hpp>
#include <Moe.Core/Exception.hpp>

using namespace std;
using namespace moe;
using namespace UV;

thread_local static RunLoop* t_pRunLoop = nullptr;

void RunLoop::OnUVIdle(::uv_idle_t* handle)noexcept
{
    auto self = static_cast<RunLoop*>(handle->data);
    self->OnIdle();
}

RunLoop* RunLoop::GetCurrent()noexcept
{
    return t_pRunLoop;
}

Time::Tick RunLoop::Now()noexcept
{
    auto runloop = GetCurrent();
    if (!runloop)
        return static_cast<Time::Tick>(Time::GetHiResTickCount());
    return ::uv_now(&runloop->m_stLoop);
}

RunLoop::RunLoop(size_t coroutineSharedStackSize)
    : m_stScheduler(coroutineSharedStackSize)
{
    if (t_pRunLoop)
        MOE_THROW(InvalidCallException, "RunLoop is already existed");

    MOE_UV_CHECK(::uv_loop_init(&m_stLoop));

    try
    {
        MOE_UV_CHECK(::uv_idle_init(&m_stLoop, &m_stIdle));
        m_stIdle.data = this;
    }
    catch (...)
    {
        ::uv_loop_close(&m_stLoop);
        throw;
    }

    t_pRunLoop = this;
}

RunLoop::~RunLoop()
{
    t_pRunLoop = nullptr;

    // 关闭Idle句柄
    ::uv_close(reinterpret_cast<uv_handle_t*>(&m_stIdle), nullptr);

    // 强制干掉所有句柄
    if (::uv_loop_alive(&m_stLoop) != 0)
    {
        MOE_DEBUG("Closing all alive handles");

        while (::uv_loop_alive(&m_stLoop) != 0 && !m_stScheduler.IsIdle())
        {
            ::uv_walk(&m_stLoop, IOHandle::OnUVCloseHandleWalker, nullptr);
            ::uv_run(&m_stLoop, UV_RUN_NOWAIT);  // 执行Close回调

            m_stScheduler.Schedule();
        }
    }

    // 关闭Loop句柄
    int ret = ::uv_loop_close(&m_stLoop);
    MOE_UV_LOG_ERROR(ret);
    assert(ret == 0);

    MOE_DEBUG("Destroyed");
}

void RunLoop::Run()
{
    MOE_DEBUG("Starting loop");

    MOE_UV_CHECK(::uv_idle_start(&m_stIdle, OnUVIdle));
    ::uv_run(&m_stLoop, UV_RUN_DEFAULT);
    ::uv_idle_stop(&m_stIdle);

    MOE_DEBUG("Loop stopped");
}

void RunLoop::Stop()noexcept
{
    ::uv_stop(&m_stLoop);
}

void RunLoop::UpdateTime()noexcept
{
    ::uv_update_time(&m_stLoop);
}

void RunLoop::OnIdle()noexcept
{
    m_stScheduler.Schedule();
}
