/**
 * @file
 * @author chu
 * @date 2018/8/15
 */
#include <Moe.UV/Dns.hpp>

#include <Moe.UV/RunLoop.hpp>
#include <Moe.Core/Logging.hpp>

using namespace std;
using namespace moe;
using namespace UV;

struct UVGetAddrInfoReq
{
    ::uv_getaddrinfo_t Request;

    Dns::OnResolveCallbackType OnResolveOne;
    Dns::OnResolveAllCallbackType OnResolveAll;

    static void Callback(::uv_getaddrinfo_t* req, int status, ::addrinfo* res)
    {
        UniquePooledObject<UVGetAddrInfoReq> self;
        self.reset(static_cast<UVGetAddrInfoReq*>(req->data));

        if (status != 0)
        {
            MOE_UV_CATCH_ALL_BEGIN
                if (self->OnResolveOne)
                    self->OnResolveOne(static_cast<uv_errno_t>(status), EmptyRefOf<EndPoint>());
                else
                    self->OnResolveAll(static_cast<uv_errno_t>(status), EmptyRefOf<vector<EndPoint>>());
            MOE_UV_CATCH_ALL_END
        }
        else
        {
            ::addrinfo* node = res;

            EndPoint ep;
            vector<EndPoint> result;
            while (node)
            {
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

                if (self->OnResolveOne)
                    break;

                bool ignore = false;
                for (auto& st : result)
                {
                    if (st == ep)
                    {
                        ignore = true;
                        break;
                    }
                }

                if (!ignore)
                {
                    try
                    {
                        result.push_back(ep);
                    }
                    catch (const bad_alloc&)
                    {
                        MOE_UV_CATCH_ALL_BEGIN
                            self->OnResolveAll(UV_ENOMEM, EmptyRefOf<vector<EndPoint>>());
                        MOE_UV_CATCH_ALL_END

                        if (res)
                            ::uv_freeaddrinfo(res);
                        return;
                    }
                }

                node = node->ai_next;
            }

            if (self->OnResolveOne)
            {
                MOE_UV_CATCH_ALL_BEGIN
                    self->OnResolveOne(static_cast<uv_errno_t>(0), ep);
                MOE_UV_CATCH_ALL_END
            }
            else
            {
                MOE_UV_CATCH_ALL_BEGIN
                    self->OnResolveAll(static_cast<uv_errno_t>(0), result);
                MOE_UV_CATCH_ALL_END
            }
        }

        if (res)
            ::uv_freeaddrinfo(res);
    }
};

struct UVGetNameInfoReq
{
    ::uv_getnameinfo_t Request;

    Dns::OnReverseResolveCallbackType OnReverseResolve;

    static void Callback(::uv_getnameinfo_t* req, int status, const char* hostname, const char* service)
    {
        UniquePooledObject<UVGetNameInfoReq> self;
        self.reset(static_cast<UVGetNameInfoReq*>(req->data));

        if (status != 0)
        {
            if (self->OnReverseResolve)
            {
                MOE_UV_CATCH_ALL_BEGIN
                    self->OnReverseResolve(static_cast<uv_errno_t>(status), EmptyRefOf<string>(), EmptyRefOf<string>());
                MOE_UV_CATCH_ALL_END
            }
        }
        else
        {
            if (self->OnReverseResolve)
            {
                MOE_UV_CATCH_ALL_BEGIN
                    self->OnReverseResolve(static_cast<uv_errno_t>(0), hostname, service);
                MOE_UV_CATCH_ALL_END
            }
        }
    }
};

#define MOVE_OWNER_SELF \
    do { \
        auto raw = object.release(); \
        raw->Request.data = raw; \
    } while (false)

void Dns::Resolve(const char* hostname, const OnResolveCallbackType& cb)
{
    MOE_UV_NEW_UNIQUE(UVGetAddrInfoReq);
    object->OnResolveOne = cb;

    MOE_UV_CHECK(::uv_getaddrinfo(RunLoop::GetCurrentUVLoop(), &object->Request, UVGetAddrInfoReq::Callback, hostname,
        nullptr, nullptr));
    MOVE_OWNER_SELF;
}

void Dns::Resolve(const char* hostname, OnResolveCallbackType&& cb)
{
    MOE_UV_NEW_UNIQUE(UVGetAddrInfoReq);
    object->OnResolveOne = std::move(cb);

    MOE_UV_CHECK(::uv_getaddrinfo(RunLoop::GetCurrentUVLoop(), &object->Request, UVGetAddrInfoReq::Callback, hostname,
        nullptr, nullptr));
    MOVE_OWNER_SELF;
}

void Dns::ResolveAll(const char* hostname, const OnResolveAllCallbackType& cb)
{
    MOE_UV_NEW_UNIQUE(UVGetAddrInfoReq);
    object->OnResolveAll = cb;

    MOE_UV_CHECK(::uv_getaddrinfo(RunLoop::GetCurrentUVLoop(), &object->Request, UVGetAddrInfoReq::Callback, hostname,
        nullptr, nullptr));
    MOVE_OWNER_SELF;
}

void Dns::ResolveAll(const char* hostname, OnResolveAllCallbackType&& cb)
{
    MOE_UV_NEW_UNIQUE(UVGetAddrInfoReq);
    object->OnResolveAll = std::move(cb);

    MOE_UV_CHECK(::uv_getaddrinfo(RunLoop::GetCurrentUVLoop(), &object->Request, UVGetAddrInfoReq::Callback, hostname,
        nullptr, nullptr));
    MOVE_OWNER_SELF;
}

void Dns::ReverseResolve(const EndPoint& address, const OnReverseResolveCallbackType& cb)
{
    MOE_UV_NEW_UNIQUE(UVGetNameInfoReq);
    object->OnReverseResolve = cb;

    MOE_UV_CHECK(::uv_getnameinfo(RunLoop::GetCurrentUVLoop(), &object->Request, UVGetNameInfoReq::Callback,
        reinterpret_cast<const ::sockaddr*>(&address.Storage), 0));
    MOVE_OWNER_SELF;
}

void Dns::ReverseResolve(const EndPoint& address, OnReverseResolveCallbackType&& cb)
{
    MOE_UV_NEW_UNIQUE(UVGetNameInfoReq);
    object->OnReverseResolve = std::move(cb);

    MOE_UV_CHECK(::uv_getnameinfo(RunLoop::GetCurrentUVLoop(), &object->Request, UVGetNameInfoReq::Callback,
        reinterpret_cast<const ::sockaddr*>(&address.Storage), 0));
    MOVE_OWNER_SELF;
}
