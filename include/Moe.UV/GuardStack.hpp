/**
 * @file
 * @author chu
 * @date 2017/11/28
 */
#pragma once
#include <Moe.Core/Utils.hpp>

namespace moe
{
namespace UV
{
    /**
     * @brief 受保护堆栈
     *
     * 用于分配协程的栈空间，并在栈溢出时提供必要保护。
     */
    class GuardStack :
        public NonCopyable
    {
    public:
        /**
         * @brief 获取系统页大小
         */
        static size_t GetPageSize()noexcept;

        /**
         * @brief 获取系统最小的栈大小
         */
        static size_t GetMinStackSize()noexcept;

        /**
         * @brief 获取系统最大的栈大小
         */
        static size_t GetMaxStackSize()noexcept;

        /**
         * @brief 获取系统默认栈大小
         */
        static size_t GetDefaultStackSize()noexcept;

    public:
        GuardStack();
        ~GuardStack();

        GuardStack(GuardStack&& rhs)noexcept;
        GuardStack& operator=(GuardStack&& rhs)noexcept;

        operator bool()const noexcept { return GetBuffer() != nullptr; }

    public:
        /**
         * @brief 获取分配的总内存空间
         */
        void* GetBuffer()noexcept { return m_pBuffer; }
        const void* GetBuffer()const noexcept { return m_pBuffer; }

        /**
         * @brief 获取栈顶地址
         */
        void* GetStackTop()noexcept
        {
            return static_cast<void*>(static_cast<uint8_t*>(m_pBuffer) + m_uProtectedSize);
        }

        /**
         * @brief 获取栈底地址
         */
        void* GetStackBase()noexcept
        {
            return static_cast<void*>(static_cast<uint8_t*>(m_pBuffer) + GetAllocSize());
        }

        /**
         * @brief 获取保护页大小
         *
         * 通常等于一个页大小。
         */
        size_t GetProtectedSize()const noexcept { return m_uProtectedSize; }

        /**
         * @brief 获取有效栈空间大小
         */
        size_t GetStackSize()const noexcept { return m_uStackSize; }

        /**
         * @brief 获取总分配大小
         */
        size_t GetAllocSize()const noexcept { return m_uProtectedSize + m_uStackSize; }

        /**
         * @brief 使用指定栈大小分配栈空间
         * @param stackSize 栈大小，若为0则使用默认大小
         */
        void Alloc(size_t stackSize=0);

        /**
         * @brief 回收内存
         */
        void Free()noexcept;

    private:
        void* m_pBuffer = nullptr;  // 分配大小 = ProtectedSize + StackSize
        size_t m_uProtectedSize = 0;
        size_t m_uStackSize = 0;
    };
}
}
