/**
 * @file
 * @author chu
 * @date 2018/9/2
 */
#pragma once
#include "AsyncHandle.hpp"

#include <functional>

struct uv_shutdown_s;
struct uv_write_s;
struct uv_stream_s;
struct uv_buf_t;

namespace moe
{
namespace UV
{
    /**
     * @brief 流式接口
     */
    class Stream :
        public AsyncHandle
    {
        friend class ObjectPool;

    public:
        using OnWriteCallbackType = std::function<void(int)>;
        using OnErrorCallbackType = std::function<void(int)>;
        using OnShutdownCallbackType = std::function<void()>;
        using OnDataCallbackType = std::function<void(BytesView)>;

    private:
        static void OnUVShutdown(::uv_shutdown_s* request, int status)noexcept;
        static void OnUVWrite(::uv_write_s* request, int status)noexcept;
        static void OnUVAllocBuffer(::uv_handle_s* handle, size_t suggestedSize, ::uv_buf_t* buf)noexcept;
        static void OnUVRead(::uv_stream_s* handle, ssize_t nread, const ::uv_buf_t* buf)noexcept;

    protected:
        using AsyncHandle::AsyncHandle;

    public:
        Stream(Stream&& org)noexcept;
        Stream& operator=(Stream&& rhs)noexcept;

    public:
        /**
         * @brief 是否是可读的
         */
        bool IsReadable()const noexcept;

        /**
         * @brief 是否是可写的
         */
        bool IsWritable()const noexcept;

        /**
         * @brief 获取写队列大小
         */
        size_t GetWriteQueueSize()const noexcept;

        /**
         * @brief 关闭数据流
         * @param cb 回调函数
         *
         * 关闭写端。
         */
        void Shutdown();

        /**
         * @brief 开始读操作
         */
        void StartRead();

        /**
         * @brief 停止读操作
         */
        bool StopRead()noexcept;

        /**
         * @brief 写数据
         * @param buf 数据
         *
         * 方法将会拷贝数据，因此无需保持对数据的所有权。
         */
        void Write(BytesView buf);

        /**
         * @brief 无拷贝地写数据
         * @param buffer 数据
         * @param cb 回调函数
         */
        void WriteNoCopy(BytesView buffer, const OnWriteCallbackType& cb);
        void WriteNoCopy(BytesView buffer, OnWriteCallbackType&& cb);

        /**
         * @brief 尝试写数据
         * @param buffer 数据
         * @return 是否成功写
         */
        bool TryWrite(BytesView buffer);

    public:
        const OnErrorCallbackType& GetOnErrorCallback()const noexcept { return m_pOnError; }
        void SetOnErrorCallback(const OnErrorCallbackType& cb) { m_pOnError = cb; }
        void SetOnErrorCallback(OnErrorCallbackType&& cb)noexcept { m_pOnError = std::move(cb); }

        const OnShutdownCallbackType& GetOnShutdownCallback()const noexcept { return m_pOnShutdown; }
        void SetOnShutdownCallback(const OnShutdownCallbackType& cb) { m_pOnShutdown = cb; }
        void SetOnShutdownCallback(OnShutdownCallbackType&& cb)noexcept { m_pOnShutdown = std::move(cb); }

        const OnDataCallbackType& GetOnDataCallback()const noexcept { return m_pOnData; }
        void SetOnDataCallback(const OnDataCallbackType& cb) { m_pOnData = cb; }
        void SetOnDataCallback(OnDataCallbackType&& cb)noexcept { m_pOnData = std::move(cb); }

    protected:  // 事件
        void OnError(int error);
        void OnShutdown();
        void OnData(BytesView data);

    private:
        OnErrorCallbackType m_pOnError;
        OnShutdownCallbackType m_pOnShutdown;
        OnDataCallbackType m_pOnData;
    };
}
}
