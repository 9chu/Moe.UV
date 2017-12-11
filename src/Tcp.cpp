/**
 * @file
 * @author chu
 * @date 2017/12/11
 */
#include <Moe.UV/Tcp.hpp>

using namespace std;
using namespace moe;
using namespace UV;

namespace
{
    struct ConnectRequest :
        public ObjectPool::RefBase<ConnectRequest>
    {
        RefPtr<TcpSocket> self;
        ::uv_connect_t Request;
    };
}

IoHandleHolder<TcpSocket> TcpSocket::Create()
{
    auto self = IoHandleHolder<TcpSocket>(ObjectPool::Create<TcpSocket>());
    return self;
}

TcpSocket::TcpSocket()
{
    MOE_UV_CHECK(::uv_tcp_init(GetCurrentUVLoop(), &m_stHandle));
    BindHandle(reinterpret_cast<::uv_handle_t*>(&m_stHandle));
}

EndPoint TcpSocket::GetLocalEndPoint()const noexcept
{
    EndPoint ret;

    if (!IsClosing())
    {
        int len = sizeof(EndPoint::Storage);
        sockaddr* st = reinterpret_cast<sockaddr*>(&ret.Storage);
        ::uv_tcp_getsockname(&m_stHandle, st, &len);
    }

    return ret;
}

EndPoint TcpSocket::GetRemoteEndPoint()const noexcept
{
    EndPoint ret;

    if (!IsClosing())
    {
        int len = sizeof(EndPoint::Storage);
        sockaddr* st = reinterpret_cast<sockaddr*>(&ret.Storage);
        ::uv_tcp_getpeername(&m_stHandle, st, &len);
    }

    return ret;
}

void TcpSocket::SetNoDelay(bool enable)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");

    MOE_UV_CHECK(::uv_tcp_nodelay(&m_stHandle, enable ? 1 : 0));
}

void TcpSocket::SetKeepAlive(bool enable, uint32_t delay)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");

    MOE_UV_CHECK(::uv_tcp_keepalive(&m_stHandle, enable ? 1 : 0, delay));
}

void TcpSocket::Bind(const EndPoint& address, bool reuse, bool ipv6Only)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");

    unsigned flags = 0;
    flags |= (reuse ? UV_UDP_REUSEADDR : 0);
    flags |= (ipv6Only ? UV_UDP_IPV6ONLY : 0);
    MOE_UV_CHECK(::uv_tcp_bind(&m_stHandle, reinterpret_cast<const sockaddr*>(&address.Storage), flags));
}

void TcpSocket::CoConnect(const EndPoint& address)
{

}

bool TcpSocket::IsReadable()const noexcept
{
    if (IsClosing())
        return false;

    return ::uv_is_readable(reinterpret_cast<const ::uv_stream_t*>(&m_stHandle)) == 1;
}

bool TcpSocket::IsWritable()const noexcept
{
    if (IsClosing())
        return false;

    return ::uv_is_writable(reinterpret_cast<const ::uv_stream_t*>(&m_stHandle)) == 1;
}

void TcpSocket::CoShutdown()
{
    // TODO
    MOE_THROW(NotImplementException, "Not Implement");
}

void TcpSocket::Send(BytesView buffer)
{
    // TODO
    MOE_THROW(NotImplementException, "Not Implement");
}

void TcpSocket::CoSend(BytesView buffer)
{
    // TODO
    MOE_THROW(NotImplementException, "Not Implement");
}

bool TcpSocket::CoRead(ObjectPool::BufferPtr& buffer, size_t& size)
{
    // TODO
    MOE_THROW(NotImplementException, "Not Implement");
}

bool TcpSocket::CancelRead()noexcept
{
    // TODO
    return false;
}
