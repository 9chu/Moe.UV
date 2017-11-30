/**
 * @file
 * @author chu
 * @date 2017/11/28
 */
#include <Moe.UV/Coroutine.hpp>
#include <Moe.Core/Logging.hpp>

using namespace std;
using namespace moe;
using namespace UV;

//////////////////////////////////////////////////////////////////////////////// Coroutine

void Coroutine::Yield()
{
    auto scheduler = Scheduler::GetCurrent();
    if (!scheduler)
        MOE_THROW(InvalidCallException, "Bad execution context");
    scheduler->YieldCurrent();
}

void Coroutine::Suspend(WaitHandle& handle)
{
    auto scheduler = Scheduler::GetCurrent();
    if (!scheduler)
        MOE_THROW(InvalidCallException, "Bad execution context");
    scheduler->SuspendCurrent(handle);
}

void Coroutine::Start(std::function<void()> entry)
{
    auto scheduler = Scheduler::GetCurrent();
    if (!scheduler)
        MOE_THROW(InvalidCallException, "Bad execution context");
    scheduler->Start(entry);
}

//////////////////////////////////////////////////////////////////////////////// CoroutineController

void Scheduler::CoroutineController::Reset(std::function<void()> entry)
{
    Prev = nullptr;
    Next = nullptr;
    Entry = std::move(entry);
    State = CoroutineState::Created;
    ResumeType = CoroutineResumeType::Normally;
    ExceptionPtr = std::exception_ptr();
    Context = nullptr;
    StackBuffer.clear();
    WaitHandle = nullptr;
    WaitNext = nullptr;
}

//////////////////////////////////////////////////////////////////////////////// Scheduler

#define REMOVE_LINK_NODE(head, tail, node) \
    do { \
        if (node) { \
            (!node->Prev) ? (head = node->Next) : (node->Prev->Next = node->Next); \
            (!node->Next) ? (tail = node->Prev) : (node->Next->Prev = node->Prev); \
            node->Prev = nullptr; \
            node->Next = nullptr; \
        } \
    } while (false)

#define INSERT_AT_LINK_HEAD(head, tail, node) \
    do { \
        if (!head) \
            head = tail = node; \
        else { \
            node->Next = head; \
            head->Prev = node; \
            head = node; \
        } \
    } while (false)

#define INSERT_AT_LINK_TAIL(head, tail, node) \
    do { \
        if (!tail) \
            head = tail = node; \
        else { \
            node->Prev = tail; \
            tail->Next = node; \
            tail = node; \
        } \
    } while (false)

thread_local static Scheduler* t_pScheduler = nullptr;

void Scheduler::CoroutineWrapper(ContextTransfer transfer)noexcept
{
    Scheduler* scheduler = static_cast<Scheduler*>(transfer.Data);
    CoroutineController* self = scheduler->m_pRunningCoroutine.GetPointer();

    // 更新Context
    assert(scheduler == t_pScheduler);
    scheduler->m_pMainThreadContext = transfer.State;

    try
    {
        if (self->Entry)
            self->Entry();
    }
    catch (const moe::Exception& ex)
    {
        MOE_ERROR("Unhandled moe::Exception: {0}", ex.ToString());
    }
    catch (const std::exception& ex)
    {
        MOE_ERROR("Unhandled std::exception: {0}", ex.what());
    }
    catch (...)
    {
        MOE_ERROR("Unhandled unknown exception");
    }

    self->State = CoroutineState::Terminated;
    JumpContext(scheduler->m_pMainThreadContext, nullptr);

    assert(false);
    ::abort();
}

Scheduler* Scheduler::GetCurrent()noexcept
{
    return t_pScheduler;
}

Scheduler::Scheduler(size_t stackSize)
{
    if (t_pScheduler)
        MOE_THROW(InvalidCallException, "Scheduler is already existed");

    m_stSharedStack.Alloc(stackSize);  // 分配共享栈

    t_pScheduler = this;
}

Scheduler::~Scheduler()
{
    assert(IsIdle());

    t_pScheduler = nullptr;
}

