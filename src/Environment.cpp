/**
 * @file
 * @author chu
 * @date 2017/12/1
 */
#include <Moe.UV/Environment.hpp>

using namespace std;
using namespace moe;
using namespace UV;

std::string Environment::InterfaceInfo::GetPhysicsAddressString()
{
    return StringUtils::Format("{0,2[0]:H}-{1,2[0]:H}-{2,2[0]:H}-{3,2[0]:H}-{4,2[0]:H}-{5,2[0]:H}",
        PhysicsAddress[0], PhysicsAddress[1], PhysicsAddress[2], PhysicsAddress[3], PhysicsAddress[4],
        PhysicsAddress[5]);
}

size_t Environment::EnumCpuInfo(std::vector<CpuInfo>& info)
{
    info.clear();

    ::uv_cpu_info_t* list = nullptr;
    int count = 0;
    MOE_UV_CHECK(::uv_cpu_info(&list, &count));

    for (int i = 0; i < count; ++i)
    {
        ::uv_cpu_info_t& t = list[i];

        try
        {
            CpuInfo tmp;
            tmp.Model = t.model;
            tmp.Speed = static_cast<uint32_t>(t.speed);
            tmp.CpuTimes.User = t.cpu_times.user;
            tmp.CpuTimes.Nice = t.cpu_times.nice;
            tmp.CpuTimes.Sys = t.cpu_times.sys;
            tmp.CpuTimes.Idle = t.cpu_times.idle;
            tmp.CpuTimes.Irq = t.cpu_times.irq;
            info.push_back(tmp);
        }
        catch (...)
        {
            ::uv_free_cpu_info(list, count);
            throw;
        }
    }

    ::uv_free_cpu_info(list, count);

    return info.size();
}

size_t Environment::EnumInterface(std::vector<InterfaceInfo>& info)
{
    static_assert(sizeof(InterfaceInfo::PhysicsAddress) == sizeof(uv_interface_address_t::phys_addr), "Error");

    info.clear();

    ::uv_interface_address_t* list = nullptr;
    int count = 0;
    MOE_UV_CHECK(::uv_interface_addresses(&list, &count));

    for (int i = 0; i < count; ++i)
    {
        ::uv_interface_address_t& t = list[i];

        // 过滤不识别的地址簇类型
        auto address = reinterpret_cast<sockaddr_storage*>(reinterpret_cast<uint8_t*>(&t.address));
        auto netmask = reinterpret_cast<sockaddr_storage*>(reinterpret_cast<uint8_t*>(&t.netmask));
        if ((address->ss_family != AF_INET6 && address->ss_family != AF_INET) ||
            (netmask->ss_family != AF_INET6 && netmask->ss_family != AF_INET))
        {
            continue;
        }

        try
        {
            InterfaceInfo tmp;
            tmp.Name = t.name;
            ::memcpy(tmp.PhysicsAddress, t.phys_addr, sizeof(tmp.PhysicsAddress));
            tmp.IsInternal = (t.is_internal != 0);
            tmp.Address = EndPoint(*address);
            tmp.NetMask = EndPoint(*netmask);

            info.push_back(tmp);
        }
        catch (...)
        {
            ::uv_free_interface_addresses(list, count);
            throw;
        }
    }

    ::uv_free_interface_addresses(list, count);

    return info.size();
}

void Environment::GetLoadAverage(double& avg1, double& avg5, double& avg15)noexcept
{
    double ret[3] = { 0., 0., 0. };
    ::uv_loadavg(ret);

    avg1 = ret[0];
    avg5 = ret[1];
    avg15 = ret[2];
}

uint64_t Environment::GetTotalMemory()noexcept
{
    return ::uv_get_total_memory();
}

std::string Environment::GetHostName()
{
    string result;
    result.resize(256);
    size_t size = result.length();

    int ret = ::uv_os_gethostname(&result[0], &size);
    if (ret == UV_ENOBUFS)
    {
        result.resize(size);
        MOE_UV_CHECK(::uv_os_gethostname(&result[0], &size));
    }

    result.resize(std::strlen(result.c_str()));
    return result;
}

size_t Environment::GetResidentSetMemorySize()
{
    size_t rss = 0;
    MOE_UV_CHECK(::uv_resident_set_memory(&rss));
    return rss;
}

Environment::ProcessResourceUsage Environment::GetResourceUsage()
{
    ProcessResourceUsage ret;
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

uint64_t Environment::GetPorcessId()noexcept
{
    return static_cast<uint64_t>(::uv_os_getpid());
}

uint64_t Environment::GetProcessParentId()noexcept
{
    return static_cast<uint64_t>(::uv_os_getppid());
}

std::string Environment::GetProcessExePath()
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

std::string Environment::GetWorkingDirectory()
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

void Environment::SetWorkingDirectory(const char* path)
{
    MOE_UV_CHECK(::uv_chdir(path));
}

std::string Environment::GetHomeDirectory()
{
    string result;
    result.resize(256);
    size_t size = result.length();

    int ret = ::uv_os_homedir(&result[0], &size);
    if (ret == UV_ENOBUFS)
    {
        result.resize(size);
        MOE_UV_CHECK(::uv_os_homedir(&result[0], &size));
    }

    result.resize(std::strlen(result.c_str()));
    return result;
}

std::string Environment::GetTempDirectory()
{
    string result;
    result.resize(256);
    size_t size = result.length();

    int ret = ::uv_os_tmpdir(&result[0], &size);
    if (ret == UV_ENOBUFS)
    {
        result.resize(size);
        MOE_UV_CHECK(::uv_os_tmpdir(&result[0], &size));
    }

    result.resize(std::strlen(result.c_str()));
    return result;
}

std::string Environment::GetEnvironment(const char* name)
{
    string result;
    result.resize(64);
    size_t size = result.length();

    int ret = ::uv_os_getenv(name, &result[0], &size);
    if (ret == UV_ENOBUFS)
    {
        result.resize(size);
        MOE_UV_CHECK(::uv_os_getenv(name, &result[0], &size));
    }
    else if (ret == UV_ENOENT)
        MOE_THROW(ObjectNotFoundException, "Environment key \"{0}\" not exists", name);

    result.resize(std::strlen(result.c_str()));
    return result;
}

void Environment::SetEnvironment(const char* name, const char* value)
{
    MOE_UV_CHECK(::uv_os_setenv(name, value));
}

void Environment::DeleteEnvironment(const char* name)
{
    auto ret = ::uv_os_unsetenv(name);
    if (ret == UV_ENOENT)
        return;
    else if (ret != 0)
        MOE_UV_THROW(ret);
}
