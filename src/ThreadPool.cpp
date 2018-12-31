/**
 * @file
 * @author chu
 * @date 2018/12/30
 */
#include <Moe.UV/ThreadPool.hpp>

#include "UV.inl"

using namespace std;
using namespace moe;
using namespace UV;

struct UVWorkReq
{
    ::uv_work_t Request;

    ThreadPool::OnWorkCallbackType OnWork;
    ThreadPool::OnAfterWorkCallbackType OnAfterWork;

    void* UserData = nullptr;

    static void WorkCallback(::uv_work_t* req)
    {
        auto self = static_cast<UVWorkReq*>(req->data);  // 此处需要一个弱引用

        MOE_UV_CATCH_ALL_BEGIN
            if (self->OnWork)
                self->OnWork(self->UserData);
        MOE_UV_CATCH_ALL_END
    }

    static void AfterWorkCallback(::uv_work_t* req, int status)
    {
        UniquePooledObject<UVWorkReq> self;
        self.reset(static_cast<UVWorkReq*>(req->data));

        MOE_UV_CATCH_ALL_BEGIN
            if (self->OnAfterWork)
                self->OnAfterWork(status, self->UserData);
        MOE_UV_CATCH_ALL_END
    }
};

#define MOVE_OWNER_SELF \
    do { \
        auto raw = object.release(); \
        raw->Request.data = raw; \
    } while (false)

void ThreadPool::QueueWork(const OnWorkCallbackType& work, const OnAfterWorkCallbackType& afterWork, void* userData)
{
    MOE_UV_NEW(UVWorkReq);
    object->OnWork = work;
    object->OnAfterWork = afterWork;
    object->UserData = userData;

    MOE_UV_CHECK(::uv_queue_work(GetCurrentUVLoop(), &object->Request, UVWorkReq::WorkCallback,
        UVWorkReq::AfterWorkCallback));
    MOVE_OWNER_SELF;
}

void ThreadPool::QueueWork(OnWorkCallbackType&& work, OnAfterWorkCallbackType&& afterWork, void* userData)
{
    MOE_UV_NEW(UVWorkReq);
    object->OnWork = std::move(work);
    object->OnAfterWork = std::move(afterWork);
    object->UserData = userData;

    MOE_UV_CHECK(::uv_queue_work(GetCurrentUVLoop(), &object->Request, UVWorkReq::WorkCallback,
        UVWorkReq::AfterWorkCallback));
    MOVE_OWNER_SELF;
}
