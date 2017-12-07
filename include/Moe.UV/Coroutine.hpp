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
    class CoCondVar;

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
         * @brief 获取当前协程的Id
         * @exception InvalidCallException 不在协程上下文时抛出异常
         * @return 协程Id
         *
         * 协程ID从1开始给每个协程赋值，注意到复用的协程会保持之前的ID。
         * 理论上不会用完0xFFFFFFFF，因此可以认为是唯一的。
         */
        static uint32_t GetCoroutineId();

        /**
         * @brief 主动释放当前协程的时间片并等待下一次调度时恢复运行
         *
         * 只能在协程上执行。
         */
        static void Yield();

        /**
         * @brief 主动挂起协程
         * @param handle 条件变量
         * @return 返回数据
         *
         * 只能在协程上执行。
         * 挂起协程并等待一个条件变量被激活。
         */
        static ptrdiff_t Suspend(CoCondVar& handle);

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
        friend class CoCondVar;

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
            public NonCopyable
        {
            uint32_t Id = 0;

            CoroutineController* Prev;
            CoroutineController* Next;

            std::function<void()> Entry;
            CoroutineState State = CoroutineState::Terminated;

            CoroutineResumeType ResumeType = CoroutineResumeType::Normally;
            ptrdiff_t ResumeData = 0;
            std::exception_ptr ExceptionPtr;

            ContextState Context = nullptr;
            std::vector<uint8_t> StackBuffer;

            // 等待链
            CoCondVar* CondVar = nullptr;
            CoroutineController* WaitNext;

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
        CoroutineController* GetRunningCoroutine()noexcept { return m_pRunningCoroutine; }
        uint32_t GetRunningCoroutineId();

        void YieldCurrent();
        ptrdiff_t SuspendCurrent(CoCondVar& handle);

        CoroutineController* Alloc();
        void Start(std::function<void()> entry);

    private:
        // 就绪链表
        size_t m_uReadyCount = 0;
        CoroutineController* m_pReadyHead = nullptr;
        CoroutineController* m_pReadyTail = nullptr;

        // 等待链表
        size_t m_uPendingCount = 0;
        CoroutineController* m_pPendingHead = nullptr;
        CoroutineController* m_pPendingTail = nullptr;

        // 空闲链表
        // 释放操作总是在空闲链表进行
        uint32_t m_uId = 0;
        size_t m_uFreeCount = 0;
        CoroutineController* m_pFreeHead = nullptr;
        CoroutineController* m_pFreeTail = nullptr;

        // 运行状态
        GuardStack m_stSharedStack;
        CoroutineController* m_pRunningCoroutine;  // 正在运行的协程
        ContextState m_pMainThreadContext = nullptr;
    };

    /**
     * @brief 协程条件变量
     *
     * 用于实现协程的等待机制。
     */
    class CoCondVar :
        public NonCopyable
    {
        friend class Scheduler;

    public:
        CoCondVar();
        CoCondVar(CoCondVar&& rhs)noexcept;
        ~CoCondVar();

        CoCondVar& operator=(CoCondVar&& rhs)noexcept;

    public:
        /**
         * @brief 是否没有协程在该句柄上等待
         */
        bool Empty()const noexcept { return !m_pHead; }

        /**
         * @brief 继续执行所有协程
         * @param data 继续执行时传递的参数
         */
        void Resume(ptrdiff_t data=0)noexcept;

        /**
         * @brief 继续执行单个协程
         * @param data 继续执行时传递的参数
         */
        void ResumeOne(ptrdiff_t data=0)noexcept;

        /**
         * @brief 以异常继续所有协程
         */
        void ResumeException(const std::exception_ptr& ptr)noexcept;

    private:
        Scheduler* m_pScheduler = nullptr;
        Scheduler::CoroutineController* m_pHead = nullptr;
    };
}
}
