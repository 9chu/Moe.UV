/**
 * @file
 * @author chu
 * @date 2018/12/30
 */
#pragma once
#include "AsyncHandle.hpp"

#include <vector>
#include <functional>

namespace moe
{
namespace UV
{
    /**
     * @brief 线程池
     */
    class ThreadPool
    {
    public:
        using OnWorkCallbackType = std::function<void(void*)>;  // (userdata)
        using OnAfterWorkCallbackType = std::function<void(int, void*)>;  // (errno, userdata)

    public:
        /**
         * @brief 在线程池中增加任务
         * @param work 任务回调（在工作线程执行）
         * @param afterWork 任务完成回调（在RunLoop线程执行）
         * @param userData 用户数据
         */
        static void QueueWork(const OnWorkCallbackType& work, const OnAfterWorkCallbackType& afterWork,
            void* userData=nullptr);
        static void QueueWork(OnWorkCallbackType&& work, OnAfterWorkCallbackType&& afterWork, void* userData=nullptr);
    };
}
}
