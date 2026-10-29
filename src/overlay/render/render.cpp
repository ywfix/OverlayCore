#include "render.h"
#include "../config.h"
#include "../window_manager.h"
#include <cmath>
#include <cstdio>
#include <d3d11.h>
#include <dxgi.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <memory>
#include <string>
#include <wrl/client.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace renderer {
namespace {
using Microsoft::WRL::ComPtr;

struct RenderState {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> device_context;
    ComPtr<IDXGISwapChain> swap_chain;
    ComPtr<ID3D11RenderTargetView> render_target;
    ImGuiContext* context = nullptr;
    UINT pending_width = 0;
    UINT pending_height = 0;
    ULONGLONG retry_at = 0;
    bool device_lost = false;
    HRESULT last_error = S_OK;
};

std::unique_ptr<RenderState> state;

FrameStatus check_result(HRESULT result) {
    if (result == DXGI_STATUS_OCCLUDED)
        return FrameStatus::Occluded;
    if (SUCCEEDED(result))
        return FrameStatus::Ready;
    if (result != state->last_error) {
        char message[96];
        sprintf_s(message, "Overlay renderer error: 0x%08lX\n", static_cast<unsigned long>(result));
        OutputDebugStringA(message);
        state->last_error = result;
    }
    state->device_lost = state->device_lost || result == DXGI_ERROR_DEVICE_REMOVED ||
                         result == DXGI_ERROR_DEVICE_RESET || result == DXGI_ERROR_DEVICE_HUNG ||
                         result == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    return state->device_lost ? FrameStatus::Fatal : FrameStatus::Retry;
}

bool create_device(RenderState& data) {
    constexpr D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT result =
        D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
                          data.device.GetAddressOf(), nullptr, data.device_context.GetAddressOf());
    if (result == DXGI_ERROR_UNSUPPORTED) {
        result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
                                   data.device.ReleaseAndGetAddressOf(), nullptr,
                                   data.device_context.ReleaseAndGetAddressOf());
    }
    return SUCCEEDED(result);
}

HRESULT create_render_target(RenderState& data) {
    ComPtr<ID3D11Texture2D> buffer;
    const HRESULT result = data.swap_chain->GetBuffer(0, IID_PPV_ARGS(buffer.GetAddressOf()));
    if (FAILED(result))
        return result;
    return data.device->CreateRenderTargetView(buffer.Get(), nullptr,
                                               data.render_target.ReleaseAndGetAddressOf());
}

HRESULT resize_buffers(UINT width, UINT height) {
    DXGI_SWAP_CHAIN_DESC current{};
    const HRESULT described = state->swap_chain->GetDesc(&current);
    if (FAILED(described))
        return described;
    if (current.BufferDesc.Width == width && current.BufferDesc.Height == height && state->render_target)
        return S_OK;
    state->device_context->OMSetRenderTargets(0, nullptr, nullptr);
    state->render_target.Reset();
    const HRESULT resized = state->swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    check_result(resized);
    const HRESULT restored = create_render_target(*state);
    check_result(restored);
    return FAILED(resized) ? resized : restored;
}
}

bool initialize(HWND window, float dpi_scale) {
    if (state)
        return false;
    auto data = std::make_unique<RenderState>();
    if (!create_device(*data))
        return false;
    RECT bounds{};
    if (!GetClientRect(window, &bounds))
        return false;
    DXGI_SWAP_CHAIN_DESC description{};
    description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    description.BufferCount = 2;
    description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.BufferDesc.Width = bounds.right > 0 ? static_cast<UINT>(bounds.right) : 1;
    description.BufferDesc.Height = bounds.bottom > 0 ? static_cast<UINT>(bounds.bottom) : 1;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.OutputWindow = window;
    description.SampleDesc.Count = 1;
    description.Windowed = TRUE;

    ComPtr<IDXGIDevice> dxgi_device;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory> factory;
    if (FAILED(data->device.As(&dxgi_device)) || FAILED(dxgi_device->GetAdapter(adapter.GetAddressOf())) ||
        FAILED(adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf()))))
        return false;
    if (FAILED(factory->CreateSwapChain(data->device.Get(), &description, data->swap_chain.GetAddressOf())) ||
        FAILED(create_render_target(*data)))
        return false;
    factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);

    data->context = ImGui::CreateContext();
    if (!ImGui_ImplWin32_Init(window)) {
        ImGui::DestroyContext(data->context);
        return false;
    }
    if (!ImGui_ImplDX11_Init(data->device.Get(), data->device_context.Get())) {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(data->context);
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    state = std::move(data);
    if (set_scale(dpi_scale))
        return true;
    shutdown();
    return false;
}

void shutdown() {
    if (!state)
        return;
    state->device_context->ClearState();
    ImGui::SetCurrentContext(state->context);
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(state->context);
    state->render_target.Reset();
    state->swap_chain.Reset();
    state->device_context->Flush();
    state.reset();
}

bool set_scale(float scale) {
    if (!state)
        return false;
    ImGui::SetCurrentContext(state->context);
    ImGui_ImplDX11_InvalidateDeviceObjects();
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    scale = scale > 0.0f ? scale : 1.0f;
    ImFontConfig config;
    config.PixelSnapH = true;
    config.OversampleH = config.OversampleV = 1;
    config.SizePixels = std::floor(16.0f * scale);
    io.FontDefault = nullptr;
    char directory[MAX_PATH]{};
    const UINT length = GetWindowsDirectoryA(directory, MAX_PATH);
    if (length && length < MAX_PATH) {
        const std::string path = std::string(directory) + "\\Fonts\\verdana.ttf";
        if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            io.FontDefault = io.Fonts->AddFontFromFileTTF(path.c_str(), config.SizePixels, &config,
                                                          io.Fonts->GetGlyphRangesCyrillic());
        }
    }
    if (!io.FontDefault)
        io.FontDefault = io.Fonts->AddFontDefault(&config);
    if (!io.Fonts->Build())
        return false;
    ImGuiStyle style;
    style.ScaleAllSizes(scale);
    ImGui::GetStyle() = style;
    return ImGui_ImplDX11_CreateDeviceObjects();
}

