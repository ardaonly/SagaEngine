/// @file DiligentFactoryInit.inl
/// @brief API-specific device/swapchain creation helpers, included from DiligentRenderBackend.cpp.
///
/// This file lives inside the anonymous namespace of the main .cpp.
/// It must NOT be compiled as a standalone translation unit.

// ─── Swapchain descriptor conversion ─────────────────────────────────

Diligent::SwapChainDesc MakeSwapChainDesc(const SwapchainDesc& desc)
{
    Diligent::SwapChainDesc out{};
    out.Width             = desc.width;
    out.Height            = desc.height;
    out.ColorBufferFormat = desc.hdr ? Diligent::TEX_FORMAT_RGBA16_FLOAT
                                     : Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
    out.DepthBufferFormat = Diligent::TEX_FORMAT_D32_FLOAT;
    out.BufferCount       = 2;
    return out;
}

#if PLATFORM_WIN32 || defined(_WIN32)
Diligent::Win32NativeWindow AsWin32NativeWindow(void* nativeWindow)
{
    Diligent::Win32NativeWindow w{};
    w.hWnd = nativeWindow;
    return w;
}
#endif

// ─── API-specific init helpers ───────────────────────────────────────

#if D3D12_SUPPORTED
bool InitD3D12(const SwapchainDesc& desc, const DiligentBackendConfig& cfg,
               Diligent::RefCntAutoPtr<Diligent::IRenderDevice>& outDevice,
               Diligent::RefCntAutoPtr<Diligent::IDeviceContext>& outContext,
               Diligent::RefCntAutoPtr<Diligent::ISwapChain>& outSwap)
{
#   if ENGINE_DLL
    auto getFactoryFn = Diligent::LoadGraphicsEngineD3D12();
    if (!getFactoryFn) return false;
    auto* factory = getFactoryFn();
#   else
    auto* factory = Diligent::GetEngineFactoryD3D12();
#   endif
    if (!factory) return false;
    Diligent::EngineD3D12CreateInfo ci;
    ci.EnableValidation = cfg.enableValidation;
    factory->CreateDeviceAndContextsD3D12(ci, &outDevice, &outContext);
    if (!outDevice || !outContext) return false;
    const auto scDesc = MakeSwapChainDesc(desc);
#   if PLATFORM_WIN32 || defined(_WIN32)
    factory->CreateSwapChainD3D12(outDevice, outContext, scDesc,
                                  Diligent::FullScreenModeDesc{},
                                  AsWin32NativeWindow(desc.nativeWindow), &outSwap);
#   endif
    return outSwap != nullptr;
}
#endif

#if VULKAN_SUPPORTED
bool InitVulkan(const SwapchainDesc& desc, const DiligentBackendConfig& cfg,
                Diligent::RefCntAutoPtr<Diligent::IRenderDevice>& outDevice,
                Diligent::RefCntAutoPtr<Diligent::IDeviceContext>& outContext,
                Diligent::RefCntAutoPtr<Diligent::ISwapChain>& outSwap)
{
#   if ENGINE_DLL
    auto getFactoryFn = Diligent::LoadGraphicsEngineVk();
    if (!getFactoryFn) return false;
    auto* factory = getFactoryFn();
#   else
    auto* factory = Diligent::GetEngineFactoryVk();
#   endif
    if (!factory) return false;
    Diligent::EngineVkCreateInfo ci;
    ci.EnableValidation = cfg.enableValidation;
    factory->CreateDeviceAndContextsVk(ci, &outDevice, &outContext);
    if (!outDevice || !outContext) return false;
    const auto scDesc = MakeSwapChainDesc(desc);
#   if PLATFORM_WIN32 || defined(_WIN32)
    factory->CreateSwapChainVk(outDevice, outContext, scDesc,
                               AsWin32NativeWindow(desc.nativeWindow), &outSwap);
#   else
    if (!desc.nativeWindow) return false;
    factory->CreateSwapChainVk(outDevice, outContext, scDesc,
                               *reinterpret_cast<Diligent::NativeWindow*>(desc.nativeWindow),
                               &outSwap);
#   endif
    return outSwap != nullptr;
}
#endif

