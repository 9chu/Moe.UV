/**
 * @file
 * @author chu
 * @date 2017/11/28
 */
#pragma once
#include <vector>
#include <functional>

#include <Moe.Core/RefPtr.hpp>

#include "Context.hpp"
#include "GuardStack.hpp"

namespace moe
{
namespace UV
{
    class Scheduler;
    class Coroutine;

    class WaitHandle
    {

    };

    /**
     * @brief 协程
     *
     * 该类实现了非对称式共享栈协程。
     */
    class Coroutine
    {
    public:
        /**
         * @brief 获取当前线程上正在执行的协程
         * @return 如果没有协程正在执行则返回nullptr
         */
        static Coroutine* GetCurrent()noexcept;

        /**
         * @brief 主动释放当前协程的时间片并等待下一次调度时恢复运行
         *
         * 只能在协程上执行。
         */
        static void Yield()noexcept;

        /**
         * @brief 主动挂起协程
         * @param handle 等待句柄
         *
         * 将当前协程挂起并将调度权交予WaitHandle，通过WaitHandle来使协程继续运行或者抛出异常。
         */
        static void Suspend(WaitHandle& handle);

        /**
         * @brief 创建协程
         * @param entry 入口
         */
        static void Create(std::function<void()> entry);
        static void Create(std::function<void()>&& entry);
    };

    /**
     * @brief 协程调度器
     */
    class Scheduler :
        public NonCopyable
    {
        enum class CoroutineState
        {
            Ready,
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
            CoroutineController* WaitNext = nullptr;

            void Reset(std::function<void()> entry);
            void Reset(std::function<void()>&& entry);
        };

    public:
        /**
         * @brief 获取当前线程上的调度器
         * @return 如果当前线程上尚未创建调度器则返回nullptr
         */
        static Scheduler* GetCurrent()noexcept;

    public:
        Scheduler();
        ~Scheduler();

        Scheduler(Scheduler&&)noexcept = delete;
        Scheduler& operator=(Scheduler&&)noexcept = delete;

    public:
        size_t GetRunningCount()const noexcept { return m_uRunningCount; }
        size_t GetPendingCount()const noexcept { return m_uPendingCount; }
        size_t GetFreeCount()const noexcept { return m_uFreeCount; }

        /**
         * @brief 创建Coroutine并加入调度队列
         * @param entry 入口函数
         */
        void Create(std::function<void()> entry);
        void Create(std::function<void()>&& entry);

        /**
         * @brief 执行队列中的协程
         * @param count 执行数量，设为0不限
         */
        void Schedule(uint32_t count=0)noexcept;

        /**
         * @brief 回收空闲协程以释放内存
         */
        void CollectGarbage()noexcept;

    private:
        // 就绪链表
        size_t m_uRunningCount = 0;
        RefPtr<CoroutineController> m_pRunningHead = nullptr;
        RefPtr<CoroutineController> m_pRunningTail = nullptr;

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
        ContextState m_pMainThreadContext = nullptr;
        RefPtr<CoroutineController> m_pRunningCoroutine;
    };
}
}
