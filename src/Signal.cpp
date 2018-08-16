/**
 * @file
 * @author chu
 * @date 2018/8/13
 */
#include <Moe.UV/Signal.hpp>

#include <Moe.Core/Logging.hpp>
#include <Moe.UV/RunLoop.hpp>

using namespace std;
using namespace moe;
using namespace UV;

//////////////////////////////////////////////////////////////////////////////// SignalBase

void SignalBase::OnUVSignal(::uv_signal_t* handle, int signum)noexcept
{
    auto* self = GetSelf<SignalBase>(handle);

    MOE_UV_CATCH_ALL_BEGIN
        self->OnSignal(signum);
    MOE_UV_CATCH_ALL_END
}

SignalBase::SignalBase()
{
    MOE_UV_CHECK(::uv_signal_init(GetCurrentUVLoop(), &m_stHandle));
    BindHandle(reinterpret_cast<::uv_handle_t*>(&m_stHandle));
}

void SignalBase::Start(int signum)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Signal is already closed");
    MOE_UV_CHECK(::uv_signal_start(&m_stHandle, OnUVSignal, signum));
}

bool SignalBase::Stop()noexcept
{
    if (IsClosing())
        return false;
    return ::uv_signal_stop(&m_stHandle) == 0;
}

//////////////////////////////////////////////////////////////////////////////// Signal

UniqueAsyncHandlePtr<Signal> Signal::Create()
{
    MOE_UV_NEW(Signal);
    return object;
}

UniqueAsyncHandlePtr<Signal> Signal::Create(const OnSignalCallbackType& callback)
{
    MOE_UV_NEW(Signal);
    object->SetOnSignalCallback(callback);
    return object;
}

UniqueAsyncHandlePtr<Signal> Signal::Create(OnSignalCallbackType&& callback)
{
    MOE_UV_NEW(Signal);
    object->SetOnSignalCallback(std::move(callback));
    return object;
}

void Signal::OnSignal(int signum)
{
    if (m_pOnSignal)
        m_pOnSignal(signum);
}
