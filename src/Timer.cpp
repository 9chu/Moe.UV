/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#include <Moe.UV/Timer.hpp>

using namespace std;
using namespace moe;
using namespace UV;

namespace moe
{
namespace UV
{
namespace details
{
    class UVTimer :
        public IOHandle
    {
        static void OnUVTimer(::uv_timer_t* handle)noexcept
        {
            auto* self = GetSelf<UVTimer>(handle);
            self->OnTick();
        }

    public:
        UVTimer()
        {
            // 初始化libuv句柄
            MOE_UV_CHECK(::uv_timer_init(GetCurrentLoop(), &m_stHandle));
            BindHandle(reinterpret_cast<::uv_handle_t*>(&m_stHandle));
        }

    public:
        Timer* GetParent() const noexcept { return m_pParent; }
        void SetParent(Timer* parent)noexcept { m_pParent = parent; }

        void Start()
        {
            assert(m_pParent);
            if (IsClosing())
                MOE_THROW(InvalidCallException, "Timer is already closed");
            auto interval = m_pParent->GetInterval();
            MOE_UV_CHECK(::uv_timer_start(&m_stHandle, OnUVTimer, interval, interval));
        }

        void Stop()
        {
            assert(m_pParent);
            if (IsClosing())
                MOE_THROW(InvalidCallException, "Timer is already closed");
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
        Timer* m_pParent = nullptr;
        ::uv_timer_t m_stHandle;
    };
}
}
}

Timer::Timer(Timer&& rhs)noexcept
    : m_pHandle(std::move(rhs.m_pHandle)), m_ullInterval(rhs.m_ullInterval),
    m_stCoTickWaitHandle(std::move(rhs.m_stCoTickWaitHandle))
{
    if (m_pHandle)
        m_pHandle.CastTo<details::UVTimer>()->SetParent(this);
}

Timer::~Timer()
{
    if (m_pHandle)
    {
        m_pHandle->Close();
        m_pHandle.CastTo<details::UVTimer>()->SetParent(nullptr);
    }
}

Timer& Timer::operator=(Timer&& rhs)noexcept
{
    if (m_pHandle)
    {
        m_pHandle->Close();
        m_pHandle.CastTo<details::UVTimer>()->SetParent(nullptr);
    }

    m_pHandle = std::move(rhs.m_pHandle);
    m_ullInterval = rhs.m_ullInterval;
    m_stCoTickWaitHandle = std::move(rhs.m_stCoTickWaitHandle);

    if (m_pHandle)
        m_pHandle.CastTo<details::UVTimer>()->SetParent(this);
    return *this;
}

void Timer::Start(bool background)
{
    // 如果Timer已处于关闭状态，则移除之
    if (m_pHandle && m_pHandle->IsClosing())
    {
        m_pHandle.CastTo<details::UVTimer>()->SetParent(nullptr);
        m_pHandle = nullptr;
    }

    // 如果Timer尚未创建，则创建一个
    if (!m_pHandle)
    {
        auto handle = ObjectPool::Create<details::UVTimer>();
        handle->SetParent(this);

        m_pHandle = handle.CastTo<IOHandle>();
    }

    // 通知Timer启动
    m_pHandle.CastTo<details::UVTimer>()->Start();

    // 设置是否为背景时钟
    if (background)
        m_pHandle->Unref();
    else
        m_pHandle->Ref();
}

void Timer::Stop()
{
    if (!m_pHandle)
        MOE_THROW(InvalidCallException, "Timer is not started");
    m_pHandle.CastTo<details::UVTimer>()->Stop();
}

void Timer::CoWait()
{
    if (!m_pHandle)
        MOE_THROW(InvalidCallException, "Timer is not started");
    Coroutine::Suspend(m_stCoTickWaitHandle);
}

void Timer::OnTick()noexcept
{
    m_stCoTickWaitHandle.Resume();
}
