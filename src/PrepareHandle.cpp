/**
 * @file
 * @author chu
 * @date 2017/12/10
 */
#include <Moe.UV/PrepareHandle.hpp>
#include <Moe.UV/RunLoop.hpp>

using namespace std;
using namespace moe;
using namespace UV;

IoHandleHolder<PrepareHandle> PrepareHandle::Create()
{
    auto self = ObjectPool::Create<PrepareHandle>();
    return self;
}

IoHandleHolder<PrepareHandle> PrepareHandle::Create(const CallbackType& callback)
{
    auto self = ObjectPool::Create<PrepareHandle>();
    self->SetCallback(callback);
    return self;
}

IoHandleHolder<PrepareHandle> PrepareHandle::Create(CallbackType&& callback)
{
    auto self = ObjectPool::Create<PrepareHandle>();
    self->SetCallback(std::move(callback));
    return self;
}

void PrepareHandle::OnUVPrepare(uv_prepare_t* handle)noexcept
{
    auto* self = GetSelf<PrepareHandle>(handle);
    self->OnPrepare();
}

PrepareHandle::PrepareHandle()
{
    MOE_UV_CHECK(::uv_prepare_init(GetCurrentUVLoop(), &m_stHandle));
    BindHandle(reinterpret_cast<::uv_handle_t*>(&m_stHandle));
}

void PrepareHandle::Start()
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Object is already closed");
    MOE_UV_CHECK(::uv_prepare_start(&m_stHandle, OnUVPrepare));
}

bool PrepareHandle::Stop()noexcept
{
    if (IsClosing())
        return false;
    auto ret = ::uv_prepare_stop(&m_stHandle);
    return ret == 0;
}

void PrepareHandle::OnPrepare()noexcept
{
    MOE_UV_EAT_EXCEPT_BEGIN
        if (m_stCallback)
            m_stCallback();
    MOE_UV_EAT_EXCEPT_END
}
