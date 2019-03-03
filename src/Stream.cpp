/**
 * @file
 * @author chu
 * @date 2018/9/2
 */
#include <Moe.UV/Stream.hpp>

#include "UV.inl"

using namespace std;
using namespace moe;
using namespace UV;

struct UVShutdownRequest
{
    ::uv_shutdown_t Request;
};

struct UVWriteRequest
{
    ::uv_write_t Request;
    ::uv_buf_t BufferDesc;

    UniquePooledObject<void> CopiedBuffer;
    Stream::OnWriteCallbackType OnWrite;
};

void Stream::OnUVShutdown(::uv_shutdown_s* request, int status)noexcept
{
    UniquePooledObject<UVShutdownRequest> owner;
    owner.reset(static_cast<UVShutdownRequest*>(request->data));

    auto handle = request->handle;
    MOE_UV_GET_SELF(Stream);

    if (status != 0)  // 通知错误发生
    {
        MOE_UV_CATCH_ALL_BEGIN
            self->OnError(static_cast<uv_errno_t>(status));
        MOE_UV_CATCH_ALL_END

        self->Close();  // 发生错误时直接关闭Socket
    }
    else
    {
        MOE_UV_CATCH_ALL_BEGIN
            self->OnShutdown();
        MOE_UV_CATCH_ALL_END
    }
}

