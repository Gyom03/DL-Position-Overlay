// Chargement indépendant de CustomPaks, avant les scripts ; aucun code de DLML repris.
#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace position { namespace custompaks {
using InitializeFn = void (*)(const char*, uint8_t);
using AddSourceFn = bool (*)(const char*, int);
InitializeFn g_initialize = nullptr;
AddSourceFn g_add = nullptr;
std::wstring g_folder;
std::vector<std::string> g_mounted;
uint8_t* g_entry = nullptr;
uint8_t g_patch[16]{};

bool SetFolder()
{
    wchar_t path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (!length || length >= MAX_PATH) return false;
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) return false;
    slash[1] = 0;
    g_folder = std::wstring(path) + L"CustomPaks";
    return true;
}

// L'API du jeu reçoit des chemins ANSI. Refuser une conversion avec perte.
bool GamePath(const std::wstring& path, std::string& result)
{
    if (path.size() >= MAX_PATH) return false;
    BOOL substituted = FALSE;
    char buffer[MAX_PATH]{};
    const UINT page = GetACP();
    const int size = WideCharToMultiByte(page, page == CP_UTF8 ? 0 : WC_NO_BEST_FIT_CHARS,
        path.c_str(), -1, buffer, MAX_PATH, nullptr, page == CP_UTF8 ? nullptr : &substituted);
    if (!size || substituted) return false;
    result = buffer;
    return true;
}

void Mount()
{
    if (!CreateDirectoryW(g_folder.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
        Log("CustomPaks: cannot create the folder; continuing without custom packs");
        return;
    }
    WIN32_FIND_DATAW data{};
    const HANDLE search = FindFirstFileW((g_folder + L"\\*").c_str(), &data);
    if (search == INVALID_HANDLE_VALUE) {
        Log("CustomPaks: cannot enumerate the folder; continuing without custom packs");
        return;
    }
    struct CloseSearch { HANDLE handle; ~CloseSearch() { FindClose(handle); } } closer{search};
    std::vector<std::wstring> packs;
    do {
        const wchar_t* dot = wcsrchr(data.cFileName, L'.');
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && dot && _wcsicmp(dot, L".pak") == 0)
            packs.emplace_back(data.cFileName);
    } while (FindNextFileW(search, &data));
    if (GetLastError() != ERROR_NO_MORE_FILES) Log("CustomPaks: folder enumeration was incomplete");
    std::sort(packs.begin(), packs.end(), [](const std::wstring& a, const std::wstring& b) {
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    });
    unsigned loaded = 0, skipped = 0, failed = 0;
    for (const auto& name : packs) {
        std::string path;
        if (!GamePath(g_folder + L"\\" + name, path)) {
            ++failed;
            Log("CustomPaks: a pack path is too long or cannot be represented in the game's code page");
            continue;
        }
        char line[512];
        const bool alreadyMounted = std::any_of(g_mounted.begin(), g_mounted.end(), [&](const std::string& other) {
            return _stricmp(other.c_str(), path.c_str()) == 0;
        });
        if (alreadyMounted) {
            ++skipped;
            sprintf_s(line, "CustomPaks: already mounted: %s", path.c_str());
        } else if (g_add(path.c_str(), 9)) { // Même valeur que DLML, vérifiée sur sa séquence d'appels.
            g_mounted.push_back(path);
            ++loaded;
            sprintf_s(line, "CustomPaks: mounted: %s", path.c_str());
        } else {
            ++failed;
            sprintf_s(line, "CustomPaks: failed to mount: %s", path.c_str());
        }
        Log(line);
    }
    char line[160];
    sprintf_s(line, "CustomPaks: %u mounted, %u already mounted, %u failed; initializing game scripts",
              loaded, skipped, failed);
    Log(line);
}

void MountNoThrow()
{
    try { Mount(); }
    catch (...) { Log("CustomPaks: loading failed; continuing with game script initialization"); }
}

void Initialize(const char* name, uint8_t flag)
{
    __try {
        // Un autre chargeur peut chaîner notre hook après l'installation : lui laisser les packs.
        if (g_entry && memcmp(g_entry, g_patch, sizeof(g_patch)) != 0)
            Log("CustomPaks: a later script hook is active; keeping its custom pack loading");
        else
            MountNoThrow();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("CustomPaks: filesystem exception; continuing with game script initialization");
    }
    // Le deuxième argument (DL) existe réellement dans le binaire 1.55 ; le conserver.
    g_initialize(name, flag);
}

bool Install(uint8_t* engine, uint8_t* filesystem)
{
    if (!engine || !filesystem || !Identity(engine, 1783270525, 14659584) ||
        !Identity(filesystem, 1783270366, 372736)) {
        Log("CustomPaks: unsupported or missing engine/filesystem; custom pack loading disabled");
        return false;
    }
    auto* entry = reinterpret_cast<uint8_t*>(GetProcAddress(reinterpret_cast<HMODULE>(engine), "InitializeGameScript"));
    const auto add = reinterpret_cast<AddSourceFn>(GetProcAddress(reinterpret_cast<HMODULE>(filesystem),
        "?add_source@fs@@YA_NPEBDW4ENUM@FFSAddSourceFlags@@@Z"));
    // Export = jmp rel32 puis 11 int3 : 16 octets disponibles, sans relocalisation d'instructions.
    const uint8_t thunk[]{0xe9,0xdb,0x16,0x20,0x00,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc};
    const uint8_t addBytes[]{0x40,0x55,0x56,0x41,0x54,0x41,0x55,0x41,0x56,0x48,0x8d,0xac,0x24,0x00,0xff,0xff,0xff};
    const uint8_t initializeBytes[]{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x81,0xec,0x50,0x01,0x00,0x00,0x0f,0xb6,0xfa};
    if (entry != engine + 0x27ba0 || reinterpret_cast<uint8_t*>(add) != filesystem + 0x12dc0 ||
        !Bytes(filesystem, 0x12dc0, addBytes, sizeof(addBytes)) ||
        !Bytes(engine, 0x229280, initializeBytes, sizeof(initializeBytes))) {
        Log("CustomPaks: unexpected filesystem/script code; custom pack loading disabled");
        return false;
    }
    if (memcmp(entry, thunk, sizeof(thunk)) != 0) {
        Log("CustomPaks: script initialization is already hooked; keeping the existing loader (e.g. DLML)");
        return false;
    }
    if (!SetFolder()) { Log("CustomPaks: cannot determine the game folder"); return false; }
    uint8_t patch[16]{0xff,0x25,0x00,0x00,0x00,0x00};
    const auto target = reinterpret_cast<uintptr_t>(&Initialize);
    memcpy(patch + 6, &target, sizeof(target));
    patch[14] = patch[15] = 0xcc;
    DWORD old = 0;
    if (!VirtualProtect(entry, sizeof(patch), PAGE_EXECUTE_READWRITE, &old)) {
        Log("CustomPaks: cannot install the script initialization hook");
        return false;
    }
    g_initialize = reinterpret_cast<InitializeFn>(engine + 0x229280);
    g_add = add;
    g_entry = entry;
    memcpy(g_patch, patch, sizeof(patch));
    memcpy(entry, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), entry, sizeof(patch));
    DWORD ignored;
    if (!VirtualProtect(entry, sizeof(patch), old, &ignored)) Log("CustomPaks: could not restore hook memory protection");
    Log("CustomPaks: loader ready; .pak files will be mounted before game scripts (flags=9)");
    return true;
}
} } // namespace position::custompaks
