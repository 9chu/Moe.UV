/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#include <Moe.UV/IoHandle.hpp>
#include <Moe.UV/RunLoop.hpp>

using namespace std;
using namespace moe;
using namespace UV;

::uv_loop_t* IoHandle::GetCurrentUVLoop()
{
    return RunLoop::GetCurrentUVLoop();
}

void IoHandle::OnUVClose(::uv_handle_t* handle)noexcept
{
    RefPtr<IoHandle> self(static_cast<IoHandle*>(handle->data));  // 获取所有权

    self->m_bHandleClosed = true;
    self->OnClose();
}

IoHandle::IoHandle()
{
    auto runloop = RunLoop::GetCurrent();
    if (!runloop || runloop->IsClosing())
        MOE_THROW(InvalidCallException, "Runloop is closing");
}

IoHandle::~IoHandle()
{
    assert(m_bHandleClosed);
}

bool IoHandle::Close()noexcept
{
    if (IsClosing())
        return false;

    // 发起Close操作
    ::uv_close(m_pHandle, OnUVClose);
    return true;
}

void IoHandle::BindHandle(::uv_handle_t* handle)noexcept
{
    assert(!m_pHandle && m_bHandleClosed);
    assert(handle);

    m_pHandle = handle;
    m_bHandleClosed = false;

    // 在句柄上持有所有权
    // 这意味着只有正确调用Close方法后才能销毁对象
    // 否则引用计数将无法得到释放
    RefPtr<IoHandle> self = RefFromThis();
    m_pHandle->data = self.Release();
}

void IoHandle::OnClose()noexcept
{
    m_bHandleClosed = true;
}
