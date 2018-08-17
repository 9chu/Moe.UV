/**
 * @file
 * @author chu
 * @date 2018/8/17
 */
#pragma once
#include <Moe.UV/RunLoop.hpp>
#include <Moe.UV/EventHandle.hpp>
#include <Moe.UV/Timer.hpp>
#include <Moe.Coroutine/Scheduler.hpp>

namespace moe
{
namespace UV
{
    /**
     * @brief 协程RunLoop
     */
    class CoRunLoop :
        public NonCopyable
    {
    public:
        CoRunLoop(RunLoop& loop, size_t stackSize=1024*1024);
        CoRunLoop(CoRunLoop&&) = delete;
        CoRunLoop& operator=(CoRunLoop&&) = delete;

    protected:
        void OnPrepare();
        void OnCheck();

    private:
        RunLoop& m_stLoop;
        Coroutine::Scheduler m_stScheduler;

        UniqueAsyncHandlePtr<EventHandle> m_pOnPrepareEvent;
        UniqueAsyncHandlePtr<EventHandle> m_pOnCheckEvent;
        UniqueAsyncHandlePtr<Timer> m_pTimer;  // 仅用作激活RunLoop
    };
}
}
