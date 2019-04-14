/**
 * @file
 * @author chu
 * @date 2019/4/14
 */
#include <Moe.UV/FsEvent.hpp>

#include "UV.inl"

using namespace std;
using namespace moe;
using namespace UV;

FsEvent FsEvent::Create()
{
    MOE_UV_NEW(::uv_fs_event_t);
    MOE_UV_CHECK(::uv_fs_event_init(GetCurrentUVLoop(), object.get()));
    return FsEvent(CastHandle(std::move(object)));
}

FsEvent FsEvent::Create(const OnEventCallbackType& callback)
{
    auto ret = Create();
    ret.SetOnEventCallback(callback);
    return ret;
}

FsEvent FsEvent::Create(OnEventCallbackType&& callback)
{
    auto ret = Create();
    ret.SetOnEventCallback(std::move(callback));
    return ret;
}

void FsEvent::OnUVEvent(::uv_fs_event_t* handle, const char* filename, int events, int status)noexcept
{
    MOE_UV_GET_SELF(FsEvent);

    MOE_UV_CATCH_ALL_BEGIN
        self->OnEvent(filename, events, status);
    MOE_UV_CATCH_ALL_END
}

FsEvent::FsEvent(FsEvent&& org)noexcept
    : AsyncHandle(std::move(org)), m_pOnEvent(std::move(org.m_pOnEvent))
{
}

FsEvent& FsEvent::operator=(FsEvent&& rhs)noexcept
{
    AsyncHandle::operator=(std::move(rhs));
    m_pOnEvent = std::move(rhs.m_pOnEvent);
    return *this;
}

void FsEvent::Start(const char* path, unsigned int flags)
{
    MOE_UV_GET_HANDLE(::uv_fs_event_t);
    MOE_UV_CHECK(::uv_fs_event_start(handle, OnUVEvent, path, flags));
}

bool FsEvent::Stop()noexcept
{
    if (IsClosing())
        return false;
    MOE_UV_GET_HANDLE_NOTHROW(::uv_fs_event_t);
    assert(handle);
    return ::uv_fs_event_stop(handle) == 0;
}

void FsEvent::OnEvent(const char* filename, int events, int status)
{
    if (m_pOnEvent)
        m_pOnEvent(filename, events, status);
}
