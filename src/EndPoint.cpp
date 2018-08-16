/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#include <Moe.UV/EndPoint.hpp>
#include <Moe.Core/Exception.hpp>

#include <cmath>
#include <cerrno>
#include <algorithm>

using namespace std;
using namespace moe;
using namespace UV;

namespace
{
    /**
     * @brief 解析IPV4中的数字（十进制、十六进制、八进制）
     */
    bool ParseIpv4Number(uint64_t& result, const char* start, const char* end)noexcept
    {
        unsigned radix = 10;
        if (end - start >= 2)
        {
            if (start[0] == '0' && (start[1] == 'x' || start[1] == 'X'))  // 16进制
            {
                start += 2;
                radix = 16;
            }
            else if (start[0] == '0')  // 8进制
            {
                start += 1;
                radix = 8;
            }
        }

        result = 0;
        if (start >= end)
            return true;

        while (start < end)
        {
            char ch = *(start++);

            if (ch >= 'a' && ch <= 'f')
            {
                if (radix != 16)
                    return false;
                if (result != numeric_limits<uint64_t>::max())
                    result = result * 16 + (ch - 'a') + 10;
            }
            else if (ch >= 'A' && ch <= 'F')
            {
                if (radix != 16)
                    return false;
                if (result != numeric_limits<uint64_t>::max())
                    result = result * 16 + (ch - 'A') + 10;
            }
            else if (ch >= '0' && ch <= '9')
            {
                if ((ch == '8' || ch == '9') && radix == 8)
                    return false;
                if (result != numeric_limits<uint64_t>::max())
                    result = result * radix + (ch - '0');
            }
            else
                return false;

            if (result > numeric_limits<uint32_t>::max())
                result = numeric_limits<uint64_t>::max();
        }
        return true;
    }

    bool ParseIpv4(uint32_t& out, const char* start, const char* end)
    {
        auto input = start;
        auto mark = start;
        unsigned parts = 0;
        unsigned tooBigParts = 0;
        uint64_t numbers[4] = { 0, 0, 0, 0 };

        while (start <= end)
        {
            auto ch = static_cast<char>(start < end ? *start : '\0');
            if (ch == '.' || ch == '\0')
            {
                if (++parts > 4)
                    return false;  // IPV4地址至多只有4个部分
                if (start == mark)
                    return false;  // 无效的空的点分部分

                // 解析[mark, start)部分的数字
                uint64_t n = 0;
                if (!ParseIpv4Number(n, mark, start))
                    return false;
                if (n > 255)
                    ++tooBigParts;

                numbers[parts - 1] = n;
                mark = start + 1;
                if (ch == '.' && start + 1 >= end)
                    break;  // 允许输入的最后一个字符是'.'
            }
            ++start;
        }
        assert(parts > 0);

        if (tooBigParts > 1 || (tooBigParts == 1 && numbers[parts - 1] <= 255) ||
            numbers[parts - 1] >= std::pow(256, static_cast<double>(5 - parts)))
        {
            // 规范要求每个点分部分不能超过255，但是最后一个元素例外
            // 此外，整个IPV4的解析结果也要保证不能溢出
            MOE_THROW(BadFormatException, "Bad Ipv4 string: {0}", string(input, end));
        }

        // 计算IPV4值
        auto val = numbers[parts - 1];
        for (unsigned n = 0; n < parts - 1; ++n)
        {
            double b = 3 - n;
            val += static_cast<uint64_t>(numbers[n] * std::pow(256, b));
        }
        out = static_cast<uint32_t>(val);
        return true;
    }

