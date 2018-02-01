/**
 * @file
 * @author chu
 * @date 2018/2/1
 */
#include <Moe.UV/CoEvent.hpp>

#include <cassert>
#include <Moe.Core/Exception.hpp>
#include <Moe.Coroutine/Coroutine.hpp>
#include <Moe.Coroutine/Scheduler.hpp>

using namespace std;
using namespace moe;
using namespace UV;
using namespace Coroutine;

namespace
{
    struct FlagGuard
    {
        bool& FlagRef;

        FlagGuard(bool& flag)
            : FlagRef(flag)
        {
            FlagRef = true;
        }

        ~FlagGuard()
        {
            FlagRef = false;
        }
    };
}

CoEvent::CoEvent()
{
}

CoEvent::~CoEvent()
{
    assert(!m_bInWait);
}

CoEvent::WaitResult CoEvent::Wait(Time::Tick timeout)
{
    if (m_bInWait)
        MOE_THROW(InvalidCallException, "Another coroutine is in waitting");

    auto thread = Coroutine::GetCurrentThread();
    if (!thread)
        MOE_THROW(InvalidCallException, "Bad execution context");

    if (!m_pCondVar)
    {
        m_pCondVar = thread->GetScheduler().CreateConditionVariable();
        assert(m_pCondVar);
    }

    FlagGuard guard(m_bInWait);
    m_bCancelled = false;
    m_stException = exception_ptr();
    
    auto ret = Coroutine::Wait(m_pCondVar, timeout);
    if (ret == ThreadWaitResult::Timeout)
        return WaitResult::Timeout;

    if (m_stException)
        std::rethrow_exception(m_stException);
    return m_bCancelled ? WaitResult::Cancelled : WaitResult::Succeed;
}

void CoEvent::Resume()
{
    if (m_pCondVar)
        m_pCondVar->Notify();
}

void CoEvent::Cancel()
{
    if (m_pCondVar)
    {
        m_bCancelled = true;
        m_pCondVar->Notify();
    }
}

void CoEvent::Throw(const std::exception_ptr& exception)
{
    if (m_pCondVar)
    {
        m_stException = exception;
        m_pCondVar->Notify();
    }
}
