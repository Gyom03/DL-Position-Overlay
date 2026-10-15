// Overlay de coordonnées pour Dying Light Steam 1.55, indépendant de coop6.
// Repères contrôlés dans bin/ et out/*.asm ; détails : README.md.
#define NOMINMAX
#include <windows.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>

namespace position {
struct Vec3 { float x, y, z; };
struct Vec4 { float x, y, z, w; };
using UpdateFn = void (*)(void*);
using GetObjectFn = void* (*)(void*);
using GetPositionFn = Vec3* (*)(void*, Vec3*);

constexpr uintptr_t kUpdate = 0x3479b0;
constexpr uintptr_t kUpdateSlot = 0x1480c00; // CameraManagerDI vftable + 129*8
constexpr uintptr_t kRenderSlot = 0x8bd0d0; // CRenderer vftable + 108*8
constexpr uintptr_t kRender = 0x735b10;
constexpr uintptr_t kGameSingleton = 0x1c16348;
constexpr uintptr_t kRendererSingleton = 0xa40248;
constexpr uintptr_t kPlayerPosition = 0x191c60;
constexpr uintptr_t kLevel = 0x443460;
constexpr uintptr_t kActiveCamera = 0x29c1a0;
constexpr uintptr_t kCameraPosition = 0x12ed40;

uint8_t* g_game = nullptr;
uint8_t* g_engine = nullptr;
UpdateFn g_original = nullptr;
GetObjectFn g_getLevel = nullptr, g_getCamera = nullptr;
GetPositionFn g_cameraPosition = nullptr;
bool g_visible = true, g_down = false, g_failed = false, g_firstSample = true;
volatile LONG g_renderFailed = 0;
struct Sample {
    Vec3 player{}, camera{};
    bool hasPlayer = false, hasCamera = false, visible = true;
};
SRWLOCK g_sampleLock = SRWLOCK_INIT;
Sample g_sample;
wchar_t g_logPath[MAX_PATH]{};

void Log(const char* message)
{
    FILE* file = nullptr;
    if (g_logPath[0] && _wfopen_s(&file, g_logPath, L"ab") == 0) {
        SYSTEMTIME now{};
        GetLocalTime(&now);
        fprintf(file, "%04u-%02u-%02u %02u:%02u:%02u %s\r\n", now.wYear, now.wMonth, now.wDay,
                now.wHour, now.wMinute, now.wSecond, message);
        fclose(file);
    }
}

template<class T> T Read(const void* object, size_t offset)
{
    T value;
    memcpy(&value, static_cast<const uint8_t*>(object) + offset, sizeof(value));
    return value;
}

template<class T> T Virtual(void* object, size_t offset)
{
    return Read<T>(Read<void*>(object, 0), offset);
}

bool Finite(const Vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

// Même chemin que GatherPlace::Update. Aucun pointeur conservé entre deux images.
void* LocalPlayer(void* game)
{
    if (!game) return nullptr;
    void* level = Read<void*>(game, 0x540);
    void* holder = level ? Read<void*>(level, 0xb8) : nullptr;
    return holder && Read<void*>(holder, 0x50) ? Read<void*>(holder, 0x58) : nullptr;
}

bool Toggle(bool focused, bool down)
{
    const bool changed = focused && down && !g_down;
    if (changed) g_visible = !g_visible;
    // Une touche enfoncée dans une autre application ne bascule pas à la reprise du focus.
    g_down = down;
    return changed;
}

void Format(wchar_t (&out)[160], const wchar_t* name, const Vec3* pos)
{
    if (pos && Finite(*pos))
        swprintf_s(out, L"%s   X % .3f   Y % .3f   Z % .3f", name, pos->x, pos->y, pos->z);
    else
        swprintf_s(out, L"%s   --", name);
}

// Les objets du jeu sont lus uniquement pendant la mise à jour de la caméra.
void Capture()
{
    Sample sample;
    sample.visible = g_visible && !g_failed && InterlockedCompareExchange(&g_renderFailed, 0, 0) == 0;
    void* game = Read<void*>(g_game, kGameSingleton);
    if (!game || !sample.visible) {
        sample.visible = false;
        AcquireSRWLockExclusive(&g_sampleLock);
        g_sample = sample;
        ReleaseSRWLockExclusive(&g_sampleLock);
        return;
    }
    void* player = LocalPlayer(game);
    if (player) {
        // Retour vec3 par tampon caché dans rdx, confirmé dans QuickActionTravelToPlayer::Execute.
        const Vec3* value = Virtual<GetPositionFn>(player, 0x270)(player, &sample.player);
        if (value) { sample.player = *value; sample.hasPlayer = Finite(sample.player); }
    }
    void* level = g_getLevel(game); // ILevel, distinct du LevelDI à GameDI+0x540
    void* camera = level ? g_getCamera(level) : nullptr;
    if (camera) {
        const Vec3* value = g_cameraPosition(camera, &sample.camera);
        if (value) { sample.camera = *value; sample.hasCamera = Finite(sample.camera); }
    }
    AcquireSRWLockExclusive(&g_sampleLock);
    g_sample = sample;
    ReleaseSRWLockExclusive(&g_sampleLock);
    if (g_firstSample && sample.hasPlayer && sample.hasCamera) {
        char line[256];
        sprintf_s(line, "First sample: player=(%.3f, %.3f, %.3f), camera=(%.3f, %.3f, %.3f)",
                  sample.player.x, sample.player.y, sample.player.z, sample.camera.x, sample.camera.y, sample.camera.z);
        Log(line);
        g_firstSample = false;
    }
}

} // namespace position
#include "DirectXOverlay.h"
namespace position {

void ModUpdate(void* self)
{
    // Le comportement d'origine est toujours conservé, overlay visible ou non.
    g_original(self);
    DWORD foregroundPid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);
    if (Toggle(foregroundPid == GetCurrentProcessId(), (GetAsyncKeyState(VK_F8) & 0x8000) != 0))
        Log(g_visible ? "Overlay shown (F8)" : "Overlay hidden (F8)");
    __try { Capture(); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (!g_failed) Log("Cannot read game data: overlay disabled for this session");
        g_failed = true;
        AcquireSRWLockExclusive(&g_sampleLock);
        g_sample.visible = false;
        ReleaseSRWLockExclusive(&g_sampleLock);
    }
#ifndef POSITION_TEST
    directx::Install();
#endif
}

bool Bytes(const uint8_t* base, uintptr_t rva, const uint8_t* expected, size_t size)
{
    return memcmp(base + rva, expected, size) == 0;
}

bool Identity(const uint8_t* base, DWORD stamp, DWORD size)
{
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000) return false;
    const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    return pe->Signature == IMAGE_NT_SIGNATURE && pe->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
           pe->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
           pe->FileHeader.TimeDateStamp == stamp && pe->OptionalHeader.SizeOfImage == size;
}