void Stream::OnUVWrite(::uv_write_s* request, int status)noexcept
{
    UniquePooledObject<UVWriteRequest> owner;
    owner.reset(static_cast<UVWriteRequest*>(request->data));

    auto handle = request->handle;
    MOE_UV_GET_SELF(Stream);

    if (status != 0)  // 通知错误发生
    {
        if (owner->OnWrite)
        {
            MOE_UV_CATCH_ALL_BEGIN
                owner->OnWrite(static_cast<uv_errno_t>(status));
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
        if (owner->OnWrite)
        {
            MOE_UV_CATCH_ALL_BEGIN
                owner->OnWrite(static_cast<uv_errno_t>(0));
            MOE_UV_CATCH_ALL_END
        }
    }
}

void Stream::OnUVAllocBuffer(::uv_handle_s* handle, size_t suggestedSize, ::uv_buf_t* buf)noexcept
{
    MOE_UNUSED(handle);

    MOE_UV_CATCH_ALL_BEGIN
        *buf = ::uv_buf_init(nullptr, 0);
        MOE_UV_ALLOC(suggestedSize);

        // 所有权移交到uv_buf_t中
        *buf = ::uv_buf_init(static_cast<char*>(buffer.release()), static_cast<unsigned>(suggestedSize));
    MOE_UV_CATCH_ALL_END
}

void Stream::OnUVRead(::uv_stream_s* handle, ssize_t nread, const ::uv_buf_t* buf)noexcept
{
    MOE_UV_GET_SELF(Stream);

    // 获取所有权，确保调用后对象释放
    UniquePooledObject<void> buffer;
    buffer.reset(buf->base);

    if (nread < 0)  // 通知错误发生
    {
        MOE_UV_CATCH_ALL_BEGIN
            if (nread == UV_EOF)
                self->OnEof();
            else
            {
                self->OnError(static_cast<uv_errno_t>(nread));
                self->Close();  // 发生错误时直接关闭Socket
            }
        MOE_UV_CATCH_ALL_END
    }
    else if (nread > 0)
    {
        MOE_UV_CATCH_ALL_BEGIN
            // 通知数据读取
            self->OnData(BytesView(static_cast<const uint8_t*>(buffer.get()), nread));
        MOE_UV_CATCH_ALL_END
    }
}

Stream::Stream(Stream&& org)noexcept
    : AsyncHandle(std::move(org)), m_pOnError(std::move(org.m_pOnError)), m_pOnShutdown(std::move(org.m_pOnShutdown)),
    m_pOnData(std::move(org.m_pOnData)), m_pOnEof(std::move(org.m_pOnEof))
{
}

Stream& Stream::operator=(Stream&& rhs)noexcept
{
    AsyncHandle::operator=(std::move(rhs));
    m_pOnError = std::move(rhs.m_pOnError);
    m_pOnShutdown = std::move(rhs.m_pOnShutdown);
    m_pOnData = std::move(rhs.m_pOnData);
    m_pOnEof = std::move(rhs.m_pOnEof);
    return *this;
}

bool Stream::IsReadable()const noexcept
{
    if (IsClosing())
        return false;
    MOE_UV_GET_HANDLE_NOTHROW(::uv_stream_t);
    return ::uv_is_readable(handle) != 0;
}

bool Stream::IsWritable()const noexcept
{
    if (IsClosing())
        return false;
    MOE_UV_GET_HANDLE_NOTHROW(::uv_stream_t);
    return ::uv_is_writable(handle) != 0;
}

size_t Stream::GetWriteQueueSize()const noexcept
{
    if (IsClosing())
        return 0;
    MOE_UV_GET_HANDLE_NOTHROW(::uv_stream_t);
    return ::uv_stream_get_write_queue_size(handle);
}

void Stream::Shutdown()
{
    MOE_UV_GET_HANDLE(::uv_stream_t);

    MOE_UV_NEW(UVShutdownRequest);

    // 发起关闭操作
    MOE_UV_CHECK(::uv_shutdown(&object->Request, handle, OnUVShutdown));

    // 释放所有权，交由UV管理
    auto& req = object->Request;
    req.data = object.release();
}

void Stream::StartRead()
{
    MOE_UV_GET_HANDLE(::uv_stream_t);
    auto r = ::uv_read_start(handle, OnUVAllocBuffer, OnUVRead);
    if (r == UV_EALREADY)
        return;
    MOE_UV_CHECK(r);
}

bool Stream::StopRead()noexcept
{
    if (IsClosing())
        return true;
    MOE_UV_GET_HANDLE_NOTHROW(::uv_stream_t);
    return ::uv_read_stop(handle) == 0;
}

void Stream::Write(BytesView buf)
{
    MOE_UV_GET_HANDLE(::uv_stream_t);
    if (buf.GetSize() == 0)
        return;

    MOE_UV_NEW(UVWriteRequest);

    // 分配缓冲区并拷贝数据
    MOE_UV_ALLOC(buf.GetSize());
    object->CopiedBuffer = std::move(buffer);
    object->BufferDesc = ::uv_buf_init(static_cast<char*>(object->CopiedBuffer.get()),
        static_cast<unsigned>(buf.GetSize()));
    memcpy(object->CopiedBuffer.get(), buf.GetBuffer(), buf.GetSize());

    // 发起写操作
    MOE_UV_CHECK(::uv_write(&object->Request, handle, &(object->BufferDesc), 1, OnUVWrite));

    // 释放所有权，交由UV管理
    auto& req = object->Request;
    req.data = object.release();
}

void Stream::WriteNoCopy(BytesView buffer, const OnWriteCallbackType& cb)
{
    MOE_UV_GET_HANDLE(::uv_stream_t);
    if (buffer.GetSize() == 0)
        MOE_THROW(BadArgumentException, "Buffer is empty");

    MOE_UV_NEW(UVWriteRequest);
    object->OnWrite = cb;
    object->BufferDesc = ::uv_buf_init(const_cast<char*>(reinterpret_cast<const char*>(buffer.GetBuffer())),
        static_cast<unsigned>(buffer.GetSize()));

    // 发起写操作
    MOE_UV_CHECK(::uv_write(&object->Request, handle, &(object->BufferDesc), 1, OnUVWrite));

    // 释放所有权，交由UV管理
    auto& req = object->Request;
    req.data = object.release();
}

void Stream::WriteNoCopy(BytesView buffer, OnWriteCallbackType&& cb)
{
    MOE_UV_GET_HANDLE(::uv_stream_t);
    if (buffer.GetSize() == 0)
        MOE_THROW(BadArgumentException, "Buffer is empty");

    MOE_UV_NEW(UVWriteRequest);
    object->OnWrite = std::move(cb);
    object->BufferDesc = ::uv_buf_init(const_cast<char*>(reinterpret_cast<const char*>(buffer.GetBuffer())),
        static_cast<unsigned>(buffer.GetSize()));

    // 发起写操作
    MOE_UV_CHECK(::uv_write(&object->Request, handle, &(object->BufferDesc), 1, OnUVWrite));

    // 释放所有权，交由UV管理
    auto& req = object->Request;
    req.data = object.release();
}

bool Stream::TryWrite(BytesView buffer)
{
    MOE_UV_GET_HANDLE(::uv_stream_t);
    if (buffer.GetSize() == 0)
        MOE_THROW(BadArgumentException, "Buffer is empty");

    ::uv_buf_t desc = ::uv_buf_init(const_cast<char*>(reinterpret_cast<const char*>(buffer.GetBuffer())),
        static_cast<unsigned>(buffer.GetSize()));

    // 发起写操作
    auto r = ::uv_try_write(handle, &desc, 1);
    if (r >= 0)
        return true;
    else if (r == UV_EAGAIN)
        return false;
    MOE_UV_THROW(r);
}

void Stream::OnError(int error)
{
    if (m_pOnError)
        m_pOnError(error);
}

void Stream::OnShutdown()
{
    if (m_pOnShutdown)
        m_pOnShutdown();
}

void Stream::OnData(BytesView data)
{
    if (m_pOnData)
        m_pOnData(data);
}

void Stream::OnEof()
{
    if (m_pOnEof)
        m_pOnEof();
}
