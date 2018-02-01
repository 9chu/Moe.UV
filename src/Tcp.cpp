/**
 * @file
 * @author chu
 * @date 2017/12/11
 */
#include <Moe.UV/Tcp.hpp>

#include <Moe.Coroutine/Coroutine.hpp>

using namespace std;
using namespace moe;
using namespace UV;
using namespace Coroutine;

//////////////////////////////////////////////////////////////////////////////// TcpSocket

namespace
{
    struct ConnectRequest :
        public ObjectPool::RefBase<ConnectRequest>
    {
        RefPtr<TcpSocket> Self;
        ::uv_connect_t Request;

        CoEvent Event;
    };

    struct ShutdownRequest :
        public ObjectPool::RefBase<ShutdownRequest>
    {
        RefPtr<TcpSocket> Self;
        ::uv_shutdown_t Request;

        CoEvent Event;
    };

    struct WriteRequest :
        public ObjectPool::RefBase<WriteRequest>
    {
        RefPtr<TcpSocket> Self;
        ::uv_write_t Request;

        ::uv_buf_t BufferDesc;
        ObjectPool::BufferPtr Buffer;

        CoEvent Event;
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
            holder->Event.Throw(current_exception());
        }
    }
    else
    {
        // 恢复Coroutine
        holder->Event.Resume();
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
            holder->Event.Throw(current_exception());
        }
    }
    else
    {
        // 恢复Coroutine
        holder->Event.Resume();
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

void TcpSocket::OnUVAllocBuffer(::uv_handle_t* handle, size_t suggestedSize, ::uv_buf_t* buf)noexcept
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

void TcpSocket::OnUVRead(::uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)noexcept
{
    auto* self = GetSelf<TcpSocket>(stream);
    ObjectPool::BufferPtr buffer(buf->base);  // 获取所有权，确保调用后对象释放

    if (nread == UV_EOF)  // nread == 0 等价于 EAGAIN 忽略即可
    {
        self->OnEof();
    }
    else if (nread < 0)
    {
        // 通知错误发生
        self->OnError(static_cast<int>(nread));
    }
    else if (nread > 0)
    {
        // 通知数据读取
        self->OnRead(std::move(buffer), static_cast<size_t>(nread));
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

void TcpSocket::Bind(const EndPoint& address, bool ipv6Only)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");

    unsigned flags = 0;
    flags |= (ipv6Only ? UV_TCP_IPV6ONLY : 0);
    MOE_UV_CHECK(::uv_tcp_bind(&m_stHandle, reinterpret_cast<const sockaddr*>(&address.Storage), flags));
}

void TcpSocket::CoConnect(const EndPoint& address)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");
    if (!Coroutine::GetCurrentThread())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto request = ObjectPool::Create<ConnectRequest>();
    request->Self = RefFromThis().CastTo<TcpSocket>();

    // 发起连接操作
    MOE_UV_CHECK(::uv_tcp_connect(&request->Request, &m_stHandle, reinterpret_cast<const ::sockaddr*>(&address.Storage),
        OnUVConnect));

    // Coroutine版本复制一份所有权
    request->Request.data = RefPtr<ConnectRequest>(request).Release();

    // 发起等待操作
    request->Event.Wait();
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
    if (!Coroutine::GetCurrentThread())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto request = ObjectPool::Create<ShutdownRequest>();
    request->Self = RefFromThis().CastTo<TcpSocket>();

    // 发起Shutdown操作
    MOE_UV_CHECK(::uv_shutdown(&request->Request, reinterpret_cast<::uv_stream_t*>(&m_stHandle), OnUVShutdown));

    // Coroutine版本复制一份所有权
    request->Request.data = RefPtr<ShutdownRequest>(request).Release();

    // 发起等待操作
    request->Event.Wait();
}

void TcpSocket::Send(BytesView buffer)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");

    while (buffer.GetSize() > 0)
    {
        size_t sz = std::min(buffer.GetSize(), static_cast<size_t>(ObjectPool::kMaxAllocSize));

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
    if (!Coroutine::GetCurrentThread())
        MOE_THROW(InvalidCallException, "Bad execution context");

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
    request->Event.Wait();
}

bool TcpSocket::CoRead(ObjectPool::BufferPtr& holder, MutableBytesView& view, Time::Tick timeout)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");

    // 如果没有在读，则发起读操作
    if (!m_bReading)
    {
        MOE_UV_CHECK(::uv_read_start(reinterpret_cast<::uv_stream_t*>(&m_stHandle), OnUVAllocBuffer, OnUVRead));
        m_bReading = true;
    }

    // 检查缓冲区是否有未处理数据
    if (!m_stQueuedBuffer.IsEmpty())
    {
        // 如果有，则直接返回
        auto q = m_stQueuedBuffer.Pop();
        holder = std::move(q.Holder);
        view = q.View;
        return true;
    }

    // 如果缓冲区无数据，则等待
    auto self = RefFromThis();  // 此时持有一个强引用，这意味着必须由外部事件强制触发，否则不会释放
    auto ret = m_stReadEvent.Wait(timeout);
    if (ret == CoEvent::WaitResult::Succeed)
    {
        // 此时缓冲区必然有数据
        assert(!m_stQueuedBuffer.IsEmpty());
        auto q = m_stQueuedBuffer.Pop();
        holder = std::move(q.Holder);
        view = q.View;
        return true;
    }
    return false;
}

