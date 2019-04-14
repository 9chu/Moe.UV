/**
 * @file
 * @author chu
 * @date 2019/4/14
 */
#pragma once
#include "AsyncHandle.hpp"

#include <functional>

struct uv_fs_event_s;

namespace moe
{
namespace UV
{
    /**
     * @brief 文件系统事件
     */
    class FsEvent :
        public AsyncHandle
    {
    public:
        using OnEventCallbackType = std::function<void(const char*, int, int)>;  // filename, events, status

        static FsEvent Create();
        static FsEvent Create(const OnEventCallbackType& callback);
        static FsEvent Create(OnEventCallbackType&& callback);

    private:
        static void OnUVEvent(::uv_fs_event_s* handle, const char* filename, int events, int status)noexcept;

    protected:
        using AsyncHandle::AsyncHandle;

    public:
        FsEvent(FsEvent&& org)noexcept;
        FsEvent& operator=(FsEvent&& rhs)noexcept;

    public:
        /**
         * @brief 激活句柄
         * @param path 被监控文件
         * @param flags 标志位
         */
        void Start(const char* path, unsigned int flags=0);

        /**
         * @brief 终止句柄
         */
        bool Stop()noexcept;

    public:
        /**
         * @brief 获取事件回调
         */
        const OnEventCallbackType& GetOnEventCallback()const noexcept { return m_pOnEvent; }

        /**
         * @brief 设置事件回调
         * @param type 事件类型
         */
        void SetOnEventCallback(const OnEventCallbackType& cb) { m_pOnEvent = cb; }
        void SetOnEventCallback(OnEventCallbackType&& cb) { m_pOnEvent = std::move(cb); }

    protected:  // 事件
        void OnEvent(const char* filename, int events, int status);

    private:
        OnEventCallbackType m_pOnEvent;
    };
}
}
