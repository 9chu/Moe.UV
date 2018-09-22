/**
 * @file
 * @author chu
 * @date 2018/8/14
 */
#include <Moe.UV/Process.hpp>

#include "UV.inl"

using namespace std;
using namespace moe;
using namespace UV;

//////////////////////////////////////////////////////////////////////////////// Process

void Process::Kill(int pid, int signum)
{
    MOE_UV_CHECK(::uv_kill(pid, signum));
}

//////////////////////////////////////////////////////////////////////////////// SubProcess

SubProcess SubProcess::Spawn(const char* path, const char* args[], const char* env[], const char* cwd, unsigned flags,
    uint32_t uid, uint32_t gid)
{
    ::uv_process_options_t options;
    ::memset(&options, 0, sizeof(options));
    options.file = path;
    options.args = const_cast<char**>(args);
    options.env = const_cast<char**>(env);
    options.cwd = cwd;
    options.flags = flags;
    options.uid = uid;
    options.gid = gid;

    MOE_UV_NEW(::uv_process_t);
    MOE_UV_CHECK(::uv_spawn(GetCurrentUVLoop(), object.get(), &options));
    return SubProcess(CastHandle(std::move(object)));
}

void SubProcess::OnUVProcessExit(::uv_process_t* handle, int64_t exitStatus, int termSignal)noexcept
{
    MOE_UV_GET_SELF(SubProcess);

    MOE_UV_CATCH_ALL_BEGIN
        self->OnExit(exitStatus, termSignal);
    MOE_UV_CATCH_ALL_END
}

SubProcess::SubProcess(SubProcess&& org)noexcept
    : AsyncHandle(std::move(org))
{
    m_bRunning = org.m_bRunning;
    m_llExitStatus = org.m_llExitStatus;
    m_iTermSignal = org.m_iTermSignal;
    m_pOnExit = std::move(org.m_pOnExit);

    org.m_bRunning = false;
    org.m_llExitStatus = 0;
    org.m_iTermSignal = 0;
}

SubProcess& SubProcess::operator=(SubProcess&& rhs)noexcept
{
    AsyncHandle::operator=(std::move(rhs));

    m_bRunning = rhs.m_bRunning;
    m_llExitStatus = rhs.m_llExitStatus;
    m_iTermSignal = rhs.m_iTermSignal;
    m_pOnExit = std::move(rhs.m_pOnExit);

    rhs.m_bRunning = false;
    rhs.m_llExitStatus = 0;
    rhs.m_iTermSignal = 0;
    return *this;
}

int SubProcess::GetPid()const
{
    MOE_UV_GET_HANDLE(::uv_process_t);
    return ::uv_process_get_pid(handle);
}

void SubProcess::Kill(int signum)
{
    MOE_UV_GET_HANDLE(::uv_process_t);
    MOE_UV_CHECK(::uv_process_kill(handle, signum));
}

void SubProcess::OnExit(int64_t exitStatus, int termSignal)
{
    if (m_pOnExit)
        m_pOnExit(exitStatus, termSignal);
}