void Scheduler::Schedule()noexcept
{
    assert(t_pScheduler == this);

    auto next = m_pReadyHead;
    while (next)
    {
        auto current = next;
        next = current->Next;

        m_pRunningCoroutine = current;

        // 从就绪链表移除以等待调度
        REMOVE_LINK_NODE(m_pReadyHead, m_pReadyTail, current);
        --m_uReadyCount;

        ContextTransfer transfer;
        if (current->State == CoroutineState::Created)  // 首次运行
        {
            // 创建上下文
            current->Context = MakeContext(m_stSharedStack.GetStackTop(), m_stSharedStack.GetStackSize(),
                CoroutineWrapper);

            // 设置为运行状态
            current->State = CoroutineState::Running;

            // 执行协程（首次运行）
            transfer = JumpContext(current->Context, this);
        }
        else
        {
            assert(current->State == CoroutineState::Running);

            // 拷贝协程栈
            assert(current->StackBuffer.size() <= m_stSharedStack.GetStackSize());
            ::memcpy((void*)((size_t)m_stSharedStack.GetStackBase() - current->StackBuffer.size()),
                current->StackBuffer.data(), current->StackBuffer.size());

            // 执行协程
            transfer = JumpContext(current->Context, reinterpret_cast<void*>(static_cast<int>(current->ResumeType)));
        }

        // 更新Context
        current->Context = transfer.State;

        // 检查是否已经终止
        if (current->State == CoroutineState::Terminated)
        {
            // 直接回收
            INSERT_AT_LINK_HEAD(m_pFreeHead, m_pFreeTail, current);
            ++m_uFreeCount;
        }
        else
        {
            // 保存协程栈
            assert(m_stSharedStack.GetStackBase() > current->Context);
            size_t stackSize = (size_t)m_stSharedStack.GetStackBase() - (size_t)current->Context;
            if (current->StackBuffer.size() < stackSize)
            {
                try
                {
                    current->StackBuffer.resize(stackSize);
                }
                catch (...)
                {
                    CollectGarbage();  // 尝试进行一次GC

                    try
                    {
                        current->StackBuffer.resize(stackSize);
                    }
                    catch (...)
                    {
                        MOE_FATAL("Allocate stack memory failed, size={0}", stackSize);
                        ::abort();  // 无法恢复，直接失败退出程序
                    }
                }
            }
            ::memcpy(current->StackBuffer.data(), current->Context, stackSize);
        }

        m_pRunningCoroutine = nullptr;
    }
}

void Scheduler::CollectGarbage()noexcept
{
    while (m_pFreeHead)
    {
        REMOVE_LINK_NODE(m_pFreeHead, m_pFreeTail, m_pFreeHead);
        --m_uFreeCount;
    }
}

void Scheduler::YieldCurrent()
{
    if (!m_pRunningCoroutine)
        MOE_THROW(InvalidCallException, "No coroutine is running");

    // 插回到链表头结点
    INSERT_AT_LINK_HEAD(m_pReadyHead, m_pReadyTail, m_pRunningCoroutine);
    ++m_uReadyCount;

    // 切回到主线程
    auto transfer = JumpContext(m_pMainThreadContext, nullptr);
    m_pMainThreadContext = transfer.State;

    auto resumeType = static_cast<CoroutineResumeType>(reinterpret_cast<ptrdiff_t>(transfer.Data));
    assert(resumeType == CoroutineResumeType::Normally);
}

void Scheduler::SuspendCurrent(WaitHandle& handle)
{
    if (!m_pRunningCoroutine)
        MOE_THROW(InvalidCallException, "No coroutine is running");

    auto current = m_pRunningCoroutine;

    // 检查Scheduler
    if (handle.m_pScheduler == nullptr)
        handle.m_pScheduler = this;
    assert(handle.m_pScheduler == this);

    // 插入到等待链表
    INSERT_AT_LINK_HEAD(m_pPendingHead, m_pPendingTail, current);
    ++m_uPendingCount;

    // 加入到WaitHandle
    current->WaitHandle = &handle;
    current->WaitNext = handle.m_pHead;
    handle.m_pHead = current;

    // 设置状态变动
    current->State = CoroutineState::Suspend;

    // 切回到主线程
    auto transfer = JumpContext(m_pMainThreadContext, nullptr);
    m_pMainThreadContext = transfer.State;

    // 检查是否需要抛出异常
    auto resumeType = static_cast<CoroutineResumeType>(reinterpret_cast<ptrdiff_t>(transfer.Data));
    if (resumeType == CoroutineResumeType::Exception)
    {
        auto exception = current->ExceptionPtr;
        current->ResumeType = CoroutineResumeType::Normally;
        current->ExceptionPtr = std::exception_ptr();

        std::rethrow_exception(exception);  // 重新抛出异常
    }
}

