/**
 * @file
 * @author chu
 * @date 2017/12/13
 */
#include <Moe.UV/Filesystem.hpp>
#include <Moe.UV/RunLoop.hpp>
#include <Moe.Core/Utils.hpp>

#ifdef MOE_WINDOWS
#include <io.h>
#ifndef O_RDONLY
#define O_RDONLY _O_RDONLY
#endif
#ifndef O_WRONLY
#define O_WRONLY _O_WRONLY
#endif
#ifndef O_RDWR
#define O_RDWR _O_RDWR
#endif
#ifndef O_CREAT
#define O_CREAT _O_CREAT
#endif
#ifndef O_TRUNC
#define O_TRUNC _O_TRUNC
#endif
#ifndef O_EXCL
#define O_EXCL _O_EXCL
#endif
#else
#include <unistd.h>
#endif

using namespace std;
using namespace moe;
using namespace UV;

namespace
{
    struct UVFileSysReq :
        public ObjectPool::RefBase<UVFileSysReq>
    {
        ::uv_fs_t Request;
        CoCondVar CondVar;

        UVFileSysReq()
        {
            ::memset(&Request, 0, sizeof(Request));
        }

        ~UVFileSysReq()
        {
            ::uv_fs_req_cleanup(&Request);
        }

        static void Callback(uv_fs_t* req)noexcept
        {
            RefPtr<UVFileSysReq> self(static_cast<UVFileSysReq*>(req->data));

            // 恢复协程
            self->CondVar.Resume();
        }
    };

    inline FileStatus ToFileStatus(const ::uv_stat_t& stat)
    {
        FileStatus ret;
        ::memset(&ret, 0, sizeof(ret));

        ret.DeviceId = stat.st_dev;
        ret.Mode = stat.st_mode;
        ret.HardLinks = stat.st_nlink;
        ret.Uid = stat.st_uid;
        ret.Gid = stat.st_gid;
        ret.RealDeviceId = stat.st_rdev;
        ret.InodeNumber = stat.st_ino;
        ret.Size = stat.st_size;
        ret.BlockSize = stat.st_blksize;
        ret.Blocks = stat.st_blocks;
        ret.Flags = stat.st_flags;
        ret.FileGeneration = stat.st_gen;
        ret.LastAccessTime = static_cast<Time::Tick>(stat.st_atim.tv_sec * 1000ull + stat.st_atim.tv_nsec / 1000000);
        ret.LastModificationTime = static_cast<Time::Tick>(stat.st_mtim.tv_sec * 1000ull +
            stat.st_mtim.tv_nsec / 1000000);
        ret.LastStatusChangeTime = static_cast<Time::Tick>(stat.st_ctim.tv_sec * 1000ull +
            stat.st_ctim.tv_nsec / 1000000);
        ret.CreationTime = static_cast<Time::Tick>(stat.st_birthtim.tv_sec * 1000ull +
            stat.st_birthtim.tv_nsec / 1000000);
        return ret;
    }
}

//////////////////////////////////////////////////////////////////////////////// File

File::File(::uv_file fd)noexcept
    : m_iFd(fd)
{
}

File::File(File&& rhs)noexcept
    : m_iFd(rhs.m_iFd)
{
    rhs.m_iFd = 0;
}

File::~File()
{
    Close();
}

File::operator bool()const noexcept
{
    return m_iFd != 0;
}

File& File::operator=(File&& rhs)noexcept
{
    Close();
    m_iFd = 0;  // 无视关闭失败

    std::swap(m_iFd, rhs.m_iFd);
    return *this;
}

