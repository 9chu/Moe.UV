/**
 * @file
 * @author chu
 * @date 2018/8/19
 */
#include <Moe.UV/AsyncNotifier.hpp>

#include <Moe.Core/Logging.hpp>
#include <Moe.UV/RunLoop.hpp>

using namespace std;
using namespace moe;
using namespace UV;

//////////////////////////////////////////////////////////////////////////////// AsyncNotifierBase

void AsyncNotifierBase::OnUVAsync(::uv_async_t* handle)noexcept
{
    auto* self = GetSelf<AsyncNotifierBase>(handle);

    MOE_UV_CATCH_ALL_BEGIN
        self->OnAsync();
    MOE_UV_CATCH_ALL_END
}

AsyncNotifierBase::AsyncNotifierBase()
{
    MOE_UV_CHECK(::uv_async_init(GetCurrentUVLoop(), &m_stHandle, OnUVAsync));
    BindHandle(reinterpret_cast<::uv_handle_t*>(&m_stHandle));
}

void AsyncNotifierBase::Notify()
{
    MOE_UV_CHECK(::uv_async_send(&m_stHandle));
}

//////////////////////////////////////////////////////////////////////////////// AsyncNotifier

UniqueAsyncHandlePtr<AsyncNotifier> AsyncNotifier::Create()
{
    MOE_UV_NEW(AsyncNotifier);
    return object;
}

UniqueAsyncHandlePtr<AsyncNotifier> AsyncNotifier::Create(const OnAsyncCallbackType& callback)
{
    MOE_UV_NEW(AsyncNotifier);
    object->SetOnAsyncCallback(callback);
    return object;
}

UniqueAsyncHandlePtr<AsyncNotifier> AsyncNotifier::Create(OnAsyncCallbackType&& callback)
{
    MOE_UV_NEW(AsyncNotifier);
    object->SetOnAsyncCallback(std::move(callback));
    return object;
}

void AsyncNotifier::OnAsync()
{
    if (m_pOnAsync)
        m_pOnAsync();
}
