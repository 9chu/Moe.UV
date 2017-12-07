/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#include <Moe.UV/RunLoop.hpp>
#include <Moe.Core/Exception.hpp>
#include <chrono>
#include <thread>

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
    auto runloop = GetCurrent();
    if (!runloop)
        return ::uv_hrtime();
    return ::uv_now(&runloop->m_stLoop);
}

::uv_loop_t* RunLoop::GetCurrentUVLoop()
{
    auto runloop = GetCurrent();
    if (!runloop)
        MOE_THROW(InvalidCallException, "Bad execution context");
    return &runloop->m_stLoop;
}

void RunLoop::UVClosingHandleWalker(::uv_handle_t* handle, void* arg)noexcept
{
    MOE_UNUSED(arg);
    if (!::uv_is_closing(handle))
    {
        if (handle->data != nullptr)
            ::uv_close(handle, IoHandle::OnUVClose);
        else
        {
            assert(false);
            ::uv_close(handle, nullptr);
        }
    }
}

RunLoop::RunLoop(size_t coroutineSharedStackSize)
    : m_stScheduler(coroutineSharedStackSize)
{
    if (t_pRunLoop)
        MOE_THROW(InvalidCallException, "RunLoop is already existed");

    MOE_UV_CHECK(::uv_loop_init(&m_stLoop));

    t_pRunLoop = this;

    // 创建Idle句柄
    try
    {
        m_stIdleHandle = IdleHandle::Create(std::bind(&RunLoop::OnIdle, this));
        m_stIdleHandle->Unref();  // 后台对象
    }
    catch (...)
    {
        t_pRunLoop = nullptr;

        MOE_UV_CHECK(::uv_loop_close(&m_stLoop));
        throw;
    }
}

RunLoop::~RunLoop()
{
    m_bClosing = true;

    // 关闭Idle句柄
    m_stIdleHandle.Clear();

    // 等待所有句柄和请求结束
    while (::uv_loop_alive(&m_stLoop) != 0 || !m_stScheduler.IsIdle())
    {
        ::uv_walk(&m_stLoop, UVClosingHandleWalker, nullptr);
        ::uv_run(&m_stLoop, UV_RUN_NOWAIT);  // 执行Close回调

        m_stScheduler.Schedule();

        this_thread::sleep_for(chrono::milliseconds(5));
    }

    // 关闭Loop句柄
    int ret = ::uv_loop_close(&m_stLoop);
    MOE_UV_LOG_ERROR(ret);
    assert(ret == 0);

    t_pRunLoop = nullptr;
}

void RunLoop::Run()
{
    // 先执行一次协程调度，防止在uv_run之前对象还没创建出来就退出了循环
    m_stScheduler.Schedule();

    // 启动Idle句柄
    m_stIdleHandle->Start();

    // 开始程序循环
    ::uv_run(&m_stLoop, UV_RUN_DEFAULT);

    // 终止Idle句柄
    m_stIdleHandle->Stop();
}

void RunLoop::Stop()noexcept
{
    ::uv_stop(&m_stLoop);
}

void RunLoop::UpdateTime()noexcept
{
    ::uv_update_time(&m_stLoop);
}

void RunLoop::OnIdle()
{
    m_stScheduler.Schedule();
}