    bool ParseIpv6(EndPoint::Ipv6AddressType& address, const char* start, const char* end)noexcept
    {
        auto ch = static_cast<char>(start < end ? *start : '\0');
        unsigned current = 0;  // 指示当前解析的部分
        uint32_t compress = 0xFFFFFFFFu;  // 指示压缩可扩充的位置

        address.fill(0);

        // 压缩的case '::xxxx'
        if (ch == ':')
        {
            if (end - start < 2 || start[1] != ':')
                return false;  // 无效的':
            start += 2;
            ch = static_cast<char>(start < end ? *start : '\0');
            ++current;
            compress = current;
        }

        while (ch != '\0')
        {
            if (current >= address.max_size())
                return false;  // 无效的地址（至多8个部分）

            // 压缩的case 'fe80::xxxxx'
            if (ch == ':')
            {
                if (compress != 0xFFFFFFFFu)
                    return false;  // 不可能同时存在两个压缩部分
                ++start;
                ch = static_cast<char>(start < end ? *start : '\0');
                address[current++] = 0;
                compress = current;
                continue;
            }

            uint32_t value = 0;
            unsigned len = 0;
            while (len < 4 && StringUtils::IsHexDigit(ch))
            {
                value = value * 16 + StringUtils::HexDigitToNumber(ch);

                ++start;
                ch = static_cast<char>(start < end ? *start : '\0');
                ++len;
            }

            switch (ch)
            {
                case '.':  // 内嵌IPV4地址
                    if (len == 0)
                        return false;
                    start -= len;  // 回退
                    ch = static_cast<char>(start < end ? *start : '\0');
                    if (current > address.max_size() - 2)  // IPV4地址占两个Piece
                        return false;

                    // 解析IPV4部分（这一地址只允许使用十进制点分结构）
                    {
                        unsigned numbers = 0;
                        while (ch != '\0')
                        {
                            value = 0xFFFFFFFF;
                            if (numbers > 0)
                            {
                                if (ch == '.' && numbers < 4)
                                {
                                    ++start;
                                    ch = static_cast<char>(start < end ? *start : '\0');
                                }
                                else
                                    return false;
                            }
                            if (!StringUtils::IsDigit(ch))
                                return false;
                            while (StringUtils::IsDigit(ch))
                            {
                                auto number = static_cast<unsigned>(ch - '0');
                                if (value == 0xFFFFFFFF)
                                    value = number;
                                else if (value == 0)
                                    return false;
                                else
                                    value = value * 10 + number;
                                if (value > 255)
                                    return false;
                                ++start;
                                ch = static_cast<char>(start < end ? *start : '\0');
                            }
                            address[current] = static_cast<uint16_t>(address[current] * 0x100 + value);
                            ++numbers;
                            if (numbers == 2 || numbers == 4)
                                ++current;
                        }
                        if (numbers != 4)
                            return false;
                    }
                    continue;
                case ':':
                    ++start;
                    ch = static_cast<char>(start < end ? *start : '\0');
                    if (ch == '\0')
                        return false;
                    break;
                case '\0':
                    break;
                default:
                    return false;
            }
            address[current] = static_cast<uint16_t>(value);
            ++current;
        }

        if (compress != 0xFFFFFFFFu)
        {
            auto swaps = current - compress;
            current = static_cast<unsigned>(address.max_size() - 1);
            while (current != 0 && swaps > 0)
            {
                auto swap = compress + swaps - 1;
                std::swap(address[current], address[swap]);
                --current;
                --swaps;
            }
        }
        else if (current != address.max_size())  // 没有压缩，则必然读取了所有的部分
            return false;
        return true;
    }
}

EndPoint::EndPoint()noexcept
{
    ::sockaddr_in v4 {};
    memset(&v4, 0, sizeof(v4));
    v4.sin_family = AF_INET;
    v4.sin_port = 0;
#ifdef MOE_WINDOWS
    v4.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
#else
    v4.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
    memcpy(&Storage, &v4, sizeof(v4));
}

EndPoint::EndPoint(const ::sockaddr_in& ipv4)noexcept
{
    assert(ipv4.sin_family == AF_INET);
    memcpy(&Storage, &ipv4, sizeof(ipv4));
}

