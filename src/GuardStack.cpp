/**
 * @file
 * @author chu
 * @date 2017/11/28
 */
#include <Moe.UV/GuardStack.hpp>

#include <algorithm>
#include <cmath>

#include <Moe.Core/Exception.hpp>

using namespace std;
using namespace moe;
using namespace UV;

#ifdef MOE_WINDOWS
#include <Windows.h>

size_t GuardStack::GetPageSize()noexcept
{
    ::SYSTEM_INFO info;
    ::memset(&info, 0, sizeof(info));
    ::GetSystemInfo(&info);
    return info.dwPageSize;
}

size_t GuardStack::GetMinStackSize()noexcept
{
#if defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64) || defined(_M_X64) || \
    defined(_M_AMD64)
    return 8 * 1024;
#else
    return 4 * 1024;
#endif
}

size_t GuardStack::GetMaxStackSize()noexcept
{
    return 1 * 1024 * 1024 * 1024;  // 1GB
}

size_t GuardStack::GetDefaultStackSize()noexcept
{
    return 1 * 1024 * 1024;  // 1MB
}

#elif (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>

size_t GuardStack::GetPageSize()noexcept
{
    return static_cast<size_t>(::sysconf(_SC_PAGESIZE));
}

size_t GuardStack::GetMinStackSize()noexcept
{
    size_t maxSize = GetMaxStackSize();
#ifndef SIGSTKSZ
    size_t minSize = 8 * 1024;
#else
    size_t minSize = SIGSTKSZ;
#endif
    return std::min(minSize, maxSize);
}

size_t GuardStack::GetMaxStackSize()noexcept
{
    rlimit limit;
    ::getrlimit(RLIMIT_STACK, &limit);
    if (RLIM_INFINITY == limit.rlim_max)
        return 1 * 1024 * 1024 * 1024;  // 1GB
    return static_cast<size_t>(limit.rlim_max);
}

size_t GuardStack::GetDefaultStackSize()noexcept
{
    size_t sugguestSize = 1 * 1024 * 1024;  // 1MB
    sugguestSize = std::max(sugguestSize, GetMinStackSize());
    return std::min(sugguestSize, GetMaxStackSize());
}

#else
#error "Bad platform"
#endif

GuardStack::GuardStack()
{
}

GuardStack::~GuardStack()
{
    Free();
}

GuardStack::GuardStack(GuardStack&& rhs)noexcept
{
    std::swap(m_pBuffer, rhs.m_pBuffer);
    std::swap(m_uProtectedSize, rhs.m_uProtectedSize);
    std::swap(m_uStackSize, rhs.m_uStackSize);
}

GuardStack& GuardStack::operator=(GuardStack&& rhs)noexcept
{
    if (m_pBuffer)
        Free();

    std::swap(m_pBuffer, rhs.m_pBuffer);
    std::swap(m_uProtectedSize, rhs.m_uProtectedSize);
    std::swap(m_uStackSize, rhs.m_uStackSize);
    return *this;
}

void GuardStack::Alloc(size_t stackSize)
{
    static const auto kPageSize = GetPageSize();

    assert(!m_pBuffer);
    if (m_pBuffer)
        MOE_THROW(InvalidCallException, "Stack is already allocated");

    if (stackSize == 0)
        stackSize = GetDefaultStackSize();
    stackSize = std::max(stackSize, GetMinStackSize());
    stackSize = std::min(stackSize, GetMaxStackSize());

    auto pages = static_cast<size_t>(std::ceil(stackSize / (double)kPageSize));
    stackSize = pages * kPageSize;  // 规格化到页大小
    auto fullSize = stackSize + kPageSize;

#ifdef MOE_WINDOWS
    auto memory = ::VirtualAlloc(0, fullSize, MEM_COMMIT, PAGE_READWRITE);
    if (!memory)
        MOE_THROW(APIException, "Allocating stack memory failed, size={0}, err={1}", fullSize, GetLastError());

    // FIXME: 为了支持栈扩展可以设置PAGE_READWRITE | PAGE_GUARD并捕获SEH异常
    DWORD old = 0;
    if (FALSE == ::VirtualProtect(memory, kPageSize, PAGE_NOACCESS, &old))
    {
        ::VirtualFree(memory, 0, MEM_RELEASE);
        MOE_THROW(APIException, "Creating guard memory failed, err={0}", GetLastError());
    }
#else

#ifdef MAP_ANON
    auto memory = ::mmap(0, fullSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
#else
    auto memory = ::mmap(0, size_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

    if (memory == MAP_FAILED)
        MOE_THROW(APIException, "Allocating stack memory failed, size={0}, err={1}", fullSize, errno);
    if (0 != ::mprotect(memory, kPageSize, PROT_NONE))
    {
        auto err = errno;
        ::munmap(memory, fullSize);
        MOE_THROW(APIException, "Creating guard memory failed, err={0}", err);
    }
#endif

    m_pBuffer = memory;
    m_uProtectedSize = kPageSize;
    m_uStackSize = stackSize;
}

void GuardStack::Free()noexcept
{
    if (!m_pBuffer)
        return;

#ifdef MOE_WINDOWS
    ::VirtualFree(m_pBuffer, 0, MEM_RELEASE);
#else
    ::munmap(m_pBuffer, GetAllocSize());
#endif

    m_pBuffer = nullptr;
    m_uProtectedSize = 0;
    m_uStackSize = 0;
}
