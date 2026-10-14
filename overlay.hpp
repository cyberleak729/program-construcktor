// overlay.hpp — полноэкранное click-through окно + D3D11 + ImGui (Win32+DX11 backend).
#pragma once
#include <windows.h>
#include <d3d11.h>
#include <cstdint>
#include <cmath>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "imgui/backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma comment(lib, "d3d11.lib")

class Overlay {
public:
  bool Init(const wchar_t* title) {
    m_W = GetSystemMetrics(SM_CXSCREEN);
    m_H = GetSystemMetrics(SM_CYSCREEN);

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = &Overlay::StaticProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"ImGuiOverlayWC";
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    // WS_EX_TRANSPARENT|LAYERED — click-through; TOPMOST — поверх игры (borderless/windowed)
    m_Hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
                             wc.lpszClassName, title, WS_POPUP,
                             0, 0, m_W, m_H, nullptr, nullptr, wc.hInstance, this);
    if (!m_Hwnd) return false;
    SetLayeredWindowAttributes(m_Hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY); // чёрный = прозрачный

    if (!InitD3D()) return false;
    if (!InitImGui()) return false;
    return true;
  }

  void BeginFrame() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    if (msg.message == WM_QUIT) m_Quit = true;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::GetIO().DisplaySize = ImVec2((float)m_W, (float)m_H);
    ImGui::NewFrame();
  }

  void EndFrame() {
    ImGui::Render();
    float clear[4] = { 0.f, 0.f, 0.f, 0.f }; // alpha 0 -> colorkey делает фон прозрачным
    m_Ctx->OMSetRenderTargets(1, &m_RTV, nullptr);
    m_Ctx->ClearRenderTargetView(m_RTV, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    m_Swap->Present(1, 0);
  }

  int  Width()  const { return m_W; }
  int  Height() const { return m_H; }
  HWND Hwnd()   const { return m_Hwnd; }
  bool Quit()   const { return m_Quit; }
  bool ShowMenu() const { return m_ShowMenu; }
  void ToggleMenu() { m_ShowMenu = !m_ShowMenu; }

  static LRESULT CALLBACK StaticProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    Overlay* self = nullptr;
    if (msg == WM_NCCREATE) {
      auto cs = reinterpret_cast<CREATESTRUCTW*>(l);
      self = static_cast<Overlay*>(cs->lpCreateParams);
      SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
      self = reinterpret_cast<Overlay*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    }
    if (self) return self->Proc(msg, w, l);
    return DefWindowProcW(h, msg, w, l);
  }

private:
  LRESULT Proc(UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
      case WM_DESTROY: PostQuitMessage(0); return 0;
      case WM_KEYDOWN:
        if (w == VK_ESCAPE) DestroyWindow(m_Hwnd);
        if (w == VK_INSERT) ToggleMenu();          // INS — меню ESP (в оверлейном окне клавиши не перехватываются игрой)
        break;
    }
    // click-through окно не получает мышь от игры, но бэкенд вызываем для корректности
    ImGui_ImplWin32_WndProcHandler(m_Hwnd, msg, w, l);
    return DefWindowProcW(m_Hwnd, msg, w, l);
  }

  bool InitD3D() {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_Hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL fl;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        flags, nullptr, 0, D3D11_SDK_VERSION, &sd, &m_Swap, &m_Dev, &fl, &m_Ctx);
    if (FAILED(hr)) return false;

    ID3D11Texture2D* back = nullptr;
    m_Swap->GetBuffer(0, __uuidof(back), (void**)&back);
    hr = m_Dev->CreateRenderTargetView(back, nullptr, &m_RTV);
    back->Release();
    return SUCCEEDED(hr);
  }

  bool InitImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouse; // click-through: мышь не нужна
    ImGui::StyleColorsDark();
    ImGuiStyle& st = ImGui::GetStyle();
    st.Alpha = 0.95f;
    st.WindowRounding = 0.f;
    return ImGui_ImplWin32_Init(m_Hwnd) && ImGui_ImplDX11_Init(m_Dev, m_Ctx);
  }

  int   m_W = 0, m_H = 0;
  bool  m_ShowMenu = false;
  bool  m_Quit = false;
  HWND  m_Hwnd = nullptr;
  IDXGISwapChain*            m_Swap = nullptr;
  ID3D11Device*              m_Dev  = nullptr;
  ID3D11DeviceContext*       m_Ctx  = nullptr;
  ID3D11RenderTargetView*    m_RTV  = nullptr;
};