EndPoint::EndPoint(const ::sockaddr_in6& ipv6)noexcept
{
    assert(ipv6.sin6_family == AF_INET6);
    memcpy(&Storage, &ipv6, sizeof(ipv6));
}

EndPoint::EndPoint(const ::sockaddr_storage& storage)noexcept
{
    assert(storage.ss_family == AF_INET || storage.ss_family == AF_INET6);
    memcpy(&Storage, &storage, sizeof(storage));
}

EndPoint::EndPoint(uint32_t addr, uint16_t port)noexcept
{
    ::sockaddr_in v4 {};
    memset(&v4, 0, sizeof(v4));
    v4.sin_family = AF_INET;
    v4.sin_port = htons(port);
#ifdef MOE_WINDOWS
    v4.sin_addr.S_un.S_addr = htonl(addr);
#else
    v4.sin_addr.s_addr = htonl(addr);
#endif
    memcpy(&Storage, &v4, sizeof(v4));
}

EndPoint::EndPoint(const Ipv4AddressType& addr, uint16_t port)noexcept
{
    uint32_t caddr = (addr[0] << 24) | (addr[1] << 16) | (addr[2] << 8) | addr[3];

    ::sockaddr_in v4 {};
    memset(&v4, 0, sizeof(v4));
    v4.sin_family = AF_INET;
    v4.sin_port = htons(port);
#ifdef MOE_WINDOWS
    v4.sin_addr.S_un.S_addr = htonl(caddr);
#else
    v4.sin_addr.s_addr = htonl(caddr);
#endif
    memcpy(&Storage, &v4, sizeof(v4));
}

EndPoint::EndPoint(const Ipv6AddressType& addr, uint16_t port)noexcept
{
    ::sockaddr_in6 v6 {};
    memset(&v6, 0, sizeof(v6));
    v6.sin6_family = AF_INET6;
    v6.sin6_port = htons(port);
#ifdef MOE_WINDOWS
    auto* b = v6.sin6_addr.u.Byte;
#else
    auto* b = v6.sin6_addr.s6_addr;
#endif
    for (auto it = addr.begin(); it != addr.end(); ++it)
    {
        auto s = *it;
        auto hi = static_cast<uint8_t>((s >> 8) & 0xFF);
        auto lo = static_cast<uint8_t>(s & 0xFF);
        *(b++) = hi;
        *(b++) = lo;
    }
}

EndPoint::EndPoint(const char* addr, uint16_t port)
{
    ::sockaddr_in v4 {};
    memset(&v4, 0, sizeof(v4));
    v4.sin_family = AF_INET;
    v4.sin_port = htons(port);
#ifdef MOE_WINDOWS
    v4.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
#else
    v4.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
    memcpy(&Storage, &v4, sizeof(v4));

    SetAddress(addr);
}

EndPoint::EndPoint(const std::string& addr, uint16_t port)
{
    ::sockaddr_in v4 {};
    memset(&v4, 0, sizeof(v4));
    v4.sin_family = AF_INET;
    v4.sin_port = htons(port);
#ifdef MOE_WINDOWS
    v4.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
#else
    v4.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
    memcpy(&Storage, &v4, sizeof(v4));

    SetAddress(addr);
}

EndPoint::EndPoint(ArrayView<char> addr, uint16_t port)
{
    ::sockaddr_in v4 {};
    memset(&v4, 0, sizeof(v4));
    v4.sin_family = AF_INET;
    v4.sin_port = htons(port);
#ifdef MOE_WINDOWS
    v4.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
#else
    v4.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
    memcpy(&Storage, &v4, sizeof(v4));

    SetAddress(addr);
}

bool EndPoint::operator==(const EndPoint& rhs)const noexcept
{
    if (Storage.ss_family != rhs.Storage.ss_family)
        return false;

    if (Storage.ss_family == AF_INET)
        return memcmp(&Storage, &rhs.Storage, sizeof(::sockaddr_in)) == 0;
    if (Storage.ss_family == AF_INET6)
        return memcmp(&Storage, &rhs.Storage, sizeof(::sockaddr_in6)) == 0;

    assert(false);
    return memcmp(&Storage, &rhs.Storage, sizeof(Storage)) == 0;
}