bool TcpSocket::CoRead(uint8_t* target, size_t count, Time::Tick timeout)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");

    // 如果没有在读，则发起读操作
    if (!m_bReading)
    {
        MOE_UV_CHECK(::uv_read_start(reinterpret_cast<::uv_stream_t*>(&m_stHandle), OnUVAllocBuffer, OnUVRead));
        m_bReading = true;
    }

    while (count > 0)
    {
        // 从缓冲区读出数据填入目标数组
        while (!m_stQueuedBuffer.IsEmpty())
        {
            auto& top = m_stQueuedBuffer.Top();
            auto view = top.View;

            auto copySize = std::min(view.GetSize(), count);
            ::memcpy(target, view.GetBuffer(), copySize);
            target += copySize;
            count -= copySize;

            if (copySize == view.GetSize())
            {
                // 缓冲区被消耗完
                m_stQueuedBuffer.Pop();
            }
            else
            {
                // 去掉已读部分
                top.View = view.Slice(copySize, view.GetSize());
            }

            if (count == 0)  // 读完需要的长度
                return true;
        }

        // 此时，需要后续数据，阻塞等待
        auto self = RefFromThis();  // 此时持有一个强引用，这意味着必须由外部事件强制触发，否则不会释放
        auto ret = m_stReadEvent.Wait();
        if (ret != CoEvent::WaitResult::Succeed)
            return false;  // 操作取消
    }
    return true;
}

bool TcpSocket::CancelRead()noexcept
{
    if (IsClosing())
        return false;
    auto ret = ::uv_read_stop(reinterpret_cast<::uv_stream_t*>(&m_stHandle));
    if (ret == 0)
    {
        // TCP版本不会清空缓冲区，只通知句柄退出
        m_stReadEvent.Cancel();

        m_bReading = false;
        return true;
    }
    return false;
}

void TcpSocket::OnClose()noexcept
{
    IoHandle::Close();
    m_bReading = false;
}

void TcpSocket::OnError(int error)noexcept
{
    // 记录错误日志
    MOE_UV_LOG_ERROR(error);

    // 关闭连接
    Close();

    // 终止读句柄
    try
    {
        MOE_UV_THROW(error);
    }
    catch (...)
    {
        m_stReadEvent.Throw(current_exception());
    }
}

void TcpSocket::OnSend(size_t len)noexcept
{
    // TODO: 数据统计
    MOE_UNUSED(len);
}

