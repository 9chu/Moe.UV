/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#pragma once
#include <Moe.Core/Time.hpp>
#include <Moe.Core/Utils.hpp>

#include "AsyncHandle.hpp"

struct uv_loop_s;

namespace moe
{
namespace UV
{
    class Dns;

    /**
     * @brief 程序循环
     */
    class RunLoop :
        public NonCopyable
    {
        friend class AsyncHandle;
        friend class Dns;

    public:
        using CallbackType = std::function<void()>;

        /**
         * @brief 获取当前线程上的RunLoop
         */
        static RunLoop* GetCurrent()noexcept;

        /**
         * @brief 获取当前滴答数（毫秒级）
         *
         * 该值会在RunLoop单次迭代时更新。
         * 如果RunLoop不存在则直接获得系统当前的Tick。
         */
        static Time::Tick Now()noexcept;

    private:
        static void UVClosingHandleWalker(::uv_handle_s* handle, void* arg)noexcept;

    public:
        /**
         * @brief 构造消息循环
         * @param pool 共享的内存池
         */
        RunLoop(ObjectPool& pool);
        ~RunLoop();

        RunLoop(RunLoop&&)noexcept = delete;
        RunLoop& operator=(RunLoop&&) = delete;

    public:
        /**
         * @brief 获取共享的内存池对象
         */
        const ObjectPool& GetObjectPool()const noexcept { return m_stObjectPool; }
        ObjectPool& GetObjectPool()noexcept { return m_stObjectPool; }

        /**
         * @brief 获取内部句柄
         */
        ::uv_loop_s* GetHandle()const noexcept;

        /**
         * @brief 是否处于关闭状态
         */
        bool IsClosing()const noexcept { return m_bClosing; }

        /**
         * @brief 启动程序循环
         *
         * 运行循环直到所有句柄结束。
         */
        void Run();

        /**
         * @brief 尽快结束循环
         *
         * 通知RunLoop停止循环迭代。
         */
        void Stop()noexcept;

        /**
         * @brief 执行一次
         * @param wait 若设置为true，则等待直到一次事件发生
         */
        void RunOnce(bool wait);

        /**
         * @brief 强制关闭所有句柄
         */
        void ForceCloseAllHandle()noexcept;

        /**
         * @brief 立即更新滴答数
         */
        void UpdateTime()noexcept;

    private:
        ObjectPool& m_stObjectPool;

        UniquePooledObject<::uv_loop_s> m_pHandle;
        bool m_bClosing = false;
    };
}
}
