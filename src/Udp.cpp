/**
 * @file
 * @author chu
 * @date 2017/12/3
 */
#include <Moe.UV/Udp.hpp>

using namespace std;
using namespace moe;

namespace moe
{
namespace UV
{
namespace details
{
    class UVUdpBuffer :
        public ObjectPool::RefBase<UVUdpBuffer>
    {
    public:

    private:

    };

    static void OnUVAllocBuffer(::uv_handle_t* handle, size_t suggestedSize, ::uv_buf_t* buf)noexcept
    {
    }
}
}
}