void TcpSocket::OnRead(ObjectPool::BufferPtr buffer, size_t len)noexcept
{
    QueueData data;
    data.Holder = std::move(buffer);
    data.View = MutableBytesView(reinterpret_cast<uint8_t*>(data.Holder.get()), len);

    bool ret = m_stQueuedBuffer.TryPush(std::move(data));
    MOE_UNUSED(ret);
    assert(ret);

    // 数据已满，停止读操作
    if (m_stQueuedBuffer.IsFull())
    {
        if (m_bReading)
        {
            auto error = ::uv_read_stop(reinterpret_cast<::uv_stream_t*>(&m_stHandle));
            MOE_UNUSED(error);
            assert(error == 0);
            m_bReading = false;
        }
    }

    // 激活协程
    m_stReadEvent.Resume();
}

void TcpSocket::OnEof()noexcept
{
    m_bReading = false;

    // 终止读句柄
    m_stReadEvent.Cancel();
}

//////////////////////////////////////////////////////////////////////////////// TcpListener

IoHandleHolder<TcpListener> TcpListener::Create()
{
    auto self = IoHandleHolder<TcpListener>(ObjectPool::Create<TcpListener>());
    return self;
}

IoHandleHolder<TcpListener> TcpListener::Create(const EndPoint& address, bool ipv6Only)
{
    auto self = IoHandleHolder<TcpListener>(ObjectPool::Create<TcpListener>());
    self->Bind(address, ipv6Only);
    return self;
}

void TcpListener::OnUVNewConnection(::uv_stream_t* server, int status)noexcept
{
    auto self = GetSelf<TcpListener>(server);

    if (status != 0)
    {
        // 通知错误发生
        self->OnError(status);
    }
    else
    {
        // 通知新连接
        self->OnNewConnection();
    }
}

TcpListener::TcpListener()
{
    MOE_UV_CHECK(::uv_tcp_init(GetCurrentUVLoop(), &m_stHandle));
    BindHandle(reinterpret_cast<::uv_handle_t*>(&m_stHandle));
}

EndPoint TcpListener::GetLocalEndPoint()const noexcept
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

void TcpListener::Bind(const EndPoint& address, bool ipv6Only)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");

    unsigned flags = 0;
    flags |= (ipv6Only ? UV_TCP_IPV6ONLY : 0);
    MOE_UV_CHECK(::uv_tcp_bind(&m_stHandle, reinterpret_cast<const sockaddr*>(&address.Storage), flags));
}

void TcpListener::Listen(int backlog)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");

    MOE_UV_CHECK(::uv_listen(reinterpret_cast<::uv_stream_t*>(&m_stHandle), backlog, OnUVNewConnection));
}

IoHandleHolder<TcpSocket> TcpListener::CoAccept(Time::Tick timeout)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Socket has been shutdown");

    int ret = UV_EAGAIN;
    auto socket = TcpSocket::Create();
    while (ret == UV_EAGAIN)
    {
        ret = ::uv_accept(reinterpret_cast<::uv_stream_t*>(&m_stHandle),
            reinterpret_cast<::uv_stream_t*>(&socket->m_stHandle));

        if (ret == 0)
            return socket;
        else if (ret != UV_EAGAIN)
            MOE_UV_THROW(ret);

        // 等待协程
        auto ret = m_stAcceptEvent.Wait(timeout);
        if (ret != CoEvent::WaitResult::Succeed)
            return IoHandleHolder<TcpSocket>();
    }

    assert(false);
    return IoHandleHolder<TcpSocket>();
}

void TcpListener::CancelWait()noexcept
{
    m_stAcceptEvent.Cancel();
}

void TcpListener::OnClose()noexcept
{
    IoHandle::Close();

    // 取消等待的协程
    CancelWait();
}

void TcpListener::OnError(int error)noexcept
{
    // 记录错误日志
    MOE_UV_LOG_ERROR(error);

    // 关闭连接
    Close();

    // 终止读句柄
    try
    {
        MOE_UV_THROW(error);
    }
    catch (...)
    {
        m_stAcceptEvent.Throw(current_exception());
    }
}

void TcpListener::OnNewConnection()noexcept
{
    m_stAcceptEvent.Resume();
}
