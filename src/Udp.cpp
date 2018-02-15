/**
 * @file
 * @author chu
 * @date 2017/12/8
 */
#include <Moe.UV/Udp.hpp>

#include <Moe.Coroutine/Coroutine.hpp>

using namespace std;
using namespace moe;
using namespace UV;
using namespace Coroutine;

namespace
{
    struct SendRequest :
        public ObjectPool::RefBase<SendRequest>
    {
        RefPtr<UdpSocket> Self;
        ::uv_udp_send_t Request;

        CoEvent Event;

        ::uv_buf_t BufferDesc;
        ObjectPool::BufferPtr Buffer;
    };
}

IoHandleHolder<UdpSocket> UdpSocket::Create()
{
    auto self = IoHandleHolder<UdpSocket>(ObjectPool::Create<UdpSocket>());
    return self;
}

IoHandleHolder<UdpSocket> UdpSocket::Create(const EndPoint& bind, bool reuse, bool ipv6Only)
{
    auto self = IoHandleHolder<UdpSocket>(ObjectPool::Create<UdpSocket>());
    self->Bind(bind, reuse, ipv6Only);
    return self;
}

void UdpSocket::OnUVSend(::uv_udp_send_t* request, int status)noexcept
{
    RefPtr<SendRequest> holder(static_cast<SendRequest*>(request->data));
    auto self = holder->Self;

    if (status != 0)
    {
        // 通知错误发生
        self->OnError(status);

        // 触发一个异常给Coroutine
        try
        {
            MOE_UV_THROW(status);
        }
        catch (...)
        {
            holder->Event.Throw(current_exception());
        }
    }
    else
    {
        // 通知数据发送
        self->OnSend(holder->BufferDesc.len);

        // 恢复Coroutine
        holder->Event.Resume();
    }
}

void UdpSocket::OnUVDirectSend(::uv_udp_send_t* request, int status)noexcept
{
    RefPtr<SendRequest> holder(static_cast<SendRequest*>(request->data));
    auto self = holder->Self;

    if (status != 0)
    {
        // 通知错误发生
        self->OnError(status);
    }
    else
    {
        // 通知数据发送
        self->OnSend(holder->BufferDesc.len);
    }
}

void UdpSocket::OnUVAllocBuffer(::uv_handle_t* handle, size_t suggestedSize, ::uv_buf_t* buf)noexcept
{
    MOE_UNUSED(handle);

    try
    {
        *buf = ::uv_buf_init(nullptr, 0);
        auto buffer = ObjectPool::Alloc(suggestedSize);

        // 所有权移交到uv_buf_t中
        *buf = ::uv_buf_init(static_cast<char*>(buffer.release()), static_cast<unsigned>(suggestedSize));
    }
    catch (const std::bad_alloc&)
    {
        MOE_ERROR("Out of memory when allocating buffer");
    }
}

void UdpSocket::OnUVRecv(::uv_udp_t* udp, ssize_t nread, const ::uv_buf_t* buf, const ::sockaddr* addr,
    unsigned flags)noexcept
{
    auto* self = GetSelf<UdpSocket>(udp);
    ObjectPool::BufferPtr buffer(buf->base);  // 获取所有权，确保调用后对象释放

    if (nread < 0)
    {
        // 通知错误发生
        self->OnError(static_cast<int>(nread));
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

        // 通知数据读取
        self->OnRead(remote, std::move(buffer), static_cast<size_t>(nread));
    }
}

UdpSocket::UdpSocket()
{
    MOE_UV_CHECK(::uv_udp_init(GetCurrentUVLoop(), &m_stHandle));
    BindHandle(reinterpret_cast<::uv_handle_t*>(&m_stHandle));
}

EndPoint UdpSocket::GetLocalEndPoint()const noexcept
{
    EndPoint ret;

    if (!IsClosing())
    {
        int len = sizeof(EndPoint::Storage);
        sockaddr* st = reinterpret_cast<sockaddr*>(&ret.Storage);
        ::uv_udp_getsockname(&m_stHandle, st, &len);
    }

    return ret;
}

