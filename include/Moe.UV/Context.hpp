/**
 * @file
 * @author chu
 * @date 2017/11/22
 */
#pragma once
#include <cstdint>

namespace moe
{
namespace UV
{
    using ContextState = void*;

    struct ContextTransfer
    {
        ContextState State;
        void* Data;
    };

    /**
     * @brief 上下文函数（协程）
     *
     * 使用MakeContext创建一个协程时入口函数需要符合这一定义。
     * 参数将捎带从某个上下文切换过来的信息，包括上一个切换的上下文状态和用户数据。
     * 注意当切回这一上下文时必须使用ContextTransfer::State进行。
     */
    using ContextEntry = void(*)(ContextTransfer);

    /**
     * @brief 创建一个新的上下文
     * @param sp 指向栈空间的指针
     * @param size 栈大小
     * @param entry 协程入口
     * @return 尚未执行的一个初始化上下文状态
     */
    ContextState MakeContext(void* sp, size_t size, ContextEntry entry)noexcept;

    /**
     * @brief 跳转到某个上下文
     * @param to 目标上下文
     * @param data 用户数据
     * @return 从目标上下文切出时捎带的数据
     *
     * 注意：
     * - 当从当前的上下文切入一个新的上下文时，若发生在协程内，则当前协程的ContextState会立即失效。
     * 新的ContextState会随着ContextEntry的参数传入目标上下文。
     * - 当一个协程函数完成执行却又没有切到其他上下文时程序会直接退出。
     * - 上下文切换不处理异常，需要用户自行包裹。
     */
    ContextTransfer JumpContext(const ContextState to, void* data)noexcept;
}
}