#if D3D11_SUPPORTED
bool InitD3D11(const SwapchainDesc& desc, const DiligentBackendConfig& cfg,
               Diligent::RefCntAutoPtr<Diligent::IRenderDevice>& outDevice,
               Diligent::RefCntAutoPtr<Diligent::IDeviceContext>& outContext,
               Diligent::RefCntAutoPtr<Diligent::ISwapChain>& outSwap)
{
#   if ENGINE_DLL
    auto getFactoryFn = Diligent::LoadGraphicsEngineD3D11();
    if (!getFactoryFn) return false;
    auto* factory = getFactoryFn();
#   else
    auto* factory = Diligent::GetEngineFactoryD3D11();
#   endif
    if (!factory) return false;
    Diligent::EngineD3D11CreateInfo ci;
    ci.EnableValidation = cfg.enableValidation;
    factory->CreateDeviceAndContextsD3D11(ci, &outDevice, &outContext);
    if (!outDevice || !outContext) return false;
    const auto scDesc = MakeSwapChainDesc(desc);
#   if PLATFORM_WIN32 || defined(_WIN32)
    factory->CreateSwapChainD3D11(outDevice, outContext, scDesc,
                                  Diligent::FullScreenModeDesc{},
                                  AsWin32NativeWindow(desc.nativeWindow), &outSwap);
#   endif
    return outSwap != nullptr;
}
#endif

#if GL_SUPPORTED
bool InitOpenGL(const SwapchainDesc& desc, const DiligentBackendConfig& /*cfg*/,
                Diligent::RefCntAutoPtr<Diligent::IRenderDevice>& outDevice,
                Diligent::RefCntAutoPtr<Diligent::IDeviceContext>& outContext,
                Diligent::RefCntAutoPtr<Diligent::ISwapChain>& outSwap)
{
#   if ENGINE_DLL
    auto getFactoryFn = Diligent::LoadGraphicsEngineOpenGL();
    if (!getFactoryFn) return false;
    auto* factory = getFactoryFn();
#   else
    auto* factory = Diligent::GetEngineFactoryOpenGL();
#   endif
    if (!factory) return false;
    const auto scDesc = MakeSwapChainDesc(desc);
    Diligent::EngineGLCreateInfo ci;
#   if PLATFORM_WIN32 || defined(_WIN32)
    ci.Window.hWnd = desc.nativeWindow;
#   endif
    factory->CreateDeviceAndSwapChainGL(ci, &outDevice, &outContext, scDesc, &outSwap);
    return outDevice && outContext && outSwap;
}
#endif

// ─── Unified API selection ───────────────────────────────────────────

DiligentBackendAPI TryInitAPI(
    DiligentBackendAPI preferred, const SwapchainDesc& desc,
    const DiligentBackendConfig& cfg,
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice>& outDevice,
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext>& outContext,
    Diligent::RefCntAutoPtr<Diligent::ISwapChain>& outSwap)
{
    // Log which APIs are compiled in for diagnostics.
    LogInfo("Compiled API support:"
#if D3D12_SUPPORTED
            " D3D12"
#endif
#if VULKAN_SUPPORTED
            " Vulkan"
#endif
#if D3D11_SUPPORTED
            " D3D11"
#endif
#if GL_SUPPORTED
            " OpenGL"
#endif
    );

    auto tryOne = [&](DiligentBackendAPI api) -> bool
    {
        outDevice.Release(); outContext.Release(); outSwap.Release();
        switch (api)
        {
#if D3D12_SUPPORTED
            case DiligentBackendAPI::kD3D12:  return InitD3D12(desc, cfg, outDevice, outContext, outSwap);
#endif
#if VULKAN_SUPPORTED
            case DiligentBackendAPI::kVulkan: return InitVulkan(desc, cfg, outDevice, outContext, outSwap);
#endif
#if D3D11_SUPPORTED
            case DiligentBackendAPI::kD3D11:  return InitD3D11(desc, cfg, outDevice, outContext, outSwap);
#endif
#if GL_SUPPORTED
            case DiligentBackendAPI::kOpenGL: return InitOpenGL(desc, cfg, outDevice, outContext, outSwap);
#endif
            default: return false;
        }
    };

    if (preferred != DiligentBackendAPI::kAuto)
        return tryOne(preferred) ? preferred : DiligentBackendAPI::kAuto;

    constexpr DiligentBackendAPI kOrder[] = {
        DiligentBackendAPI::kD3D12, DiligentBackendAPI::kVulkan,
        DiligentBackendAPI::kD3D11, DiligentBackendAPI::kOpenGL,
    };
    for (auto api : kOrder)
        if (tryOne(api)) return api;

    return DiligentBackendAPI::kAuto;
}
