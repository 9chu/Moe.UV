/**
 * @file
 * @author chu
 * @date 2018/9/2
 */
#include <Moe.UV/Pipe.hpp>

#include "UV.inl"

using namespace std;
using namespace moe;
using namespace UV;

Pipe Pipe::Create(bool ipc)
{
    MOE_UV_NEW(::uv_pipe_t);
    MOE_UV_CHECK(::uv_pipe_init(GetCurrentUVLoop(), object.get(), ipc ? 1 : 0));
    return Pipe(CastHandle(std::move(object)));
}

struct UVConnectRequest
{
    ::uv_connect_t Request;
};

void Pipe::OnUVConnect(::uv_connect_t* request, int status)noexcept
{
    UniquePooledObject<UVConnectRequest> owner;
    owner.reset(static_cast<UVConnectRequest*>(request->data));

    auto handle = request->handle;
    MOE_UV_GET_SELF(Pipe);

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
            self->OnConnect();
        MOE_UV_CATCH_ALL_END
    }
}

Pipe::Pipe(Pipe&& org)noexcept
    : Stream(std::move(org)), m_pOnConnect(std::move(org.m_pOnConnect))
{
}

Pipe& Pipe::operator=(Pipe&& rhs)noexcept
{
    Stream::operator=(std::move(rhs));
    m_pOnConnect = std::move(rhs.m_pOnConnect);
    return *this;
}

void Pipe::Open(int fd)
{
    MOE_UV_GET_HANDLE(::uv_pipe_t);
    MOE_UV_CHECK(::uv_pipe_open(handle, fd));
}

void Pipe::Bind(const char* name)
{
    MOE_UV_GET_HANDLE(::uv_pipe_t);
    MOE_UV_CHECK(::uv_pipe_bind(handle, name));
}

void Pipe::Connect(const char* name)
{
    MOE_UV_GET_HANDLE(::uv_pipe_t);

    MOE_UV_NEW(UVConnectRequest);

    // 发起连接操作
    ::uv_pipe_connect(&object->Request, handle, name, OnUVConnect);

    // 释放所有权，交由UV管理
    auto& req = object->Request;
    req.data = object.release();
}

std::string Pipe::GetSockName()
{
    MOE_UV_GET_HANDLE(::uv_pipe_t);

    string result;
    result.resize(256);
    size_t size = result.length();

    int ret = ::uv_pipe_getsockname(handle, &result[0], &size);
    if (ret == UV_ENOBUFS)
    {
        result.resize(size);
        MOE_UV_CHECK(::uv_pipe_getsockname(handle, &result[0], &size));
    }

    result.resize(std::strlen(result.c_str()));
    return result;
}

std::string Pipe::GetPeerName()
{
    MOE_UV_GET_HANDLE(::uv_pipe_t);

    string result;
    result.resize(256);
    size_t size = result.length();

    int ret = ::uv_pipe_getpeername(handle, &result[0], &size);
    if (ret == UV_ENOBUFS)
    {
        result.resize(size);
        MOE_UV_CHECK(::uv_pipe_getpeername(handle, &result[0], &size));
    }

    result.resize(std::strlen(result.c_str()));
    return result;
}

void Pipe::OnConnect()
{
    if (m_pOnConnect)
        m_pOnConnect();
}
