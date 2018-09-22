/**
 * @file
 * @author chu
 * @date 2018/8/19
 */
#include <Moe.UV/AsyncNotifier.hpp>

#include "UV.inl"

using namespace std;
using namespace moe;
using namespace UV;

void AsyncNotifier::OnUVAsync(::uv_async_s* handle)noexcept
{
    MOE_UV_GET_SELF(AsyncNotifier);

    MOE_UV_CATCH_ALL_BEGIN
        self->OnAsync();
    MOE_UV_CATCH_ALL_END
}

AsyncNotifier AsyncNotifier::Create()
{
    MOE_UV_NEW(::uv_async_t);
    MOE_UV_CHECK(::uv_async_init(GetCurrentUVLoop(), object.get(), OnUVAsync));
    return AsyncNotifier(CastHandle(std::move(object)));
}

AsyncNotifier AsyncNotifier::Create(const OnAsyncCallbackType& callback)
{
    auto ret = Create();
    ret.SetOnAsyncCallback(callback);
    return ret;
}

AsyncNotifier AsyncNotifier::Create(OnAsyncCallbackType&& callback)
{
    auto ret = Create();
    ret.SetOnAsyncCallback(std::move(callback));
    return ret;
}

AsyncNotifier::AsyncNotifier(AsyncNotifier&& org)noexcept
    : AsyncHandle(std::move(org)), m_pOnAsync(std::move(org.m_pOnAsync))
{
}

AsyncNotifier& AsyncNotifier::operator=(AsyncNotifier&& rhs)noexcept
{
    AsyncHandle::operator=(std::move(rhs));
    m_pOnAsync = std::move(rhs.m_pOnAsync);
    return *this;
}

void AsyncNotifier::Notify()
{
    MOE_UV_GET_HANDLE(::uv_async_t);
    MOE_UV_CHECK(::uv_async_send(handle));
}

void AsyncNotifier::OnAsync()
{
    if (m_pOnAsync)
        m_pOnAsync();
}
