/**
 * @file
 * @author chu
 * @date 2017/12/2
 */
#include <Moe.UV/Dns.hpp>
#include <Moe.UV/RunLoop.hpp>

using namespace std;
using namespace moe;
using namespace UV;

namespace
{
    struct UVGetAddrInfoReq :
        public ObjectPool::RefBase<UVGetAddrInfoReq>
    {
        ::uv_getaddrinfo_t Request;
        WaitHandle WaitHandle;
        vector<EndPoint> Result;

        static void Callback(::uv_getaddrinfo_t* req, int status, ::addrinfo* res)
        {
            RefPtr<UVGetAddrInfoReq> self(static_cast<UVGetAddrInfoReq*>(req->data));

            if (status != 0)
            {
                try
                {
                    if (status == UV_ECANCELED)
                        MOE_THROW(OperationCancelledException, "Dns resolve is cancelled");
                    MOE_UV_THROW(status);
                }
                catch (...)
                {
                    if (res)
                        ::uv_freeaddrinfo(res);
                    self->WaitHandle.ResumeException(std::current_exception());
                    return;
                }
            }

            try
            {
                ::addrinfo* node = res;
                while (node)
                {
                    EndPoint ep;
                    if (node->ai_family == AF_INET)
                    {
                        assert(node->ai_addrlen >= sizeof(::sockaddr_in));
                        ep = EndPoint(*reinterpret_cast<::sockaddr_in*>(node->ai_addr));
                    }
                    else if (node->ai_family == AF_INET6)
                    {
                        assert(node->ai_addrlen >= sizeof(::sockaddr_in6));
                        ep = EndPoint(*reinterpret_cast<::sockaddr_in*>(node->ai_addr));
                    }

                    bool ignore = false;
                    for (auto& st : self->Result)
                    {
                        if (st == ep)
                        {
                            ignore = true;
                            break;
                        }
                    }

                    if (!ignore)
                        self->Result.push_back(ep);

                    node = node->ai_next;
                }

                // 恢复协程
                self->WaitHandle.Resume();
            }
            catch (...)
            {
                self->Result.clear();

                // bad_alloc
                self->WaitHandle.ResumeException(std::current_exception());
            }

            if (res)
                ::uv_freeaddrinfo(res);
        }
    };

    struct UVGetNameInfoReq :
        public ObjectPool::RefBase<UVGetNameInfoReq>
    {
        ::uv_getnameinfo_t Request;
        WaitHandle WaitHandle;
        string Hostname;
        string Service;

        static void Callback(::uv_getnameinfo_t* req, int status, const char* hostname, const char* service)
        {
            RefPtr<UVGetNameInfoReq> self(static_cast<UVGetNameInfoReq*>(req->data));

            if (status != 0)
            {
                try
                {
                    if (status == UV_ECANCELED)
                        MOE_THROW(OperationCancelledException, "Dns resolve is cancelled");
                    MOE_UV_THROW(status);
                }
                catch (...)
                {
                    self->WaitHandle.ResumeException(std::current_exception());
                    return;
                }
            }

            try
            {
                self->Hostname = hostname;
                self->Service = service;

                // 恢复协程
                self->WaitHandle.Resume();
            }
            catch (...)
            {
                // bad_alloc
                self->WaitHandle.ResumeException(std::current_exception());
            }
        }
    };
}

void Dns::CoResolve(std::vector<EndPoint>& out, const char* hostname)
{
    auto runloop = RunLoop::GetCurrent();
    if (!runloop || !Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVGetAddrInfoReq>();
    MOE_UV_CHECK(::uv_getaddrinfo(&runloop->m_stLoop, &req->Request, UVGetAddrInfoReq::Callback, hostname, nullptr,
        nullptr));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVGetAddrInfoReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->WaitHandle);

    // 如果没有错误就移动结果
    out = std::move(req->Result);
}

void Dns::CoReverse(const EndPoint& address, std::string& hostname)
{
    auto runloop = RunLoop::GetCurrent();
    if (!runloop || !Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVGetNameInfoReq>();
    MOE_UV_CHECK(::uv_getnameinfo(&runloop->m_stLoop, &req->Request, UVGetNameInfoReq::Callback,
        reinterpret_cast<const ::sockaddr*>(&address.Storage), 0));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVGetNameInfoReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->WaitHandle);

    // 如果没有错误就移动结果
    hostname = std::move(req->Hostname);
}

void Dns::CoReverse(const EndPoint& address, std::string& hostname, std::string& service)
{
    auto runloop = RunLoop::GetCurrent();
    if (!runloop || !Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVGetNameInfoReq>();
    MOE_UV_CHECK(::uv_getnameinfo(&runloop->m_stLoop, &req->Request, UVGetNameInfoReq::Callback,
        reinterpret_cast<const ::sockaddr*>(&address.Storage), 0));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVGetNameInfoReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->WaitHandle);

    // 如果没有错误就移动结果
    hostname = std::move(req->Hostname);
    service = std::move(req->Service);
}
