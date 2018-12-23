/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#pragma once
#include <functional>
#include <Moe.Core/ObjectPool.hpp>

struct uv_handle_s;

namespace moe
{
namespace UV
{
    class RunLoop;

    /**
     * @brief 异步句柄
     *
     * - 该类封装了uv_handle_t上的一系列操作。
     * - 该类只能继承。
     */
    class AsyncHandle :
        public NonCopyable
    {
        friend class RunLoop;

    public:
        /**
         * @brief 句柄转数据
         *
         * 为了使得Moe.UV支持外部库共享libuv，针对Moe.UV的句柄在指针最高位设置为1进行区分。
         */
        static void* HandleToData(AsyncHandle* data)noexcept;

        /**
         * @brief 数据转句柄
         */
        static AsyncHandle* DataToHandle(void* data)noexcept;

    protected:
        static void OnUVClose(::uv_handle_s* handle)noexcept;

    protected:
        AsyncHandle(UniquePooledObject<::uv_handle_s>&& handle);

    public:
        AsyncHandle(AsyncHandle&& org)noexcept;
        ~AsyncHandle();

        AsyncHandle& operator=(AsyncHandle&& org)noexcept;

    public:
        /**
         * @brief 获取内部句柄
         */
        ::uv_handle_s* GetHandle()const noexcept;

        /**
         * @brief 是否已经关闭
         */
        bool IsClosed()const noexcept { return m_bHandleClosed; }

        /**
         * @brief 是否正在关闭
         *
         * 若已关闭或者正在关闭则返回true。
         */
        bool IsClosing()const noexcept;

        /**
         * @brief 增加RunLoop对句柄的引用
         */
        void Ref()noexcept;

        /**
         * @brief 解除RunLoop对句柄的引用
         *
         * 当句柄被RunLoop解除引用后RunLoop将不等待这一句柄结束。
         */
        void Unref()noexcept;

        /**
         * @brief 关闭句柄
         * @return 若已经处于Close流程，则返回false
         */
        virtual bool Close()noexcept;

    protected:  // 句柄事件
        /**
         * @brief 当句柄被关闭时触发
         */
        virtual void OnClose();

    private:
        UniquePooledObject<::uv_handle_s> m_pHandle;
        bool m_bHandleClosed = true;
    };
}
}
