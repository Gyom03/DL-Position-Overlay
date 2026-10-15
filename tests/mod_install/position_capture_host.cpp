// Banc de la capture des coordonnées et du chargement ; rendu réel dans position_dxgi.cpp.
#define POSITION_TEST
#include "../../mods/position/PositionMod.cpp"
#include <limits>
namespace {
int checks=0,failures=0,originals=0;
uint8_t gameObject[0x548]{},logicalLevel[0xc0]{},holder[0x60]{},player[16]{};
void* playerTable[79]{};
position::Vec3 playerPos{245.699f,136.974f,87.679f},cameraPos{245.677f,138.636f,87.7f};
bool haveCamera=true,haveLevel=true;
void Check(bool ok,const char* name) { ++checks; if(!ok) { ++failures; printf("FAIL : %s\n",name); } }
void Write(void* p,const void* value,size_t size) {
    DWORD old,ignored;
    if(!VirtualProtect(p,size,PAGE_READWRITE,&old)) ExitProcess(3);
    memcpy(p,value,size); VirtualProtect(p,size,old,&ignored);
}
void Put(void* p,size_t offset,void* value) { Write(static_cast<uint8_t*>(p)+offset,&value,sizeof(value)); }
void Original(void*) { ++originals; }
void* Level(void*) { return haveLevel ? logicalLevel : nullptr; }
void* Camera(void*) { return haveCamera ? holder : nullptr; }
position::Vec3* PlayerPos(void*,position::Vec3* out) { *out=playerPos; return out; }
position::Vec3* CameraPos(void*,position::Vec3* out) { *out=cameraPos; return out; }
}
int wmain(int argc,wchar_t** argv)
{
    using namespace position;
    if(argc<3) return 2;
    g_game=reinterpret_cast<uint8_t*>(LoadLibraryExW(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES));
    g_engine=reinterpret_cast<uint8_t*>(LoadLibraryExW(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES));
    if(!g_game || !g_engine) return 3;
    auto* slot=reinterpret_cast<void**>(g_game+kUpdateSlot);
    auto* renderSlot=reinterpret_cast<void**>(g_engine+kRenderSlot);
    void* before=*slot; void* beforeRender=*renderSlot;
    if(argc==5) {
        const bool invalid=wcscmp(argv[4],L"invalid")==0;
        if(invalid) { const uint8_t byte=0xcc; Write(g_game+kUpdate,&byte,1); }
        HMODULE mod=LoadLibraryW(argv[3]);
        Check(invalid ? mod==nullptr : mod!=nullptr,"DLL accepts 1.55 and rejects altered code");
        if(mod) {
            MEMORY_BASIC_INFORMATION info{}; VirtualQuery(*slot,&info,sizeof(info));
            Check(*slot!=before && info.AllocationBase==mod,"Sampling hook belongs to the DLL");
            Check(*renderSlot==beforeRender,"Original debug renderer preserved");
            FreeLibrary(mod); Check(GetModuleHandleW(L"position.dll")==mod,"DLL remains loaded");
        } else Check(*slot==before && *renderSlot==beforeRender,"Rejection without changes");
    } else {
        Check(Verify155(),"Verified 1.55 signatures");
        for(uintptr_t offset:{kUpdate,kLevel,kPlayerPosition}) {
            const uint8_t saved=g_game[offset],damaged=saved^1;
            Write(g_game+offset,&damaged,1); Check(!Install() && *slot==before,"Altered game signature rejected");
            Write(g_game+offset,&saved,1);
        }
        for(uintptr_t offset:{kActiveCamera,kCameraPosition,kRender}) {
            const uint8_t saved=g_engine[offset],damaged=saved^1;
            Write(g_engine+offset,&damaged,1); Check(!Install() && *slot==before,"Altered engine signature rejected");
            Write(g_engine+offset,&saved,1);
        }
        void* conflict=reinterpret_cast<void*>(&Original);
        Write(slot,&conflict,8); Check(!Install() && *slot==conflict,"Conflicting sampling hook rejected"); Write(slot,&before,8);
        Write(renderSlot,&conflict,8); Check(!Install() && *slot==before && *renderSlot==conflict,"Unknown render hook rejected"); Write(renderSlot,&beforeRender,8);
        Check(Install() && g_original==reinterpret_cast<UpdateFn>(before) && *slot==reinterpret_cast<void*>(&ModUpdate) && *renderSlot==beforeRender,"Sampling installed and original preserved");
        g_original=&Original; g_getLevel=&Level; g_getCamera=&Camera; g_cameraPosition=&CameraPos;
        Put(g_game,kGameSingleton,gameObject); Put(gameObject,0x540,logicalLevel); Put(logicalLevel,0xb8,holder);
        Put(holder,0x50,player); Put(holder,0x58,player); playerTable[78]=reinterpret_cast<void*>(&PlayerPos); Put(player,0,playerTable);
        Capture();
        Check(g_sample.hasPlayer && g_sample.hasCamera && g_sample.player.x==playerPos.x && g_sample.camera.y==cameraPos.y,"Distinct player and camera coordinates sampled");
        const auto captured=g_sample.player;
        playerPos.x=-9000;
        Check(g_sample.player.x==captured.x,"Snapshot independent from game objects"); playerPos.x=captured.x;
        wchar_t line[160]; Format(line,L"Player",&g_sample.player);
        Check(wcsstr(line,L"245.699") && wcsstr(line,L"Player"),"Text labels and decimal formatting");
        Put(holder,0x50,nullptr); Capture(); Check(!g_sample.hasPlayer && g_sample.hasCamera,"Missing player"); Put(holder,0x50,player);
        haveCamera=false; Capture(); Check(!g_sample.hasCamera && g_sample.hasPlayer,"Missing camera"); haveCamera=true;
        haveLevel=false; Capture(); Check(!g_sample.hasCamera,"Missing level"); haveLevel=true;
        playerPos.x=std::numeric_limits<float>::quiet_NaN(); Capture(); Check(!g_sample.hasPlayer,"NaN unavailable");
        playerPos.x=std::numeric_limits<float>::infinity(); Capture(); Check(!g_sample.hasPlayer,"Infinity unavailable"); playerPos.x=captured.x;
        Put(g_game,kGameSingleton,nullptr); Capture(); Check(!g_sample.visible,"Missing GameDI hides the overlay"); Put(g_game,kGameSingleton,gameObject);
        g_down=false; g_visible=true;
        Check(Toggle(true,true) && !g_visible,"F8 hides the overlay"); Check(!Toggle(true,true),"Held F8 does not repeat");
        Check(!Toggle(true,false),"F8 released"); Check(Toggle(true,true) && g_visible,"F8 shows the overlay");
        Toggle(false,false); Check(!Toggle(false,true) && g_visible,"F8 outside the game ignored"); Check(!Toggle(true,true),"Focus regained without toggling");
        g_visible=false; ModUpdate(nullptr); Check(originals==1 && !g_sample.visible,"Original called when hidden");
        g_visible=true; ModUpdate(nullptr); Check(originals==2 && g_sample.hasPlayer && g_sample.hasCamera,"Original called and latest sample captured");
        Put(g_game,kGameSingleton,reinterpret_cast<void*>(1)); ModUpdate(nullptr);
        Check(originals==3 && g_failed && !g_sample.visible,"Read exception contained");
        Put(g_game,kGameSingleton,gameObject); ModUpdate(nullptr); Check(originals==4 && !g_sample.visible,"Original preserved after exception");
    }
    printf("RESULT capture: %s (%d checks, %d failures)\n",failures ? "FAIL" : "PASS",checks,failures);
    return failures ? 1 : 0;
}
