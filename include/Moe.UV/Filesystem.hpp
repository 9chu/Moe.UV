/**
 * @file
 * @author chu
 * @date 2017/12/13
 */
#pragma once
#include "IoHandle.hpp"
#include "Coroutine.hpp"

namespace moe
{
namespace UV
{
    class Filesystem
    {
    public:
        struct FileStatus
        {
            uint64_t DeviceId;  ///< @brief 设备ID
            uint64_t Mode;  ///< @brief 文件保护模式
            uint64_t HardLinks;  ///< @brief 硬连接个数
            uint64_t Uid;  ///< @brief 用户ID
            uint64_t Gid;  ///< @brief 组ID
            uint64_t RealDeviceId;  ///< @brief 实际设备ID（特殊文件）
            uint64_t InodeNumber;  ///< @brief 文件系统节点索引
            uint64_t Size;  ///< @brief 文件大小（字节）
            uint64_t BlockSize;  ///< @brief 系统单个块的大小
            uint64_t Blocks;  ///< @brief 占用的块数
            uint64_t Flags;  ///< @brief 标志（OSX）
            uint64_t FileGeneration;  ///< @brief 文件代数（OSX）
            Time::Timestamp LastAccessTime;  ///< @brief 最后访问时间
            Time::Timestamp LastModificationTime;  ///< @brief 最后修改时间
            Time::Timestamp LastStatusChangeTime;  ///< @brief 最后属性变动时间
            Time::Timestamp CreationTime;  ///< @brief 创建时间
        };

        /**
         * @brief （协程）检查文件是否存在
         * @param path 路径
         * @return 是否存在
         */
        static bool CoExists(const char* path);

        /**
         * @brief （协程）检查文件权限
         * @param path 路径
         * @param readAccess 读权限
         * @param writeAccess 写权限
         * @param execAccess 运行权限
         * @return 权限是否可满足
         */
        static bool CoAccess(const char* path, bool readAccess=true, bool writeAccess=false, bool execAccess=false);

        /**
         * @brief （协程）改变文件权限
         * @see https://linux.die.net/man/2/chmod
         * @param path 路径
         * @param mode 模式
         *
         * 请参照chmod手册。
         */
        static void CoChangeMode(const char* path, int mode);

        /**
         * @brief （协程）改变文件拥有者
         * @see https://linux.die.net/man/2/chown
         * @param path 路径
         * @param uid 用户ID
         * @param gid 组ID
         *
         * 请参照chown手册。
         * Windows不支持该操作，总是返回成功。
         */
        static void CoChangeOwner(const char* path, uv_uid_t uid, uv_gid_t gid);

        /**
         * @brief （协程）创建文件夹
         * @see https://linux.die.net/man/2/mkdir
         * @param path 路径
         * @param mode 模式，Windows忽略该参数
         *
         * 请参照mkdir手册。
         */
        static void CoCreateDirectory(const char* path, int mode=0755);

        /**
         * @brief (协程）移除文件夹
         * @see https://linux.die.net/man/2/rmdir
         * @param path 路径
         *
         * 删除一个目录，该目录必须为空。
         * 请参考rmdir手册。
         */
        static void CoRemoveDirectory(const char* path);

        /**
         * @brief (协程）获取文件属性
         * @see https://linux.die.net/man/2/stat
         * @param path 路径
         * @return 文件属性
         *
         * 请参考stat手册。
         */
        static FileStatus CoGetFileStatus(const char* path);

        /**
         * @brief (协程）获取符号文件属性
         * @see https://linux.die.net/man/2/stat
         * @param path 路径
         * @return 文件属性
         *
         * 针对符号链接返回符号链接本身的属性而不是对应文件的属性。
         * 请参考lstat手册。
         */
        static FileStatus CoGetSymbolicLinkStatus(const char* path);

        /**
         * @brief (协程）删除文件
         * @see https://linux.die.net/man/2/unlink
         * @param path 路径
         *
         * 请参考unlink手册。
         */
        static void CoUnlink(const char* path);

        /**
         * @brief （协程）创建硬连接
         * @see https://linux.die.net/man/2/link
         * @param path 原始路径
         * @param newPath 新路径
         *
         * 请参考link手册。
         */
        static void CoLink(const char* path, const char* newPath);

        /**
         * @brief （协程）创建符号（软）连接
         * @see https://linux.die.net/man/2/symlink
         * @param path 原始路径
         * @param newPath 新路径
         * @param flags 选项
         *
         * 请参考symlink手册。
         *
         * 在Windows上，flags需要填写：
         *  - UV_FS_SYMLINK_DIR 表明创建目录的符号连接
         *  - UV_FS_SYMLINK_JUNCTION 使用junction points创建符号连接
         */
        static void CoSymbolicLink(const char* path, const char* newPath, int flags);

        /**
         * @brief （协程）读取连接
         * @see https://linux.die.net/man/2/readlink
         * @param path 路径
         *
         * 请参考readlink手册。
         */
        static std::string CoReadLink(const char* path);

        /* TODO
         * uv_fs_utime(uv_loop_t* loop, uv_fs_t* req, const char* path, double atime, double mtime, uv_fs_cb cb)
         * uv_fs_rename(uv_loop_t* loop, uv_fs_t* req, const char* path, const char* new_path, uv_fs_cb cb)
         * uv_fs_mkdtemp(uv_loop_t* loop, uv_fs_t* req, const char* tpl, uv_fs_cb cb)
         * uv_fs_copyfile(uv_loop_t* loop, uv_fs_t* req, const char* path, const char* new_path, int flags, uv_fs_cb cb)
         *
         * uv_fs_scandir(uv_loop_t* loop, uv_fs_t* req, const char* path, int flags, uv_fs_cb cb)
         * uv_fs_scandir_next
         *
         * uv_fs_fstat
         * uv_fs_fsync
         * uv_fs_fdatasync
         * uv_fs_ftruncate
         * uv_fs_fchmod
         * uv_fs_futime
         * uv_fs_fchown
         */
    };
}
}
