/**
 * @file
 * @author chu
 * @date 2017/12/27
 */
#pragma once
#include "IoHandle.hpp"
#include "CoEvent.hpp"

#include <functional>

namespace moe
{
namespace UV
{
    /**
     * @brief 子进程
     */
    class SubProcess :
        public IoHandle
    {
        friend class ObjectPool;

    public:
        using CallbackType = std::function<void(int64_t, int)>;

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
        static IoHandleHolder<SubProcess> Spawn(const char* path, const char* args[], const char* env[]=nullptr,
            const char* cwd=nullptr, unsigned flags=0, uv_uid_t uid=0, uv_gid_t gid=0);

        /**
         * @brief （阻塞）向进程发送信号
         * @param pid 进程ID
         * @param signum 信号
         */
        static void Kill(int pid, int signum);

    private:
        static void OnUVProcessExit(uv_process_t* process, int64_t exitStatus, int termSignal)noexcept;

    protected:
        explicit SubProcess(const ::uv_process_options_t& options);

    public:
        /**
        * @brief 获取或设置信号触发回调函数
        */
        CallbackType GetCallback()const noexcept { return m_stCallback; }
        void SetCallback(const CallbackType& callback) { m_stCallback = callback; }
        void SetCallback(CallbackType&& callback)noexcept { m_stCallback = std::move(callback); }

        /**
         * @brief 获取进程ID
         */
        int GetPid()const noexcept { return m_stHandle.pid; }

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

        /**
         * @brief （协程）等待进程终止
         * @param timeout 超时时间
         * @return 事件是否触发，若超时或者取消均会返回false
         *
         * 该方法只能在协程上执行。
         * 只允许单个协程进行等待。
         */
        bool CoWait(Time::Tick timeout=Coroutine::kInfinityTimeout);

        /**
         * @brief 取消协程等待
         */
        void CancelWait()noexcept;

    protected:
        void OnClose()noexcept override;
        void OnExit(int64_t exitStatus, int termSignal)noexcept;

    private:
        ::uv_process_t m_stHandle;

        bool m_bRunning = true;
        int64_t m_llExitStatus = 0;
        int m_iTermSignal = 0;

        // 协程
        CoEvent m_stExitEvent;  // 进程退出事件

        // 回调
        CallbackType m_stCallback;
    };
}
}
