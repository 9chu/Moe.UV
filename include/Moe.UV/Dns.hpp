/**
 * @file
 * @author chu
 * @date 2017/12/2
 */
#pragma once
#include "AsyncHandle.hpp"
#include "EndPoint.hpp"

#include <vector>
#include <functional>

namespace moe
{
namespace UV
{
    /**
     * @brief 异步DNS解析
     */
    class Dns
    {
    public:
        using OnResolveCallbackType = std::function<void(uv_errno_t, const EndPoint&)>;
        using OnResolveAllCallbackType = std::function<void(uv_errno_t, const std::vector<EndPoint>&)>;
        // (status, hostname, service)
        using OnReverseResolveCallbackType = std::function<void(uv_errno_t, const std::string&, const std::string&)>;

        /**
         * @brief 解析地址
         * @param hostname 待解析主机名
         * @param cb 回调函数
         *
         * 必须有RunLoop才能调用。
         * 方法选取首个解析的地址。
         */
        static void Resolve(const char* hostname, const OnResolveCallbackType& cb);
        static void Resolve(const char* hostname, OnResolveCallbackType&& cb);

        /**
         * @brief 解析地址
         * @param hostname 待解析主机名
         * @param cb 回调函数
         *
         * 必须有RunLoop才能调用。
         */
        static void ResolveAll(const char* hostname, const OnResolveAllCallbackType& cb);
        static void ResolveAll(const char* hostname, OnResolveAllCallbackType&& cb);

        /**
         * @brief 反向解析地址
         * @param address 待解析IP，此处复用EndPoint结构，忽略端口
         * @param cb 回调函数
         *
         * 必须有RunLoop才能调用。
         */
        static void ReverseResolve(const EndPoint& address, const OnReverseResolveCallbackType& cb);
        static void ReverseResolve(const EndPoint& address, OnReverseResolveCallbackType&& cb);
    };
}
}
