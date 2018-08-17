/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#pragma once
#include <Moe.Core/Time.hpp>
#include <Moe.Core/Utils.hpp>

#include "AsyncHandle.hpp"
#include <functional>

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

    protected:
        static ::uv_loop_t* GetCurrentUVLoop();

    private:
        static void UVClosingHandleWalker(::uv_handle_t* handle, void* arg)noexcept;

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
         * @brief 强制关闭所有句柄
         */
        void ForceCloseAllHandle()noexcept;

        /**
         * @brief 立即更新滴答数
         */
        void UpdateTime()noexcept;

    private:
        ObjectPool& m_stObjectPool;

        bool m_bClosing = false;
        ::uv_loop_t m_stLoop;
    };

    /**
     * @brief
     * @tparam T
     */
    template <typename T>
    class RunLoopAllocator
    {
    public:
        typedef T value_type;
        typedef T* pointer;
        typedef const T* const_pointer;
        typedef T& reference;
        typedef const T& const_reference;
        typedef std::size_t size_type;
        typedef std::ptrdiff_t difference_type;

        template <typename U>
        struct rebind
        {
            typedef RunLoopAllocator<U> other;
        };

    public:
        RunLoopAllocator()noexcept {}
        ~RunLoopAllocator()noexcept {}

        RunLoopAllocator(const RunLoopAllocator&)noexcept {}

        template <class U>
        RunLoopAllocator(const RunLoopAllocator<U>&)noexcept {}

        pointer address(reference value)const { return &value; }
        const_pointer address(const_reference value)const { return &value; }

        size_type max_size()const noexcept { return std::numeric_limits<std::size_t>::max() / sizeof(T); }

        pointer allocate(size_type num, const void* hint=nullptr)
        {
            MOE_UNUSED(hint);
            MOE_UV_ALLOC(num * sizeof(T));
            return static_cast<T*>(buffer.release());
        }

        void deallocate(pointer p, size_type num)
        {
            MOE_UNUSED(num);
            UniquePooledObject<void> object;
            object.reset(p);
        }

        void construct(pointer p, const T& value) { new((void*)p) T(value); }
        void destroy(pointer p) { p->~T(); }
    };
}
}
