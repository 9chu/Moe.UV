/**
 * @file
 * @author chu
 * @date 2017/12/7
 */
#include <Moe.UV/Timer.hpp>
#include <Moe.UV/RunLoop.hpp>

using namespace std;
using namespace moe;
using namespace UV;

IoHandleHolder<Timer> Timer::Create(Time::Tick interval)
{
    auto self = IoHandleHolder<Timer>(ObjectPool::Create<Timer>());
    self->SetInterval(interval);
    return self;
}

void Timer::OnUVTimer(::uv_timer_t* handle)noexcept
{
    auto* self = GetSelf<Timer>(handle);
    self->OnTick();
}

Timer::Timer()
{
    MOE_UV_CHECK(::uv_timer_init(GetCurrentUVLoop(), &m_stHandle));
    BindHandle(reinterpret_cast<::uv_handle_t*>(&m_stHandle));
}

bool Timer::Start()noexcept
{
    if (IsClosing())
        return false;

    // 理论上不会抛出错误
    int ret = ::uv_timer_start(&m_stHandle, OnUVTimer, m_ullInterval, m_ullInterval);
    MOE_UNUSED(ret);
    assert(ret == 0);
    return true;
}

void Timer::Stop()noexcept
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

bool Timer::CoWait()
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Timer is already closed");

    auto self = RefFromThis();  // 此时持有一个强引用，这意味着必须由外部事件强制触发，否则不会释放
    return m_stTickEvent.Wait() == CoEvent::WaitResult::Succeed;
}

void Timer::CancelWait()noexcept
{
    m_stTickEvent.Cancel();
}

void Timer::OnClose()noexcept
{
    IoHandle::OnClose();

    // 通知协程取消
    CancelWait();
}

void Timer::OnTick()noexcept
{
    m_stTickEvent.Resume();

    if (m_stCallback)
    {
        MOE_UV_EAT_EXCEPT_BEGIN
            m_stCallback();
        MOE_UV_EAT_EXCEPT_END
    }
}
