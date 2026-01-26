#include <windows.h>
#include <wrl.h>
#include <dxgi1_3.h>
#include <d3d11_2.h>
#include <d2d1_2.h>
#include <dcomp.h>
#include <atomic>
using namespace Microsoft::WRL;

HWND hwnd;
int x{ 100 },y{100},w{ 800 }, h{ 600 };
ComPtr<ID3D11Device>        d3d;
ComPtr<IDXGISwapChain1>     swap;
ComPtr<ID2D1Factory1>       d2dFactory;
ComPtr<ID2D1Device1>        d2dDev;
ComPtr<ID2D1DeviceContext>  dc;
ComPtr<IDCompositionDevice> compDev;
ComPtr<IDCompositionTarget> compTgt;
ComPtr<IDCompositionVisual> compVis;
ComPtr<ID2D1Bitmap1>        targetBmp;
BOOL                        allowTearing = FALSE;


void createTargetBitmap()
{
    ComPtr<IDXGISurface> surf;
    swap->GetBuffer(0, IID_PPV_ARGS(&surf));
    D2D1_BITMAP_PROPERTIES1 bp{};
    bp.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    bp.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED);
    dc->CreateBitmapFromDxgiSurface(surf.Get(), &bp, &targetBmp);
    dc->SetTarget(targetBmp.Get());
}

void initD2D() {
    D2D1_FACTORY_OPTIONS opt{};
#ifdef _DEBUG
    opt.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, opt, d2dFactory.GetAddressOf());
    ComPtr<IDXGIDevice1> dxgiDev;
    auto hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED,
        nullptr, 0, D3D11_SDK_VERSION, d3d.GetAddressOf(), nullptr, nullptr);
    hr = d3d.As(&dxgiDev);
    ComPtr<IDXGIFactory5> fac5;
    CreateDXGIFactory2(0, IID_PPV_ARGS(&fac5));
    fac5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,&allowTearing, sizeof(allowTearing));
    ComPtr<ID2D1Device> d2d;
    hr = d2dFactory->CreateDevice(dxgiDev.Get(), d2d.GetAddressOf());
    d2d.As(&d2dDev);
    hr = d2dDev->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,dc.GetAddressOf());
    RECT rc;
    GetClientRect(hwnd, &rc);
    DXGI_SWAP_CHAIN_DESC1 scd{};
    scd.Width = rc.right;
    scd.Height = rc.bottom;
    scd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.SampleDesc.Count = 1;
    scd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    if (allowTearing) scd.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    hr = fac5->CreateSwapChainForComposition(d3d.Get(), &scd, nullptr, swap.GetAddressOf());
    hr = DCompositionCreateDevice(dxgiDev.Get(), IID_PPV_ARGS(&compDev));
    hr = compDev->CreateTargetForHwnd(hwnd, TRUE, &compTgt);
    hr = compDev->CreateVisual(&compVis);
    hr = compVis->SetContent(swap.Get());
    hr = compTgt->SetRoot(compVis.Get());
    hr = compDev->Commit();
	createTargetBitmap();
}

void paint()
{
    dc->BeginDraw();
    dc->Clear(D2D1::ColorF(0.0f, 0.6f, 0.9f, 0.6f));
    ComPtr<ID2D1SolidColorBrush> br;
    dc->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &br);
    RECT rc;
    GetClientRect(hwnd, &rc);
    dc->DrawLine({ 0,0 }, { (float)rc.right,(float)rc.bottom }, br.Get(), 2.0f);
    dc->EndDraw();
    UINT presentFlags = 0;
    if (allowTearing) presentFlags |= DXGI_PRESENT_ALLOW_TEARING;
    swap->Present(0, presentFlags);
}


LRESULT CALLBACK winProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_SIZE) {
        dc->SetTarget(nullptr);
        targetBmp.Reset();
        RECT rc;
        GetClientRect(hwnd, &rc);
        UINT newW = rc.right - rc.left;
        UINT newH = rc.bottom - rc.top;
        if (newW && newH)
        {
            swap->ResizeBuffers(0, newW, newH, DXGI_FORMAT_UNKNOWN, 0);
            createTargetBitmap();
        }
		paint();
        return 0;
    }
    else if (uMsg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPTSTR lpCmdLine, _In_ int nCmdShow)
{
    WNDCLASS wc = {};
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hInstance = hInstance;
    wc.lpszClassName = L"window";
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = winProc;
    RegisterClass(&wc);
    hwnd = CreateWindowEx(WS_EX_NOREDIRECTIONBITMAP, wc.lpszClassName, L"Sample", WS_OVERLAPPEDWINDOW, x, y, w, h, nullptr, nullptr, hInstance, nullptr);
    initD2D();
	paint();
    ShowWindow(hwnd, SW_SHOW);
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}