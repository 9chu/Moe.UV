/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#pragma once
#include "ObjectPool.hpp"
#include "Coroutine.hpp"
#include "IOHandle.hpp"

#include <Moe.Core/Time.hpp>
#include <Moe.Core/Utils.hpp>

namespace moe
{
namespace UV
{
    /**
     * @brief 程序循环
     */
    class RunLoop :
        public NonCopyable
    {
        friend class IOHandle;

    private:
        static void OnUVIdle(::uv_idle_t* handle)noexcept;

    public:
        /**
         * @brief 获取当前线程上的RunLoop
         */
        static RunLoop* GetCurrent()noexcept;

        /**
         * @brief 获取当前滴答数（毫秒级）
         *
         * 该值会在RunLoop单次迭代时更新。
         * 如果RunLoop不存在则直接获得系统当前的Tick。
         */
        static Time::Tick Now()noexcept;

    public:
        /**
         * @brief 构造消息循环
         * @param coroutineSharedStackSize 协程的共享栈大小
         */
        RunLoop(size_t coroutineSharedStackSize=4*1024*1024);

        ~RunLoop();

        RunLoop(RunLoop&&)noexcept = delete;
        RunLoop& operator=(RunLoop&&) = delete;

    public:
        /**
         * @brief 启动程序循环
         *
         * 运行循环直到所有句柄结束。
         */
        void Run();

        /**
         * @brief 尽快结束循环
         *
         * 通知RunLoop停止循环迭代。
         */
        void Stop()noexcept;

        /**
         * @brief 立即更新滴答数
         */
        void UpdateTime()noexcept;

    protected:  // 内部事件
        void OnIdle()noexcept;

    private:
        ObjectPool m_stObjectPool;
        Scheduler m_stScheduler;
        ::uv_loop_t m_stLoop;
        ::uv_idle_t m_stIdle;
    };
}
}