bool AllowedUpdatePointer()
{
    const auto pointer = Read<uintptr_t>(g_game, kUpdateSlot);
    if (pointer == reinterpret_cast<uintptr_t>(g_game + kUpdate)) return true;
#ifndef POSITION_TEST
    HMODULE oldMod=GetModuleHandleW(L"position.dll");
    MEMORY_BASIC_INFORMATION info{};
    return oldMod && VirtualQuery(reinterpret_cast<void*>(pointer),&info,sizeof(info)) && info.AllocationBase==oldMod;
#else
    return false;
#endif
}

bool AllowedRenderPointer()
{
    const auto pointer=Read<uintptr_t>(g_engine,kRenderSlot);
    if (pointer==reinterpret_cast<uintptr_t>(g_engine+kRender)) return true;
#ifndef POSITION_TEST
    HMODULE oldMod=GetModuleHandleW(L"position.dll");
    MEMORY_BASIC_INFORMATION info{};
    return oldMod && VirtualQuery(reinterpret_cast<void*>(pointer),&info,sizeof(info)) && info.AllocationBase==oldMod;
#else
    return false;
#endif
}

bool Verify155()
{
    if (!g_game || !g_engine || !Identity(g_game, 1783270871, 32915456) ||
        !Identity(g_engine, 1783270525, 14659584)) return false;
    const uint8_t update[]{0x48,0x8b,0x49,0x50,0x48,0x85,0xc9,0x74,0x0d,0x80,0x79,0x58,0x00,
                           0x74,0x07,0x48,0x8b,0x01,0x48,0xff,0x60,0x48,0xc3};
    const uint8_t pos[]{0x4c,0x8b,0x41,0x08,0x41,0x8b,0x40,0x4c,0x89,0x02,0x41,0x8b,0x40,0x5c,
                        0x89,0x42,0x04,0x41,0x8b,0x40,0x6c,0x89,0x42,0x08,0x48,0x8b,0xc2,0xc3};
    const uint8_t level[]{0x48,0x8b,0x89,0x40,0x05,0x00,0x00,0x48,0x85,0xc9,0x74,0x0a,0x48,0x8b,0x01};
    const uint8_t active[]{0x48,0x83,0xec,0x28,0x48,0x8b,0x49,0x08,0xb2,0x01,0x48,0x81,0xc1,0x00,0x08,0x00};
    const uint8_t player[]{0x8b,0x81,0x08,0x08,0x00,0x00,0x89,0x02};
    const uint8_t render[]{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0x01};
    return AllowedUpdatePointer() &&
           AllowedRenderPointer() &&
           Bytes(g_engine, kRender, render, sizeof(render)) &&
           Bytes(g_game, kUpdate, update, sizeof(update)) && Bytes(g_game, kLevel, level, sizeof(level)) &&
           Bytes(g_game, kPlayerPosition, player, sizeof(player)) &&
           Bytes(g_engine, kCameraPosition, pos, sizeof(pos)) &&
           Bytes(g_engine, kActiveCamera, active, sizeof(active));
}

