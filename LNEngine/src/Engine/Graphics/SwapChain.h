#pragma once

namespace lne
{
class SwapChain
{
    public:
    SwapChain(class GfxContext ctx, vk::SurfaceKHR);
    ~SwapChain();


private:
    class vk::SwapchainKHR m_SwapChain;
};
}
