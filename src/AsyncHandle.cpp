/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#include <Moe.UV/AsyncHandle.hpp>

#include "UV.inl"

using namespace std;
using namespace moe;
using namespace UV;

void* AsyncHandle::HandleToData(AsyncHandle* data)noexcept
{
    static const auto kMask = BitCast<size_t>(numeric_limits<ssize_t>::min());

    assert((BitCast<size_t>(data) & kMask) == 0);
    return BitCast<void*>(BitCast<size_t>(data) | kMask);
}

AsyncHandle* AsyncHandle::DataToHandle(void* data)noexcept
{
    static const auto kMask = BitCast<size_t>(numeric_limits<ssize_t>::min());

    if (!data || (BitCast<size_t>(data) & kMask) == 0)
        return nullptr;
    return BitCast<AsyncHandle*>(BitCast<size_t>(data) & ~kMask);
}

void AsyncHandle::OnUVClose(::uv_handle_s* handle)noexcept
{
    auto self = DataToHandle(handle->data);
    if (!self)
    {
        // 所有权已经提前释放
        UniquePooledObject<::uv_handle_s> p(handle);
        return;
    }

    self->m_bHandleClosed = true;

    MOE_UV_CATCH_ALL_BEGIN
        self->OnClose();
    MOE_UV_CATCH_ALL_END
}

AsyncHandle::AsyncHandle(UniquePooledObject<::uv_handle_s>&& handle)
    : m_pHandle(std::move(handle))
{
    assert(m_pHandle);
    GetHandle()->data = HandleToData(this);
    m_bHandleClosed = false;
}

AsyncHandle::AsyncHandle(AsyncHandle&& org)noexcept
    : m_pHandle(std::move(org.m_pHandle)), m_bHandleClosed(org.m_bHandleClosed)
{
    org.m_bHandleClosed = true;

    if (m_pHandle)
        GetHandle()->data = HandleToData(this);
}

AsyncHandle::~AsyncHandle()
{
    if (!m_bHandleClosed)  // 此时需要强行释放句柄
    {
        Close();

        // 由于libuv依旧具有所有权，此时需要延后释放内存
        auto p = m_pHandle.release();
        p->data = nullptr;
    }
}

AsyncHandle& AsyncHandle::operator=(AsyncHandle&& org)noexcept
{
    if (m_pHandle)
    {
        Close();
        GetHandle()->data = nullptr;
    }

    m_pHandle = std::move(org.m_pHandle);
    m_bHandleClosed = org.m_bHandleClosed;

    org.m_bHandleClosed = true;

    if (m_pHandle)
        GetHandle()->data = HandleToData(this);
    return *this;
}

::uv_handle_s* AsyncHandle::GetHandle()const noexcept
{
    return m_pHandle.get();
}

bool AsyncHandle::IsClosing()const noexcept
{
    auto ret = !m_pHandle || m_bHandleClosed || ::uv_is_closing(GetHandle());
    return ret;
}

bool AsyncHandle::Close()noexcept
{
    if (IsClosing())
        return false;

    // 发起Close操作
    ::uv_close(GetHandle(), OnUVClose);
    return true;
}

void AsyncHandle::Ref()noexcept
{
    if (IsClosed())
        return;
    ::uv_ref(GetHandle());
}

void AsyncHandle::Unref()noexcept
{
    if (IsClosed())
        return;
    ::uv_unref(GetHandle());
}

void AsyncHandle::OnClose()
{
}
