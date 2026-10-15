// Rendu réel Direct3D WARP dans une texture synthétique, sans capture du jeu ni du bureau.
#define POSITION_TEST
#include "../../mods/position/PositionMod.cpp"

int wmain()
{
    using namespace position;
    using namespace position::directx;
    int tests=0, failures=0;
    auto check=[&](bool ok,const char* name) { ++tests; if(!ok) { ++failures; printf("FAIL : %s\n",name); } };
    ID3D11Device* device=nullptr; ID3D11DeviceContext* context=nullptr;
    HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context);
    if(FAILED(hr)) { printf("Cannot create WARP device : %08lx\n",hr); return 2; }
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=960; desc.Height=240; desc.MipLevels=1; desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_DEFAULT; desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ID3D11Texture2D *target=nullptr,*staging=nullptr;
    ID3D11RenderTargetView* view=nullptr;
    hr=device->CreateTexture2D(&desc,nullptr,&target);
    if(SUCCEEDED(hr)) hr=device->CreateRenderTargetView(target,nullptr,&view);
    if(FAILED(hr)) return 3;
    const float clear[]{0,0,1,1}; context->ClearRenderTargetView(view,clear);
    context->OMSetRenderTargets(1,&view,nullptr);
    const D3D11_VIEWPORT originalViewport{3,5,900,180,0.2f,0.8f};
    context->RSSetViewports(1,&originalViewport);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
    Sample sample;
    sample.player={245.699f,136.974f,87.679f}; sample.camera={245.677f,138.636f,87.700f};
    sample.hasPlayer=sample.hasCamera=sample.visible=true;
    check(Paint(device,context,target,sample),"Shaders created and actual Direct3D draw");
    ID3D11RenderTargetView* restored=nullptr;
    context->OMGetRenderTargets(1,&restored,nullptr);
    check(restored==view,"Render target restored"); Release(restored);
    D3D11_VIEWPORT restoredViewport{}; UINT count=1;
    context->RSGetViewports(&count,&restoredViewport);
    check(count==1 && memcmp(&restoredViewport,&originalViewport,sizeof(originalViewport))==0,"Viewport restored");
    D3D11_PRIMITIVE_TOPOLOGY topology{}; context->IAGetPrimitiveTopology(&topology);
    check(topology==D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,"Topology restored");
    desc.Usage=D3D11_USAGE_STAGING; desc.BindFlags=0; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    hr=device->CreateTexture2D(&desc,nullptr,&staging);
    if(FAILED(hr)) return 4;
    context->CopyResource(staging,target);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr=context->Map(staging,0,D3D11_MAP_READ,0,&mapped);
    if(FAILED(hr)) return 5;
    auto* pixel=static_cast<uint8_t*>(mapped.pData);
    check(pixel[0]==0 && pixel[1]==0 && pixel[2]==255 && pixel[3]==255,"Pixels outside the overlay unchanged");
    auto* panel=pixel+25*mapped.RowPitch+25*4;
    check(panel[0]>0 && panel[1]>0 && panel[2]<200,"Panel pixels rendered into the texture");
    int bright=0,cyan=0;
    for(UINT y=24;y<124;++y) for(UINT x=24;x<784;++x) {
        auto* p=pixel+y*mapped.RowPitch+x*4;
        if(p[0]>180 && p[1]>180 && p[2]>180) ++bright;
        if(p[1]>150 && p[2]>180 && p[0]<150) ++cyan;
    }
    check(bright>300,"White text glyphs rendered");
    check(cyan>100,"Cyan camera glyphs rendered");
    context->Unmap(staging,0);
    context->ClearRenderTargetView(view,clear);
    sample.visible=false;
    check(!Paint(device,context,target,sample),"Hidden overlay does not draw");
    context->CopyResource(staging,target);
    hr=context->Map(staging,0,D3D11_MAP_READ,0,&mapped);
    if(FAILED(hr)) return 6;
    panel=static_cast<uint8_t*>(mapped.pData)+25*mapped.RowPitch+25*4;
    check(panel[0]==0 && panel[1]==0 && panel[2]==255,"Hidden panel pixels unchanged");
    context->Unmap(staging,0);
    ResetResources();
    Release(staging); Release(view); Release(target); Release(context); Release(device);
    printf("RESULT DirectX: %s (%d checks, %d failures; %d white pixels, %d cyan)\n",failures ? "FAIL" : "PASS",tests,failures,bright,cyan);
    return failures ? 1 : 0;
}