bool Install()
{
    if (!Verify155()) { Log("Unsupported Dying Light 1.55 code or conflicting hook: no changes made"); return false; }
    g_getLevel = reinterpret_cast<GetObjectFn>(g_game + kLevel);
    g_getCamera = reinterpret_cast<GetObjectFn>(g_engine + kActiveCamera);
    g_cameraPosition = reinterpret_cast<GetPositionFn>(g_engine + kCameraPosition);
    auto* slot = reinterpret_cast<void**>(g_game + kUpdateSlot);
    auto* renderSlot = reinterpret_cast<void**>(g_engine + kRenderSlot);
    DWORD old = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
    DWORD renderOld = 0;
    if (!VirtualProtect(renderSlot, sizeof(void*), PAGE_READWRITE, &renderOld)) {
        DWORD ignored;
        VirtualProtect(slot, sizeof(void*), old, &ignored);
        return false;
    }
    g_original = reinterpret_cast<UpdateFn>(g_game + kUpdate);
    void* expectedUpdate = *slot;
    const void* previous = InterlockedCompareExchangePointer(slot, reinterpret_cast<void*>(&ModUpdate),
                                                            expectedUpdate);
    void* expectedRender=*renderSlot;
    const void* previousRender = previous == expectedUpdate ?
        InterlockedCompareExchangePointer(renderSlot, g_engine+kRender, expectedRender) : nullptr;
    if (previous == expectedUpdate && previousRender != expectedRender)
        InterlockedCompareExchangePointer(slot, expectedUpdate, reinterpret_cast<void*>(&ModUpdate));
    DWORD ignored = 0;
    const BOOL restored = VirtualProtect(slot, sizeof(void*), old, &ignored);
    VirtualProtect(renderSlot, sizeof(void*), renderOld, &ignored);
    if (previous != expectedUpdate || previousRender != expectedRender) return false;
    if (!restored) Log("Could not restore the virtual table memory protection");
    Log("Installed v1.0.3: camera sampling and DirectX Present overlay; F8 toggles visibility");
    return true;
}
} // namespace position

#ifndef POSITION_TEST
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(module);
    GetModuleFileNameW(module, position::g_logPath, MAX_PATH);
    wchar_t* slash = wcsrchr(position::g_logPath, L'\\');
    if (slash) wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - position::g_logPath), L"position.log");
    position::g_game = reinterpret_cast<uint8_t*>(GetModuleHandleW(L"gamedll_x64_rwdi.dll"));
    position::g_engine = reinterpret_cast<uint8_t*>(GetModuleHandleW(L"engine_x64_rwdi.dll"));
    if (!position::Install()) return FALSE;
    // Le pointeur de vftable reste valable jusqu'à la fin du processus, même si un chargeur appelle FreeLibrary.
    HMODULE pinned = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                      reinterpret_cast<LPCWSTR>(&position::ModUpdate), &pinned);
    return TRUE;
}
#endif
