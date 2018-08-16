/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#include <Moe.UV/AsyncHandle.hpp>

#include <Moe.UV/RunLoop.hpp>
#include <Moe.Core/Logging.hpp>

using namespace std;
using namespace moe;
using namespace UV;

::uv_loop_t* AsyncHandle::GetCurrentUVLoop()
{
    return RunLoop::GetCurrentUVLoop();
}

void AsyncHandle::OnUVClose(::uv_handle_t* handle)noexcept
{
    RefPtr<AsyncHandle> self(static_cast<AsyncHandle*>(handle->data));  // 获取所有权

    self->m_bHandleClosed = true;

    MOE_UV_CATCH_ALL_BEGIN
        self->OnClose();
    MOE_UV_CATCH_ALL_END
}

AsyncHandle::AsyncHandle()
{
    auto runloop = RunLoop::GetCurrent();
    if (!runloop || runloop->IsClosing())
        MOE_THROW(InvalidCallException, "RunLoop is closing");
}

AsyncHandle::~AsyncHandle()
{
    assert(m_bHandleClosed);
}

bool AsyncHandle::Close()noexcept
{
    if (IsClosing())
        return false;

    // 发起Close操作
    ::uv_close(m_pHandle, OnUVClose);
    return true;
}

void AsyncHandle::BindHandle(::uv_handle_t* handle)noexcept
{
    assert(!m_pHandle && m_bHandleClosed);
    assert(handle);

    m_pHandle = handle;
    m_bHandleClosed = false;

    // 在句柄上持有所有权
    // 这意味着只有正确调用Close方法后才能销毁对象
    // 否则引用计数将无法得到释放
    RefPtr<AsyncHandle> self = RefFromThis();
    m_pHandle->data = self.Release();
}

void AsyncHandle::OnClose()
{
}