FrameStatus begin_frame() {
    if (!state || state->device_lost)
        return FrameStatus::Fatal;
    const ULONGLONG now = GetTickCount64();
    if (now >= state->retry_at) {
        HRESULT result = S_OK;
        if (state->pending_width && state->pending_height) {
            result = resize_buffers(state->pending_width, state->pending_height);
            if (SUCCEEDED(result))
                state->pending_width = state->pending_height = 0;
        } else if (!state->render_target)
            result = create_render_target(*state);
        if (FAILED(result)) {
            check_result(result);
            state->retry_at = now + 100;
        }
    }
    if (state->device_lost)
        return FrameStatus::Fatal;
    if (!state->render_target)
        return FrameStatus::Retry;
    ImGui::SetCurrentContext(state->context);
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    return FrameStatus::Ready;
}

FrameStatus present() {
    ImGui::Render();
    if (!state || state->device_lost)
        return FrameStatus::Fatal;
    if (!state->render_target)
        return FrameStatus::Retry;
    constexpr float clear_color[4]{};
    state->device_context->OMSetRenderTargets(1, state->render_target.GetAddressOf(), nullptr);
    state->device_context->ClearRenderTargetView(state->render_target.Get(), clear_color);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    return check_result(state->swap_chain->Present(1, 0));
}

void wait_for_vsync() {
    if (state) {
        ComPtr<IDXGIOutput> output;
        if (SUCCEEDED(state->swap_chain->GetContainingOutput(output.GetAddressOf())) &&
            SUCCEEDED(output->WaitForVBlank()))
            return;
    }
    MsgWaitForMultipleObjects(0, nullptr, FALSE, 16, QS_ALLINPUT);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    auto* manager = WindowManager::get_instance();
    if (ImGui::GetCurrentContext()) {
#if OVERLAY_VIRTUAL_CURSOR
        if (manager) {
            auto& cursor = manager->get_cursor();
            if (message == WM_KILLFOCUS)
                cursor.disable();
            if (message == WM_SETFOCUS)
                cursor.mark_rect_dirty();
            if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN ||
                message == WM_XBUTTONDOWN) {
#if OVERLAY_TARGET_MODE
                if (!manager->is_target_focused())
                    manager->focus_target();
#else
                manager->focus_target();
#endif
            }
            if (cursor.is_active()) {
                switch (message) {
                case WM_SETCURSOR:
                    SetCursor(nullptr);
                    return TRUE;
                case WM_MOUSEMOVE:
                    return 0;
                case WM_INPUT:
                    cursor.raw_input(l_param);
                    return DefWindowProcA(window, message, w_param, l_param);
                case WM_LBUTTONDOWN:
                case WM_LBUTTONDBLCLK:
                case WM_LBUTTONUP:
                case WM_RBUTTONDOWN:
                case WM_RBUTTONDBLCLK:
                case WM_RBUTTONUP:
                case WM_MBUTTONDOWN:
                case WM_MBUTTONDBLCLK:
                case WM_MBUTTONUP:
                case WM_XBUTTONDOWN:
                case WM_XBUTTONDBLCLK:
                case WM_XBUTTONUP: {
                    const bool left =
                        message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK || message == WM_LBUTTONUP;
                    const bool right =
                        message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK || message == WM_RBUTTONUP;
                    const bool middle =
                        message == WM_MBUTTONDOWN || message == WM_MBUTTONDBLCLK || message == WM_MBUTTONUP;
                    const int button = left                                      ? 0
                                       : right                                   ? 1
                                       : middle                                  ? 2
                                       : GET_XBUTTON_WPARAM(w_param) == XBUTTON1 ? 3
                                                                                 : 4;
                    const bool down = message != WM_LBUTTONUP && message != WM_RBUTTONUP &&
                                      message != WM_MBUTTONUP && message != WM_XBUTTONUP;
                    ImGui::GetIO().AddMouseButtonEvent(button, down);
                    return left || right || middle ? 0 : TRUE;
                }
                case WM_MOUSEWHEEL:
                    ImGui::GetIO().AddMouseWheelEvent(
                        0.0f, static_cast<float>(GET_WHEEL_DELTA_WPARAM(w_param)) / WHEEL_DELTA);
                    return 0;
                case WM_MOUSEHWHEEL:
                    ImGui::GetIO().AddMouseWheelEvent(
                        -static_cast<float>(GET_WHEEL_DELTA_WPARAM(w_param)) / WHEEL_DELTA, 0.0f);
                    return 0;
                }
            }
        }
#endif
        if (ImGui_ImplWin32_WndProcHandler(window, message, w_param, l_param))
            return TRUE;
    }

    switch (message) {
    case WM_SIZE:
        if (w_param != SIZE_MINIMIZED) {
            if (state) {
                state->pending_width = LOWORD(l_param);
                state->pending_height = HIWORD(l_param);
                state->retry_at = 0;
            }
#if OVERLAY_VIRTUAL_CURSOR
            if (manager)
                manager->get_cursor().update_bounds(LOWORD(l_param), HIWORD(l_param));
#endif
        }
        return 0;
#if OVERLAY_VIRTUAL_CURSOR
    case WM_MOVE:
        if (manager)
            manager->get_cursor().mark_rect_dirty();
        return 0;
#endif
    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED:
        if (manager)
            manager->mark_dpi_dirty();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(window, message, w_param, l_param);
    }
}
}
