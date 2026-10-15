// Test optionnel : vraie bibliothèque filesystem, archive synthétique uniquement.
#define POSITION_TEST
#include "../../mods/position/PositionMod.cpp"

int wmain(int argc, wchar_t** argv)
{
    if (argc != 3) return 2; // DLL filesystem puis dossier de test contenant CustomPaks/probe.pak.
    const auto fs = LoadLibraryExW(argv[1], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!fs || !position::Identity(reinterpret_cast<uint8_t*>(fs), 1783270366, 372736)) {
        printf("Native filesystem unavailable or unsupported (Windows error %lu)\n", GetLastError());
        return 3;
    }
    using InitFn = bool (*)(const char*, int, const char*, bool, bool, const char**);
    const auto init = reinterpret_cast<InitFn>(GetProcAddress(fs, "?init@fs@@YA_NPEBDW4ENUM@FFSAddSourceFlags@@0_N2PEAPEBD@Z"));
    const auto exists = reinterpret_cast<bool (*)(const char*)>(GetProcAddress(fs, "?exists@fs@@YA_NPEBD@Z"));
    const auto shutdown = reinterpret_cast<void (*)()>(GetProcAddress(fs, "?shutdown@fs@@YAXXZ"));
    position::custompaks::g_add = reinterpret_cast<position::custompaks::AddSourceFn>(GetProcAddress(fs,
        "?add_source@fs@@YA_NPEBDW4ENUM@FFSAddSourceFlags@@@Z"));
    if (!init || !exists || !shutdown || !position::custompaks::g_add) return 4;
    std::string root, archive;
    if (!position::custompaks::GamePath(argv[2], root) ||
        !position::custompaks::GamePath(std::wstring(argv[2]) + L"\\CustomPaks\\probe.pak", archive)) return 5;
    if (!init(root.c_str(), 0, "", false, false, nullptr)) { printf("Filesystem init failed\n"); return 6; }
    const bool absent = !exists("position_native_probe.txt");
    position::custompaks::g_folder = std::wstring(argv[2]) + L"\\CustomPaks";
    position::custompaks::Mount();
    const bool registered = position::custompaks::g_mounted.size() == 1;
    const bool contentVisible = exists("position_native_probe.txt");
    position::custompaks::Mount();
    const bool deduplicated = position::custompaks::g_mounted.size() == 1;
    shutdown();
    const bool ok = absent && registered && contentVisible && deduplicated;
    printf("Native filesystem: %s (probe absent before mount: %d, archive registered: %d, ZIP content visible: %d, duplicate skipped: %d)\n",
        ok ? "PASS" : "FAIL", absent, registered, contentVisible, deduplicated);
    return ok ? 0 : 1;
}
