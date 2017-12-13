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
    class Filesystem;

    /**
     * @brief 文件状态
     */
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
     * @brief 异步文件IO
     */
    class File :
        public NonCopyable
    {
        friend class Filesystem;

    protected:
        File(::uv_file fd);

    public:
        File(File&& rhs)noexcept;
        ~File();

        operator bool()const noexcept;
        File& operator=(File&& rhs)noexcept;

    public:
        /**
         * @brief （协程）读取数据
         * @param[in,out] buffer 缓冲区
         * @param offset 偏移量
         * @warning 缓冲区不能在协程栈上分配
         */
        void CoRead(MutableBytesView buffer, uint64_t offset=0);

        /**
         * @brief （协程）写入数据
         * @param buffer 缓冲区
         * @param offset 偏移量
         * @warning 缓冲区不能在协程栈上分配
         */
        void CoWrite(BytesView buffer, uint64_t offset=0);

        /**
         * @brief （协程）截断文件到指定长度
         * @param length 长度（字节）
         */
        void CoTruncate(uint64_t length);

        /**
         * @brief （协程）刷出数据
         */
        void CoFlush();

        /**
         * @brief （协程）刷出数据（仅数据，不含元信息）
         */
        void CoFlushData();

        /**
         * @brief （协程）获取文件状态
         * @return 文件状态
         */
        FileStatus CoGetStatus();

        /**
         * @brief （协程）改变文件权限
         * @param mode 模式
         */
        void CoChangeMode(int mode);

        /**
         * @brief （协程）改变文件拥有者
         * @param uid 用户ID
         * @param gid 组ID
         *
         * Windows不支持该操作，总是成功。
         */
        void CoChangeOwner(uv_uid_t uid, uv_gid_t gid);

        /**
         * @brief （协程）设置文件时间
         * @param accessTime 访问时间
         * @param modificationTime 修改时间
         *
         * 修改文件的时间，必须拥有对文件的所有权。
         */
        void CoSetFileTime(Time::Timestamp accessTime, Time::Timestamp modificationTime);

        /**
         * @brief （协程）关闭文件
         */
        void CoClose()noexcept;

    private:
        ::uv_file m_iFd = static_cast<::uv_file>(0);
    };

    /**
     * @brief 异步文件系统操作
     */
    class Filesystem
    {
    public:
        /**
         * @brief 目录项类型
         */
        enum class DirectoryEntryType
        {
            Unknown,
            File,
            Directory,
            Link,
            Fifo,
            Socket,
            Char,
            Block,
        };

        /**
         * @brief 目录项
         */
        struct DirectoryEntry
        {
            const char* Name;
            DirectoryEntryType Type;
        };

        /**
         * @brief 文件夹迭代器
         */
        class DirectoryEnumerator :
            public NonCopyable
        {
            friend class Filesystem;

        public:
            DirectoryEnumerator() = default;
            DirectoryEnumerator(DirectoryEnumerator&& rhs)noexcept;
            ~DirectoryEnumerator();

            operator bool() const noexcept { return m_pUVRequest != nullptr; }
            DirectoryEnumerator& operator=(DirectoryEnumerator&& rhs)noexcept;

        public:
            bool IsEof()const noexcept { return m_bIsEof; }

            /**
             * @brief 枚举下一个元素
             * @param[out] entry 枚举结果
             * @return 是否成功枚举元素，如果返回true则entry为一个有效的项目
             */
            bool Next(DirectoryEntry& entry);

        private:
            void* m_pUVRequest = nullptr;  // 擦除类型
            bool m_bIsEof = false;
        };

        /**
         * @brief （协程）检查文件是否存在
         * @param path 路径
         * @return 是否存在
         */
        static bool CoExists(const char* path);

        /**
         * @brief （协程）检查文件权限
         * @see https://linux.die.net/man/2/access
         * @param path 路径
         * @param readAccess 读权限
         * @param writeAccess 写权限
         * @param execAccess 运行权限
         * @return 权限是否可满足
         *
         * 请参考access手册。
         */
        static bool CoAccess(const char* path, bool readAccess=true, bool writeAccess=false, bool execAccess=false);

        /**
         * @brief （协程）改变文件权限
         * @see https://linux.die.net/man/2/chmod
         * @param path 路径
         * @param mode 模式
         *
         * 请参考chmod手册。
         */
        static void CoChangeMode(const char* path, int mode);

        /**
         * @brief （协程）改变文件拥有者
         * @see https://linux.die.net/man/2/chown
         * @param path 路径
         * @param uid 用户ID
         * @param gid 组ID
         *
         * 请参考chown手册。
         * Windows不支持该操作，总是成功。
         */
        static void CoChangeOwner(const char* path, uv_uid_t uid, uv_gid_t gid);

        /**
         * @brief （协程）创建文件夹
         * @see https://linux.die.net/man/2/mkdir
         * @param path 路径
         * @param mode 模式，Windows忽略该参数
         *
         * 请参考mkdir手册。
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
         * @brief （协程）构造临时文件
         * @see https://linux.die.net/man/3/mkdtemp
         * @param tpl 文件名模板，如"/tmp/temp.XXXXXX"，其中XXXXXX必须为后缀
         * @return 文件名
         *
         * 通过传入的模板构造唯一的临时文件。
         * 请参考mkdtemp手册。
         */
        static std::string CoMakeTempDirectory(const char* tpl);

        /**
         * @brief (协程）获取文件属性
         * @see https://linux.die.net/man/2/stat
         * @param path 路径
         * @return 文件属性
         *
         * 请参考stat手册。
         */
        static FileStatus CoGetStatus(const char* path);

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
        static void CoSymbolicLink(const char* path, const char* newPath, int flags=0);

        /**
         * @brief （协程）读取连接
         * @see https://linux.die.net/man/2/readlink
         * @param path 路径
         *
         * 请参考readlink手册。
         */
        static std::string CoReadLink(const char* path);

        /**
         * @brief （协程）设置文件时间
         * @see https://linux.die.net/man/2/utime
         * @param path 路径
         * @param accessTime 访问时间
         * @param modificationTime 修改时间
         *
         * 修改文件的时间，必须拥有对文件的所有权。
         * 请参考utime手册。
         */
        static void CoSetFileTime(const char* path, Time::Timestamp accessTime, Time::Timestamp modificationTime);

        /**
         * @brief （协程）重命名（移动）文件
         * @see https://linux.die.net/man/2/rename
         * @param path 路径
         * @param newPath 新路径
         *
         * 重命名文件，如果新文件所在路径存在则会先删除。
         * 请参考rename手册。
         */
        static void CoRename(const char* path, const char* newPath);

        /**
         * @brief （协程）扫描目录
         * @see https://linux.die.net/man/3/scandir
         * @param path 目录
         * @param flags 标志位
         * @return 文件夹迭代器
         *
         * 相比较scandir，不会显示"."和".."的项目。
         * 请参考scandir手册。
         */
        static DirectoryEnumerator CoScanDirectory(const char* path, int flags=0);
    };
}
}
