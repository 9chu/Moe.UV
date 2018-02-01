/**
 * @file
 * @author chu
 * @date 2017/12/2
 */
#include <Moe.UV/Dns.hpp>
#include <Moe.UV/RunLoop.hpp>

#include <Moe.Coroutine/Event.hpp>
#include <Moe.Coroutine/Coroutine.hpp>
#include <Moe.Coroutine/Scheduler.hpp>

using namespace std;
using namespace moe;
using namespace UV;
using namespace Coroutine;

namespace
{
    struct UVGetAddrInfoReq :
        public ObjectPool::RefBase<UVGetAddrInfoReq>
    {
        ::uv_getaddrinfo_t Request;

        RefPtr<Event> Event;
        exception_ptr Exception;

        vector<EndPoint> Result;

        static void Callback(::uv_getaddrinfo_t* req, int status, ::addrinfo* res)
        {
            RefPtr<UVGetAddrInfoReq> self(static_cast<UVGetAddrInfoReq*>(req->data));

            if (status != 0)
            {
                try
                {
                    MOE_UV_THROW(status);
                }
                catch (...)
                {
                    if (res)
                        ::uv_freeaddrinfo(res);
                    self->Exception = std::current_exception();
                    self->Event->SetEvent();
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
                self->Event->SetEvent();
            }
            catch (...)
            {
                self->Result.clear();

                // bad_alloc
                self->Exception = std::current_exception();
                self->Event->SetEvent();
            }

            if (res)
                ::uv_freeaddrinfo(res);
        }
    };

    struct UVGetNameInfoReq :
        public ObjectPool::RefBase<UVGetNameInfoReq>
    {
        ::uv_getnameinfo_t Request;

        RefPtr<Event> Event;
        exception_ptr Exception;

        string Hostname;
        string Service;

        static void Callback(::uv_getnameinfo_t* req, int status, const char* hostname, const char* service)
        {
            RefPtr<UVGetNameInfoReq> self(static_cast<UVGetNameInfoReq*>(req->data));

            if (status != 0)
            {
                try
                {
                    MOE_UV_THROW(status);
                }
                catch (...)
                {
                    self->Exception = std::current_exception();
                    self->Event->SetEvent();
                    return;
                }
            }

            try
            {
                self->Hostname = hostname;
                self->Service = service;

                // 恢复协程
                self->Event->SetEvent();
            }
            catch (...)
            {
                // bad_alloc
                self->Exception = std::current_exception();
                self->Event->SetEvent();
            }
        }
    };
}

EndPoint Dns::CoResolve(const char* hostname)
{
    auto thread = Coroutine::GetCurrentThread();
    if (!thread)
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVGetAddrInfoReq>();
    req->Event = thread->GetScheduler().CreateEvent(true);
    assert(req->Event);

    MOE_UV_CHECK(::uv_getaddrinfo(RunLoop::GetCurrentUVLoop(), &req->Request, UVGetAddrInfoReq::Callback, hostname,
        nullptr, nullptr));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVGetAddrInfoReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    auto ret = Coroutine::Wait(req->Event);
    MOE_UNUSED(ret);
    assert(ret == ThreadWaitResult::Succeed);
    if (req->Exception)
        rethrow_exception(req->Exception);

    if (req->Result.size() > 0)
        return req->Result[0];

    MOE_THROW(ObjectNotFoundException, "Can't resolve domain \"{0}\"", hostname);
}

void Dns::CoResolve(std::vector<EndPoint>& out, const char* hostname)
{
    auto thread = Coroutine::GetCurrentThread();
    if (!thread)
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVGetAddrInfoReq>();
    req->Event = thread->GetScheduler().CreateEvent(true);
    assert(req->Event);

    MOE_UV_CHECK(::uv_getaddrinfo(RunLoop::GetCurrentUVLoop(), &req->Request, UVGetAddrInfoReq::Callback, hostname,
        nullptr, nullptr));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVGetAddrInfoReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    auto ret = Coroutine::Wait(req->Event);
    MOE_UNUSED(ret);
    assert(ret == ThreadWaitResult::Succeed);
    if (req->Exception)
        rethrow_exception(req->Exception);

    // 如果没有错误就移动结果
    out = std::move(req->Result);
}

void Dns::CoReverse(const EndPoint& address, std::string& hostname)
{
    auto thread = Coroutine::GetCurrentThread();
    if (!thread)
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVGetNameInfoReq>();
    req->Event = thread->GetScheduler().CreateEvent(true);
    assert(req->Event);

    MOE_UV_CHECK(::uv_getnameinfo(RunLoop::GetCurrentUVLoop(), &req->Request, UVGetNameInfoReq::Callback,
        reinterpret_cast<const ::sockaddr*>(&address.Storage), 0));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVGetNameInfoReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    auto ret = Coroutine::Wait(req->Event);
    MOE_UNUSED(ret);
    assert(ret == ThreadWaitResult::Succeed);
    if (req->Exception)
        rethrow_exception(req->Exception);

    // 如果没有错误就移动结果
    hostname = std::move(req->Hostname);
}

void Dns::CoReverse(const EndPoint& address, std::string& hostname, std::string& service)
{
    auto thread = Coroutine::GetCurrentThread();
    if (!thread)
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVGetNameInfoReq>();
    req->Event = thread->GetScheduler().CreateEvent(true);
    assert(req->Event);

    MOE_UV_CHECK(::uv_getnameinfo(RunLoop::GetCurrentUVLoop(), &req->Request, UVGetNameInfoReq::Callback,
        reinterpret_cast<const ::sockaddr*>(&address.Storage), 0));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVGetNameInfoReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    auto ret = Coroutine::Wait(req->Event);
    MOE_UNUSED(ret);
    assert(ret == ThreadWaitResult::Succeed);
    if (req->Exception)
        rethrow_exception(req->Exception);

    // 如果没有错误就移动结果
    hostname = std::move(req->Hostname);
    service = std::move(req->Service);
}
