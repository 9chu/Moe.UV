/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#include <Moe.UV/Timeout.hpp>

using namespace std;
using namespace moe;
using namespace UV;

namespace moe
{
namespace UV
{
namespace details
{
    class UVTimeout :
        public IOHandle
    {
        static void OnUVTimer(::uv_timer_t* handle)noexcept
        {
            auto* self = GetSelf<UVTimeout>(handle);
            self->OnTick();
        }

    public:
        UVTimeout()
        {
            // 初始化libuv句柄
            MOE_UV_CHECK(::uv_timer_init(GetCurrentLoop(), &m_stHandle));
            BindHandle(reinterpret_cast<::uv_handle_t*>(&m_stHandle));
        }

    public:
        Timeout *GetParent() const noexcept { return m_pParent; }
        void SetParent(Timeout *parent)noexcept { m_pParent = parent; }

        void Start(Time::Tick timeout)
        {
            assert(m_pParent);
            if (IsClosing())
                MOE_THROW(InvalidCallException, "Timeout is already closed");
            MOE_UV_CHECK(::uv_timer_start(&m_stHandle, OnUVTimer, timeout, 0));
        }

        void Stop()
        {
            assert(m_pParent);
            if (IsClosing())
                MOE_THROW(InvalidCallException, "Timeout is already closed");
            MOE_UV_CHECK(::uv_timer_stop(&m_stHandle));
        }

    protected:
        void OnTick()noexcept
        {
            if (!m_pParent)
            {
                Close();
                return;
            }
            m_pParent->OnTick();
        }

    private:
        Timeout* m_pParent = nullptr;
        ::uv_timer_t m_stHandle;
    };
}
}
}

Timeout::Timeout(Timeout&& rhs)noexcept
    : m_pHandle(std::move(rhs.m_pHandle)), m_bCoTimeout(rhs.m_bCoTimeout),
    m_stCoTickWaitHandle(std::move(rhs.m_stCoTickWaitHandle))
{
    if (m_pHandle)
        m_pHandle.CastTo<details::UVTimeout>()->SetParent(this);
}

Timeout::~Timeout()
{
    if (m_pHandle)
    {
        m_pHandle->Close();
        m_pHandle.CastTo<details::UVTimeout>()->SetParent(nullptr);
    }
}

Timeout& Timeout::operator=(Timeout&& rhs)noexcept
{
    if (m_pHandle)
    {
        m_pHandle->Close();
        m_pHandle.CastTo<details::UVTimeout>()->SetParent(nullptr);
    }

    m_pHandle = std::move(rhs.m_pHandle);
    m_bCoTimeout = rhs.m_bCoTimeout;
    m_stCoTickWaitHandle = std::move(rhs.m_stCoTickWaitHandle);

    if (m_pHandle)
        m_pHandle.CastTo<details::UVTimeout>()->SetParent(this);
    return *this;
}

bool Timeout::CoTimeout(Time::Tick timeout)
{
    // 如果Timer已处于关闭状态，则移除之
    if (m_pHandle && m_pHandle->IsClosing())
    {
        m_pHandle.CastTo<details::UVTimeout>()->SetParent(nullptr);
        m_pHandle = nullptr;
    }

    // 如果Timer尚未创建，则创建一个
    if (!m_pHandle)
    {
        auto handle = ObjectPool::Create<details::UVTimeout>();
        handle->SetParent(this);

        m_pHandle = handle.CastTo<IOHandle>();
    }

    // 通知Timer启动
    m_pHandle.CastTo<details::UVTimeout>()->Start(timeout);

    // 等待超时事件
    Coroutine::Suspend(m_stCoTickWaitHandle);
    return m_bCoTimeout;
}

void Timeout::Cancel()
{
    if (!m_pHandle)
        MOE_THROW(InvalidCallException, "Timeout is not started");
    m_pHandle.CastTo<details::UVTimeout>()->Stop();

    // 激活协程
    m_bCoTimeout = false;
    m_stCoTickWaitHandle.Resume();
}

void Timeout::OnTick()noexcept
{
    if (!m_pHandle)
        MOE_THROW(InvalidCallException, "Timer is not started");
    m_bCoTimeout = true;
    m_stCoTickWaitHandle.Resume();
}
