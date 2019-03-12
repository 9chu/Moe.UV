/**
 * @file
 * @author chu
 * @date 2018/9/21
 */
#include <Moe.UV/TcpSocket.hpp>

#include "UV.inl"

using namespace std;
using namespace moe;
using namespace UV;

TcpSocket TcpSocket::Create()
{
    MOE_UV_NEW(::uv_tcp_t);
    MOE_UV_CHECK(::uv_tcp_init(GetCurrentUVLoop(), object.get()));
    return TcpSocket(CastHandle(std::move(object)));
}

struct UVConnectRequest
{
    ::uv_connect_t Request;
};

void TcpSocket::OnUVConnect(::uv_connect_s* request, int status)noexcept
{
    UniquePooledObject<UVConnectRequest> owner;
    owner.reset(static_cast<UVConnectRequest*>(request->data));

    auto handle = request->handle;
    MOE_UV_GET_SELF(TcpSocket);

    MOE_UV_CATCH_ALL_BEGIN
        self->OnConnect(static_cast<uv_errno_t>(status));
    MOE_UV_CATCH_ALL_END
}

void TcpSocket::OnUVConnection(::uv_stream_s* handle, int status)noexcept
{
    MOE_UV_GET_SELF(TcpSocket);

    if (status != 0)  // 通知错误发生
    {
        MOE_UV_CATCH_ALL_BEGIN
            self->OnError(static_cast<uv_errno_t>(status));
        MOE_UV_CATCH_ALL_END

        if (GetSelf<TcpSocket>(handle) == self)
            self->Close();  // 发生错误时直接关闭Socket
    }
    else
    {
        MOE_UV_CATCH_ALL_BEGIN
            self->OnConnection();
        MOE_UV_CATCH_ALL_END
    }
}

TcpSocket::TcpSocket(TcpSocket&& org)noexcept
    : Stream(std::move(org)), m_pOnConnect(std::move(org.m_pOnConnect)), m_pOnConnection(std::move(org.m_pOnConnection))
{
}

TcpSocket& TcpSocket::operator=(TcpSocket&& rhs)noexcept
{
    Stream::operator=(std::move(rhs));
    m_pOnConnect = std::move(rhs.m_pOnConnect);
    m_pOnConnection = std::move(rhs.m_pOnConnection);
    return *this;
}

void TcpSocket::Open(int fd)
{
    MOE_UV_GET_HANDLE(::uv_tcp_t);
    MOE_UV_CHECK(::uv_tcp_open(handle, fd));
}

void TcpSocket::SetNoDelay(bool enable)
{
    MOE_UV_GET_HANDLE(::uv_tcp_t);
    MOE_UV_CHECK(::uv_tcp_nodelay(handle, enable ? 1 : 0));
}

void TcpSocket::SetKeepAlive(bool enable, unsigned delay)
{
    MOE_UV_GET_HANDLE(::uv_tcp_t);
    MOE_UV_CHECK(::uv_tcp_keepalive(handle, enable ? 1 : 0, delay));
}

void TcpSocket::SetSimultaneousAccepts(bool enable)
{
    MOE_UV_GET_HANDLE(::uv_tcp_t);
    MOE_UV_CHECK(::uv_tcp_simultaneous_accepts(handle, enable ? 1 : 0));
}

void TcpSocket::Bind(const EndPoint& addr, bool ipv6Only)
{
    MOE_UV_GET_HANDLE(::uv_tcp_t);
    MOE_UV_CHECK(::uv_tcp_bind(handle, reinterpret_cast<const sockaddr*>(&addr.Storage), ipv6Only ? UV_TCP_IPV6ONLY : 0));
}

void TcpSocket::Connect(const EndPoint& addr)
{
    MOE_UV_GET_HANDLE(::uv_tcp_t);

    MOE_UV_NEW(UVConnectRequest);

    // 发起关闭操作
    MOE_UV_CHECK(::uv_tcp_connect(&object->Request, handle, reinterpret_cast<const sockaddr*>(&addr.Storage),
        OnUVConnect));

    // 释放所有权，交由UV管理
    auto& req = object->Request;
    req.data = object.release();
}

void TcpSocket::Listen(unsigned backlog)
{
    MOE_UV_GET_HANDLE(::uv_stream_t);
    MOE_UV_CHECK(::uv_listen(handle, backlog, OnUVConnection));
}

TcpSocket TcpSocket::Accept()
{
    MOE_UV_GET_HANDLE(::uv_stream_t);

    MOE_UV_NEW(::uv_tcp_t);
    MOE_UV_CHECK(::uv_tcp_init(GetCurrentUVLoop(), object.get()));
    MOE_UV_CHECK(::uv_accept(handle, reinterpret_cast<::uv_stream_t*>(object.get())));
    return TcpSocket(CastHandle(std::move(object)));
}

EndPoint TcpSocket::GetSockName()
{
    MOE_UV_GET_HANDLE(::uv_tcp_t);

    EndPoint ep;
    int len = sizeof(ep.Storage);
    MOE_UV_CHECK(::uv_tcp_getsockname(handle, reinterpret_cast<sockaddr*>(&ep.Storage), &len));
    return ep;
}

EndPoint TcpSocket::GetPeerName()
{
    MOE_UV_GET_HANDLE(::uv_tcp_t);

    EndPoint ep;
    int len = sizeof(ep.Storage);
    MOE_UV_CHECK(::uv_tcp_getpeername(handle, reinterpret_cast<sockaddr*>(&ep.Storage), &len));
    return ep;
}

void TcpSocket::OnConnect(int error)
{
    if (m_pOnConnect)
        m_pOnConnect(error);
}

void TcpSocket::OnConnection()
{
    if (m_pOnConnection)
        m_pOnConnection();
}
