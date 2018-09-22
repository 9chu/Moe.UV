/**
 * @file
 * @author chu
 * @date 2017/12/10
 */
#pragma once
#include "AsyncHandle.hpp"

#include <functional>

struct uv_process_s;
struct uv_process_options_s;

namespace moe
{
namespace UV
{
    /**
     * @brief 进程
     */
    class Process  // TODO: libuv没有提供这系列的操作，需要自行实现
    {
    public:
        /**
         * @brief （阻塞）向进程发送信号
         * @param pid 进程ID
         * @param signum 信号
         */
        static void Kill(int pid, int signum);
    };

    /**
     * @brief 子进程
     */
    class SubProcess :
        public AsyncHandle
    {
    public:
        using OnExitCallbackType = std::function<void(int64_t, int)>;  // (exitStatus, termSignal)

        /**
         * @brief （阻塞）创建并执行进程
         * @param path 进程路径
         * @param args 命令行参数，以nullptr结尾的数组，其中argv[0]必须为进程路径
         * @param env 执行环境，若为nullptr则继承父进程
         * @param cwd 工作目录，若为nullptr则继承父进程
         * @param flags 见uv_process_flags
         * @param uid 用户ID
         * @param gid 组ID
         * @return 进程对象
         */
        static SubProcess Spawn(const char* path, const char* args[], const char* env[]=nullptr,
            const char* cwd=nullptr, unsigned flags=0, uint32_t uid=0, uint32_t gid=0);

    private:
        static void OnUVProcessExit(::uv_process_s* handle, int64_t exitStatus, int termSignal)noexcept;

    protected:
        using AsyncHandle::AsyncHandle;

    public:
        SubProcess(SubProcess&& org)noexcept;
        SubProcess& operator=(SubProcess&& rhs)noexcept;

    public:
        /**
         * @brief 获取进程ID
         */
        int GetPid()const;

        /**
         * @brief 是否还存活
         */
        bool IsAlive()const noexcept { return m_bRunning; }

        /**
         * @brief 获取退出状态
         */
        int64_t GetExitStatus()const noexcept { return m_llExitStatus; }

        /**
         * @brief 获取终止时的信号
         */
        int GetTermSignal()const noexcept { return m_iTermSignal; }

        /**
         * @brief （阻塞）发送信号
         * @param signum 信号
         */
        void Kill(int signum);

    public:
        /**
         * @brief 获取事件回调
         */
        const OnExitCallbackType& GetOnExitCallback()const noexcept { return m_pOnExit; }

        /**
         * @brief 设置事件回调
         * @param type 事件类型
         */
        void SetOnExitCallback(const OnExitCallbackType& cb) { m_pOnExit = cb; }
        void SetOnExitCallback(OnExitCallbackType&& cb) { m_pOnExit = std::move(cb); }

    protected:  // 事件
        void OnExit(int64_t exitStatus, int termSignal);

    private:
        bool m_bRunning = true;
        int64_t m_llExitStatus = 0;
        int m_iTermSignal = 0;

        OnExitCallbackType m_pOnExit;
    };
}
}