RefPtr<Scheduler::CoroutineController> Scheduler::Alloc()
{
    if (m_pFreeHead)
    {
        auto ret = m_pFreeHead;

        REMOVE_LINK_NODE(m_pFreeHead, m_pFreeTail, m_pFreeHead);
        --m_uFreeCount;

        return ret;
    }

    return MakeRef<CoroutineController>();
}

void Scheduler::Start(std::function<void()> entry)
{
    auto p = Alloc();
    p->Reset(std::move(entry));

    INSERT_AT_LINK_TAIL(m_pReadyHead, m_pReadyTail, p);
    ++m_uReadyCount;
}

//////////////////////////////////////////////////////////////////////////////// WaitHandle

WaitHandle::WaitHandle()
{
}

WaitHandle::~WaitHandle()
{
    if (!Empty())
    {
        try
        {
            MOE_THROW(WaitHandleCancelledException, "Wait handle cancelled");
        }
        catch (...)
        {
            ResumeException(std::current_exception());
        }
    }
}

void WaitHandle::Resume()noexcept
{
    while (m_pHead)
    {
        auto current = m_pHead;
        m_pHead = current->WaitNext;

        REMOVE_LINK_NODE(m_pScheduler->m_pPendingHead, m_pScheduler->m_pPendingTail, current);
        --m_pScheduler->m_uPendingCount;

        INSERT_AT_LINK_HEAD(m_pScheduler->m_pReadyHead, m_pScheduler->m_pReadyTail, current);
        ++m_pScheduler->m_uReadyCount;

        // 重置current的状态
        current->State = Scheduler::CoroutineState::Running;
        current->ResumeType = Scheduler::CoroutineResumeType::Normally;
        current->WaitHandle = nullptr;
        current->WaitNext = nullptr;
    }

    m_pScheduler = nullptr;
}

void WaitHandle::ResumeOne()noexcept
{
    auto current = m_pHead;
    if (current)
    {
        m_pHead = current->WaitNext;

        REMOVE_LINK_NODE(m_pScheduler->m_pPendingHead, m_pScheduler->m_pPendingTail, current);
        --m_pScheduler->m_uPendingCount;

        INSERT_AT_LINK_HEAD(m_pScheduler->m_pReadyHead, m_pScheduler->m_pReadyTail, current);
        ++m_pScheduler->m_uReadyCount;

        // 重置current的状态
        current->State = Scheduler::CoroutineState::Running;
        current->ResumeType = Scheduler::CoroutineResumeType::Normally;
        current->WaitHandle = nullptr;
        current->WaitNext = nullptr;

        if (Empty())
            m_pScheduler = nullptr;
    }
}

void WaitHandle::ResumeException(const std::exception_ptr& ptr)noexcept
{
    while (m_pHead)
    {
        auto current = m_pHead;
        m_pHead = current->WaitNext;

        REMOVE_LINK_NODE(m_pScheduler->m_pPendingHead, m_pScheduler->m_pPendingTail, current);
        --m_pScheduler->m_uPendingCount;

        INSERT_AT_LINK_HEAD(m_pScheduler->m_pReadyHead, m_pScheduler->m_pReadyTail, current);
        ++m_pScheduler->m_uReadyCount;

        // 重置current的状态
        current->State = Scheduler::CoroutineState::Running;
        current->ResumeType = Scheduler::CoroutineResumeType::Exception;
        current->ExceptionPtr = ptr;
        current->WaitHandle = nullptr;
        current->WaitNext = nullptr;
    }

    m_pScheduler = nullptr;
}
