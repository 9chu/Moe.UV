/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#pragma once
#include "ObjectPool.hpp"
#include "IoHandle.hpp"
#include "PrepareHandle.hpp"
#include "CheckHandle.hpp"
#include "Timeout.hpp"

#include <Moe.Core/Time.hpp>
#include <Moe.Core/Utils.hpp>
#include <Moe.Coroutine/Scheduler.hpp>

namespace moe
{
namespace UV
{
    class Dns;
    class File;
    class Filesystem;

    /**
     * @brief 程序循环
     */
    class RunLoop :
        public NonCopyable
    {
        friend class IoHandle;
        friend class Dns;
        friend class File;
        friend class Filesystem;

    public:
        using CallbackType = std::function<void()>;

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

    protected:
        static ::uv_loop_t* GetCurrentUVLoop();

    private:
        static void UVClosingHandleWalker(::uv_handle_t* handle, void* arg)noexcept;

    public:
        /**
         * @brief 构造消息循环
         * @param minimalTick 最小时间片
         * @param coroutineSharedStackSize 协程的共享栈大小
         */
        RunLoop(Time::Tick minimalTick=0, size_t coroutineSharedStackSize=4*1024*1024);

        ~RunLoop();

        RunLoop(RunLoop&&)noexcept = delete;
        RunLoop& operator=(RunLoop&&) = delete;

    public:
        /**
         * @brief 获取或设置Prepare回调
         *
         * 这一回调被保证发生在协程调度之前。
         */
        CallbackType GetPrepareCallback()const noexcept { return m_stPrepareCallback; }
        void SetPrepareCallback(const CallbackType& callback) { m_stPrepareCallback = callback; }
        void SetPrepareCallback(CallbackType&& callback)noexcept { m_stPrepareCallback = std::move(callback); }

        /**
         * @brief 获取或者设置Check回调
         *
         * 这一回调被保证发生在协程调度之后。
         */
        CallbackType GetCheckCallback()const noexcept { return m_stCheckCallback; }
        void SetCheckCallback(const CallbackType& callback) { m_stCheckCallback = callback; }
        void SetCheckCallback(CallbackType&& callback) { m_stCheckCallback = std::move(callback); }

        /**
         * @brief 是否处于关闭状态
         */
        bool IsClosing()const noexcept { return m_bClosing; }

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

    protected:
        void OnPrepare();
        void OnCheck();

    private:
        ObjectPool m_stObjectPool;
        Coroutine::Scheduler m_stScheduler;

        bool m_bClosing = false;
        Time::Tick m_uMinimalTick = 0;
        ::uv_loop_t m_stLoop;

        IoHandleHolder<PrepareHandle> m_stPrepareHandle;
        IoHandleHolder<CheckHandle> m_stCheckHandle;
        IoHandleHolder<Timeout> m_stSchedulerTimeout;

        CallbackType m_stPrepareCallback;
        CallbackType m_stCheckCallback;
    };
}
}