void UdpSocket::SetTTL(int ttl)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");

    MOE_UV_CHECK(::uv_udp_set_ttl(&m_stHandle, ttl));
}

void UdpSocket::Bind(const EndPoint& address, bool reuse, bool ipv6Only)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");

    unsigned flags = 0;
    flags |= (reuse ? UV_UDP_REUSEADDR : 0);
    flags |= (ipv6Only ? UV_UDP_IPV6ONLY : 0);
    MOE_UV_CHECK(::uv_udp_bind(&m_stHandle, reinterpret_cast<const sockaddr*>(&address.Storage), flags));
}

void UdpSocket::StartRead(const ReadCallbackType& callback)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");
    if (m_eReadState != ReadState::NotReading)
        return;

    MOE_UV_CHECK(::uv_udp_recv_start(&m_stHandle, OnUVAllocBuffer, OnUVRecv));

    m_eReadState = ReadState::Reading;
    m_stReadCallback = callback;
}

void UdpSocket::StartRead(ReadCallbackType&& callback)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");
    if (m_eReadState != ReadState::NotReading)
        return;

    MOE_UV_CHECK(::uv_udp_recv_start(&m_stHandle, OnUVAllocBuffer, OnUVRecv));

    m_eReadState = ReadState::Reading;
    m_stReadCallback = std::move(callback);
}

bool UdpSocket::StopRead()noexcept
{
    if (IsClosing())
        return false;
    auto ret = ::uv_udp_recv_stop(&m_stHandle);
    if (ret == 0)
    {
        // 清空回调
        m_stReadCallback = nullptr;

        // 清空未读数据
        m_stQueuedBuffer.Clear();

        // 通知句柄退出
        m_stReadEvent.Cancel();

        m_eReadState = ReadState::NotReading;
        return true;
    }
    return false;
}

void UdpSocket::Send(const EndPoint& address, BytesView buffer)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");
    if (buffer.GetSize() > ObjectPool::kMaxAllocSize)
        MOE_THROW(InvalidCallException, "Buffer is too big");
    if (buffer.GetSize() == 0)
        return;

    auto request = ObjectPool::Create<SendRequest>();
    request->Self = RefFromThis().CastTo<UdpSocket>();

    // 分配缓冲区并拷贝数据
    request->Buffer = ObjectPool::Alloc(buffer.GetSize());
    request->BufferDesc = ::uv_buf_init(static_cast<char*>(request->Buffer.get()),
        static_cast<unsigned>(buffer.GetSize()));
    ::memcpy(request->Buffer.get(), buffer.GetBuffer(), buffer.GetSize());

    // 发起写操作
    MOE_UV_CHECK(::uv_udp_send(&request->Request, &m_stHandle, &(request->BufferDesc), 1,
        reinterpret_cast<const ::sockaddr*>(&address.Storage), OnUVDirectSend));

    // 释放所有权，交由UV管理
    ::uv_udp_send_t& req = request->Request;
    req.data = request.Release();
}

bool UdpSocket::CoRead(EndPoint& remote, ObjectPool::BufferPtr& buffer, size_t& size, Time::Tick timeout)
{
    // 检查缓冲区是否有未处理数据
    if (!m_stQueuedBuffer.IsEmpty())
    {
        // 如果有，则直接返回
        auto q = m_stQueuedBuffer.Pop();
        remote = q.Remote;
        buffer = std::move(q.Buffer);
        size = q.Length;
        return true;
    }

    // 如果缓冲区无数据，则等待
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");
    if (m_stReadCallback)
        MOE_THROW(InvalidCallException, "Already set callback");
    if (!Coroutine::GetCurrentThread())
        MOE_THROW(InvalidCallException, "Bad execution context");

    // 检查状态，如果是停止状态则发起读操作
    if (m_eReadState == ReadState::Pending)
    {
        MOE_UV_CHECK(::uv_udp_recv_start(&m_stHandle, OnUVAllocBuffer, OnUVRecv));
        m_eReadState = ReadState::Reading;
    }
    else if (m_eReadState == ReadState::NotReading)
        MOE_THROW(InvalidCallException, "Socket not reading");

    auto self = RefFromThis();  // 此时持有一个强引用，这意味着必须由外部事件强制触发，否则不会释放
    auto ret = m_stReadEvent.Wait(timeout);
    if (ret == CoEvent::WaitResult::Succeed)
    {
        // 此时缓冲区必然有数据
        assert(!m_stQueuedBuffer.IsEmpty());
        auto q = m_stQueuedBuffer.Pop();
        remote = q.Remote;
        buffer = std::move(q.Buffer);
        size = q.Length;
        return true;
    }
    return false;
}

