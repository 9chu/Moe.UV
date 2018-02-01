/**
 * @file
 * @author chu
 * @date 2017/12/27
 */
#include <Moe.UV/Signal.hpp>

using namespace std;
using namespace moe;
using namespace UV;

IoHandleHolder<Signal> Signal::Create()
{
    return IoHandleHolder<Signal>(ObjectPool::Create<Signal>());
}

void Signal::OnUVSignal(uv_signal_t* handle, int signum)noexcept
{
    auto* self = GetSelf<Signal>(handle);
    self->OnSignal(signum);
}

Signal::Signal()
{
    MOE_UV_CHECK(::uv_signal_init(GetCurrentUVLoop(), &m_stHandle));
    BindHandle(reinterpret_cast<::uv_handle_t*>(&m_stHandle));
}

void Signal::Start(int signum)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Signal is already closed");
    MOE_UV_CHECK(::uv_signal_start(&m_stHandle, OnUVSignal, signum));
    m_iWatchedSignum = signum;
}

bool Signal::Stop()noexcept
{
    if (IsClosing())
        return false;
    auto ret = ::uv_signal_stop(&m_stHandle);
    if (ret == 0)
    {
        // 通知协程取消
        CancelWait();
        return true;
    }
    return false;
}

bool Signal::CoWait(Time::Tick timeout)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Signal is already closed");
    
    auto self = RefFromThis();  // 此时持有一个强引用，这意味着必须由外部事件强制触发，否则不会释放
    return m_stSignalEvent.Wait(timeout) == CoEvent::WaitResult::Succeed;
}

void Signal::CancelWait()noexcept
{
    // 通知协程取消
    m_stSignalEvent.Cancel();
}

void Signal::OnClose()noexcept
{
    IoHandle::OnClose();

    // 通知协程取消
    CancelWait();
}

void Signal::OnSignal(int signum)noexcept
{
    if (m_iWatchedSignum == signum)
        m_stSignalEvent.Resume();

    if (m_stCallback)
    {
        MOE_UV_EAT_EXCEPT_BEGIN
            m_stCallback(signum);
        MOE_UV_EAT_EXCEPT_END
    }
}
