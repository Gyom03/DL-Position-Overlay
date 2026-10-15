// Dessin Direct3D 11 à la présentation : aucun renderer debug du jeu.
// Le bitmap contient exclusivement nos trois lignes, jamais une capture du jeu.
#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

namespace position { namespace directx {
constexpr UINT kWidth = 760, kHeight = 100;
using PresentFn = HRESULT (STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
PresentFn g_present = nullptr;
volatile LONG g_installState = 0;
SRWLOCK g_drawLock = SRWLOCK_INIT;
ID3D11Device* g_device = nullptr;
ID3D11VertexShader* g_vs = nullptr;
ID3D11PixelShader* g_ps = nullptr;
ID3D11Texture2D* g_text = nullptr;
ID3D11ShaderResourceView* g_srv = nullptr;
ID3D11SamplerState* g_sampler = nullptr;
ID3D11Buffer* g_constants = nullptr;
ID3D11BlendState* g_blend = nullptr;
ID3D11RasterizerState* g_raster = nullptr;
ID3D11DepthStencilState* g_depth = nullptr;
HDC g_dc = nullptr;
HBITMAP g_bitmap = nullptr;
HFONT g_font = nullptr;
HGDIOBJ g_previousBitmap = nullptr, g_previousFont = nullptr;
uint32_t* g_pixels = nullptr;
ULONGLONG g_lastText = 0;
bool g_firstPresent = true;

template<class T> void Release(T*& object) { if (object) { object->Release(); object = nullptr; } }
void ResetResources()
{
    Release(g_vs); Release(g_ps); Release(g_text); Release(g_srv); Release(g_sampler);
    Release(g_constants); Release(g_blend); Release(g_raster); Release(g_depth); Release(g_device);
    if (g_dc) {
        if (g_previousFont) SelectObject(g_dc, g_previousFont);
        if (g_previousBitmap) SelectObject(g_dc, g_previousBitmap);
        DeleteDC(g_dc); g_dc = nullptr;
    }
    if (g_font) { DeleteObject(g_font); g_font = nullptr; }
    if (g_bitmap) { DeleteObject(g_bitmap); g_bitmap = nullptr; }
    g_pixels = nullptr;
}

void LogCompileError(ID3DBlob* error)
{
    if(error) Log(static_cast<const char*>(error->GetBufferPointer()));
}

bool Resources(ID3D11Device* device)
{
    if (g_device == device && g_srv) return true;
    ResetResources();
    g_device = device; g_device->AddRef();
    const char shader[] =
        "cbuffer Panel : register(b0) { float4 screen; };"
        "struct V { float4 p:SV_POSITION; float2 uv:TEXCOORD0; };"
        "V VS(uint i:SV_VertexID) { float2 points[6] = {float2(0,0),float2(1,0),float2(0,1),"
        "float2(0,1),float2(1,0),float2(1,1)}; V o; o.uv=points[i];"
        "float2 pixel=float2(24,24)+o.uv*screen.zw;"
        "o.p=float4(pixel.x/screen.x*2-1,1-pixel.y/screen.y*2,0,1); return o; }"
        "Texture2D image:register(t0); SamplerState linearSampler:register(s0);"
        "float4 PS(V input):SV_TARGET { return image.Sample(linearSampler,input.uv); }";
    ID3DBlob *vs = nullptr, *ps = nullptr, *errors = nullptr;
    HRESULT hr = D3DCompile(shader, sizeof(shader)-1, nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vs, &errors);
    if(FAILED(hr)) LogCompileError(errors);
    Release(errors);
    if (SUCCEEDED(hr)) hr = D3DCompile(shader, sizeof(shader)-1, nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &ps, &errors);
    if(FAILED(hr)) LogCompileError(errors);
    Release(errors);
    if (SUCCEEDED(hr)) hr = device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &g_vs);
    if (SUCCEEDED(hr)) hr = device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &g_ps);
    Release(vs); Release(ps);
    D3D11_TEXTURE2D_DESC texture{};
    texture.Width=kWidth; texture.Height=kHeight; texture.MipLevels=1; texture.ArraySize=1;
    texture.Format=DXGI_FORMAT_B8G8R8A8_UNORM; texture.SampleDesc.Count=1;
    texture.Usage=D3D11_USAGE_DEFAULT; texture.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    if (SUCCEEDED(hr)) hr = device->CreateTexture2D(&texture, nullptr, &g_text);
    if (SUCCEEDED(hr)) hr = device->CreateShaderResourceView(g_text, nullptr, &g_srv);
    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD=D3D11_FLOAT32_MAX;
    if (SUCCEEDED(hr)) hr = device->CreateSamplerState(&sampler, &g_sampler);
    D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth=16; buffer.Usage=D3D11_USAGE_DEFAULT; buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    if (SUCCEEDED(hr)) hr = device->CreateBuffer(&buffer, nullptr, &g_constants);
    D3D11_BLEND_DESC blend{};
    auto& rt = blend.RenderTarget[0];
    rt.BlendEnable=TRUE; rt.SrcBlend=D3D11_BLEND_SRC_ALPHA; rt.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOp=D3D11_BLEND_OP_ADD; rt.SrcBlendAlpha=D3D11_BLEND_ONE;
    rt.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA; rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    if (SUCCEEDED(hr)) hr = device->CreateBlendState(&blend, &g_blend);
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode=D3D11_FILL_SOLID; raster.CullMode=D3D11_CULL_NONE; raster.DepthClipEnable=TRUE;
    if (SUCCEEDED(hr)) hr = device->CreateRasterizerState(&raster, &g_raster);
    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable=FALSE; depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc=D3D11_COMPARISON_ALWAYS;
    if (SUCCEEDED(hr)) hr = device->CreateDepthStencilState(&depth, &g_depth);
    if (FAILED(hr)) { Log("DirectX: failed to create overlay resources"); ResetResources(); return false; }
    g_dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=kWidth;
    info.bmiHeader.biHeight=-static_cast<LONG>(kHeight); info.bmiHeader.biPlanes=1;
    info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    g_bitmap = CreateDIBSection(g_dc, &info, DIB_RGB_COLORS, reinterpret_cast<void**>(&g_pixels), nullptr, 0);
    g_font = CreateFontW(-19, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FF_MODERN, L"Consolas");
    if (!g_dc || !g_bitmap || !g_font) { ResetResources(); return false; }
    g_previousBitmap=SelectObject(g_dc,g_bitmap); g_previousFont=SelectObject(g_dc,g_font);
    SetBkMode(g_dc, TRANSPARENT);
    g_lastText=0;
    return true;
}

void TextBitmap(ID3D11DeviceContext* context, const Sample& sample)
{
    for (UINT i=0; i<kWidth*kHeight; ++i) g_pixels[i]=0x00141414;
    SetTextColor(g_dc, RGB(255,255,255));
    const wchar_t* title=L"World coordinates  |  F8: show / hide";
    TextOutW(g_dc, 10, 8, title, static_cast<int>(wcslen(title)));
    wchar_t line[160];
    Format(line, L"Player", sample.hasPlayer ? &sample.player : nullptr);
    TextOutW(g_dc, 10, 36, line, static_cast<int>(wcslen(line)));
    SetTextColor(g_dc, RGB(105,225,255));
    Format(line, L"Camera", sample.hasCamera ? &sample.camera : nullptr);
    TextOutW(g_dc, 10, 64, line, static_cast<int>(wcslen(line)));
    GdiFlush();
    for (UINT i=0; i<kWidth*kHeight; ++i) g_pixels[i] |= 0xe8000000;
    context->UpdateSubresource(g_text, 0, nullptr, g_pixels, kWidth*4, 0);
}

// Sauvegarde/restaure tous les états D3D11 que notre dessin remplace.
struct SavedState {
    ID3D11RenderTargetView* targets[8]{}; ID3D11DepthStencilView* depthView=nullptr;
    ID3D11BlendState* blend=nullptr; FLOAT factor[4]{}; UINT mask=0;
    ID3D11DepthStencilState* depth=nullptr; UINT stencil=0;
    ID3D11RasterizerState* raster=nullptr;
    D3D11_VIEWPORT viewports[16]{}; UINT viewportCount=16;
    ID3D11InputLayout* layout=nullptr; D3D11_PRIMITIVE_TOPOLOGY topology{};
    ID3D11VertexShader* vs=nullptr; ID3D11PixelShader* ps=nullptr;
    ID3D11GeometryShader* gs=nullptr; ID3D11HullShader* hs=nullptr; ID3D11DomainShader* ds=nullptr;
    ID3D11ClassInstance *vsClasses[256]{}, *psClasses[256]{}, *gsClasses[256]{}, *hsClasses[256]{}, *dsClasses[256]{};
    UINT nvs=256,nps=256,ngs=256,nhs=256,nds=256;
    ID3D11Buffer* constant=nullptr; ID3D11ShaderResourceView* srv=nullptr; ID3D11SamplerState* sampler=nullptr;
    explicit SavedState(ID3D11DeviceContext* c) {
        c->OMGetRenderTargets(8,targets,&depthView); c->OMGetBlendState(&blend,factor,&mask);
        c->OMGetDepthStencilState(&depth,&stencil); c->RSGetState(&raster);
        c->RSGetViewports(&viewportCount,viewports); c->IAGetInputLayout(&layout); c->IAGetPrimitiveTopology(&topology);
        c->VSGetShader(&vs,vsClasses,&nvs); c->PSGetShader(&ps,psClasses,&nps); c->GSGetShader(&gs,gsClasses,&ngs);
        c->HSGetShader(&hs,hsClasses,&nhs); c->DSGetShader(&ds,dsClasses,&nds);
        c->VSGetConstantBuffers(0,1,&constant); c->PSGetShaderResources(0,1,&srv); c->PSGetSamplers(0,1,&sampler);
    }
    void Restore(ID3D11DeviceContext* c) {
        c->OMSetRenderTargets(8,targets,depthView); c->OMSetBlendState(blend,factor,mask);
        c->OMSetDepthStencilState(depth,stencil); c->RSSetState(raster); c->RSSetViewports(viewportCount,viewports);
        c->IASetInputLayout(layout); c->IASetPrimitiveTopology(topology);
        c->VSSetShader(vs,vsClasses,nvs); c->PSSetShader(ps,psClasses,nps); c->GSSetShader(gs,gsClasses,ngs);
        c->HSSetShader(hs,hsClasses,nhs); c->DSSetShader(ds,dsClasses,nds);
        c->VSSetConstantBuffers(0,1,&constant); c->PSSetShaderResources(0,1,&srv); c->PSSetSamplers(0,1,&sampler);
        for(auto& t:targets) Release(t); Release(depthView); Release(blend); Release(depth); Release(raster); Release(layout);
        Release(vs); Release(ps); Release(gs); Release(hs); Release(ds);
        for(UINT i=0;i<nvs;++i) Release(vsClasses[i]); for(UINT i=0;i<nps;++i) Release(psClasses[i]);
        for(UINT i=0;i<ngs;++i) Release(gsClasses[i]); for(UINT i=0;i<nhs;++i) Release(hsClasses[i]);
        for(UINT i=0;i<nds;++i) Release(dsClasses[i]); Release(constant); Release(srv); Release(sampler);
    }
};

bool Paint(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* target, const Sample& sample)
{
    if (!sample.visible || !Resources(device)) return false;
    ID3D11RenderTargetView* view=nullptr;
    if (FAILED(device->CreateRenderTargetView(target,nullptr,&view))) return false;
    D3D11_TEXTURE2D_DESC desc{}; target->GetDesc(&desc);
    const ULONGLONG now=GetTickCount64();
    if (g_lastText==0 || now-g_lastText>=33) { TextBitmap(context,sample); g_lastText=now; }
    const float dimensions[]{static_cast<float>(desc.Width),static_cast<float>(desc.Height),static_cast<float>(kWidth),static_cast<float>(kHeight)};
    context->UpdateSubresource(g_constants,0,nullptr,dimensions,0,0);
    SavedState saved(context);
    context->OMSetRenderTargets(1,&view,nullptr);
    const FLOAT factor[]{0,0,0,0}; context->OMSetBlendState(g_blend,factor,0xffffffff);
    context->OMSetDepthStencilState(g_depth,0);
    const D3D11_VIEWPORT viewport{0,0,static_cast<float>(desc.Width),static_cast<float>(desc.Height),0,1};
    context->RSSetViewports(1,&viewport); context->RSSetState(g_raster);
    context->IASetInputLayout(nullptr); context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(g_vs,nullptr,0); context->PSSetShader(g_ps,nullptr,0);
    context->GSSetShader(nullptr,nullptr,0); context->HSSetShader(nullptr,nullptr,0); context->DSSetShader(nullptr,nullptr,0);
    context->VSSetConstantBuffers(0,1,&g_constants); context->PSSetShaderResources(0,1,&g_srv);
    context->PSSetSamplers(0,1,&g_sampler); context->Draw(6,0);
    saved.Restore(context); Release(view);
    return true;
}

HRESULT STDMETHODCALLTYPE ModPresent(IDXGISwapChain* swap, UINT sync, UINT flags)
{
    if (!(flags & DXGI_PRESENT_TEST)) {
        Sample sample;
        AcquireSRWLockShared(&g_sampleLock); sample=g_sample; ReleaseSRWLockShared(&g_sampleLock);
        if (sample.visible) {
            AcquireSRWLockExclusive(&g_drawLock);
            ID3D11Device* device=nullptr; ID3D11DeviceContext* context=nullptr; ID3D11Texture2D* backbuffer=nullptr;
            if (SUCCEEDED(swap->GetDevice(__uuidof(ID3D11Device),reinterpret_cast<void**>(&device)))) {
                device->GetImmediateContext(&context);
                if (SUCCEEDED(swap->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&backbuffer)))) {
                    if (Paint(device,context,backbuffer,sample) && g_firstPresent) {
                        Log("DirectX: overlay drawn into the backbuffer before Present"); g_firstPresent=false;
                    }
                }
            }
            Release(backbuffer); Release(context); Release(device);
            ReleaseSRWLockExclusive(&g_drawLock);
        }
    }
    return g_present(swap,sync,flags);
}

