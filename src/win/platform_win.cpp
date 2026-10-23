// platform_win.cpp - Windows-Anbindung: Konfiguration, IP-Dialog, UPnP, CoCreateInstance-Hook
#include "../directplay.h"
#include "../log.h"
#include "../netio.h"

#include <string>

#ifdef A3_HAVE_NATUPNP
#  include <natupnp.h>
#  include <oleauto.h>
#endif

namespace a3 {

static HMODULE g_self = nullptr;
static std::string g_iniPath;

static std::string moduleDir(HMODULE m) {
    char p[MAX_PATH] = {0};
    GetModuleFileNameA(m, p, MAX_PATH);
    std::string s(p);
    size_t k = s.find_last_of("\\/");
    return k == std::string::npos ? std::string(".") : s.substr(0, k);
}

static std::string iniStr(const char* key, const char* def) {
    char buf[512];
    GetPrivateProfileStringA("A3Net", key, def, buf, sizeof(buf), g_iniPath.c_str());
    return buf;
}
static int iniInt(const char* key, int def) { return (int)GetPrivateProfileIntA("A3Net", key, def, g_iniPath.c_str()); }

// ---------------------------------------------------------------------------
// Dialog "IP-Adresse eingeben" (Vorlage im Speicher, keine .rc-Datei noetig)
// ---------------------------------------------------------------------------
enum { IDC_ADDR = 101 };

struct DlgBuilder {
    std::vector<WORD> v;
    void align4() { if (v.size() % 2) v.push_back(0); }
    void w(WORD x) { v.push_back(x); }
    void dw(DWORD x) { v.push_back(LOWORD(x)); v.push_back(HIWORD(x)); }
    void str(const wchar_t* s) { while (*s) v.push_back((WORD)*s++); v.push_back(0); }
    void item(DWORD style, short x, short y, short cx, short cy, WORD id, WORD cls, const wchar_t* text) {
        align4();
        dw(style | WS_CHILD | WS_VISIBLE); dw(0);
        w((WORD)x); w((WORD)y); w((WORD)cx); w((WORD)cy); w(id);
        w(0xFFFF); w(cls);                  // vordefinierte Klasse
        str(text);
        w(0);                               // keine Zusatzdaten
    }
};

static std::wstring g_dlgInit;
static std::wstring g_dlgResult;

static INT_PTR CALLBACK addrDlgProc(HWND h, UINT msg, WPARAM wp, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG:
        SetDlgItemTextW(h, IDC_ADDR, g_dlgInit.c_str());
        SetWindowPos(h, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetForegroundWindow(h);
        SendDlgItemMessageW(h, IDC_ADDR, EM_SETSEL, 0, -1);
        SetFocus(GetDlgItem(h, IDC_ADDR));
        return FALSE;
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            wchar_t b[256] = {0};
            GetDlgItemTextW(h, IDC_ADDR, b, 255);
            g_dlgResult = b;
            EndDialog(h, IDOK);
            return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) {
            EndDialog(h, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static bool askAddressWin(std::string& out) {
    std::string last = iniStr("LastAddress", "");
    g_dlgInit.assign(last.begin(), last.end());

    DlgBuilder d;
    // DLGTEMPLATE
    d.dw(DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU);
    d.dw(WS_EX_TOPMOST);
    d.w(5);                                 // Anzahl Elemente
    d.w(0); d.w(0); d.w(250); d.w(86);      // x, y, cx, cy
    d.w(0); d.w(0);                         // kein Menue, Standardklasse
    d.str(L"Anstoss 3 - Netzwerkspiel beitreten (A3Net)");
    d.w(8);
    d.str(L"MS Shell Dlg");
    d.item(SS_LEFT, 8, 8, 234, 10, 0xFFFF, 0x0082, L"IP-Adresse oder Hostname des Hosts (optional mit :Port):");
    d.item(ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, 8, 20, 234, 13, IDC_ADDR, 0x0081, L"");
    d.item(SS_LEFT, 8, 38, 234, 20, 0xFFFF, 0x0082,
           L"Beispiele: 85.12.34.56   meinfreund.ddns.net:47624   [2001:db8::1]\n"
           L"Leer lassen = nur im lokalen Netzwerk suchen.");
    d.item(BS_DEFPUSHBUTTON | WS_TABSTOP, 136, 66, 50, 14, IDOK, 0x0080, L"OK");
    d.item(BS_PUSHBUTTON | WS_TABSTOP, 192, 66, 50, 14, IDCANCEL, 0x0080, L"Abbrechen");
    d.align4();

    HWND owner = GetActiveWindow();
    INT_PTR r = DialogBoxIndirectParamW(g_self, (LPCDLGTEMPLATEW)d.v.data(), owner, addrDlgProc, 0);
    if (r != IDOK) return false;

    char mb[512] = {0};
    WideCharToMultiByte(CP_ACP, 0, g_dlgResult.c_str(), -1, mb, sizeof(mb) - 1, nullptr, nullptr);
    out = mb;
    while (!out.empty() && (out.back() == ' ')) out.pop_back();
    WritePrivateProfileStringA("A3Net", "LastAddress", out.c_str(), g_iniPath.c_str());
    logf("Adresse eingegeben: '%s'", out.c_str());
    return true;
}

// ---------------------------------------------------------------------------
// UPnP-Portfreigabe ueber die in Windows eingebaute NAT-UPnP-Schnittstelle
// ---------------------------------------------------------------------------
#ifdef A3_HAVE_NATUPNP
static const CLSID kCLSID_UPnPNAT = {0xAE1E00AA, 0x3FD5, 0x403C, {0x8A, 0x27, 0x2B, 0xBD, 0xC3, 0x0C, 0xD0, 0xE1}};
static const IID kIID_IUPnPNAT = {0xB171C812, 0xCC76, 0x485A, {0x94, 0xD8, 0xB6, 0xB3, 0xA2, 0x79, 0x4E, 0x99}};

static std::wstring localIPv4() {
    // "Verbindet" einen UDP-Socket (es wird nichts gesendet), um die lokale Adresse
    // der Standardroute zu erfahren.
    sock_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == A3_BADSOCK) return L"";
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &a.sin_addr);
    std::wstring res;
    if (connect(s, (sockaddr*)&a, sizeof(a)) == 0) {
        sockaddr_in me{};
        int l = sizeof(me);
        if (getsockname(s, (sockaddr*)&me, &l) == 0) {
            char b[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &me.sin_addr, b, sizeof(b));
            for (char* p = b; *p; ++p) res += (wchar_t)*p;
        }
    }
    sockClose(s);
    return res;
}

static void portMappingWin(uint16_t port, bool add) {
    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IUPnPNAT* nat = nullptr;
    HRESULT hr = CoCreateInstance(kCLSID_UPnPNAT, nullptr, CLSCTX_ALL, kIID_IUPnPNAT, (void**)&nat);
    if (FAILED(hr) || !nat) {
        logf("UPnP: Windows-UPnP-Dienst nicht verfuegbar (0x%08X)", (unsigned)hr);
    } else {
        IStaticPortMappingCollection* col = nullptr;
        // Die Router-Suche kann ein paar Sekunden dauern; manchmal kommt sie erst beim 2. Versuch
        for (int i = 0; i < 3 && !col; ++i) {
            nat->get_StaticPortMappingCollection(&col);
            if (!col) Sleep(1500);
        }
        if (!col) {
            logf("UPnP: kein UPnP-faehiger Router gefunden - Port %u bitte manuell freigeben", port);
        } else {
            BSTR proto = SysAllocString(L"TCP");
            if (add) {
                std::wstring ip = localIPv4();
                BSTR client = SysAllocString(ip.c_str());
                BSTR descr = SysAllocString(L"Anstoss 3 (A3Net)");
                IStaticPortMapping* m = nullptr;
                col->Remove(port, proto);   // alte Eintraege entfernen
                hr = col->Add(port, proto, port, client, VARIANT_TRUE, descr, &m);
                if (SUCCEEDED(hr) && m) {
                    BSTR ext = nullptr;
                    m->get_ExternalIPAddress(&ext);
                    char eb[64] = {0};
                    if (ext) WideCharToMultiByte(CP_ACP, 0, ext, -1, eb, sizeof(eb) - 1, nullptr, nullptr);
                    logf("UPnP: TCP-Port %u -> %ls:%u freigegeben. Oeffentliche IP laut Router: %s", port,
                         ip.c_str(), port, eb[0] ? eb : "(unbekannt)");
                    if (ext) SysFreeString(ext);
                    m->Release();
                } else {
                    logf("UPnP: Portfreigabe abgelehnt (0x%08X) - Port %u bitte manuell freigeben", (unsigned)hr, port);
                }
                SysFreeString(client);
                SysFreeString(descr);
            } else {
                col->Remove(port, proto);
                logf("UPnP: Portfreigabe %u entfernt", port);
            }
            SysFreeString(proto);
            col->Release();
        }
        nat->Release();
    }
    if (SUCCEEDED(hrInit)) CoUninitialize();
}
#endif

// ---------------------------------------------------------------------------
// Hook fuer CoCreateInstance in der Importtabelle von anstoss3.exe
// ---------------------------------------------------------------------------
typedef HRESULT(WINAPI* CoCreateInstanceFn)(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);
static CoCreateInstanceFn g_realCoCreate = nullptr;

static HRESULT WINAPI hookCoCreateInstance(REFCLSID rclsid, LPUNKNOWN outer, DWORD ctx, REFIID riid, LPVOID* ppv) {
    if (isOurClsid(rclsid)) {
        if (outer) return CLASS_E_NOAGGREGATION;
        return createObject(rclsid, riid, ppv);
    }
    return g_realCoCreate(rclsid, outer, ctx, riid, ppv);
}

static bool patchImport(HMODULE mod, const char* dll, const char* func, void* hook, void** orig) {
    BYTE* base = (BYTE*)mod;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return false;
    for (IMAGE_IMPORT_DESCRIPTOR* d = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress); d->Name; ++d) {
        if (_stricmp((const char*)(base + d->Name), dll) != 0) continue;
        IMAGE_THUNK_DATA* iat = (IMAGE_THUNK_DATA*)(base + d->FirstThunk);
        IMAGE_THUNK_DATA* names = d->OriginalFirstThunk ? (IMAGE_THUNK_DATA*)(base + d->OriginalFirstThunk) : iat;
        for (; names->u1.AddressOfData; ++names, ++iat) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            IMAGE_IMPORT_BY_NAME* ibn = (IMAGE_IMPORT_BY_NAME*)(base + names->u1.AddressOfData);
            if (std::strcmp((const char*)ibn->Name, func) != 0) continue;
            DWORD old;
            if (!VirtualProtect(&iat->u1.Function, sizeof(iat->u1.Function), PAGE_READWRITE, &old)) return false;
            *orig = (void*)iat->u1.Function;
            iat->u1.Function = (ULONG_PTR)hook;
            VirtualProtect(&iat->u1.Function, sizeof(iat->u1.Function), old, &old);
            FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
            return true;
        }
    }
    return false;
}

