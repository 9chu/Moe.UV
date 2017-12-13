/**
 * @file
 * @author chu
 * @date 2017/12/13
 */
#include <Moe.UV/Filesystem.hpp>
#include <Moe.UV/RunLoop.hpp>
#include <Moe.Core/Utils.hpp>

#ifndef MOE_WINDOWS
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
}

//////////////////////////////////////////////////////////////////////////////// File

/* TODO
 * uv_fs_close(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb)
 * uv_fs_read(uv_loop_t* loop, uv_fs_t* req, uv_file file, const uv_buf_t bufs[], unsigned int nbufs, int64_t offset, uv_fs_cb cb)
 * uv_fs_write(uv_loop_t* loop, uv_fs_t* req, uv_file file, const uv_buf_t bufs[], unsigned int nbufs, int64_t offset, uv_fs_cb cb)
 * uv_fs_fstat(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb)
 * uv_fs_fsync(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb)
 * uv_fs_fdatasync(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb)
 * uv_fs_ftruncate(uv_loop_t* loop, uv_fs_t* req, uv_file file, int64_t offset, uv_fs_cb cb)
 * uv_fs_fchmod(uv_loop_t* loop, uv_fs_t* req, uv_file file, int mode, uv_fs_cb cb)
 * uv_fs_futime(uv_loop_t* loop, uv_fs_t* req, uv_file file, double atime, double mtime, uv_fs_cb cb)
 * uv_fs_fchown(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_uid_t uid, uv_gid_t gid, uv_fs_cb cb)
 */

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
    const auto& stat = req->Request.statbuf;

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
    ret.LastModificationTime = static_cast<Time::Tick>(stat.st_mtim.tv_sec * 1000ull + stat.st_mtim.tv_nsec / 1000000);
    ret.LastStatusChangeTime = static_cast<Time::Tick>(stat.st_ctim.tv_sec * 1000ull + stat.st_ctim.tv_nsec / 1000000);
    ret.CreationTime = static_cast<Time::Tick>(stat.st_birthtim.tv_sec * 1000ull + stat.st_birthtim.tv_nsec / 1000000);
    return ret;
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
    const auto& stat = req->Request.statbuf;

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
    ret.LastModificationTime = static_cast<Time::Tick>(stat.st_mtim.tv_sec * 1000ull + stat.st_mtim.tv_nsec / 1000000);
    ret.LastStatusChangeTime = static_cast<Time::Tick>(stat.st_ctim.tv_sec * 1000ull + stat.st_ctim.tv_nsec / 1000000);
    ret.CreationTime = static_cast<Time::Tick>(stat.st_birthtim.tv_sec * 1000ull + stat.st_birthtim.tv_nsec / 1000000);
    return ret;
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
