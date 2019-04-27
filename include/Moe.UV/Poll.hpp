/**
 * @file
 * @author chu
 * @date 2019/4/26
 */
#pragma once
#include "AsyncHandle.hpp"
#include <Moe.Core/Pal.hpp>

#include <functional>

struct uv_poll_s;

namespace moe
{
namespace UV
{
    /**
     * @brief Poll句柄
     */
    class Poll :
        public AsyncHandle
    {
    public:
        enum {
            EVENT_READABLE = 1,
            EVENT_WRITABLE = 2,
            EVENT_DISCONNECT = 4,
            EVENT_PRIORITIZED = 8,
        };

        using OnEventCallbackType = std::function<void(int, int)>;  // status, events

        static Poll Create(int fd);
#ifdef MOE_WINDOWS
        static Poll Create(void* socket);
#endif

    private:
        static void OnUVEvent(::uv_poll_s* handle, int status, int events)noexcept;

    protected:
        using AsyncHandle::AsyncHandle;

    public:
        Poll(Poll&& org)noexcept;
        Poll& operator=(Poll&& rhs)noexcept;

    public:
        /**
         * @brief 激活句柄
         * @param events 监听的事件
         */
        void Start(int events);

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
        void OnEvent(int status, int events);

    private:
        OnEventCallbackType m_pOnEvent;
    };
}
}
