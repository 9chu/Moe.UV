/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#pragma once
#include <tuple>
#include <string>

#include <Moe.Core/Utils.hpp>

#ifdef MOE_WINDOWS
#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#else

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace moe
{
namespace UV
{
    /**
     * @brief 套接字端点
     *
     * 封装ipv4和ipv6端点上的操作。
     * 仅支持AF_INET、AF_INET6协议簇。
     */
    struct EndPoint
    {
        using Ipv4Address = std::tuple<uint8_t, uint8_t, uint8_t, uint8_t>;

        static_assert(sizeof(::sockaddr_storage) >= sizeof(::sockaddr_in), "Bad system api");
        static_assert(sizeof(::sockaddr_storage) >= sizeof(::sockaddr_in6), "Bad system api");

        static const EndPoint Empty;

        static EndPoint FromIpv4(uint32_t addr, uint16_t port)noexcept;
        static EndPoint FromIpv4(const Ipv4Address& addr, uint16_t port)noexcept;
        static EndPoint FromIpv4(const char* addr, uint16_t port);

        EndPoint()noexcept;
        EndPoint(const ::sockaddr_in& ipv4)noexcept;
        EndPoint(const ::sockaddr_in6& ipv6)noexcept;
        EndPoint(const ::sockaddr_storage& storage)noexcept;

        bool operator==(const EndPoint& rhs)const;
        bool operator!=(const EndPoint& rhs)const;

        bool IsIpv4()const noexcept;
        bool IsIpv6()const noexcept;

        uint16_t GetPort()const noexcept;
        std::string GetAddress()const;
        uint32_t GetAddressIpv4()const;
        const uint8_t* GetAddressIpv6()const;  // return uint8_t[16]

        std::string ToString()const;

        ::sockaddr_storage Storage;
    };
}
}
