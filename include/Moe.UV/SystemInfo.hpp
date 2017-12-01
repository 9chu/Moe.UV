/**
 * @file
 * @author chu
 * @date 2017/12/1
 */
#pragma once
#include "IOHandle.hpp"
#include "EndPoint.hpp"

#include <vector>

namespace moe
{
namespace UV
{
    /**
     * @brief 系统信息
     */
    class SystemInfo
    {
    public:
        /**
         * @brief CPU状态信息
         */
        struct CpuInfo
        {
            std::string Model;
            uint32_t Speed;
            struct {
                uint64_t User;
                uint64_t Nice;
                uint64_t Sys;
                uint64_t Idle;
                uint64_t Irq;
            } CpuTimes;
        };

        /**
         * @brief 网卡信息
         */
        struct InterfaceInfo
        {
            std::string Name;
            uint8_t PhysicsAddress[6];
            bool IsInternal;
            EndPoint Address;
            EndPoint NetMask;

            std::string GetPhysicsAddressString();
        };

    public:
        /**
         * @brief 枚举CPU核心信息
         * @param[out] info 输出结果
         * @return CPU核心个数
         */
        static size_t EnumCpuInfo(std::vector<CpuInfo>& info);

        /**
         * @brief 枚举网卡信息
         * @param[out] info 输出结果
         * @return 网卡个数
         */
        static size_t EnumInterface(std::vector<InterfaceInfo>& info);

        /**
         * @brief 获取系统负载
         * @param[out] avg1 一分钟负载
         * @param[out] avg5 五分钟负载
         * @param[out] avg15 十五分钟负载
         *
         * Windows下无效。
         */
        static void GetLoadAverage(double& avg1, double& avg5, double& avg15)noexcept;

        /**
         * @brief 获取系统总内存大小（字节）
         */
        static uint64_t GetTotalMemory()noexcept;

        /**
         * @brief 获取主机名
         */
        static std::string GetHostName();
    };
}
}
