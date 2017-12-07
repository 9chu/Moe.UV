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
    auto self = ObjectPool::Create<Timeout>();
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

Timeout::~Timeout()
{
    m_stTickCondVar.Resume(static_cast<ptrdiff_t>(false));
}

bool Timeout::CoTimeout(Time::Tick timeout)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Timeout is already closed");
    MOE_UV_CHECK(::uv_timer_start(&m_stHandle, OnUVTimer, timeout, 0));

    // 等待超时事件
    return static_cast<bool>(Coroutine::Suspend(m_stTickCondVar));
}

bool Timeout::Cancel()noexcept
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

void Timeout::OnClose()noexcept
{
    IoHandle::OnClose();

    // 通知协程取消
    m_stTickCondVar.Resume(static_cast<ptrdiff_t>(false));
}

void Timeout::OnTick()noexcept
{
    m_stTickCondVar.Resume(static_cast<ptrdiff_t>(true));
}
