// Hôte synthétique : vérifie les exports winmm et le chargement avant main.
#include <windows.h>
#include <cstdio>
#pragma comment(lib, "winmm.lib")
extern "C" __declspec(dllimport) DWORD WINAPI timeGetTime();
int main()
{
    const DWORD tick=timeGetTime();
    const bool loaded=GetModuleHandleW(L"loader_marker.dll")!=nullptr;
    printf("Loader startup: %s (system timer %lu)\n",loaded ? "PASS" : "FAIL",tick);
    return loaded ? 0 : 1;
}
