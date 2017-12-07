/**
 * @file
 * @author chu
 * @date 2017/12/7
 */
#include <Moe.UV/IdleHandle.hpp>
#include <Moe.UV/RunLoop.hpp>

using namespace std;
using namespace moe;
using namespace UV;

IoHandleHolder<IdleHandle> IdleHandle::Create()
{
    auto self = ObjectPool::Create<IdleHandle>();
    return self;
}

IoHandleHolder<IdleHandle> IdleHandle::Create(CallbackType callback)
{
    auto self = ObjectPool::Create<IdleHandle>();
    self->SetCallback(callback);
    return self;
}

void IdleHandle::OnUVIdle(uv_idle_t* handle)noexcept
{
    auto* self = GetSelf<IdleHandle>(handle);
    self->OnIdle();
}

IdleHandle::IdleHandle()
{
    MOE_UV_CHECK(::uv_idle_init(GetCurrentUVLoop(), &m_stHandle));
    BindHandle(reinterpret_cast<::uv_handle_t*>(&m_stHandle));
}

void IdleHandle::Start()
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Object is already closed");
    MOE_UV_CHECK(::uv_idle_start(&m_stHandle, OnUVIdle));
}

bool IdleHandle::Stop()noexcept
{
    if (IsClosing())
        return false;
    auto ret = ::uv_idle_stop(&m_stHandle);
    return ret == 0;
}

void IdleHandle::OnIdle()noexcept
{
    MOE_UV_EAT_EXCEPT_BEGIN
        if (m_stCallback)
            m_stCallback();
    MOE_UV_EAT_EXCEPT_END
}
