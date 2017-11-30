/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#pragma once
#include <memory>

#include <Moe.Core/RefPtr.hpp>
#include <Moe.Core/FixedBufferPool.hpp>

namespace moe
{
namespace UV
{
    namespace details
    {
        template <template <typename...> class X, typename T>
        struct InstantiationOf : std::false_type {};

        template <template <typename...> class X, typename... Y>
        struct InstantiationOf<X, X<Y...>> : std::true_type {};

        template <typename T>
        struct ObjectPoolDeleter;

        template <>
        struct ObjectPoolDeleter<void>
        {
            void operator()(void* p)noexcept;
        };

        template <typename T>
        struct ObjectPoolDeleter
        {
            void operator()(T* object)noexcept
            {
                object->~T();
                ObjectPoolDeleter<void>()(object);
            }
        };
    }

    /**
     * @brief 对象池
     *
     * 用于小对象的分配和释放。
     */
    class ObjectPool :
        public NonCopyable
    {
        template <typename T>
        friend class details::ObjectPoolDeleter;

    public:
        using BufferPtr = std::unique_ptr<void, details::ObjectPoolDeleter<void>>;

        template <typename T>
        using RefBase = moe::RefBase<T, details::ObjectPoolDeleter<T>>;

    public:
        /**
         * @brief 获取当前线程上的ObjectPool实例
         * @return 若尚未创建则返回nullptr
         */
        static ObjectPool* GetCurrent()noexcept;

        /**
         * @brief 分配内存
         * @param sz 分配的内存大小
         * @return 缓冲区指针
         */
        static BufferPtr Alloc(size_t sz);

        /**
         * @brief 构造基于引用计数的对象
         * @tparam T 类型
         * @tparam Args 参数类型
         * @param args 参数
         * @return 引用计数管理的对象
         *
         * 注意：在当前线程分配的对象只能在当前线程释放，否则将引发未定义行为。
         */
        template <typename T, typename... Args>
        static RefPtr<T> Create(Args&&... args)
        {
            static_assert(!std::is_array<T>::value, "RefPtr does not accept arrays");
            static_assert(!std::is_reference<T>::value, "RefPtr does not accept references");
            static_assert(details::InstantiationOf<details::ObjectPoolDeleter, typename T::DeleterType>::value,
                "Only be used for pooled object");

            auto object = Alloc(sizeof(T)).release();

            try
            {
                return RefPtr<T>(new(object) T(std::forward<Args>(args)...));
            }
            catch (...)
            {
                details::ObjectPoolDeleter<void>()(object);
                throw;
            }
        }

    public:
        ObjectPool();
        ~ObjectPool();

        ObjectPool(ObjectPool&&) = delete;
        ObjectPool& operator=(ObjectPool&&) = delete;

    public:
        size_t GetTotalBufferSize()noexcept { return m_stPool.GetTotalBufferSize(); }
        size_t GetTotalFreeSize()noexcept { return m_stPool.GetTotalFreeSize(); }
        size_t GetTotalUsedSize()noexcept { return m_stPool.GetTotalUsedSize(); }

        void CollectGarbage()noexcept { m_stPool.CollectGarbage(); }

    private:
        void* RawAlloc(size_t sz) { return m_stPool.Alloc(sz); }
        void RawFree(void* p)noexcept { m_stPool.Free(p); }

    private:
        FixedBufferPool m_stPool;
    };
}
}
