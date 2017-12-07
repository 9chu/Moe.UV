/**
 * @file
 * @author chu
 * @date 2017/12/2
 */
#pragma once
#include "IoHandle.hpp"
#include "EndPoint.hpp"

#include <vector>

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
        /**
         * @brief （协程）解析地址
         * @exception InvalidCallException 调用无效时抛出
         * @exception OperationCancelledException 调用被取消时抛出
         * @exception APIException API错误时抛出
         * @param[out] out 输出地址，此处复用EndPoint结构，其中端口总是为0
         * @param hostname 待解析主机名
         *
         * 只能被协程调用。
         * 必须有RunLoop才能调用。
         */
        static void CoResolve(std::vector<EndPoint>& out, const char* hostname);

        /**
         * @brief （协程）反向解析地址
         * @exception InvalidCallException 调用无效时抛出
         * @exception OperationCancelledException 调用被取消时抛出
         * @exception APIException API错误时抛出
         * @param ip 待解析IP，此处复用EndPoint结构，忽略端口
         * @param[out] hostname 地址
         *
         * 只能被协程调用。
         * 必须有RunLoop才能调用。
         */
        static void CoReverse(const EndPoint& address, std::string& hostname);

        /**
         * @brief （协程）反向解析地址
         * @exception InvalidCallException 调用无效时抛出
         * @exception OperationCancelledException 调用被取消时抛出
         * @exception APIException API错误时抛出
         * @param ip 待解析IP，此处复用EndPoint结构，忽略端口
         * @param[out] hostname 地址
         * @param[out] service 服务
         *
         * 只能被协程调用。
         * 必须有RunLoop才能调用。
         */
        static void CoReverse(const EndPoint& address, std::string& hostname, std::string& service);
    };
}
}
