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
        m_stSignalCondVar.Resume(static_cast<ptrdiff_t>(false));
        return true;
    }
    return false;
}

bool Signal::CoWait()
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Signal is already closed");
    auto ret = Coroutine::Suspend(m_stSignalCondVar);
    return static_cast<bool>(ret);
}

void Signal::OnClose()noexcept
{
    IoHandle::OnClose();

    // 通知协程取消
    m_stSignalCondVar.Resume(static_cast<ptrdiff_t>(false));
}

void Signal::OnSignal(int signum)noexcept
{
    if (m_iWatchedSignum == signum)
        m_stSignalCondVar.Resume(static_cast<ptrdiff_t>(true));

    if (m_stCallback)
    {
        MOE_UV_EAT_EXCEPT_BEGIN
            m_stCallback(signum);
        MOE_UV_EAT_EXCEPT_END
    }
}
