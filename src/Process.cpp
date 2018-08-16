/**
 * @file
 * @author chu
 * @date 2018/8/14
 */
#include <Moe.UV/Process.hpp>

#include <Moe.Core/Logging.hpp>
#include <Moe.UV/RunLoop.hpp>

using namespace std;
using namespace moe;
using namespace UV;

//////////////////////////////////////////////////////////////////////////////// Process

void Process::Kill(int pid, int signum)
{
    MOE_UV_CHECK(::uv_kill(pid, signum));
}

//////////////////////////////////////////////////////////////////////////////// SubProcessBase

void SubProcessBase::OnUVProcessExit(::uv_process_t* process, int64_t exitStatus, int termSignal)noexcept
{
    auto* self = GetSelf<SubProcessBase>(process);

    MOE_UV_CATCH_ALL_BEGIN
        self->OnExit(exitStatus, termSignal);
    MOE_UV_CATCH_ALL_END
}

SubProcessBase::SubProcessBase(const ::uv_process_options_t& options)
{
    auto opt = options;
    opt.exit_cb = OnUVProcessExit;

    MOE_UV_CHECK(::uv_spawn(GetCurrentUVLoop(), &m_stHandle, &opt));
    BindHandle(reinterpret_cast<::uv_handle_t*>(&m_stHandle));
}

int SubProcessBase::GetPid()const noexcept
{
    return ::uv_process_get_pid(&m_stHandle);
}

void SubProcessBase::Kill(int signum)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Handle is closing");
    MOE_UV_CHECK(::uv_process_kill(&m_stHandle, signum));
}

//////////////////////////////////////////////////////////////////////////////// SubProcess

UniqueAsyncHandlePtr<SubProcess> SubProcess::Spawn(const char* path, const char* args[], const char* env[],
    const char* cwd, unsigned flags, uv_uid_t uid, uv_gid_t gid)
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

    MOE_UV_NEW(SubProcess, options);
    return object;
}

void SubProcess::OnExit(int64_t exitStatus, int termSignal)
{
    if (m_pOnExit)
        m_pOnExit(exitStatus, termSignal);
}
