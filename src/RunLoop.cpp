/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#include <Moe.UV/RunLoop.hpp>
#include <Moe.Core/Exception.hpp>

using namespace std;
using namespace moe;
using namespace UV;

thread_local static RunLoop* t_pRunLoop = nullptr;

RunLoop* RunLoop::GetCurrent()noexcept
{
    return t_pRunLoop;
}

Time::Tick RunLoop::Now()noexcept
{
    auto runloop = GetCurrent();
    if (!runloop)
        return static_cast<Time::Tick>(Time::GetHiResTickCount());
    return ::uv_now(&runloop->m_stLoop);
}

RunLoop::RunLoop()
{
    if (t_pRunLoop)
        MOE_THROW(InvalidCallException, "RunLoop is already existed");

    int err = ::uv_loop_init(&m_stLoop);
    if (err != 0)
        MOE_THROW(APIException, "uv_loop_init error, err={0}", err);

    t_pRunLoop = this;
}

RunLoop::~RunLoop()
{
    t_pRunLoop = nullptr;

    int err = ::uv_loop_close(&m_stLoop);
    MOE_UNUSED(err);
    assert(err == 0);
}

void RunLoop::Run()noexcept
{
    ::uv_run(&m_stLoop, UV_RUN_DEFAULT);
}

void RunLoop::Stop()noexcept
{
    ::uv_stop(&m_stLoop);
}

void RunLoop::UpdateTime()noexcept
{
    ::uv_update_time(&m_stLoop);
}
