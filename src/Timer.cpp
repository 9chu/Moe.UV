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
    auto self = ObjectPool::Create<Timer>();
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

Timer::~Timer()
{
    m_stTickCondVar.Resume(static_cast<ptrdiff_t>(false));
}

void Timer::Start()
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Timer is already closed");
    MOE_UV_CHECK(::uv_timer_start(&m_stHandle, OnUVTimer, m_ullInterval, m_ullInterval));
}

bool Timer::Stop()noexcept
{
    if (IsClosing())
        return false;
    auto ret = ::uv_timer_stop(&m_stHandle);
    if (ret == 0)
    {
        // 通知协程取消
        m_stTickCondVar.Resume(static_cast<ptrdiff_t>(false));
        return true;
    }
    return false;
}

bool Timer::CoWait()
{
    if (!m_pHandle)
        MOE_THROW(InvalidCallException, "Timer is not started");
    auto ret = Coroutine::Suspend(m_stTickCondVar);
    return static_cast<bool>(ret);
}

void Timer::OnClose()noexcept
{
    IoHandle::OnClose();

    // 通知协程取消
    m_stTickCondVar.Resume(static_cast<ptrdiff_t>(false));
}

void Timer::OnTick()noexcept
{
    m_stTickCondVar.Resume(static_cast<ptrdiff_t>(true));
}
