/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#include <Moe.UV/IOHandle.hpp>
#include <Moe.UV/RunLoop.hpp>

using namespace std;
using namespace moe;
using namespace UV;

::uv_loop_t* IOHandle::GetCurrentLoop()
{
    auto runloop = RunLoop::GetCurrent();
    if (!runloop)
        MOE_THROW(InvalidCallException, "Bad execution context");
    return &runloop->m_stLoop;
}

void IOHandle::OnUVClose(::uv_handle_t* handle)noexcept
{
    RefPtr<IOHandle> self(static_cast<IOHandle*>(handle->data));  // 获取所有权

    self->m_bHandleClosed = true;
    self->OnClose();
}

void IOHandle::OnUVCloseHandleWalker(::uv_handle_t* handle, void* arg)noexcept
{
    MOE_UNUSED(arg);
    if (!::uv_is_closing(handle))
    {
        if (handle->data != nullptr)
            ::uv_close(handle, OnUVClose);
        else
        {
            assert(false);
            ::uv_close(handle, nullptr);
        }
    }
}

IOHandle::IOHandle()
{
}

IOHandle::~IOHandle()
{
    assert(m_bHandleClosed);
}

bool IOHandle::Close()noexcept
{
    if (IsClosing())
        return false;

    // 发起Close操作
    ::uv_close(m_pHandle, OnUVClose);
    return true;
}

void IOHandle::BindHandle(::uv_handle_t* handle)noexcept
{
    assert(!m_pHandle && m_bHandleClosed);
    assert(handle);

    m_pHandle = handle;
    m_bHandleClosed = false;

    // 在句柄上持有所有权
    // 这意味着只有正确调用Close方法后才能销毁对象
    // 否则引用计数将无法得到释放
    RefPtr<IOHandle> self = RefFromThis();
    m_pHandle->data = self.Release();
}

void IOHandle::OnClose()noexcept
{
}
