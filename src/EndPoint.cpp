/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#include <Moe.UV/EndPoint.hpp>
#include <Moe.Core/Exception.hpp>

#include <cerrno>

using namespace std;
using namespace moe;
using namespace UV;

namespace
{
    static const size_t kMaxIpv4AddressLen = sizeof("255.255.255.255");
    static const size_t kMaxIpv6AddressLen = sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255");
    static_assert(INET_ADDRSTRLEN >= kMaxIpv4AddressLen, "Bad system api");
    static_assert(INET6_ADDRSTRLEN >= kMaxIpv6AddressLen, "Bad system api");

    void InetNtop4(const ::in_addr* addr, char* dst, size_t size)
    {
        static const char kFormat[128] = "%u.%u.%u.%u";

        if (addr == nullptr || dst == nullptr)
            MOE_THROW(BadArgumentException, "Invalid argument");

        const uint8_t* src = reinterpret_cast<const uint8_t*>(addr);
        char buffer[kMaxIpv4AddressLen];
        std::sprintf(buffer, kFormat, src[0], src[1], src[2], src[3]);

        if (size > 0)
        {
            ::strncpy(dst, buffer, size);
            dst[size - 1] = '\0';
        }
    }

    void InetNtop6(const in_addr6* addr, char* dst, size_t size)
    {
        const uint8_t* src = reinterpret_cast<const uint8_t*>(addr);
        char buffer[kMaxIpv6AddressLen], *tp;
        struct {
            int base, len;
        } best, cur;

        if (addr == nullptr || dst == nullptr)
            MOE_THROW(BadArgumentException, "Invalid argument");

        u_int words[8];
        ::memset(words, '\0', sizeof(words));
        for (int i = 0; i < 16; ++i)
            words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));

        best.base = -1;
        best.len = 0;
        cur.base = -1;
        cur.len = 0;
        for (int i = 0; i < 8; ++i)
        {
            if (words[i] == 0)
            {
                if (cur.base == -1)
                    cur.base = i, cur.len = 1;
                else
                    cur.len++;
            }
            else
            {
                if (cur.base != -1)
                {
                    if (best.base == -1 || cur.len > best.len)
                        best = cur;
                    cur.base = -1;
                }
            }
        }
        if (cur.base != -1)
        {
            if (best.base == -1 || cur.len > best.len)
                best = cur;
        }
        if (best.base != -1 && best.len < 2)
            best.base = -1;

        tp = buffer;
        for (int i = 0; i < 8; ++i)
        {
            if (best.base != -1 && i >= best.base && i < (best.base + best.len))
            {
                if (i == best.base)
                    *tp++ = ':';
                continue;
            }
            if (i != 0)
                *tp++ = ':';
            if (i == 6 && best.base == 0 && (best.len == 6 || (best.len == 7 && words[7] != 0x0001) ||
                (best.len == 5 && words[5] == 0xffff)))
            {
                InetNtop4(reinterpret_cast<const in_addr*>(src + 12), tp, sizeof(buffer) - (tp - buffer));
                tp += ::strlen(tp);
                break;
            }
            tp += std::sprintf(tp, "%x", words[i]);
        }
        if (best.base != -1 && (best.base + best.len) == CountOf(words))
            *tp++ = ':';
        *tp++ = '\0';

        if (size > 0)
        {
            ::strncpy(dst, buffer, size);
            dst[size - 1] = '\0';
        }
    }
}

const EndPoint EndPoint::Empty = EndPoint();

EndPoint EndPoint::FromIpv4(uint32_t addr, uint16_t port)noexcept
{
    ::sockaddr_in zero;
    ::memset(&zero, 0, sizeof(zero));
    zero.sin_family = AF_INET;
    zero.sin_port = htons(port);
#ifdef MOE_WINDOWS
    zero.sin_addr.S_un.S_addr = ::htonl(addr);
#else
    zero.sin_addr.s_addr = ::htonl(addr);
#endif

    return EndPoint(zero);
}

EndPoint EndPoint::FromIpv4(const Ipv4Address& addr, uint16_t port)noexcept
{
    uint32_t caddr = (std::get<0>(addr) << 24) | (std::get<1>(addr) << 16) | (std::get<2>(addr) << 8) |
        (std::get<3>(addr));

    ::sockaddr_in zero;
    ::memset(&zero, 0, sizeof(zero));
    zero.sin_family = AF_INET;
    zero.sin_port = ::htons(port);
#ifdef MOE_WINDOWS
    zero.sin_addr.S_un.S_addr = ::htonl(caddr);
#else
    zero.sin_addr.s_addr = ::htonl(caddr);
#endif

    return EndPoint(zero);
}

