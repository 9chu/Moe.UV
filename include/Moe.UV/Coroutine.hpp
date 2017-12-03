/**
 * @file
 * @author chu
 * @date 2017/11/28
 */
#pragma once
#include <cstdint>
#include <vector>
#include <functional>

#include <Moe.Core/RefPtr.hpp>
#include <Moe.Core/Exception.hpp>

#include "Context.hpp"
#include "GuardStack.hpp"

namespace moe
{
namespace UV
{
    class Scheduler;
    class WaitHandle;

    /**
     * @brief 协程
     *
     * 该类实现了非对称式共享栈协程。
     */
    class Coroutine
    {
    public:
        /**
         * @brief 检查是否处于协程栈上
         */
        static bool InCoroutineContext()noexcept;

        /**
         * @brief 主动释放当前协程的时间片并等待下一次调度时恢复运行
         *
         * 只能在协程上执行。
         */
        static void Yield();

        /**
         * @brief 主动挂起协程
         * @param handle 等待句柄
         *
         * 只能在协程上执行。
         * 将当前协程挂起并将调度权交予WaitHandle，通过WaitHandle来使协程继续运行或者抛出异常。
         */
        static void Suspend(WaitHandle& handle);

        /**
         * @brief 创建并启动协程
         * @param entry 入口
         */
        static void Start(std::function<void()> entry);
    };

    /**
     * @brief 协程调度器
     */
    class Scheduler :
        public NonCopyable
    {
        friend class Coroutine;
        friend class WaitHandle;

        enum class CoroutineState
        {
            Created,
            Running,
            Suspend,
            Terminated,
        };

        enum class CoroutineResumeType
        {
            Normally,
            Exception,
        };

        struct CoroutineController :
            public RefBase<CoroutineController>,
            public NonCopyable
        {
            RefPtr<CoroutineController> Prev;
            RefPtr<CoroutineController> Next;

            std::function<void()> Entry;
            CoroutineState State = CoroutineState::Terminated;

            CoroutineResumeType ResumeType = CoroutineResumeType::Normally;
            std::exception_ptr ExceptionPtr;

            ContextState Context = nullptr;
            std::vector<uint8_t> StackBuffer;

            // 等待链
            WaitHandle* WaitHandle = nullptr;
            RefPtr<CoroutineController> WaitNext;

            void Reset(std::function<void()> entry);
        };

        static void CoroutineWrapper(ContextTransfer transfer)noexcept;

    public:
        /**
         * @brief 获取当前线程上的调度器
         * @return 如果当前线程上尚未创建调度器则返回nullptr
         */
        static Scheduler* GetCurrent()noexcept;

    public:
        Scheduler(size_t stackSize=0);
        ~Scheduler();

        Scheduler(Scheduler&&)noexcept = delete;
        Scheduler& operator=(Scheduler&&)noexcept = delete;

    public:
        /**
         * @brief 获取就绪的协程的数量
         */
        size_t GetReadyCount()const noexcept { return m_uReadyCount; }

        /**
         * @brief 获取挂起的协程的数量
         */
        size_t GetPendingCount()const noexcept { return m_uPendingCount; }

        /**
         * @brief 获取等待复用的协程的数量
         */
        size_t GetFreeCount()const noexcept { return m_uFreeCount; }

        /**
         * @brief 是否空闲
         */
        bool IsIdle()const noexcept { return GetReadyCount() == 0 && GetPendingCount() == 0; }

        /**
         * @brief 执行队列中的协程
         */
        void Schedule()noexcept;

        /**
         * @brief 回收空闲协程以释放内存
         */
        void CollectGarbage()noexcept;

    private:
        RefPtr<CoroutineController> GetRunningCoroutine()noexcept { return m_pRunningCoroutine; }

        void YieldCurrent();
        void SuspendCurrent(WaitHandle& handle);

        RefPtr<CoroutineController> Alloc();
        void Start(std::function<void()> entry);

    private:
        // 就绪链表
        size_t m_uReadyCount = 0;
        RefPtr<CoroutineController> m_pReadyHead = nullptr;
        RefPtr<CoroutineController> m_pReadyTail = nullptr;

        // 等待链表
        size_t m_uPendingCount = 0;
        RefPtr<CoroutineController> m_pPendingHead = nullptr;
        RefPtr<CoroutineController> m_pPendingTail = nullptr;

        // 空闲链表
        size_t m_uFreeCount = 0;
        RefPtr<CoroutineController> m_pFreeHead = nullptr;
        RefPtr<CoroutineController> m_pFreeTail = nullptr;

        // 运行状态
        GuardStack m_stSharedStack;
        RefPtr<CoroutineController> m_pRunningCoroutine;  // 正在运行的协程
        ContextState m_pMainThreadContext = nullptr;
    };

    /**
     * @brief 同步句柄
     *
     * 用于协程的事件机制。
     */
    class WaitHandle :
        public NonCopyable
    {
        friend class Scheduler;

    public:
        WaitHandle();
        WaitHandle(WaitHandle&& rhs)noexcept;
        ~WaitHandle();

        WaitHandle& operator=(WaitHandle&& rhs)noexcept;

    public:
        /**
         * @brief 是否没有协程在该句柄上等待
         */
        bool Empty()const noexcept { return !m_pHead; }

        /**
         * @brief 继续执行所有协程
         */
        void Resume()noexcept;

        /**
         * @brief 继续执行单个协程
         */
        void ResumeOne()noexcept;

        /**
         * @brief 以异常继续所有协程
         */
        void ResumeException(const std::exception_ptr& ptr)noexcept;

    private:
        Scheduler* m_pScheduler = nullptr;
        RefPtr<Scheduler::CoroutineController> m_pHead = nullptr;
    };
}
}
