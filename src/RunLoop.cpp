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
    RunLoop* self = static_cast<RunLoop*>(arg);

    if (handle == self->m_stPrepareHandle->GetHandle() ||
        handle == self->m_stCheckHandle->GetHandle() ||
        handle == self->m_stSchedulerTimeout->GetHandle())
    {
        // 忽略内部句柄
        return;
    }

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

RunLoop::RunLoop(Time::Tick minimalTick, size_t coroutineSharedStackSize)
    : m_stScheduler(m_stObjectPool.GetPool(), 0, coroutineSharedStackSize), m_uMinimalTick(minimalTick)
{
    if (t_pRunLoop)
        MOE_THROW(InvalidCallException, "RunLoop is already existed");

    MOE_UV_CHECK(::uv_loop_init(&m_stLoop));

    t_pRunLoop = this;

    // 创建Idle句柄
    try
    {
        m_stPrepareHandle = PrepareHandle::Create(std::bind(&RunLoop::OnPrepare, this));
        m_stCheckHandle = CheckHandle::Create(std::bind(&RunLoop::OnCheck, this));
        m_stSchedulerTimeout = Timeout::Create();

        // 后台对象
        m_stPrepareHandle->Unref();
        m_stCheckHandle->Unref();
    }
    catch (...)
    {
        // 删除句柄
        m_stSchedulerTimeout.Clear();
        m_stCheckHandle.Clear();
        m_stPrepareHandle.Clear();

        t_pRunLoop = nullptr;

        MOE_UV_CHECK(::uv_loop_close(&m_stLoop));
        throw;
    }

    // 刷新调度器时间
    m_stScheduler.Update(::uv_now(&m_stLoop));
}

RunLoop::~RunLoop()
{
    m_bClosing = true;

    // 删除句柄
    m_stSchedulerTimeout.Clear();
    m_stCheckHandle.Clear();
    m_stPrepareHandle.Clear();
    
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
    // 启动句柄
    m_stPrepareHandle->Start();
    m_stCheckHandle->Start();

    // 开始程序循环
    ::uv_run(&m_stLoop, UV_RUN_DEFAULT);

    // 终止句柄
    m_stSchedulerTimeout->Stop();
    m_stPrepareHandle->Stop();
    m_stCheckHandle->Stop();
}

void RunLoop::Stop()noexcept
{
    ::uv_stop(&m_stLoop);
}

void RunLoop::ForceCloseAllHandle()noexcept
{
    ::uv_walk(&m_stLoop, UVClosingHandleWalker, this);
}

void RunLoop::UpdateTime()noexcept
{
    ::uv_update_time(&m_stLoop);
}

void RunLoop::OnPrepare()
{
    if (m_stPrepareCallback)
    {
        MOE_UV_EAT_EXCEPT_BEGIN
            m_stPrepareCallback();
        MOE_UV_EAT_EXCEPT_END
    }

    // 计时器调度发生在OnPrepare之前
    // 因此在这里执行由计时器调度触发的协程
    auto sleep = m_stScheduler.Update(::uv_now(&m_stLoop));
    m_stScheduler.IsEmpty() ? m_stSchedulerTimeout->Unref() : m_stSchedulerTimeout->Ref();
    if (sleep == Coroutine::kInfinityTimeout)
        m_stSchedulerTimeout->Stop();
    else
    {
        if (sleep < m_uMinimalTick)
            sleep = m_uMinimalTick;
        auto ret = m_stSchedulerTimeout->Start(sleep);
        MOE_UNUSED(ret);
        assert(ret);
    }
}

void RunLoop::OnCheck()
{
    // 句柄处理完成后执行OnCheck
    // 因此在这里执行由句柄事件调度触发的协程
    m_stScheduler.Update(::uv_now(&m_stLoop));

    if (m_stCheckCallback)
    {
        MOE_UV_EAT_EXCEPT_BEGIN
            m_stCheckCallback();
        MOE_UV_EAT_EXCEPT_END
    }
}