bool EndPoint::operator!=(const EndPoint& rhs)const noexcept
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

std::string EndPoint::GetAddress()const
{
    string ret;

    if (Storage.ss_family == AF_INET)
    {
        auto v4 = GetAddressIpv4();

        ret.reserve(15);
        for (uint32_t value = v4, n = 0; n < 4; n++)
        {
            char buf[4];
            Convert::ToDecimalString(static_cast<uint8_t>(value % 256), buf);

            ret.insert(0, buf);
            if (n < 3)
                ret.insert(0, 1, '.');
            value /= 256;
        }
    }
    else if (Storage.ss_family == AF_INET6)
    {
        Ipv6AddressType v6;
        GetAddressIpv6(v6);

        uint32_t start = 0;
        uint32_t compress = 0xFFFFFFFFu;

        ret.reserve(41);
        ret.push_back('[');

        // 找到最长的0的部分
        uint32_t cur = 0xFFFFFFFFu, count = 0, longest = 0;
        while (start < 8)
        {
            if (v6[start] == 0)
            {
                if (cur == 0xFFFFFFFFu)
                    cur = start;
                ++count;
            }
            else
            {
                if (count > longest && count > 1)
                {
                    longest = count;
                    compress = cur;
                }
                count = 0;
                cur = 0xFFFFFFFFu;
            }
            ++start;
        }
        if (count > longest && count > 1)
            compress = cur;

        // 序列化过程
        bool ignore0 = false;
        for (unsigned n = 0; n < 8; ++n)
        {
            auto piece = v6[n];
            if (ignore0 && piece == 0)
                continue;
            else if (ignore0)
                ignore0 = false;
            if (compress == n)
            {
                ret.append(n == 0 ? "::" : ":");
                ignore0 = true;
                continue;
            }

            char buf[5];
            Convert::ToHexStringLower(piece, buf);
            ret.append(buf);
            if (n < 7)
                ret.push_back(':');
        }

        ret.push_back(']');
    }
    return ret;
}

EndPoint& EndPoint::SetAddress(const char* addr)
{
    return SetAddress(ArrayView<char>(addr, strlen(addr)));
}

EndPoint& EndPoint::SetAddress(const std::string& addr)
{
    return SetAddress(ArrayView<char>(addr.c_str(), addr.length()));
}

EndPoint& EndPoint::SetAddress(ArrayView<char> addr)
{
    uint32_t v4 = 0;
    if (ParseIpv4(v4, addr.GetBuffer(), addr.GetBuffer() + addr.GetSize()))
        SetAddressIpv4(v4);
    else
    {
        Ipv6AddressType v6;
        if (!ParseIpv6(v6, addr.GetBuffer(), addr.GetBuffer() + addr.GetSize()))
            MOE_THROW(BadFormatException, "Bad address {0}", addr);
        SetAddressIpv6(v6);
    }
    return *this;
}

uint32_t EndPoint::GetAddressIpv4()const noexcept
{
    assert(Storage.ss_family == AF_INET);
    if (Storage.ss_family != AF_INET)
        return 0;

    auto real = reinterpret_cast<const ::sockaddr_in*>(&Storage);
#ifdef MOE_WINDOWS
    return ntohl(real->sin_addr.S_un.S_addr);
#else
    return ntohl(real->sin_addr.s_addr);
#endif
}

void EndPoint::GetAddressIpv4(Ipv4AddressType& addr)const noexcept
{
    auto v4 = GetAddressIpv4();
    addr[0] = static_cast<uint8_t>((v4 & 0xFF000000) >> 24);
    addr[1] = static_cast<uint8_t>((v4 & 0xFF0000) >> 16);
    addr[2] = static_cast<uint8_t>((v4 & 0xFF00) >> 8);
    addr[3] = static_cast<uint8_t>(v4 & 0xFF);
}

