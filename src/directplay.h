// directplay.h - Ersatzobjekte fuer CLSID_DirectPlay und CLSID_DirectPlayLobby
#pragma once

#include "dptypes.h"
#include "session.h"

#include <functional>
#include <string>

namespace a3 {

// Plattform-Anbindung (unter Windows in win/platform_win.cpp gesetzt, in Tests im Testcode)
struct Platform {
    Config config;
    bool verbose = false;
    std::function<void(void*)> signalEvent;
    std::function<bool(std::string&)> askAddress;
    std::function<void(uint16_t, bool)> portMapping;
};
Platform& platform();

// Erzeugt ein Ersatzobjekt. Liefert E_NOINTERFACE, wenn rclsid nicht DirectPlay ist
// (dann soll der Aufrufer an das echte CoCreateInstance weiterreichen).
bool isOurClsid(REFCLSID rclsid);
HRESULT createObject(REFCLSID rclsid, REFIID riid, void** ppv);

} // namespace a3