void platformAttach(HMODULE self) {
    g_self = self;
    std::string dir = moduleDir(self);
    g_iniPath = dir + "\\a3net.ini";

    bool enabled = iniInt("Enabled", 1) != 0;
    bool log = iniInt("Log", 1) != 0;
    logInit(dir + "\\a3net.log", log);
    logf("A3Net %s gestartet (Prozess %s)", "1.0", moduleDir(nullptr).c_str());

    if (!enabled) {
        logf("A3Net ist in a3net.ini deaktiviert (Enabled=0) - das Spiel nutzt das originale DirectPlay");
        return;
    }

    Platform& p = platform();
    p.config.port = (uint16_t)iniInt("Port", 47624);
    p.config.joinAddress = iniStr("JoinAddress", "");
    p.config.askForAddress = iniInt("AskForAddress", 1) != 0;
    p.config.lanBroadcast = iniInt("LanBroadcast", 1) != 0;
    p.config.upnp = iniInt("UPnP", 1) != 0;
    p.config.timeoutSec = iniInt("Timeout", 30);
    if (p.config.timeoutSec < 5) p.config.timeoutSec = 5;
    p.verbose = iniInt("LogVerbose", 0) != 0;
    p.signalEvent = [](void* ev) { if (ev) SetEvent((HANDLE)ev); };
    p.askAddress = askAddressWin;
#ifdef A3_HAVE_NATUPNP
    p.portMapping = portMappingWin;
#endif
    logf("Konfiguration: Port=%u JoinAddress='%s' AskForAddress=%d LanBroadcast=%d UPnP=%d Timeout=%d",
         p.config.port, p.config.joinAddress.c_str(), p.config.askForAddress, p.config.lanBroadcast, p.config.upnp,
         p.config.timeoutSec);

    void* orig = nullptr;
    if (patchImport(GetModuleHandleA(nullptr), "ole32.dll", "CoCreateInstance", (void*)hookCoCreateInstance, &orig)) {
        g_realCoCreate = (CoCreateInstanceFn)orig;
        logf("CoCreateInstance in der Importtabelle umgeleitet - DirectPlay wird durch A3Net ersetzt");
    } else {
        g_realCoCreate = (CoCreateInstanceFn)GetProcAddress(GetModuleHandleA("ole32.dll"), "CoCreateInstance");
        logf("Hinweis: Hauptprogramm importiert CoCreateInstance nicht - kein Hook installiert");
    }
}

} // namespace a3
