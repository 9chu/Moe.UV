/**
 * @file
 * @author chu
 * @date 2017/12/13
 */
#include <Moe.UV/Filesystem.hpp>
#include <Moe.UV/RunLoop.hpp>
#include <uv.h>

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
    if (ret == 0)
        return true;
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
    if (ret == 0)
        return true;
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

Filesystem::FileStatus Filesystem::CoGetFileStatus(const char* path)
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
    if (req->Request.result != 0)
        MOE_UV_THROW(static_cast<int>(req->Request.result));

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

Filesystem::FileStatus Filesystem::CoGetSymbolicLinkStatus(const char* path)
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
    if (req->Request.result != 0)
        MOE_UV_THROW(static_cast<int>(req->Request.result));

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
    if (req->Request.result != 0)
        MOE_UV_CHECK(static_cast<int>(req->Request.result));
    return string(reinterpret_cast<const char*>(req->Request.ptr));
}
