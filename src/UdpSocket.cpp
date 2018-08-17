/**
 * @file
 * @author chu
 * @date 2017/12/8
 */
#include <Moe.UV/UdpSocket.hpp>

#include <Moe.UV/RunLoop.hpp>
#include <Moe.Core/Logging.hpp>

using namespace std;
using namespace moe;
using namespace UV;

//////////////////////////////////////////////////////////////////////////////// UdpSocketBase

struct UVSendRequest
{
    ::uv_udp_send_t Request;
    ::uv_buf_t BufferDesc;

    RefPtr<UdpSocketBase> Parent;
    UniquePooledObject<void> CopiedBuffer;
    UdpSocketBase::OnSendCallbackType OnSend;
};

void UdpSocketBase::OnUVSend(::uv_udp_send_t* request, int status)noexcept
{
    UniquePooledObject<UVSendRequest> owner;
    owner.reset(static_cast<UVSendRequest*>(request->data));

    auto self = owner->Parent;

    if (status != 0)  // 通知错误发生
    {
        if (owner->OnSend)
        {
            MOE_UV_CATCH_ALL_BEGIN
                owner->OnSend(static_cast<uv_errno_t>(status));
            MOE_UV_CATCH_ALL_END
        }

        MOE_UV_CATCH_ALL_BEGIN
            self->OnError(static_cast<uv_errno_t>(status));
        MOE_UV_CATCH_ALL_END

        self->Close();  // 发生错误时直接关闭Socket
    }
    else
    {
        // 通知数据发送
        if (owner->OnSend)
        {
            MOE_UV_CATCH_ALL_BEGIN
                owner->OnSend(static_cast<uv_errno_t>(0));
            MOE_UV_CATCH_ALL_END
        }
    }
}

void UdpSocketBase::OnUVAllocBuffer(::uv_handle_t* handle, size_t suggestedSize, ::uv_buf_t* buf)noexcept
{
    MOE_UNUSED(handle);

    MOE_UV_CATCH_ALL_BEGIN
        *buf = ::uv_buf_init(nullptr, 0);
        MOE_UV_ALLOC(suggestedSize);

        // 所有权移交到uv_buf_t中
        *buf = ::uv_buf_init(static_cast<char*>(buffer.release()), static_cast<unsigned>(suggestedSize));
    MOE_UV_CATCH_ALL_END
}

void UdpSocketBase::OnUVRecv(::uv_udp_t* udp, ssize_t nread, const ::uv_buf_t* buf, const ::sockaddr* addr,
    unsigned flags)noexcept
{
    auto* self = GetSelf<UdpSocketBase>(udp);

    // 获取所有权，确保调用后对象释放
    UniquePooledObject<void> buffer;
    buffer.reset(buf->base);

    if (nread < 0)  // 通知错误发生
    {
        MOE_UV_CATCH_ALL_BEGIN
            self->OnError(static_cast<uv_errno_t>(nread));
        MOE_UV_CATCH_ALL_END

        self->Close();  // 发生错误时直接关闭Socket
    }
    else if (nread > 0)
    {
        if (flags == UV_UDP_PARTIAL || !(addr->sa_family == AF_INET || addr->sa_family == AF_INET6))
            return;  // 不能处理的数据包类型，直接丢包

        EndPoint remote;
        if (addr->sa_family == AF_INET)
            remote = EndPoint(*reinterpret_cast<const ::sockaddr_in*>(addr));
        else
        {
            assert(addr->sa_family == AF_INET6);
            remote = EndPoint(*reinterpret_cast<const ::sockaddr_in6*>(addr));
        }

        MOE_UV_CATCH_ALL_BEGIN
            // 通知数据读取
            self->OnRead(remote, BytesView(static_cast<const uint8_t*>(buffer.get()), nread));
        MOE_UV_CATCH_ALL_END
    }
}

UdpSocketBase::UdpSocketBase()
{
    MOE_UV_CHECK(::uv_udp_init(GetCurrentUVLoop(), &m_stHandle));
    BindHandle(reinterpret_cast<::uv_handle_t*>(&m_stHandle));
}

EndPoint UdpSocketBase::GetLocalEndPoint()const noexcept
{
    EndPoint ret;
    if (!IsClosing())
    {
        int len = sizeof(EndPoint::Storage);
        auto st = reinterpret_cast<sockaddr*>(&ret.Storage);
        ::uv_udp_getsockname(&m_stHandle, st, &len);
    }
    return ret;
}

size_t UdpSocketBase::GetSendQueueSize()const noexcept
{
    if (IsClosing())
        return 0;
    return ::uv_udp_get_send_queue_size(&m_stHandle);
}

size_t UdpSocketBase::GetSendQueueCount()const noexcept
{
    if (IsClosing())
        return 0;
    return ::uv_udp_get_send_queue_count(&m_stHandle);
}

void UdpSocketBase::SetTTL(int ttl)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");
    MOE_UV_CHECK(::uv_udp_set_ttl(&m_stHandle, ttl));
}

