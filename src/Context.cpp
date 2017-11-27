/**
 * @file
 * @author chu
 * @date 2017/11/22
 */
#include <Moe.UV/Context.hpp>

using namespace std;
using namespace moe;
using namespace UV;

#ifdef __cplusplus
extern "C"
{
#endif

// 定义见汇编代码
ContextState MoeContextMake(void* sp, size_t size, ContextEntry entry);
ContextTransfer MoeContextJump(const ContextState to, void* data);

#ifdef __cplusplus
}
#endif

ContextState UV::MakeContext(void* sp, size_t size, ContextEntry entry)noexcept
{
    return MoeContextMake(static_cast<uint8_t*>(sp) + size, size, entry);
}

ContextTransfer UV::JumpContext(const ContextState to, void* data)noexcept
{
    return MoeContextJump(to, data);
}