size_t File::CoRead(MutableBytesView buffer, uint64_t offset)
{
    assert(offset <= std::numeric_limits<int64_t>::max());
    if (m_iFd == 0)
        MOE_THROW(InvalidCallException, "No file opened");
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");
    if (buffer.GetSize() == 0)
        return 0;

    auto req = ObjectPool::Create<UVFileSysReq>();
    auto buf = ::uv_buf_init(reinterpret_cast<char*>(buffer.GetBuffer()), static_cast<unsigned>(buffer.GetSize()));
    MOE_UV_CHECK(::uv_fs_read(RunLoop::GetCurrentUVLoop(), &req->Request, m_iFd, &buf, 1, static_cast<int64_t>(offset),
        UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
    return static_cast<size_t>(req->Request.result);
}

size_t File::CoWrite(BytesView buffer, uint64_t offset)
{
    assert(offset <= std::numeric_limits<int64_t>::max());
    if (m_iFd == 0)
        MOE_THROW(InvalidCallException, "No file opened");
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");
    if (buffer.GetSize() == 0)
        return 0;

    auto req = ObjectPool::Create<UVFileSysReq>();
    auto buf = ::uv_buf_init(const_cast<char*>(reinterpret_cast<const char*>(buffer.GetBuffer())),
        static_cast<unsigned>(buffer.GetSize()));
    MOE_UV_CHECK(::uv_fs_write(RunLoop::GetCurrentUVLoop(), &req->Request, m_iFd, &buf, 1, static_cast<int64_t>(offset),
        UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
    return static_cast<size_t>(req->Request.result);
}

void File::CoTruncate(uint64_t length)
{
    assert(length <= std::numeric_limits<int64_t>::max());
    if (m_iFd == 0)
        MOE_THROW(InvalidCallException, "No file opened");
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_ftruncate(RunLoop::GetCurrentUVLoop(), &req->Request, m_iFd, static_cast<int64_t>(length),
        UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
}

void File::CoFlush()
{
    if (m_iFd == 0)
        MOE_THROW(InvalidCallException, "No file opened");
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_fsync(RunLoop::GetCurrentUVLoop(), &req->Request, m_iFd, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
}

void File::CoFlushData()
{
    if (m_iFd == 0)
        MOE_THROW(InvalidCallException, "No file opened");
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_fdatasync(RunLoop::GetCurrentUVLoop(), &req->Request, m_iFd, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
}

FileStatus File::CoGetStatus()
{
    if (m_iFd == 0)
        MOE_THROW(InvalidCallException, "No file opened");
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_fstat(RunLoop::GetCurrentUVLoop(), &req->Request, m_iFd, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
    return ToFileStatus(req->Request.statbuf);
}

void File::CoChangeMode(int mode)
{
    if (m_iFd == 0)
        MOE_THROW(InvalidCallException, "No file opened");
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_fchmod(RunLoop::GetCurrentUVLoop(), &req->Request, m_iFd, mode, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
}

void File::CoChangeOwner(uv_uid_t uid, uv_gid_t gid)
{
    if (m_iFd == 0)
        MOE_THROW(InvalidCallException, "No file opened");
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_fchown(RunLoop::GetCurrentUVLoop(), &req->Request, m_iFd, uid, gid, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
}

void File::CoSetFileTime(Time::Timestamp accessTime, Time::Timestamp modificationTime)
{
    if (m_iFd == 0)
        MOE_THROW(InvalidCallException, "No file opened");
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_futime(RunLoop::GetCurrentUVLoop(), &req->Request, m_iFd, accessTime / 1000.,
        modificationTime / 1000., UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
}

bool File::Close()noexcept
{
    if (m_iFd == 0)
        return true;

    // 协程上下文则使用协程等待
    if (Coroutine::InCoroutineContext())
    {
        MOE_UV_EAT_EXCEPT_BEGIN
            auto req = ObjectPool::Create<UVFileSysReq>();
            MOE_UV_CHECK(::uv_fs_close(RunLoop::GetCurrentUVLoop(), &req->Request, m_iFd, UVFileSysReq::Callback));

            // 手动增加一个引用计数
            req->Request.data = RefPtr<UVFileSysReq>(req).Release();

            // 发起协程等待
            // 此时协程栈和Request上各自持有一个引用计数
            Coroutine::Suspend(req->CondVar);

            // 获取结果
            MOE_UV_CHECK(static_cast<int>(req->Request.result));

            m_iFd = 0;
            return true;
        MOE_UV_EAT_EXCEPT_END
        return false;
    }

    // 否则，阻塞等待
    ::uv_fs_t request;
    ::memset(&request, 0, sizeof(request));
    MOE_UV_EAT_EXCEPT_BEGIN
        MOE_UV_CHECK(::uv_fs_close(RunLoop::GetCurrentUVLoop(), &request, m_iFd, nullptr));
        ::uv_fs_req_cleanup(&request);
        m_iFd = 0;
        return true;
    MOE_UV_EAT_EXCEPT_END
    ::uv_fs_req_cleanup(&request);
    return false;
}

//////////////////////////////////////////////////////////////////////////////// DirectoryEnumerator

Filesystem::DirectoryEnumerator::DirectoryEnumerator(DirectoryEnumerator&& rhs)noexcept
    : m_pUVRequest(rhs.m_pUVRequest), m_bIsEof(rhs.m_bIsEof)
{
    rhs.m_pUVRequest = nullptr;
    rhs.m_bIsEof = false;
}

Filesystem::DirectoryEnumerator::~DirectoryEnumerator()
{
    RefPtr<UVFileSysReq> p(static_cast<UVFileSysReq*>(m_pUVRequest));
}

Filesystem::DirectoryEnumerator& Filesystem::DirectoryEnumerator::operator=(DirectoryEnumerator&& rhs)noexcept
{
    RefPtr<UVFileSysReq> p(static_cast<UVFileSysReq*>(m_pUVRequest));

    m_pUVRequest = nullptr;
    m_bIsEof = false;
    std::swap(m_pUVRequest, rhs.m_pUVRequest);
    std::swap(m_bIsEof, rhs.m_bIsEof);
    return *this;
}

bool Filesystem::DirectoryEnumerator::Next(DirectoryEntry& entry)
{
    if (!m_pUVRequest)
        MOE_THROW(InvalidCallException, "Null object");
    if (m_bIsEof)
        return false;

    UVFileSysReq* req = static_cast<UVFileSysReq*>(m_pUVRequest);

    uv_dirent_t out;
    auto ret = ::uv_fs_scandir_next(&req->Request, &out);
    if (ret == UV_EOF)
    {
        m_bIsEof = true;
        return false;
    }
    else if (ret != 0)
        MOE_UV_THROW(ret);

    entry.Name = out.name;
    switch (out.type)
    {
        case UV_DIRENT_FILE:
            entry.Type = DirectoryEntryType::File;
            break;
        case UV_DIRENT_DIR:
            entry.Type = DirectoryEntryType::Directory;
            break;
        case UV_DIRENT_LINK:
            entry.Type = DirectoryEntryType::Link;
            break;
        case UV_DIRENT_FIFO:
            entry.Type = DirectoryEntryType::Fifo;
            break;
        case UV_DIRENT_SOCKET:
            entry.Type = DirectoryEntryType::Socket;
            break;
        case UV_DIRENT_CHAR:
            entry.Type = DirectoryEntryType::Char;
            break;
        case UV_DIRENT_BLOCK:
            entry.Type = DirectoryEntryType::Block;
            break;
        case UV_DIRENT_UNKNOWN:
        default:
            entry.Type = DirectoryEntryType::Unknown;
            break;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////////// Filesystem

bool Filesystem::CoExists(const char* path)
{
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_access(RunLoop::GetCurrentUVLoop(), &req->Request, path, F_OK, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    auto ret = static_cast<int>(req->Request.result);
    if (ret >= 0)
    {
        assert(ret == 0);
        return true;
    }
    else if (ret != UV_ENOENT)
        MOE_UV_THROW(ret);
    return false;
}

bool Filesystem::CoAccess(const char* path, bool readAccess, bool writeAccess, bool execAccess)
{
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto flags = 0;
    flags |= (readAccess ? R_OK : 0);
    flags |= (writeAccess ? W_OK : 0);
    flags |= (execAccess ? X_OK : 0);

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_access(RunLoop::GetCurrentUVLoop(), &req->Request, path, flags, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    auto ret = static_cast<int>(req->Request.result);
    if (ret >= 0)
    {
        assert(ret == 0);
        return true;
    }
    else if (ret != UV_EACCES)
        MOE_UV_THROW(ret);
    return false;
}

void Filesystem::CoChangeMode(const char* path, int mode)
{
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_chmod(RunLoop::GetCurrentUVLoop(), &req->Request, path, mode, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
}

void Filesystem::CoChangeOwner(const char* path, uv_uid_t uid, uv_gid_t gid)
{
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_chown(RunLoop::GetCurrentUVLoop(), &req->Request, path, uid, gid, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
}

void Filesystem::CoCreateDirectory(const char* path, int mode)
{
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_mkdir(RunLoop::GetCurrentUVLoop(), &req->Request, path, mode, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
}

void Filesystem::CoRemoveDirectory(const char* path)
{
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_rmdir(RunLoop::GetCurrentUVLoop(), &req->Request, path, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
}

std::string Filesystem::CoMakeTempDirectory(const char* tpl)
{
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_mkdtemp(RunLoop::GetCurrentUVLoop(), &req->Request, tpl, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
    return req->Request.path;
}

FileStatus Filesystem::CoGetStatus(const char* path)
{
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_stat(RunLoop::GetCurrentUVLoop(), &req->Request, path, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
    return ToFileStatus(req->Request.statbuf);
}

FileStatus Filesystem::CoGetSymbolicLinkStatus(const char* path)
{
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_lstat(RunLoop::GetCurrentUVLoop(), &req->Request, path, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
    return ToFileStatus(req->Request.statbuf);
}

void Filesystem::CoUnlink(const char* path)
{
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_unlink(RunLoop::GetCurrentUVLoop(), &req->Request, path, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
}

void Filesystem::CoLink(const char* path, const char* newPath)
{
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_link(RunLoop::GetCurrentUVLoop(), &req->Request, path, newPath, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
}

void Filesystem::CoSymbolicLink(const char* path, const char* newPath, int flags)
{
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_symlink(RunLoop::GetCurrentUVLoop(), &req->Request, path, newPath, flags,
        UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
}

std::string Filesystem::CoReadLink(const char* path)
{
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_readlink(RunLoop::GetCurrentUVLoop(), &req->Request, path, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
    return string(reinterpret_cast<const char*>(req->Request.ptr));
}

void Filesystem::CoSetFileTime(const char* path, Time::Timestamp accessTime, Time::Timestamp modificationTime)
{
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_utime(RunLoop::GetCurrentUVLoop(), &req->Request, path, accessTime / 1000.,
        modificationTime / 1000., UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
}

void Filesystem::CoRename(const char* path, const char* newPath)
{
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_rename(RunLoop::GetCurrentUVLoop(), &req->Request, path, newPath, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));
}

Filesystem::DirectoryEnumerator Filesystem::CoScanDirectory(const char* path, int flags)
{
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_scandir(RunLoop::GetCurrentUVLoop(), &req->Request, path, flags, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));

    // 返回结果
    DirectoryEnumerator ret;
    ret.m_pUVRequest = req.Release();
    return ret;
}

File Filesystem::CoOpen(const char* path, int flags, int mode)
{
    if (!Coroutine::InCoroutineContext())
        MOE_THROW(InvalidCallException, "Bad execution context");

    auto req = ObjectPool::Create<UVFileSysReq>();
    MOE_UV_CHECK(::uv_fs_open(RunLoop::GetCurrentUVLoop(), &req->Request, path, flags, mode, UVFileSysReq::Callback));

    // 手动增加一个引用计数
    req->Request.data = RefPtr<UVFileSysReq>(req).Release();

    // 发起协程等待
    // 此时协程栈和Request上各自持有一个引用计数
    Coroutine::Suspend(req->CondVar);

    // 获取结果
    MOE_UV_CHECK(static_cast<int>(req->Request.result));

    // 返回结果
    assert(req->Request.result != 0);
    return File(static_cast<::uv_file>(req->Request.result));
}

File Filesystem::CoOpen(const char* path, FileOpenMode openMode, FileAccessType access)
{
    int flags = 0;
    switch (openMode)
    {
        case FileOpenMode::Create:
            flags |= O_CREAT;
            break;
        case FileOpenMode::CreateNew:
            flags |= (O_CREAT | O_EXCL);
            break;
        case FileOpenMode::Truncate:
            flags |= O_TRUNC;
            break;
        case FileOpenMode::Default:
        default:
            break;
    }

    switch (access)
    {
        case FileAccessType::WriteOnly:
            flags |= O_WRONLY;
            break;
        case FileAccessType::ReadWrite:
            flags |= O_RDWR;
            break;
        case FileAccessType::ReadOnly:
        default:
            break;
    }

    return CoOpen(path, flags, 0644);
}
