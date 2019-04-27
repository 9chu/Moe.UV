/**
 * @file
 * @author chu
 * @date 2019/4/26
 */
#include <Moe.UV/Poll.hpp>

#include "UV.inl"

using namespace std;
using namespace moe;
using namespace UV;

Poll Poll::Create(int fd)
{
    MOE_UV_NEW(::uv_poll_t);
    MOE_UV_CHECK(::uv_poll_init(GetCurrentUVLoop(), object.get(), fd));
    return Poll(CastHandle(std::move(object)));
}

#ifdef MOE_WINDOWS
Poll Poll::Create(void* socket)
{
    MOE_UV_NEW(::uv_poll_t);
    MOE_UV_CHECK(::uv_poll_init_socket(GetCurrentUVLoop(), object.get(), reinterpret_cast<uv_os_sock_t>(socket)));
    return Poll(CastHandle(std::move(object)));
}
#endif

void Poll::OnUVEvent(::uv_poll_s* handle, int status, int events)noexcept
{
    MOE_UV_GET_SELF(Poll);

    MOE_UV_CATCH_ALL_BEGIN
        self->OnEvent(status, events);
    MOE_UV_CATCH_ALL_END
}

Poll::Poll(Poll&& org)noexcept
    : AsyncHandle(std::move(org)), m_pOnEvent(std::move(org.m_pOnEvent))
{
}

Poll& Poll::operator=(Poll&& rhs)noexcept
{
    AsyncHandle::operator=(std::move(rhs));
    m_pOnEvent = std::move(rhs.m_pOnEvent);
    return *this;
}

void Poll::Start(int events)
{
    MOE_UV_GET_HANDLE(::uv_poll_t);
    MOE_UV_CHECK(::uv_poll_start(handle, events, OnUVEvent));
}

bool Poll::Stop()noexcept
{
    if (IsClosing())
        return false;
    MOE_UV_GET_HANDLE_NOTHROW(::uv_poll_t);
    assert(handle);
    return ::uv_poll_stop(handle) == 0;
}

void Poll::OnEvent(int status, int events)
{
    if (m_pOnEvent)
        m_pOnEvent(status, events);
}