void UdpSocket::CoSend(const EndPoint& address, BytesView buffer)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");
    if (!Coroutine::GetCurrentThread())
        MOE_THROW(InvalidCallException, "Bad execution context");
    if (buffer.GetSize() == 0)
        return;

    auto request = ObjectPool::Create<SendRequest>();
    request->Self = RefFromThis().CastTo<UdpSocket>();

    // Coroutine版本不用复制缓冲区
    request->BufferDesc = ::uv_buf_init(reinterpret_cast<char*>(const_cast<uint8_t*>(buffer.GetBuffer())),
        static_cast<unsigned>(buffer.GetSize()));

    // 发起写操作
    MOE_UV_CHECK(::uv_udp_send(&request->Request, &m_stHandle, &(request->BufferDesc), 1,
        reinterpret_cast<const ::sockaddr*>(&address.Storage), OnUVSend));

    // Coroutine版本复制一份所有权
    request->Request.data = RefPtr<SendRequest>(request).Release();

    // 发起等待操作
    request->Event.Wait();
}

void UdpSocket::OnClose()noexcept
{
    IoHandle::Close();
    m_eReadState = ReadState::NotReading;
    m_stReadCallback = nullptr;

    // 通知句柄退出
    if (m_stReadCallback)
    {
        auto cb = std::move(m_stReadCallback);
        MOE_UV_EAT_EXCEPT_BEGIN
            cb(0, EndPoint(), nullptr, 0u);
        MOE_UV_EAT_EXCEPT_END
    }
    else
        m_stReadEvent.Cancel();
}

void UdpSocket::OnError(int error)noexcept
{
    // 记录错误日志
    MOE_UV_LOG_ERROR(error);

    // 关闭连接
    Close();

    // 终止读操作
    if (m_stReadCallback)
    {
        auto cb = std::move(m_stReadCallback);
        MOE_UV_EAT_EXCEPT_BEGIN
            cb(error, EndPoint(), nullptr, 0u);
        MOE_UV_EAT_EXCEPT_END
    }
    else
    {
        try
        {
            MOE_UV_THROW(error);
        }
        catch (...)
        {
            m_stReadEvent.Throw(current_exception());
        }
    }
}

void UdpSocket::OnSend(size_t len)noexcept
{
    // TODO: 数据统计
    MOE_UNUSED(len);
}

void UdpSocket::OnRead(const EndPoint& remote, ObjectPool::BufferPtr buffer, size_t len)noexcept
{
    // 如果有回调函数，直接调用
    if (m_stReadCallback)
    {
        MOE_UV_EAT_EXCEPT_BEGIN
            m_stReadCallback(0, remote, std::move(buffer), len);
        MOE_UV_EAT_EXCEPT_END
        return;
    }

    // 否则将数据丢入队列并激活协程
    QueueData data;
    data.Remote = remote;
    data.Buffer = std::move(buffer);
    data.Length = len;

    bool ret = m_stQueuedBuffer.TryPush(std::move(data));
    MOE_UNUSED(ret);
    assert(ret);

    // 数据已满，停止读操作
    if (m_stQueuedBuffer.IsFull())
    {
        if (m_eReadState == ReadState::Reading)
        {
            auto error = ::uv_udp_recv_stop(&m_stHandle);
            MOE_UNUSED(error);
            assert(error == 0);
            m_eReadState = ReadState::Pending;
        }
    }

    // 激活协程
    m_stReadEvent.Resume();
}
