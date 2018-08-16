/**
 * @file
 * @author chu
 * @date 2017/12/1
 */
#pragma once
#include <Moe.Core/Time.hpp>

#include "EndPoint.hpp"
#include "AsyncHandle.hpp"

#include <vector>

namespace moe
{
namespace UV
{
    /**
     * @brief 程序执行环境相关函数
     */
    namespace Environment
    {
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

        /**
         * @brief 资源用量
         */
        struct ProcessResourceUsage
        {
            Time::Tick UserCpuTimeUsed;
            Time::Tick SystemCpuTimeUsed;
            uint64_t MaxResidentSetSize; /* maximum resident set size */
            uint64_t IntegralSharedMemorySize;  // UNIX
            uint64_t IntegralUnsharedDataSize;  // UNIX
            uint64_t IntegralUnsharedStackSize;  // UNIX
            uint64_t SoftPageFaults;  // UNIX
            uint64_t HardPageFaults;
            uint64_t Swaps;  // UNIX
            uint64_t BlockInputOperations;
            uint64_t BlockOutputOperations;
            uint64_t IpcMessageSent;  // UNIX
            uint64_t IpcMessageReceived;  // UNIX
            uint64_t SignalReceived;  // UNIX
            uint64_t VoluntaryContextSwitches;  // UNIX
            uint64_t InvoluntaryContextSwitches;  // UNIX
        };

        /**
         * @brief 枚举CPU核心信息
         * @param[out] info 输出结果
         * @return CPU核心个数
         */
        size_t EnumCpuInfo(std::vector<CpuInfo>& info);

        /**
         * @brief 枚举网卡信息
         * @param[out] info 输出结果
         * @return 网卡个数
         */
        size_t EnumInterface(std::vector<InterfaceInfo>& info);

        /**
         * @brief 获取系统负载
         * @param[out] avg1 一分钟负载
         * @param[out] avg5 五分钟负载
         * @param[out] avg15 十五分钟负载
         *
         * Windows下无效，始终返回0。
         */
        void GetLoadAverage(double& avg1, double& avg5, double& avg15)noexcept;

        /**
         * @brief 获取系统总内存大小（字节）
         */
        uint64_t GetTotalMemory()noexcept;

        /**
         * @brief 获取主机名
         */
        std::string GetHostName();

        /**
         * @brief 获取当前进程的专有内存大小
         */
        size_t GetResidentSetMemorySize();

        /**
         * @brief 获取当前进程的资源用量
         */
        ProcessResourceUsage GetResourceUsage();

        /**
         * @brief 获取当前进程的ID
         */
        uint64_t GetPorcessId()noexcept;

        /**
         * @brief 获取当前父进程的ID
         */
        uint64_t GetProcessParentId()noexcept;

        /**
         * @brief 获取当前二进制映像的路径
         */
        std::string GetProcessExePath();

        /**
         * @brief 获取当前的工作目录
         */
        std::string GetWorkingDirectory();

        /**
         * @brief 设置工作目录
         * @param path 工作目录
         */
        void SetWorkingDirectory(const char* path);

        /**
         * @brief 获取系统的家目录
         */
        std::string GetHomeDirectory();

        /**
         * @brief 获取系统的临时目录
         */
        std::string GetTempDirectory();

        /**
         * @brief 获取环境变量
         * @param name 环境变量键
         */
        std::string GetEnvironment(const char* name);

        /**
         * @brief 设置环境变量
         * @param name 环境变量键
         * @param value 环境变量值
         */
        void SetEnvironment(const char* name, const char* value);

        /**
         * @brief 删除环境变量
         * @param name 环境变量键
         */
        void DeleteEnvironment(const char* name);
    };
}
}