bool Install()
{
    if (InterlockedCompareExchange(&g_installState,1,0)!=0) return g_installState==2;
    const wchar_t* className=L"DL_Position_DXGI_Probe";
    WNDCLASSW wc{}; wc.lpfnWndProc=DefWindowProcW; wc.hInstance=GetModuleHandleW(nullptr); wc.lpszClassName=className;
    RegisterClassW(&wc);
    HWND window=CreateWindowExW(0,className,L"",WS_POPUP,0,0,2,2,nullptr,nullptr,wc.hInstance,nullptr);
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width=2; desc.BufferDesc.Height=2; desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1; desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount=1; desc.OutputWindow=window; desc.Windowed=TRUE; desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    IDXGISwapChain* swap=nullptr; ID3D11Device* device=nullptr; ID3D11DeviceContext* context=nullptr;
    HRESULT hr=window ? D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,
                                                   D3D11_SDK_VERSION,&desc,&swap,&device,nullptr,&context) : E_FAIL;
    bool installed=false;
    if (SUCCEEDED(hr)) {
        auto* slot=(*reinterpret_cast<void***>(swap))+8;
        DWORD old=0,ignored=0;
        if (VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&old)) {
            g_present=reinterpret_cast<PresentFn>(*slot);
            installed=InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(&ModPresent),reinterpret_cast<void*>(g_present))==reinterpret_cast<void*>(g_present);
            VirtualProtect(slot,sizeof(void*),old,&ignored);
        }
    }
    Release(context); Release(device); Release(swap);
    if(window) DestroyWindow(window); UnregisterClassW(className,wc.hInstance);
    InterlockedExchange(&g_installState,installed ? 2 : -1);
    Log(installed ? "DirectX: Present hook installed" : "DirectX: failed to install the Present hook");
    return installed;
}
}} // namespace position::directx