EndPoint& EndPoint::SetAddressIpv4(uint32_t addr)noexcept
{
    auto port = GetPort();

    ::sockaddr_in v4 {};
    memset(&v4, 0, sizeof(v4));
    v4.sin_family = AF_INET;
    v4.sin_port = htons(port);
#ifdef MOE_WINDOWS
    v4.sin_addr.S_un.S_addr = htonl(addr);
#else
    v4.sin_addr.s_addr = htonl(addr);
#endif
    memcpy(&Storage, &v4, sizeof(v4));
    return *this;
}

EndPoint& EndPoint::SetAddressIpv4(const Ipv4AddressType& addr)noexcept
{
    auto port = GetPort();
    uint32_t caddr = (addr[0] << 24) | (addr[1] << 16) | (addr[2] << 8) | addr[3];

    ::sockaddr_in v4 {};
    memset(&v4, 0, sizeof(v4));
    v4.sin_family = AF_INET;
    v4.sin_port = htons(port);
#ifdef MOE_WINDOWS
    v4.sin_addr.S_un.S_addr = htonl(caddr);
#else
    v4.sin_addr.s_addr = htonl(caddr);
#endif
    memcpy(&Storage, &v4, sizeof(v4));
    return *this;
}

void EndPoint::GetAddressIpv6(Ipv6AddressType& addr)const noexcept
{
    assert(Storage.ss_family == AF_INET6);
    if (Storage.ss_family != AF_INET6)
    {
        addr.fill(0);
        return;
    }

    auto real = reinterpret_cast<const ::sockaddr_in6*>(&Storage);
#ifdef MOE_WINDOWS
    const auto* b = real->sin6_addr.u.Byte;
#else
    const auto* b = real->sin6_addr.s6_addr;
#endif
    for (auto it = addr.begin(); it != addr.end(); ++it)
    {
        uint16_t x = (*(b++)) << 8;
        x |= *(b++);
        *it = x;
    }
}

EndPoint& EndPoint::SetAddressIpv6(const Ipv6AddressType& addr)noexcept
{
    auto port = GetPort();

    ::sockaddr_in6 v6 {};
    memset(&v6, 0, sizeof(v6));
    v6.sin6_family = AF_INET6;
    v6.sin6_port = htons(port);
#ifdef MOE_WINDOWS
    auto* b = v6.sin6_addr.u.Byte;
#else
    auto* b = v6.sin6_addr.s6_addr;
#endif
    for (auto it = addr.begin(); it != addr.end(); ++it)
    {
        auto s = *it;
        auto hi = static_cast<uint8_t>((s >> 8) & 0xFF);
        auto lo = static_cast<uint8_t>(s & 0xFF);
        *(b++) = hi;
        *(b++) = lo;
    }
    return *this;
}

uint16_t EndPoint::GetPort()const noexcept
{
    if (Storage.ss_family == AF_INET)
    {
        auto real = reinterpret_cast<const ::sockaddr_in*>(&Storage);
        return ntohs(real->sin_port);
    }
    else if (Storage.ss_family == AF_INET6)
    {
        auto real = reinterpret_cast<const ::sockaddr_in6*>(&Storage);
        return ntohs(real->sin6_port);
    }
    return 0;
}

EndPoint& EndPoint::SetPort(uint16_t port)noexcept
{
    if (Storage.ss_family == AF_INET)
    {
        auto real = reinterpret_cast<::sockaddr_in*>(&Storage);
        real->sin_port = htons(port);
    }
    else if (Storage.ss_family == AF_INET6)
    {
        auto real = reinterpret_cast<::sockaddr_in6*>(&Storage);
        real->sin6_port = htons(port);
    }
    return *this;
}

std::string EndPoint::ToString()const
{
    return StringUtils::Format("{0}:{1}", GetAddress(), GetPort());
}
