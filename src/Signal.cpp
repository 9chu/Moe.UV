/**
 * @file
 * @author chu
 * @date 2018/8/13
 */
#include <Moe.UV/Signal.hpp>

#include "UV.inl"

using namespace std;
using namespace moe;
using namespace UV;

Signal Signal::Create()
{
    MOE_UV_NEW(::uv_signal_t);
    MOE_UV_CHECK(::uv_signal_init(GetCurrentUVLoop(), object.get()));
    return Signal(CastHandle(std::move(object)));
}

Signal Signal::Create(const OnSignalCallbackType& callback)
{
    auto ret = Create();
    ret.SetOnSignalCallback(callback);
    return ret;
}

Signal Signal::Create(OnSignalCallbackType&& callback)
{
    auto ret = Create();
    ret.SetOnSignalCallback(std::move(callback));
    return ret;
}

void Signal::OnUVSignal(::uv_signal_t* handle, int signum)noexcept
{
    MOE_UV_GET_SELF(Signal);

    MOE_UV_CATCH_ALL_BEGIN
        self->OnSignal(signum);
    MOE_UV_CATCH_ALL_END
}

Signal::Signal(Signal&& org)noexcept
    : AsyncHandle(std::move(org)), m_pOnSignal(std::move(org.m_pOnSignal))
{
}

Signal& Signal::operator=(Signal&& rhs)noexcept
{
    AsyncHandle::operator=(std::move(rhs));
    m_pOnSignal = std::move(rhs.m_pOnSignal);
    return *this;
}

void Signal::Start(int signum)
{
    MOE_UV_GET_HANDLE(::uv_signal_t);
    MOE_UV_CHECK(::uv_signal_start(handle, OnUVSignal, signum));
}

bool Signal::Stop()noexcept
{
    if (IsClosing())
        return false;
    MOE_UV_GET_HANDLE_NOTHROW(::uv_signal_t);
    assert(handle);
    return ::uv_signal_stop(handle) == 0;
}

void Signal::OnSignal(int signum)
{
    if (m_pOnSignal)
        m_pOnSignal(signum);
}
