/**
 * @file
 * @author chu
 * @date 2017/12/10
 */
#include <Moe.UV/CheckHandle.hpp>
#include <Moe.UV/RunLoop.hpp>

using namespace std;
using namespace moe;
using namespace UV;

IoHandleHolder<CheckHandle> CheckHandle::Create()
{
    auto self = ObjectPool::Create<CheckHandle>();
    return self;
}

IoHandleHolder<CheckHandle> CheckHandle::Create(CallbackType callback)
{
    auto self = ObjectPool::Create<CheckHandle>();
    self->SetCallback(callback);
    return self;
}

void CheckHandle::OnUVCheck(uv_check_t* handle)noexcept
{
    auto* self = GetSelf<CheckHandle>(handle);
    self->OnCheck();
}

CheckHandle::CheckHandle()
{
    MOE_UV_CHECK(::uv_check_init(GetCurrentUVLoop(), &m_stHandle));
    BindHandle(reinterpret_cast<::uv_handle_t*>(&m_stHandle));
}

void CheckHandle::Start()
{
    if (IsClosing())
        MOE_THROW(InvalidCallException, "Object is already closed");
    MOE_UV_CHECK(::uv_check_start(&m_stHandle, OnUVCheck));
}

bool CheckHandle::Stop()noexcept
{
    if (IsClosing())
        return false;
    auto ret = ::uv_check_stop(&m_stHandle);
    return ret == 0;
}

void CheckHandle::OnCheck()noexcept
{
    MOE_UV_EAT_EXCEPT_BEGIN
        if (m_stCallback)
            m_stCallback();
    MOE_UV_EAT_EXCEPT_END
}
