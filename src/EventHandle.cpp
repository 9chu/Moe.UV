/**
 * @file
 * @author chu
 * @date 2018/8/13
 */
#include <Moe.UV/EventHandle.hpp>

#include "UV.inl"

using namespace std;
using namespace moe;
using namespace UV;

EventHandle EventHandle::Create(EventType type)
{
    switch (type)
    {
        case EventType::Prepare:
            {
                MOE_UV_NEW(::uv_prepare_t);
                MOE_UV_CHECK(::uv_prepare_init(GetCurrentUVLoop(), object.get()));
                return EventHandle(type, CastHandle(std::move(object)));
            }
        case EventType::Idle:
            {
                MOE_UV_NEW(::uv_idle_t);
                MOE_UV_CHECK(::uv_idle_init(GetCurrentUVLoop(), object.get()));
                return EventHandle(type, CastHandle(std::move(object)));
            }
        case EventType::Check:
            {
                MOE_UV_NEW(::uv_check_t);
                MOE_UV_CHECK(::uv_check_init(GetCurrentUVLoop(), object.get()));
                return EventHandle(type, CastHandle(std::move(object)));
            }
        default:
            assert(false);
            MOE_THROW(InvalidCallException, "Bad param");
    }
}

EventHandle EventHandle::Create(EventType type, const OnEventCallbackType& callback)
{
    auto ret = Create(type);
    ret.SetOnEventCallback(callback);
    return ret;
}

EventHandle EventHandle::Create(EventType type, OnEventCallbackType&& callback)
{
    auto ret = Create(type);
    ret.SetOnEventCallback(std::move(callback));
    return ret;
}

void EventHandle::OnUVPrepare(::uv_prepare_t* handle)noexcept
{
    MOE_UV_GET_SELF(EventHandle);

    MOE_UV_CATCH_ALL_BEGIN
        self->OnEvent();
    MOE_UV_CATCH_ALL_END
}

void EventHandle::OnUVIdle(::uv_idle_t* handle)noexcept
{
    MOE_UV_GET_SELF(EventHandle);

    MOE_UV_CATCH_ALL_BEGIN
        self->OnEvent();
    MOE_UV_CATCH_ALL_END
}

void EventHandle::OnUVCheck(::uv_check_t* handle)noexcept
{
    MOE_UV_GET_SELF(EventHandle);

    MOE_UV_CATCH_ALL_BEGIN
        self->OnEvent();
    MOE_UV_CATCH_ALL_END
}

EventHandle::EventHandle(EventType type, UniquePooledObject<::uv_handle_s>&& handle)
    : AsyncHandle(std::move(handle)), m_uEventType(type)
{
}

EventHandle::EventHandle(EventHandle&& org)noexcept
    : AsyncHandle(std::move(org)), m_uEventType(org.m_uEventType), m_pOnEvent(std::move(org.m_pOnEvent))
{
}

EventHandle& EventHandle::operator=(EventHandle&& rhs)noexcept
{
    AsyncHandle::operator=(std::move(rhs));
    m_uEventType = rhs.m_uEventType;
    m_pOnEvent = std::move(rhs.m_pOnEvent);
    return *this;
}

void EventHandle::Start()
{
    switch (m_uEventType)
    {
        case EventType::Prepare:
            {
                MOE_UV_GET_HANDLE(::uv_prepare_t);
                MOE_UV_CHECK(::uv_prepare_start(handle, OnUVPrepare));
            }
            break;
        case EventType::Idle:
            {
                MOE_UV_GET_HANDLE(::uv_idle_t);
                MOE_UV_CHECK(::uv_idle_start(handle, OnUVIdle));
            }
            break;
        case EventType::Check:
            {
                MOE_UV_GET_HANDLE(::uv_check_t);
                MOE_UV_CHECK(::uv_check_start(handle, OnUVCheck));
            }
            break;
        default:
            assert(false);
            break;
    }
}

bool EventHandle::Stop()noexcept
{
    if (IsClosing())
        return false;

    switch (m_uEventType)
    {
        case EventType::Prepare:
            {
                MOE_UV_GET_HANDLE_NOTHROW(::uv_prepare_t);
                assert(handle);
                return ::uv_prepare_stop(handle) == 0;
            }
        case EventType::Idle:
            {
                MOE_UV_GET_HANDLE_NOTHROW(::uv_idle_t);
                assert(handle);
                return ::uv_idle_stop(handle) == 0;
            }
        case EventType::Check:
            {
                MOE_UV_GET_HANDLE_NOTHROW(::uv_check_t);
                assert(handle);
                return ::uv_check_stop(handle) == 0;
            }
        default:
            assert(false);
            return false;
    }
}

void EventHandle::OnEvent()
{
    if (m_pOnEvent)
        m_pOnEvent();
}
