/**
 * @file
 * @author chu
 * @date 2017/12/7
 */
#include <Moe.UV/Timeout.hpp>
#include <Moe.UV/RunLoop.hpp>

using namespace std;
using namespace moe;
using namespace UV;

IoHandleHolder<Timeout> Timeout::Create()
{
    auto self = IoHandleHolder<Timeout>(ObjectPool::Create<Timeout>());
    return self;
}

void Timeout::OnUVTimer(::uv_timer_t* handle)noexcept
{
    auto* self = GetSelf<Timeout>(handle);
    self->OnTick();
}

Timeout::Timeout()
{
    MOE_UV_CHECK(::uv_timer_init(GetCurrentUVLoop(), &m_stHandle));
    BindHandle(reinterpret_cast<::uv_handle_t*>(&m_stHandle));
}

bool Timeout::Start(Time::Tick timeout)noexcept
{
    if (IsClosing())
        return false;

    // 理论上不会抛出错误
    int ret = ::uv_timer_start(&m_stHandle, OnUVTimer, timeout, 0);
    MOE_UNUSED(ret);
    assert(ret == 0);
    return true;
}

void Timeout::Stop()noexcept
{
    if (IsClosing())
        return;

    // 理论上总是成功的
    auto ret = ::uv_timer_stop(&m_stHandle);
    MOE_UNUSED(ret);
    assert(ret == 0);
    
    // 通知协程取消
    CancelWait();
}

bool Timeout::CoWait()
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Timer is already closed");

    auto self = RefFromThis();  // 此时持有一个强引用，这意味着必须由外部事件强制触发，否则不会释放
    return m_stTimeoutEvent.Wait() == CoEvent::WaitResult::Succeed;
}

void Timeout::CancelWait()noexcept
{
    m_stTimeoutEvent.Cancel();
}

void Timeout::OnClose()noexcept
{
    IoHandle::OnClose();

    // 通知协程取消
    CancelWait();
}

void Timeout::OnTick()noexcept
{
    m_stTimeoutEvent.Resume();

    if (m_stCallback)
    {
        MOE_UV_EAT_EXCEPT_BEGIN
            m_stCallback();
        MOE_UV_EAT_EXCEPT_END
    }
}
