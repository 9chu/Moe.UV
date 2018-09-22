/**
 * @file
 * @author chu
 * @date 2018/8/17
 */
#include <Moe.UV/Timer.hpp>

#include "UV.inl"

using namespace std;
using namespace moe;
using namespace UV;

Timer Timer::Create()
{
    MOE_UV_NEW(::uv_timer_t);
    MOE_UV_CHECK(::uv_timer_init(GetCurrentUVLoop(), object.get()));
    return Timer(CastHandle(std::move(object)));
}

Timer Timer::CreateTickTimer(Time::Tick interval)
{
    auto ret = Create();
    ret.SetFirstTime(interval);
    ret.SetInterval(interval);
    return ret;
}

void Timer::OnUVTimer(::uv_timer_t* handle)noexcept
{
    MOE_UV_GET_SELF(Timer);

    MOE_UV_CATCH_ALL_BEGIN
        self->OnTime();
    MOE_UV_CATCH_ALL_END
}

Timer::Timer(Timer&& org)noexcept
    : AsyncHandle(std::move(org)), m_ullFirstTime(org.m_ullFirstTime), m_ullInterval(org.m_ullInterval),
    m_stOnTime(std::move(org.m_stOnTime))
{
}

Timer& Timer::operator=(Timer&& rhs)noexcept
{
    AsyncHandle::operator=(std::move(rhs));
    m_ullFirstTime = rhs.m_ullFirstTime;
    m_ullInterval = rhs.m_ullInterval;
    m_stOnTime = std::move(rhs.m_stOnTime);
    return *this;
}

bool Timer::Start()noexcept
{
    if (IsClosing())
        return false;
    MOE_UV_GET_HANDLE_NOTHROW(::uv_timer_t);
    assert(handle);

    // 理论上不会抛出错误
    int ret = ::uv_timer_start(handle, OnUVTimer, m_ullFirstTime, m_ullInterval);
    MOE_UNUSED(ret);
    assert(ret == 0);
    return true;
}

void Timer::Stop()noexcept
{
    if (IsClosing())
        return;
    MOE_UV_GET_HANDLE_NOTHROW(::uv_timer_t);
    assert(handle);

    // 理论上总是成功的
    auto ret = ::uv_timer_stop(handle);
    MOE_UNUSED(ret);
    assert(ret == 0);
}

void Timer::OnTime()
{
    if (m_stOnTime)
        m_stOnTime();
}
