// Banc hors jeu : vrais exports mappés sans exécution du moteur, appels filesystem simulés.
#define POSITION_TEST
#include "../../mods/position/PositionMod.cpp"
#include <stdexcept>

namespace {
int checks = 0, failures = 0, initialized = 0;
std::vector<std::string> added, mounted;
const char* receivedName = nullptr;
uint8_t receivedFlag = 0;
bool wrongFlags = false, throwOnAdd = false;
void Check(bool ok, const char* message) {
    ++checks;
    if (!ok) { ++failures; printf("FAIL: %s\n", message); }
}
bool Add(const char* path, int flags) {
    wrongFlags |= flags != 9;
    if (throwOnAdd) throw std::runtime_error("synthetic failure");
    added.emplace_back(path);
    if (strstr(path, "02_failure.pak")) return false;
    mounted.emplace_back(path);
    return true;
}
void Original(const char* name, uint8_t flag) {
    ++initialized;
    receivedName = name;
    receivedFlag = flag;
    Check(!mounted.empty(), "Packs mounted before game script initialization");
}
void Write(void* target, const void* bytes, size_t size) {
    DWORD old, ignored;
    if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &old)) ExitProcess(3);
    memcpy(target, bytes, size);
    FlushInstructionCache(GetCurrentProcess(), target, size);
    VirtualProtect(target, size, old, &ignored);
}
void File(const std::wstring& path) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) ExitProcess(4);
    CloseHandle(file);
}
}

int wmain(int argc, wchar_t** argv)
{
    using namespace position;
    using namespace position::custompaks;
    wchar_t temporary[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temporary);
    g_folder = std::wstring(temporary) + L"PositionCustomPaks_" + std::to_wstring(GetCurrentProcessId());
    const std::wstring fixture = g_folder;
    g_add = &Add; g_initialize = &Original;
    Mount();
    Check(GetFileAttributesW(g_folder.c_str()) != INVALID_FILE_ATTRIBUTES, "Missing CustomPaks folder created");
    File(g_folder + L"\\03_upper.PAK");
    File(g_folder + L"\\02_failure.pak");
    File(g_folder + L"\\01_first.pak");
    File(g_folder + L"\\ignored.txt");
    CreateDirectoryW((g_folder + L"\\directory.pak").c_str(), nullptr);
    File(g_folder + L"\\directory.pak\\nested.pak");
    const char* name = "SyntheticGame";
    Initialize(name, 0xa5);
    Check(added.size() == 3, "Only top-level .pak files selected, case-insensitive");
    Check(added.size() == 3 && strstr(added[0].c_str(), "01_first.pak") && strstr(added[1].c_str(), "02_failure.pak") && strstr(added[2].c_str(), "03_upper.PAK"), "Packs mounted in alphabetical order");
    Check(!wrongFlags, "Every pack receives the DLML add_source flags (9)");
    Check(mounted.size() == 2, "Mount failure does not prevent remaining packs from loading");
    Check(initialized == 1 && receivedName == name && receivedFlag == 0xa5, "Both original script arguments preserved, exactly one original call");
    added.clear();
    Initialize(name, 0x5a);
    Check(added.size() == 1 && strstr(added[0].c_str(), "02_failure.pak"), "Packs already mounted by this loader skipped");
    Check(initialized == 2 && receivedFlag == 0x5a, "Repeated script initialization preserved");
    throwOnAdd = true;
    Initialize(name, 0x39);
    throwOnAdd = false;
    Check(initialized == 3 && receivedFlag == 0x39, "Filesystem failure cannot skip original script initialization");
    std::string narrow;
    Check(!GamePath(std::wstring(MAX_PATH, L'x'), narrow), "Overlong paths rejected without truncation");
    g_folder = fixture + L"\\ignored.txt";
    Initialize(name, 0x71);
    Check(initialized == 4 && receivedFlag == 0x71, "Invalid CustomPaks folder preserves game initialization");

    if (argc == 3) {
        auto* engine = reinterpret_cast<uint8_t*>(LoadLibraryExW(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES));
        auto* fs = reinterpret_cast<uint8_t*>(LoadLibraryExW(argv[2], nullptr, DONT_RESOLVE_DLL_REFERENCES));
        if (!engine || !fs) return 5;
        uint8_t before[16]; memcpy(before, engine + 0x27ba0, sizeof(before));
        Check(!custompaks::Install(engine, nullptr) && memcmp(before, engine + 0x27ba0, sizeof(before)) == 0, "Missing filesystem rejected without changes");
        uint8_t saved = fs[0x12dc0], damaged = saved ^ 1;
        Write(fs + 0x12dc0, &damaged, 1);
        Check(!custompaks::Install(engine, fs) && memcmp(before, engine + 0x27ba0, sizeof(before)) == 0, "Altered add_source code rejected without changes");
        Write(fs + 0x12dc0, &saved, 1);
        saved = engine[0x229280]; damaged = saved ^ 1; Write(engine + 0x229280, &damaged, 1);
        Check(!custompaks::Install(engine, fs), "Altered original script code rejected");
        Write(engine + 0x229280, &saved, 1);
        damaged = 0xcc; Write(engine + 0x27ba0, &damaged, 1);
        Check(!custompaks::Install(engine, fs) && engine[0x27ba0] == 0xcc, "Existing DLML or other hook preserved");
        Write(engine + 0x27ba0, before, sizeof(before));
        Check(custompaks::Install(engine, fs), "Verified 1.55 exports hooked successfully");
        Check(g_initialize == reinterpret_cast<InitializeFn>(engine + 0x229280), "Original target decoded correctly, no relocated instructions");
        MEMORY_BASIC_INFORMATION info{}; VirtualQuery(engine + 0x27ba0, &info, sizeof(info));
        Check(!(info.Protect & PAGE_EXECUTE_READWRITE), "Export memory protection restored");
        uint8_t patch[16]; memcpy(patch, engine + 0x27ba0, sizeof(patch));
        Check(!custompaks::Install(engine, fs) && memcmp(patch, engine + 0x27ba0, sizeof(patch)) == 0, "Second installation leaves the existing hook unchanged");
        // Exécuter uniquement le saut modifié vers NOS fonctions ; aucune fonction du jeu appelée.
        g_add = &Add; g_initialize = &Original; g_folder = fixture;
        reinterpret_cast<InitializeFn>(engine + 0x27ba0)(name, 0xc3);
        Check(initialized == 5 && receivedFlag == 0xc3, "Actual patched export reaches hook and preserves both arguments");
        damaged = 0xcc; Write(engine + 0x27ba0, &damaged, 1);
        added.clear();
        Initialize(name, 0x83);
        Check(added.empty() && initialized == 6 && receivedFlag == 0x83, "Later chained loader keeps pack loading without duplicate mounts");
        Write(engine + 0x27ba0, before, sizeof(before));
        g_entry = nullptr;
        FreeLibrary(reinterpret_cast<HMODULE>(engine)); FreeLibrary(reinterpret_cast<HMODULE>(fs));
    }
    DeleteFileW((fixture + L"\\03_upper.PAK").c_str());
    DeleteFileW((fixture + L"\\02_failure.pak").c_str());
    DeleteFileW((fixture + L"\\01_first.pak").c_str());
    DeleteFileW((fixture + L"\\ignored.txt").c_str());
    DeleteFileW((fixture + L"\\directory.pak\\nested.pak").c_str());
    RemoveDirectoryW((fixture + L"\\directory.pak").c_str());
    RemoveDirectoryW(fixture.c_str());
    printf("RESULT: %d CustomPaks checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
