/**
 * @file
 * @author chu
 * @date 2018/8/13
 */
#include <Moe.UV/EventHandle.hpp>

#include <Moe.Core/Logging.hpp>
#include <Moe.UV/RunLoop.hpp>

using namespace std;
using namespace moe;
using namespace UV;

//////////////////////////////////////////////////////////////////////////////// EventHandleBase

void EventHandleBase::OnUVPrepare(::uv_prepare_t* handle)noexcept
{
    auto* self = GetSelf<EventHandleBase>(handle);

    MOE_UV_CATCH_ALL_BEGIN
        self->OnEvent();
    MOE_UV_CATCH_ALL_END
}

void EventHandleBase::OnUVIdle(::uv_idle_t* handle)noexcept
{
    auto* self = GetSelf<EventHandleBase>(handle);

    MOE_UV_CATCH_ALL_BEGIN
        self->OnEvent();
    MOE_UV_CATCH_ALL_END
}

void EventHandleBase::OnUVCheck(::uv_check_t* handle)noexcept
{
    auto* self = GetSelf<EventHandleBase>(handle);

    MOE_UV_CATCH_ALL_BEGIN
        self->OnEvent();
    MOE_UV_CATCH_ALL_END
}

EventHandleBase::EventHandleBase(EventType type)
    : m_uEventType(type)
{
    switch (type)
    {
        case EventType::Prepare:
            MOE_UV_CHECK(::uv_prepare_init(GetCurrentUVLoop(), &m_stHandle.Prepare));
            break;
        case EventType::Idle:
            MOE_UV_CHECK(::uv_idle_init(GetCurrentUVLoop(), &m_stHandle.Idle));
            break;
        case EventType::Check:
            MOE_UV_CHECK(::uv_check_init(GetCurrentUVLoop(), &m_stHandle.Check));
            break;
        default:
            assert(false);
            MOE_THROW(InvalidCallException, "Bad param");
    }
    BindHandle(reinterpret_cast<::uv_handle_t*>(&m_stHandle));
}

void EventHandleBase::Start()
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Object is already closed");

    switch (m_uEventType)
    {
        case EventType::Prepare:
            MOE_UV_CHECK(::uv_prepare_start(&m_stHandle.Prepare, OnUVPrepare));
            break;
        case EventType::Idle:
            MOE_UV_CHECK(::uv_idle_start(&m_stHandle.Idle, OnUVIdle));
            break;
        case EventType::Check:
            MOE_UV_CHECK(::uv_check_start(&m_stHandle.Check, OnUVCheck));
            break;
        default:
            assert(false);
            break;
    }
}

bool EventHandleBase::Stop()noexcept
{
    if (IsClosing())
        return false;

    switch (m_uEventType)
    {
        case EventType::Prepare:
            return ::uv_prepare_stop(&m_stHandle.Prepare) == 0;
        case EventType::Idle:
            return ::uv_idle_stop(&m_stHandle.Idle) == 0;
        case EventType::Check:
            return ::uv_check_stop(&m_stHandle.Check) == 0;
        default:
            assert(false);
            return false;
    }
}

//////////////////////////////////////////////////////////////////////////////// EventHandle

UniqueAsyncHandlePtr<EventHandle> EventHandle::Create(EventType type)
{
    MOE_UV_NEW(EventHandle, type);
    return object;
}

UniqueAsyncHandlePtr<EventHandle> EventHandle::Create(EventType type, const OnEventCallbackType& callback)
{
    MOE_UV_NEW(EventHandle, type);
    object->SetOnEventCallback(callback);
    return object;
}

UniqueAsyncHandlePtr<EventHandle> EventHandle::Create(EventType type, OnEventCallbackType&& callback)
{
    MOE_UV_NEW(EventHandle, type);
    object->SetOnEventCallback(std::move(callback));
    return object;
}

void EventHandle::OnEvent()
{
    if (m_pOnEvent)
        m_pOnEvent();
}
