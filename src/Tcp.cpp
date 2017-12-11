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
        RefPtr<TcpSocket> Self;
        ::uv_connect_t Request;

        CoCondVar CondVar;
    };

    struct ShutdownRequest :
        public ObjectPool::RefBase<ShutdownRequest>
    {
        RefPtr<TcpSocket> Self;
        ::uv_shutdown_t Request;

        CoCondVar CondVar;
    };

    struct WriteRequest :
        public ObjectPool::RefBase<WriteRequest>
    {
        RefPtr<TcpSocket> Self;
        ::uv_write_t Request;

        ::uv_buf_t BufferDesc;
        ObjectPool::BufferPtr Buffer;

        CoCondVar CondVar;
    };
}

IoHandleHolder<TcpSocket> TcpSocket::Create()
{
    auto self = IoHandleHolder<TcpSocket>(ObjectPool::Create<TcpSocket>());
    return self;
}

void TcpSocket::OnUVConnect(::uv_connect_t* req, int status)noexcept
{
    RefPtr<ConnectRequest> holder(static_cast<ConnectRequest*>(req->data));
    auto self = holder->Self;

    if (status != 0)
    {
        // 触发一个异常给Coroutine
        try
        {
            MOE_UV_THROW(status);
        }
        catch (...)
        {
            holder->CondVar.ResumeException(current_exception());
        }
    }
    else
    {
        // 恢复Coroutine
        holder->CondVar.Resume();
    }
}

void TcpSocket::OnUVShutdown(::uv_shutdown_t* req, int status)noexcept
{
    RefPtr<ShutdownRequest> holder(static_cast<ShutdownRequest*>(req->data));
    auto self = holder->Self;

    if (status != 0)
    {
        // 触发一个异常给Coroutine
        try
        {
            MOE_UV_THROW(status);
        }
        catch (...)
        {
            holder->CondVar.ResumeException(current_exception());
        }
    }
    else
    {
        // 恢复Coroutine
        holder->CondVar.Resume();
    }
}

void TcpSocket::OnUVWrite(::uv_write_t* req, int status)noexcept
{
    RefPtr<WriteRequest> holder(static_cast<WriteRequest*>(req->data));
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
            holder->CondVar.ResumeException(current_exception());
        }
    }
    else
    {
        // 通知数据发送
        self->OnSend(holder->BufferDesc.len);

        // 恢复Coroutine
        holder->CondVar.Resume();
    }
}

void TcpSocket::OnUVDirectWrite(::uv_write_t* req, int status)noexcept
{
    RefPtr<WriteRequest> holder(static_cast<WriteRequest*>(req->data));
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
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");

    auto request = ObjectPool::Create<ConnectRequest>();
    request->Self = RefFromThis().CastTo<TcpSocket>();

    // 发起连接操作
    MOE_UV_CHECK(::uv_tcp_connect(&request->Request, &m_stHandle, reinterpret_cast<const ::sockaddr*>(&address.Storage),
        OnUVConnect));

    // 释放所有权，交由UV管理
    ::uv_connect_t& req = request->Request;
    req.data = request.Release();
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
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");

    auto request = ObjectPool::Create<ShutdownRequest>();
    request->Self = RefFromThis().CastTo<TcpSocket>();

    // 发起Shutdown操作
    MOE_UV_CHECK(::uv_shutdown(&request->Request, reinterpret_cast<::uv_stream_t*>(&m_stHandle), OnUVShutdown));

    // 释放所有权，交由UV管理
    ::uv_shutdown_t& req = request->Request;
    req.data = request.Release();
}

void TcpSocket::Send(BytesView buffer)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");

    while (buffer.GetSize() > 0)
    {
        size_t sz = std::min<size_t>(buffer.GetSize(), ObjectPool::kMaxAllocSize);

        auto request = ObjectPool::Create<WriteRequest>();
        request->Self = RefFromThis().CastTo<TcpSocket>();

        // 分配缓冲区并拷贝数据
        request->Buffer = ObjectPool::Alloc(sz);
        request->BufferDesc = ::uv_buf_init(static_cast<char*>(request->Buffer.get()), static_cast<unsigned>(sz));
        ::memcpy(request->Buffer.get(), buffer.GetBuffer(), sz);

        // 调整未发送大小
        buffer = buffer.Slice(sz, buffer.GetSize());

        // 发起写操作
        MOE_UV_CHECK(::uv_write(&request->Request, reinterpret_cast<::uv_stream_t*>(&m_stHandle),
            &(request->BufferDesc), 1, OnUVDirectWrite));

        // 释放所有权，交由UV管理
        ::uv_write_t& req = request->Request;
        req.data = request.Release();
    }
}

void TcpSocket::CoSend(BytesView buffer)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");
    if (buffer.GetSize() == 0)
        return;

    auto request = ObjectPool::Create<WriteRequest>();
    request->Self = RefFromThis().CastTo<TcpSocket>();

    // Coroutine版本不用复制缓冲区
    request->BufferDesc = ::uv_buf_init(reinterpret_cast<char*>(const_cast<uint8_t*>(buffer.GetBuffer())),
        static_cast<unsigned>(buffer.GetSize()));

    // 发起写操作
    MOE_UV_CHECK(::uv_write(&request->Request, reinterpret_cast<::uv_stream_t*>(&m_stHandle), &(request->BufferDesc),
        1, OnUVWrite));

    // Coroutine版本复制一份所有权
    request->Request.data = RefPtr<WriteRequest>(request).Release();

    // 发起等待操作
    Coroutine::Suspend(request->CondVar);
}

bool TcpSocket::CoRead(ObjectPool::BufferPtr& buffer, size_t& size)
{
    // TODO
    MOE_THROW(NotImplementException, "Not Implement");
    
#if 0
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");

    // 如果没有在读，则发起读操作
    if (!m_bReading)
    {
        MOE_UV_CHECK(::uv_udp_recv_start(&m_stHandle, OnUVAllocBuffer, OnUVRecv));
        m_bReading = true;
    }

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
    bool ret = static_cast<bool>(Coroutine::Suspend(m_stReadCondVar));
    if (ret)
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
#endif
}

bool TcpSocket::CoRead(uint8_t* target, size_t count)
{
    // TODO
    MOE_THROW(NotImplementException, "Not Implement");
}

bool TcpSocket::CancelRead()noexcept
{
    // TODO
    return false;
}
