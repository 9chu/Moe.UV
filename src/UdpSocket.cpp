/**
 * @file
 * @author chu
 * @date 2017/12/8
 */
#include <Moe.UV/UdpSocket.hpp>

#include "UV.inl"

using namespace std;
using namespace moe;
using namespace UV;

UdpSocket UdpSocket::Create()
{
    MOE_UV_NEW(::uv_udp_t);
    MOE_UV_CHECK(::uv_udp_init(GetCurrentUVLoop(), object.get()));
    return UdpSocket(CastHandle(std::move(object)));
}

UdpSocket UdpSocket::Create(const EndPoint& bind, bool reuse, bool ipv6Only)
{
    auto ret = Create();
    ret.Bind(bind, reuse, ipv6Only);
    return ret;
}

struct UVSendRequest
{
    ::uv_udp_send_t Request;
    ::uv_buf_t BufferDesc;

    UniquePooledObject<void> CopiedBuffer;
    UdpSocket::OnSendCallbackType OnSend;
};

void UdpSocket::OnUVSend(::uv_udp_send_t* request, int status)noexcept
{
    UniquePooledObject<UVSendRequest> owner;
    owner.reset(static_cast<UVSendRequest*>(request->data));

    auto handle = request->handle;
    MOE_UV_GET_SELF(UdpSocket);

    if (status != 0)  // 通知错误发生
    {
        if (owner->OnSend)
        {
            MOE_UV_CATCH_ALL_BEGIN
                owner->OnSend(static_cast<uv_errno_t>(status));
            MOE_UV_CATCH_ALL_END
        }

        if (GetSelf<UdpSocket>(handle) == self)  // 由于穿越回调函数，需要检查所有权
        {
            MOE_UV_CATCH_ALL_BEGIN
                self->OnError(static_cast<uv_errno_t>(status));
            MOE_UV_CATCH_ALL_END

            if (GetSelf<UdpSocket>(handle) == self)
                self->Close();  // 发生错误时直接关闭Socket
        }
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

void UdpSocket::OnUVAllocBuffer(::uv_handle_t* handle, size_t suggestedSize, ::uv_buf_t* buf)noexcept
{
    MOE_UNUSED(handle);

    MOE_UV_CATCH_ALL_BEGIN
        *buf = ::uv_buf_init(nullptr, 0);
        MOE_UV_ALLOC(suggestedSize);

        // 所有权移交到uv_buf_t中
        *buf = ::uv_buf_init(static_cast<char*>(buffer.release()), static_cast<unsigned>(suggestedSize));
    MOE_UV_CATCH_ALL_END
}

void UdpSocket::OnUVRecv(::uv_udp_t* handle, ssize_t nread, const ::uv_buf_t* buf, const ::sockaddr* addr,
    unsigned flags)noexcept
{
    MOE_UV_GET_SELF(UdpSocket);

    // 获取所有权，确保调用后对象释放
    UniquePooledObject<void> buffer;
    buffer.reset(buf->base);

    if (nread < 0)  // 通知错误发生
    {
        MOE_UV_CATCH_ALL_BEGIN
            self->OnError(static_cast<uv_errno_t>(nread));
        MOE_UV_CATCH_ALL_END

        if (GetSelf<UdpSocket>(handle) == self)
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
            self->OnData(remote, BytesView(static_cast<const uint8_t*>(buffer.get()), nread));
        MOE_UV_CATCH_ALL_END
    }
}

UdpSocket::UdpSocket(UdpSocket&& org)noexcept
    : AsyncHandle(std::move(org)), m_pOnError(std::move(org.m_pOnError)), m_pOnData(std::move(org.m_pOnData))
{
}

UdpSocket& UdpSocket::operator=(UdpSocket&& rhs)noexcept
{
    AsyncHandle::operator=(std::move(rhs));
    m_pOnError = std::move(rhs.m_pOnError);
    m_pOnData = std::move(rhs.m_pOnData);
    return *this;
}

EndPoint UdpSocket::GetLocalEndPoint()const noexcept
{
    EndPoint ret;
    if (!IsClosing())
    {
        MOE_UV_GET_HANDLE_NOTHROW(::uv_udp_t);
        assert(handle);

        int len = sizeof(EndPoint::Storage);
        auto st = reinterpret_cast<sockaddr*>(&ret.Storage);
        ::uv_udp_getsockname(handle, st, &len);
    }
    return ret;
}

size_t UdpSocket::GetSendQueueSize()const noexcept
{
    if (IsClosing())
        return 0;

    MOE_UV_GET_HANDLE_NOTHROW(::uv_udp_t);
    assert(handle);
    return ::uv_udp_get_send_queue_size(handle);
}

size_t UdpSocket::GetSendQueueCount()const noexcept
{
    if (IsClosing())
        return 0;

    MOE_UV_GET_HANDLE_NOTHROW(::uv_udp_t);
    assert(handle);
    return ::uv_udp_get_send_queue_count(handle);
}

void UdpSocket::SetTTL(int ttl)
{
    MOE_UV_GET_HANDLE(::uv_udp_t);
    MOE_UV_CHECK(::uv_udp_set_ttl(handle, ttl));
}

void UdpSocket::Bind(const EndPoint& address, bool reuse, bool ipv6Only)
{
    MOE_UV_GET_HANDLE(::uv_udp_t);

    unsigned flags = 0;
    flags |= (reuse ? UV_UDP_REUSEADDR : 0);
    flags |= (ipv6Only ? UV_UDP_IPV6ONLY : 0);
    MOE_UV_CHECK(::uv_udp_bind(handle, reinterpret_cast<const sockaddr*>(&address.Storage), flags));
}

void UdpSocket::StartRead()
{
    MOE_UV_GET_HANDLE(::uv_udp_t);

    auto r = ::uv_udp_recv_start(handle, OnUVAllocBuffer, OnUVRecv);
    if (r == UV_EALREADY)
        return;
    MOE_UV_CHECK(r);
}

bool UdpSocket::StopRead()noexcept
{
    if (IsClosing())
        return true;
    MOE_UV_GET_HANDLE_NOTHROW(::uv_udp_t);
    assert(handle);
    return ::uv_udp_recv_stop(handle) == 0;
}

void UdpSocket::Send(const EndPoint& address, BytesView buf)
{
    MOE_UV_GET_HANDLE(::uv_udp_t);
    if (buf.GetSize() == 0)
        return;

    MOE_UV_NEW(UVSendRequest);

    // 分配缓冲区并拷贝数据
    MOE_UV_ALLOC(buf.GetSize());
    object->CopiedBuffer = std::move(buffer);
    object->BufferDesc = ::uv_buf_init(static_cast<char*>(object->CopiedBuffer.get()),
        static_cast<unsigned>(buf.GetSize()));
    memcpy(object->CopiedBuffer.get(), buf.GetBuffer(), buf.GetSize());

    // 发起写操作
    MOE_UV_CHECK(::uv_udp_send(&object->Request, handle, &(object->BufferDesc), 1,
        reinterpret_cast<const ::sockaddr*>(&address.Storage), OnUVSend));

    // 释放所有权，交由UV管理
    ::uv_udp_send_t& req = object->Request;
    req.data = object.release();
}

void UdpSocket::SendNoCopy(const EndPoint& address, BytesView buffer, const OnSendCallbackType& cb)
{
    MOE_UV_GET_HANDLE(::uv_udp_t);
    if (buffer.GetSize() == 0)
        MOE_THROW(BadArgumentException, "Buffer is empty");

    MOE_UV_NEW(UVSendRequest);
    object->OnSend = cb;
    object->BufferDesc = ::uv_buf_init(const_cast<char*>(reinterpret_cast<const char*>(buffer.GetBuffer())),
        static_cast<unsigned>(buffer.GetSize()));

    // 发起写操作
    MOE_UV_CHECK(::uv_udp_send(&object->Request, handle, &(object->BufferDesc), 1,
        reinterpret_cast<const ::sockaddr*>(&address.Storage), OnUVSend));

    // 释放所有权，交由UV管理
    ::uv_udp_send_t& req = object->Request;
    req.data = object.release();
}

void UdpSocket::SendNoCopy(const EndPoint& address, BytesView buffer, OnSendCallbackType&& cb)
{
    MOE_UV_GET_HANDLE(::uv_udp_t);
    if (buffer.GetSize() == 0)
        MOE_THROW(BadArgumentException, "Buffer is empty");

    MOE_UV_NEW(UVSendRequest);
    object->OnSend = std::move(cb);
    object->BufferDesc = ::uv_buf_init(const_cast<char*>(reinterpret_cast<const char*>(buffer.GetBuffer())),
        static_cast<unsigned>(buffer.GetSize()));

    // 发起写操作
    MOE_UV_CHECK(::uv_udp_send(&object->Request, handle, &(object->BufferDesc), 1,
        reinterpret_cast<const ::sockaddr*>(&address.Storage), OnUVSend));

    // 释放所有权，交由UV管理
    ::uv_udp_send_t& req = object->Request;
    req.data = object.release();
}

bool UdpSocket::TrySend(const EndPoint& address, BytesView buffer)
{
    MOE_UV_GET_HANDLE(::uv_udp_t);
    if (buffer.GetSize() == 0)
        MOE_THROW(BadArgumentException, "Buffer is empty");

    ::uv_buf_t desc = ::uv_buf_init(const_cast<char*>(reinterpret_cast<const char*>(buffer.GetBuffer())),
        static_cast<unsigned>(buffer.GetSize()));

    // 发起写操作
    auto r = ::uv_udp_try_send(handle, &desc, 1, reinterpret_cast<const ::sockaddr*>(&address.Storage));
    if (r >= 0)
        return true;
    else if (r == UV_EAGAIN)
        return false;
    MOE_UV_THROW(r);
}

void UdpSocket::OnError(int error)
{
    if (m_pOnError)
        m_pOnError(error);
}

void UdpSocket::OnData(const EndPoint& remote, BytesView data)
{
    if (m_pOnData)
        m_pOnData(remote, data);
}
