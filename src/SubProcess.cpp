/**
 * @file
 * @author chu
 * @date 2017/12/27
 */
#include <Moe.UV/SubProcess.hpp>

using namespace std;
using namespace moe;
using namespace UV;

IoHandleHolder<SubProcess> SubProcess::Spawn(const char* path, const char* args[], const char* env[], const char* cwd,
    unsigned flags, uv_uid_t uid, uv_gid_t gid)
{
    ::uv_process_options_t options;
    ::memset(&options, 0, sizeof(options));
    options.exit_cb = OnUVProcessExit;
    options.file = path;
    options.args = const_cast<char**>(args);
    options.env = const_cast<char**>(env);
    options.cwd = cwd;
    options.flags = flags;
    options.uid = uid;
    options.gid = gid;

    return IoHandleHolder<SubProcess>(ObjectPool::Create<SubProcess>(options));
}

void SubProcess::Kill(int pid, int signum)
{
    MOE_UV_CHECK(::uv_kill(pid, signum));
}

void SubProcess::OnUVProcessExit(uv_process_t* process, int64_t exitStatus, int termSignal)noexcept
{
    auto* self = GetSelf<SubProcess>(process);
    self->OnExit(exitStatus, termSignal);
}

SubProcess::SubProcess(const ::uv_process_options_t& options)
{
    MOE_UV_CHECK(::uv_spawn(GetCurrentUVLoop(), &m_stHandle, &options));
    BindHandle(reinterpret_cast<::uv_handle_t*>(&m_stHandle));
}

void SubProcess::Kill(int signum)
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Handle is closing");
    MOE_UV_CHECK(::uv_process_kill(&m_stHandle, signum));
}

bool SubProcess::CoWait()
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Handle is closing");
    if (!m_bRunning)
        return true;  // 已经退出
    auto ret = Coroutine::Suspend(m_stExitCondVar);
    return static_cast<bool>(ret);
}

void SubProcess::CancelWait()noexcept
{
    // 通知协程取消
    m_stExitCondVar.Resume(static_cast<ptrdiff_t>(false));
}

void SubProcess::OnClose()noexcept
{
    IoHandle::OnClose();

    // 通知协程取消
    m_stExitCondVar.Resume(static_cast<ptrdiff_t>(false));
}

void SubProcess::OnExit(int64_t exitStatus, int termSignal)noexcept
{
    m_bRunning = false;
    m_llExitStatus = exitStatus;
    m_iTermSignal = termSignal;

    m_stExitCondVar.Resume(static_cast<ptrdiff_t>(true));
}
