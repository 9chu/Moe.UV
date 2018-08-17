/**
 * @file
 * @author chu
 * @date 2018/8/17
 */
#include <Moe.UV/Timer.hpp>

#include <Moe.UV/RunLoop.hpp>
#include <Moe.Core/Logging.hpp>

using namespace std;
using namespace moe;
using namespace UV;

//////////////////////////////////////////////////////////////////////////////// TimerBase

void TimerBase::OnUVTimer(::uv_timer_t* handle)noexcept
{
    auto* self = GetSelf<TimerBase>(handle);

    MOE_UV_CATCH_ALL_BEGIN
        self->OnTime();
    MOE_UV_CATCH_ALL_END
}

TimerBase::TimerBase()
{
    MOE_UV_CHECK(::uv_timer_init(GetCurrentUVLoop(), &m_stHandle));
    BindHandle(reinterpret_cast<::uv_handle_t*>(&m_stHandle));
}

bool TimerBase::Start()noexcept
{
    if (IsClosing())
        return false;

    // 理论上不会抛出错误
    int ret = ::uv_timer_start(&m_stHandle, OnUVTimer, m_ullFirstTime, m_ullInterval);
    MOE_UNUSED(ret);
    assert(ret == 0);
    return true;
}

void TimerBase::Stop()noexcept
{
    if (IsClosing())
        return;

    // 理论上总是成功的
    auto ret = ::uv_timer_stop(&m_stHandle);
    MOE_UNUSED(ret);
    assert(ret == 0);
}

//////////////////////////////////////////////////////////////////////////////// Timer

UniqueAsyncHandlePtr<Timer> Timer::Create()
{
    MOE_UV_NEW(Timer);
    return object;
}

UniqueAsyncHandlePtr<Timer> Timer::CreateTickTimer(Time::Tick interval)
{
    MOE_UV_NEW(Timer);
    object->SetFirstTime(interval);
    object->SetInterval(interval);
    return object;
}

void Timer::OnTime()
{
    if (m_stOnTime)
        m_stOnTime();
}