EndPoint EndPoint::FromIpv4(const char* addr, uint16_t port)
{
    ::sockaddr_in zero;
    ::memset(&zero, 0, sizeof(zero));
    zero.sin_family = AF_INET;
    zero.sin_port = ::htons(port);

    char ch = '\0';
    int octets = 0;
    bool sawDigit = false;
#ifdef MOE_WINDOWS
    uint8_t* tp = reinterpret_cast<uint8_t*>(&zero.sin_addr.S_un.S_addr);
#else
    uint8_t* tp = reinterpret_cast<uint8_t*>(&zero.sin_addr.s_addr);
#endif
    while ((ch = *(addr++)) != '\0')
    {
        if (ch >= '0' && ch <= '9')
        {
            unsigned int nw = *tp * 10 + static_cast<unsigned>(ch - '0');
            if (sawDigit && *tp == 0)
                MOE_THROW(BadFormat, "Invalid ipv4 address");
            if (nw > 255)
                MOE_THROW(BadFormat, "Invalid ipv4 address");
            *tp = static_cast<uint8_t>(nw);

            if (!sawDigit)
            {
                if (++octets > 4)
                    MOE_THROW(BadFormat, "Invalid ipv4 address");
                sawDigit = 1;
            }
        }
        else if (ch == '.' && sawDigit)
        {
            if (octets == 4)
                MOE_THROW(BadFormat, "Invalid ipv4 address");
            *(++tp) = 0;
            sawDigit = 0;
        }
        else
            MOE_THROW(BadFormat, "Invalid ipv4 address");
    }
    if (octets < 4)
        MOE_THROW(BadFormat, "Invalid ipv4 address");

    return EndPoint(zero);
}

EndPoint::EndPoint()noexcept
{
    ::sockaddr_in zero;
    ::memset(&zero, 0, sizeof(zero));
    zero.sin_family = AF_INET;
    zero.sin_port = 0;
#ifdef MOE_WINDOWS
    zero.sin_addr.S_un.S_addr = ::htonl(INADDR_ANY);
#else
    zero.sin_addr.s_addr = ::htonl(INADDR_ANY);
#endif

    ::memcpy(&Storage, &zero, sizeof(zero));
}

EndPoint::EndPoint(const ::sockaddr_in& ipv4)noexcept
{
    assert(ipv4.sin_family == AF_INET);

    ::memcpy(&Storage, &ipv4, sizeof(ipv4));
}

EndPoint::EndPoint(const ::sockaddr_in6& ipv6)noexcept
{
    assert(ipv6.sin6_family == AF_INET6);

    ::memcpy(&Storage, &ipv6, sizeof(ipv6));
}

EndPoint::EndPoint(const ::sockaddr_storage& storage)noexcept
    : Storage(storage)
{
    assert(storage.ss_family == AF_INET || storage.ss_family == AF_INET6);
}

bool EndPoint::operator==(const EndPoint& rhs)const
{
    if (Storage.ss_family != rhs.Storage.ss_family)
        return false;

    if (Storage.ss_family == AF_INET)
        return ::memcmp(&Storage, &rhs.Storage, sizeof(::sockaddr_in)) == 0;
    if (Storage.ss_family == AF_INET6)
        return ::memcmp(&Storage, &rhs.Storage, sizeof(::sockaddr_in6)) == 0;

    assert(false);
    return ::memcmp(&Storage, &rhs.Storage, sizeof(Storage)) == 0;
}

bool EndPoint::operator!=(const EndPoint& rhs)const
{
    return !this->operator==(rhs);
}

bool EndPoint::IsIpv4()const noexcept
{
    return Storage.ss_family == AF_INET;
}

bool EndPoint::IsIpv6()const noexcept
{
    return Storage.ss_family == AF_INET6;
}

uint16_t EndPoint::GetPort()const noexcept
{
    if (Storage.ss_family == AF_INET)
    {
        const ::sockaddr_in* real = reinterpret_cast<const ::sockaddr_in*>(&Storage);
        return ntohs(real->sin_port);
    }
    else
    {
        const ::sockaddr_in6* real = reinterpret_cast<const ::sockaddr_in6*>(&Storage);
        return ntohs(real->sin6_port);
    }
}

std::string EndPoint::GetAddress()const
{
    char address[INET6_ADDRSTRLEN] = { 0 };
    static_assert(INET6_ADDRSTRLEN >= INET_ADDRSTRLEN, "Bad system api");

    if (Storage.ss_family == AF_INET)
    {
        const ::sockaddr_in* real = reinterpret_cast<const ::sockaddr_in*>(&Storage);
        InetNtop4(&real->sin_addr, address, sizeof(address));
        return address;
    }
    else
    {
        const ::sockaddr_in6* real = reinterpret_cast<const ::sockaddr_in6*>(&Storage);
        InetNtop6(&real->sin6_addr, address, sizeof(address));
        return address;
    }
}

uint32_t EndPoint::GetAddressIpv4()const
{
    if (Storage.ss_family != AF_INET)
        MOE_THROW(InvalidCallException, "Address is not an ipv4 one");

    const ::sockaddr_in* real = reinterpret_cast<const ::sockaddr_in*>(&Storage);
#ifdef MOE_WINDOWS
    return real->sin_addr.S_un.S_addr;
#else
    return real->sin_addr.s_addr;
#endif
}

const uint8_t* EndPoint::GetAddressIpv6()const
{
    if (Storage.ss_family != AF_INET6)
        MOE_THROW(InvalidCallException, "Address is not an ipv6 one");

    const ::sockaddr_in6* real = reinterpret_cast<const ::sockaddr_in6*>(&Storage);
#ifdef MOE_WINDOWS
    return real->sin6_addr.u.Byte;
#else
    return real->sin6_addr.s6_addr;
#endif
}

std::string EndPoint::ToString()const
{
    if (Storage.ss_family == AF_INET6)
        return StringUtils::Format("[{0}]:{1}", GetAddress(), GetPort());
    return StringUtils::Format("{0}:{1}", GetAddress(), GetPort());
}
