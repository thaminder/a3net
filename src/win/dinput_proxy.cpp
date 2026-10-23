// dinput_proxy.cpp - Diese DLL heisst "dinput.dll" und liegt im Spielordner.
//
// activesim.dll (vom Spiel beim Start geladen) importiert DirectInputCreateA aus
// dinput.dll. Windows findet zuerst diese Datei im Spielordner, unsere DllMain laeuft,
// noch bevor das Spiel startet, und richtet den DirectPlay-Ersatz ein. Alle
// DirectInput-Aufrufe werden unveraendert an die echte System-dinput.dll weitergereicht.
#include "../dptypes.h"

namespace a3 { void platformAttach(HMODULE self); }

static HMODULE g_realDinput = nullptr;

static HMODULE realDinput() {
    if (!g_realDinput) {
        wchar_t path[MAX_PATH] = {0};
        UINT n = GetSystemDirectoryW(path, MAX_PATH);   // bei 32-Bit-Prozessen automatisch SysWOW64
        if (n == 0 || n > MAX_PATH - 16) return nullptr;
        lstrcatW(path, L"\\dinput.dll");
        g_realDinput = LoadLibraryW(path);
    }
    return g_realDinput;
}

template <typename F>
static F realProc(const char* name) {
    HMODULE h = realDinput();
    return h ? (F)(void*)GetProcAddress(h, name) : nullptr;
}

extern "C" {

HRESULT WINAPI DirectInputCreateA(HINSTANCE hinst, DWORD ver, LPVOID* out, LPUNKNOWN outer) {
    typedef HRESULT(WINAPI * F)(HINSTANCE, DWORD, LPVOID*, LPUNKNOWN);
    F f = realProc<F>("DirectInputCreateA");
    return f ? f(hinst, ver, out, outer) : E_FAIL;
}

HRESULT WINAPI DirectInputCreateW(HINSTANCE hinst, DWORD ver, LPVOID* out, LPUNKNOWN outer) {
    typedef HRESULT(WINAPI * F)(HINSTANCE, DWORD, LPVOID*, LPUNKNOWN);
    F f = realProc<F>("DirectInputCreateW");
    return f ? f(hinst, ver, out, outer) : E_FAIL;
}

HRESULT WINAPI DirectInputCreateEx(HINSTANCE hinst, DWORD ver, REFIID riid, LPVOID* out, LPUNKNOWN outer) {
    typedef HRESULT(WINAPI * F)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
    F f = realProc<F>("DirectInputCreateEx");
    return f ? f(hinst, ver, riid, out, outer) : E_FAIL;
}

HRESULT WINAPI DllCanUnloadNow(void) {
    typedef HRESULT(WINAPI * F)(void);
    F f = realProc<F>("DllCanUnloadNow");
    return f ? f() : S_FALSE;
}

HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    typedef HRESULT(WINAPI * F)(REFCLSID, REFIID, LPVOID*);
    F f = realProc<F>("DllGetClassObject");
    return f ? f(rclsid, riid, ppv) : E_FAIL;
}

HRESULT WINAPI DllRegisterServer(void) {
    typedef HRESULT(WINAPI * F)(void);
    F f = realProc<F>("DllRegisterServer");
    return f ? f() : E_FAIL;
}

HRESULT WINAPI DllUnregisterServer(void) {
    typedef HRESULT(WINAPI * F)(void);
    F f = realProc<F>("DllUnregisterServer");
    return f ? f() : E_FAIL;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        a3::platformAttach(hinst);
    }
    return TRUE;
}

} // extern "C"
