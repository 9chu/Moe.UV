/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#pragma once
#include <Moe.Core/Pal.hpp>
#include <Moe.Core/ArrayView.hpp>

#ifdef MOE_WINDOWS

#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>

#ifdef Yield  // fvck Windows
#undef Yield
#endif
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#ifdef SetPort
#undef SetPort
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
        using Ipv4AddressType = std::array<uint8_t, 4>;
        using Ipv6AddressType = std::array<uint16_t, 8>;

        static_assert(sizeof(::sockaddr_storage) >= sizeof(::sockaddr_in), "Bad system api");
        static_assert(sizeof(::sockaddr_storage) >= sizeof(::sockaddr_in6), "Bad system api");

        EndPoint()noexcept;
        EndPoint(const ::sockaddr_in& ipv4)noexcept;
        EndPoint(const ::sockaddr_in6& ipv6)noexcept;
        EndPoint(const ::sockaddr_storage& storage)noexcept;
        EndPoint(const char* addr, uint16_t port);
        EndPoint(const std::string& addr, uint16_t port);
        EndPoint(ArrayView<char> addr, uint16_t port);
        EndPoint(uint32_t addr, uint16_t port)noexcept;
        EndPoint(const Ipv4AddressType& addr, uint16_t port)noexcept;
        EndPoint(const Ipv6AddressType& addr, uint16_t port)noexcept;

        bool operator==(const EndPoint& rhs)const noexcept;
        bool operator!=(const EndPoint& rhs)const noexcept;

        bool IsIpv4()const noexcept;
        bool IsIpv6()const noexcept;

        std::string GetAddress()const;
        EndPoint& SetAddress(const char* addr);  // 当格式错误抛出异常，下同
        EndPoint& SetAddress(const std::string& addr);
        EndPoint& SetAddress(ArrayView<char> addr);

        uint32_t GetAddressIpv4()const noexcept;
        void GetAddressIpv4(Ipv4AddressType& addr)const noexcept;
        EndPoint& SetAddressIpv4(uint32_t addr)noexcept;
        EndPoint& SetAddressIpv4(const Ipv4AddressType& addr)noexcept;

        void GetAddressIpv6(Ipv6AddressType& addr)const noexcept;
        EndPoint& SetAddressIpv6(const Ipv6AddressType& addr)noexcept;

        uint16_t GetPort()const noexcept;
        EndPoint& SetPort(uint16_t port)noexcept;

        std::string ToString()const;

        ::sockaddr_storage Storage;
    };
}
}
