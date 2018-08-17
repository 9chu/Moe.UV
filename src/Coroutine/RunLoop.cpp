/**
 * @file
 * @author chu
 * @date 2018/8/17
 */
#include <Moe.UV/Coroutine/RunLoop.hpp>

using namespace std;
using namespace moe;
using namespace UV;

CoRunLoop::CoRunLoop(RunLoop& loop, size_t stackSize)
    : m_stLoop(loop), m_stScheduler(loop.GetObjectPool(), loop.Now(), stackSize)
{
    // 构造基本的事件句柄，用于协程触发
    m_pOnPrepareEvent = EventHandle::Create(EventType::Prepare);
    m_pOnCheckEvent = EventHandle::Create(EventType::Check);
    m_pTimer = Timer::Create();

    m_pOnPrepareEvent->Unref();
    m_pOnCheckEvent->Unref();
    m_pTimer->Unref();

    m_pOnPrepareEvent->SetOnEventCallback(bind(&CoRunLoop::OnPrepare, this));
    m_pOnCheckEvent->SetOnEventCallback(bind(&CoRunLoop::OnCheck, this));
}

void CoRunLoop::OnPrepare()
{
    auto timeout = m_stScheduler.Update(m_stLoop.Now());
    if (timeout == Coroutine::kInfinityTimeout)
        m_pTimer->Stop();
    else
    {
        m_pTimer->SetFirstTime(timeout);
        m_pTimer->Start();
    }
}

void CoRunLoop::OnCheck()
{
    m_stScheduler.Update(m_stLoop.Now());
}