void UdpSocketBase::Bind(const EndPoint& address, bool reuse, bool ipv6Only)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");

    unsigned flags = 0;
    flags |= (reuse ? UV_UDP_REUSEADDR : 0);
    flags |= (ipv6Only ? UV_UDP_IPV6ONLY : 0);
    MOE_UV_CHECK(::uv_udp_bind(&m_stHandle, reinterpret_cast<const sockaddr*>(&address.Storage), flags));
}

void UdpSocketBase::StartRead()
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");
    auto r = ::uv_udp_recv_start(&m_stHandle, OnUVAllocBuffer, OnUVRecv);
    if (r == UV_EALREADY)
        return;
    MOE_UV_CHECK(r);
}

bool UdpSocketBase::StopRead()noexcept
{
    if (IsClosing())
        return true;
    return ::uv_udp_recv_stop(&m_stHandle) == 0;
}

void UdpSocketBase::Send(const EndPoint& address, BytesView buf)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");
    if (buf.GetSize() == 0)
        return;

    MOE_UV_NEW_UNIQUE(UVSendRequest);
    object->Parent = RefFromThis().CastTo<UdpSocketBase>();

    // 分配缓冲区并拷贝数据
    MOE_UV_ALLOC(buf.GetSize());
    object->CopiedBuffer = std::move(buffer);
    object->BufferDesc = ::uv_buf_init(static_cast<char*>(object->CopiedBuffer.get()),
        static_cast<unsigned>(buf.GetSize()));
    memcpy(object->CopiedBuffer.get(), buf.GetBuffer(), buf.GetSize());

    // 发起写操作
    MOE_UV_CHECK(::uv_udp_send(&object->Request, &m_stHandle, &(object->BufferDesc), 1,
        reinterpret_cast<const ::sockaddr*>(&address.Storage), OnUVSend));

    // 释放所有权，交由UV管理
    ::uv_udp_send_t& req = object->Request;
    req.data = object.release();
}

void UdpSocketBase::SendNoCopy(const EndPoint& address, BytesView buffer, const OnSendCallbackType& cb)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");
    if (buffer.GetSize() == 0)
        MOE_THROW(BadArgumentException, "Buffer is empty");

    MOE_UV_NEW_UNIQUE(UVSendRequest);
    object->Parent = RefFromThis().CastTo<UdpSocketBase>();
    object->OnSend = cb;
    object->BufferDesc = ::uv_buf_init(const_cast<char*>(reinterpret_cast<const char*>(buffer.GetBuffer())),
        static_cast<unsigned>(buffer.GetSize()));

    // 发起写操作
    MOE_UV_CHECK(::uv_udp_send(&object->Request, &m_stHandle, &(object->BufferDesc), 1,
        reinterpret_cast<const ::sockaddr*>(&address.Storage), OnUVSend));

    // 释放所有权，交由UV管理
    ::uv_udp_send_t& req = object->Request;
    req.data = object.release();
}

void UdpSocketBase::SendNoCopy(const EndPoint& address, BytesView buffer, OnSendCallbackType&& cb)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");
    if (buffer.GetSize() == 0)
        MOE_THROW(BadArgumentException, "Buffer is empty");

    MOE_UV_NEW_UNIQUE(UVSendRequest);
    object->Parent = RefFromThis().CastTo<UdpSocketBase>();
    object->OnSend = std::move(cb);
    object->BufferDesc = ::uv_buf_init(const_cast<char*>(reinterpret_cast<const char*>(buffer.GetBuffer())),
        static_cast<unsigned>(buffer.GetSize()));

    // 发起写操作
    MOE_UV_CHECK(::uv_udp_send(&object->Request, &m_stHandle, &(object->BufferDesc), 1,
        reinterpret_cast<const ::sockaddr*>(&address.Storage), OnUVSend));

    // 释放所有权，交由UV管理
    ::uv_udp_send_t& req = object->Request;
    req.data = object.release();
}

bool UdpSocketBase::TrySend(const EndPoint& address, BytesView buffer)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");
    if (buffer.GetSize() == 0)
        MOE_THROW(BadArgumentException, "Buffer is empty");

    ::uv_buf_t desc = ::uv_buf_init(const_cast<char*>(reinterpret_cast<const char*>(buffer.GetBuffer())),
        static_cast<unsigned>(buffer.GetSize()));

    // 发起写操作
    auto r = ::uv_udp_try_send(&m_stHandle, &desc, 1, reinterpret_cast<const ::sockaddr*>(&address.Storage));
    if (r >= 0)
        return true;
    else if (r == UV_EAGAIN)
        return false;
    MOE_UV_THROW(r);
}

//////////////////////////////////////////////////////////////////////////////// UdpSocket

UniqueAsyncHandlePtr<UdpSocket> UdpSocket::Create()
{
    MOE_UV_NEW(UdpSocket);
    return object;
}

UniqueAsyncHandlePtr<UdpSocket> UdpSocket::Create(const EndPoint& bind, bool reuse, bool ipv6Only)
{
    MOE_UV_NEW(UdpSocket);
    object->Bind(bind, reuse, ipv6Only);
    return object;
}

void UdpSocket::OnError(::uv_errno_t error)
{
    if (m_pOnError)
        m_pOnError(error);
}

void UdpSocket::OnRead(const EndPoint& remote, BytesView data)
{
    if (m_pOnRead)
        m_pOnRead(remote, data);
}
