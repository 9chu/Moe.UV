/**
 * @file
 * @author chu
 * @date 2017/12/1
 */
#include <Moe.UV/ProcessInfo.hpp>
#include <uv.h>

using namespace std;
using namespace moe;
using namespace UV;

size_t ProcessInfo::GetResidentSetMemorySize()
{
    size_t rss = 0;
    MOE_UV_CHECK(::uv_resident_set_memory(&rss));
    return rss;
}

ProcessInfo::ResourceUsage ProcessInfo::GetResourceUsage()
{
    ResourceUsage ret;
    ::memset(&ret, 0, sizeof(ret));

    uv_rusage_t usage;
    MOE_UV_CHECK(::uv_getrusage(&usage));

    ret.UserCpuTimeUsed = static_cast<Time::Tick>(usage.ru_utime.tv_sec * 1000 + usage.ru_utime.tv_usec / 1000);
    ret.SystemCpuTimeUsed = static_cast<Time::Tick>(usage.ru_stime.tv_sec * 1000 + usage.ru_stime.tv_usec / 1000);
    ret.MaxResidentSetSize = usage.ru_maxrss;
    ret.IntegralSharedMemorySize = usage.ru_ixrss;
    ret.IntegralUnsharedDataSize = usage.ru_idrss;
    ret.IntegralUnsharedStackSize = usage.ru_isrss;
    ret.SoftPageFaults = usage.ru_minflt;
    ret.HardPageFaults = usage.ru_majflt;
    ret.Swaps = usage.ru_nswap;
    ret.BlockInputOperations = usage.ru_inblock;
    ret.BlockOutputOperations = usage.ru_oublock;
    ret.IpcMessageSent = usage.ru_msgsnd;
    ret.IpcMessageReceived = usage.ru_msgrcv;
    ret.SignalReceived = usage.ru_nsignals;
    ret.VoluntaryContextSwitches = usage.ru_nvcsw;
    ret.InvoluntaryContextSwitches = usage.ru_nivcsw;
    return ret;
}

std::string ProcessInfo::GetExePath()
{
    string result;
    result.resize(256);
    size_t size = result.length();

    int ret = ::uv_exepath(&result[0], &size);
    if (ret == UV_ENOBUFS)
    {
        result.resize(size);
        MOE_UV_CHECK(::uv_exepath(&result[0], &size));
    }

    result.resize(std::strlen(result.c_str()));
    return result;
}

std::string ProcessInfo::GetWorkingDirectory()
{
    string result;
    result.resize(256);
    size_t size = result.length();

    int ret = ::uv_cwd(&result[0], &size);
    if (ret == UV_ENOBUFS)
    {
        result.resize(size);
        MOE_UV_CHECK(::uv_cwd(&result[0], &size));
    }

    result.resize(std::strlen(result.c_str()));
    return result;
}

void ProcessInfo::SetWorkingDirectory(const char* path)
{
    MOE_UV_CHECK(::uv_chdir(path));
}
