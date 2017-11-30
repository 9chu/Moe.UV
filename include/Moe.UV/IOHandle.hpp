/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#pragma once
#include "ObjectPool.hpp"

#include <uv.h>

#ifdef Yield  // fvck Windows
#undef Yield
#endif
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace moe
{
namespace UV
{
    class RunLoop;

    /**
     * @brief IO句柄
     *
     * - 该类封装了uv_handle_t上的一系列操作。
     * - 该类只能继承。
     * - 一旦创建了一个实例，则对应的uv_handle_t就持有一个自身的强引用，这意味着必须Close才能释放内存。
     */
    class IOHandle :
        public ObjectPool::RefBase<IOHandle>
    {
        friend class RunLoop;

    protected:
        template <typename T, typename P>
        static T* GetSelf(P* handle)
        {
            return static_cast<T*>(static_cast<IOHandle*>(handle->data));
        }

        static ::uv_loop_t* GetCurrentLoop();

        static void OnUVClose(::uv_handle_t* handle)noexcept;
        static void OnUVCloseHandleWalker(::uv_handle_t* handle, void* arg)noexcept;

    protected:
        IOHandle();

    public:
        virtual ~IOHandle();

    public:
        /**
         * @brief 是否已经关闭
         */
        bool IsClosed()const noexcept { return m_bHandleClosed; }

        /**
         * @brief 是否正在关闭
         *
         * 若已关闭或者正在关闭则返回true。
         */
        bool IsClosing()const noexcept { return m_bHandleClosed || ::uv_is_closing(m_pHandle); }

        /**
         * @brief 增加RunLoop对句柄的引用
         */
        void Ref()
        {
            if (IsClosed())
                MOE_THROW(InvalidCallException, "Handle is already closed");
            ::uv_ref(m_pHandle);
        }

        /**
         * @brief 解除RunLoop对句柄的引用
         *
         * 当句柄被RunLoop解除引用后RunLoop将不等待这一句柄结束。
         */
        void Unref()
        {
            if (IsClosed())
                MOE_THROW(InvalidCallException, "Handle is already closed");
            ::uv_unref(m_pHandle);
        }

        /**
         * @brief 关闭句柄
         * @return 若已经处于Close流程，则返回false
         */
        virtual bool Close()noexcept;

    protected:
        void BindHandle(::uv_handle_t* handle)noexcept;

    protected:  // 句柄事件
        /**
         * @brief 当句柄被关闭时触发
         */
        virtual void OnClose()noexcept;

    protected:
        ::uv_handle_t* m_pHandle = nullptr;
        bool m_bHandleClosed = true;
    };

    using IOHandlePtr = RefPtr<IOHandle>;
}
}

#define MOE_UV_THROW(status) \
    do { \
        moe::APIException ex; \
        const char* err = ::uv_strerror((status)); \
        ex.SetSourceFile(__FILE__); \
        ex.SetFunctionName(__FUNCTION__); \
        ex.SetLineNumber(__LINE__); \
        ex.SetDescription(moe::StringUtils::Format("libuv error {0}: {1}", status, err)); \
        ex.SetInfo("ErrCode", (status)); \
        throw ex; \
    } while (false)

#define MOE_UV_LOG_ERROR(status) \
    do { \
        if ((status) != 0) { \
            const char* err = ::uv_strerror((status)); \
            MOE_ERROR("libuv error {0}: {1}", (status), err); \
        } \
    } while (false)

#define MOE_UV_CHECK(status) \
    do { \
        auto ret = (status); \
        if (ret != 0) \
            MOE_UV_THROW(ret); \
    } while (false)

#define MOE_UV_EAT_EXCEPT_BEGIN \
    try {

#define MOE_UV_EAT_EXCEPT_END \
    } catch (const moe::Exception& ex) { \
        MOE_ERROR("Uncaught exception, desc: {0}", ex); \
    } \
    catch (const std::exception& ex) { \
        MOE_ERROR("Uncaught exception, desc: {0}", ex.what()); \
    } \
    catch (...) { \
        MOE_ERROR("Uncaught unknown exception"); \
    }
