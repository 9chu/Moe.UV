/**
 * @file
 * @author chu
 * @date 2017/12/1
 */
#pragma once
#include "IoHandle.hpp"

namespace moe
{
namespace UV
{
    /**
     * @brief （当前）进程信息
     */
    class ProcessInfo
    {
    public:
        /**
         * @brief 资源用量
         */
        struct ResourceUsage
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

    public:
        /**
         * @brief 获取专有内存大小
         */
        size_t GetResidentSetMemorySize();

        /**
         * @brief 获取资源用量
         */
        ResourceUsage GetResourceUsage();

        /**
         * @brief 获取当前二进制映像的路径
         */
        std::string GetExePath();

        /**
         * @brief 获取当前的工作目录
         */
        std::string GetWorkingDirectory();

        /**
         * @brief 设置工作目录
         * @param path 工作目录
         */
        void SetWorkingDirectory(const char* path);
    };
}
}
