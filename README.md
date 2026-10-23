# A3Net – Netzwerk-Fix für Anstoss 3

A3Net ersetzt das veraltete **DirectPlay** von Anstoss 3 durch eine eigene, moderne
Netzwerkschicht. Damit lässt sich Anstoss 3 unter Windows 10/11 **übers Internet per
direkter IP-Verbindung** spielen, und im LAN funktioniert es weiterhin auch.

- **Ein einziger Port:** Der Host gibt nur **TCP 47624** frei, oder UPnP erledigt das automatisch.
- **IP-Eingabe beim Beitreten:** Der Client gibt eine IP-Adresse oder einen Hostnamen ein (z. B. DynDNS). IPv4 und IPv6 gehen beide.
- **DirectPlay muss nicht installiert sein.** Die Windows-Komponente „DirectPlay“ wird nicht mehr gebraucht.
- **Spieldateien bleiben unverändert.** Es kommen nur zwei Dateien in den Spielordner dazu.

> **Status:** Erste Version. Der Netzwerkkern und die DirectPlay-Schicht sind mit einem
> automatischen Test geprüft, der die Aufrufe von Anstoss 3 nachspielt: Host plus zwei
> Clients, Beitritt, Spielerlisten, Daten, Abmelden und Verbindungsabbruch.
> Wenn etwas hakt, hilft die Datei `a3net.log` weiter (siehe „Fehlersuche“).

---

## 1. Installation (für jeden Mitspieler)

1. Kopiere **`dinput.dll`** und **`a3net.ini`** in den Spielordner, also dorthin, wo `anstoss3.exe` liegt.
2. Fertig. Beim nächsten Spielstart ist A3Net aktiv, und im Spielordner entsteht `a3net.log`.

Alle Mitspieler brauchen **dieselbe Version** von A3Net.
Zum **Deinstallieren** löschst du `dinput.dll` oder setzt `Enabled=0` in `a3net.ini`.

> Warum `dinput.dll`? Anstoss 3 lädt beim Start DirectInput. Windows nimmt dabei zuerst die
> Datei aus dem Spielordner. A3Net leitet alle DirectInput-Aufrufe unverändert an die echte
> System-DLL weiter und biegt nur die Erzeugung von DirectPlay auf sich selbst um.

## 2. Übers Internet spielen

### Host (erstellt das Spiel)

1. Im Spiel: Netzwerkspiel → Verbindung **„Internet TCP/IP (A3Net)“** → Spiel erstellen.
2. Wenn Windows fragt, **erlaubst du `anstoss3.exe` in der Firewall** (privat und öffentlich).
3. **Den Port erreichbar machen:**
   - Mit **UPnP** (Standard): A3Net versucht, den Port automatisch im Router zu öffnen. Ob das geklappt hat, steht in `a3net.log` („UPnP: TCP-Port 47624 … freigegeben“), meist zusammen mit der öffentlichen IP.
   - **Manuell:** Richte im Router eine Portweiterleitung **TCP 47624** auf deinen PC ein.
4. **Deine öffentliche IP an den Mitspieler geben**, z. B. von wieistmeineip.at, oder einen DynDNS-Namen.

> **DS-Lite / CGNAT:** Viele Anschlüsse (Kabel, mobiles Internet, teils Glasfaser) haben keine
> eigene öffentliche IPv4-Adresse. Dann funktioniert die Portweiterleitung über IPv4 nicht.
> Meist klappt es aber über **IPv6**: Der Client gibt dann die IPv6-Adresse des Hosts ein, z. B.
> `[2a02:8388:...]`. Eine Portweiterleitung ist dafür nicht nötig, nur die Firewall-Freigabe
> (bei manchen Routern auch eine IPv6-Freigabe).

### Client (tritt bei)

1. Im Spiel: Netzwerkspiel → **„Internet TCP/IP (A3Net)“** → Spiel suchen bzw. beitreten.
2. Es erscheint ein Fenster: Gib die **IP oder den Hostnamen des Hosts** ein und bestätige mit OK.
   - Beispiele: `85.12.34.56`, `meinfreund.ddns.net`, `meinfreund.ddns.net:47624`, `[2001:db8::1]`
   - Leer lassen bedeutet: nur im lokalen Netzwerk suchen.
3. Das Spiel des Hosts erscheint in der Liste und kann beigetreten werden.

Die Adresse wird gespeichert und beim nächsten Mal vorausgefüllt. Wer immer zum selben Host
verbindet, kann sie auch fest in `a3net.ini` eintragen (`JoinAddress=`). Dann erscheint kein Fenster mehr.

> **Vollbild:** Läuft das Spiel im exklusiven Vollbildmodus, kann das Eingabefenster unsichtbar
> hinter dem Spiel liegen. Dann trägst du die Adresse als `JoinAddress` in `a3net.ini` ein oder
> stellst deinen Grafik-Wrapper (`ddraw.dll`) auf Fenstermodus.

## 3. Im LAN spielen

Das klappt ohne Einstellungen: Der Client lässt das Adressfeld leer oder bricht das Fenster ab.
A3Net sucht dann per Broadcast im lokalen Netz (UDP 47624).

## 4. Einstellungen (`a3net.ini`)

| Schlüssel | Standard | Bedeutung |
|---|---|---|
| `Enabled` | 1 | 0 schaltet A3Net ab, das Spiel nutzt dann wieder das originale DirectPlay |
| `Port` | 47624 | TCP-Port (Spiel) und UDP-Port (LAN-Suche); muss bei allen gleich sein |
| `JoinAddress` | leer | feste Host-Adresse, dann ohne Dialog |
| `AskForAddress` | 1 | Dialog für IP/Hostname beim Suchen anzeigen |
| `LanBroadcast` | 1 | zusätzlich im LAN suchen |
| `UPnP` | 1 | als Host den Port automatisch im Router öffnen |
| `Timeout` | 30 | Sekunden ohne Lebenszeichen bis „Verbindung verloren“ |
| `Log` / `LogVerbose` | 1 / 0 | Protokoll in `a3net.log`; mit `LogVerbose=1` landet jede Nachricht darin |
| `LastAddress` | – | wird automatisch gesetzt |

## 5. Fehlersuche

Schau zuerst in **`a3net.log`** im Spielordner.

| Im Log | Ursache / Lösung |
|---|---|
| Datei fehlt ganz | `dinput.dll` liegt nicht im richtigen Ordner, oder es ist die 64-Bit-Version (siehe Bauen) |
| `Hinweis: Hauptprogramm importiert CoCreateInstance nicht` | Falsche EXE (z. B. der Editor), oder es ist eine andere Spielversion |
| `Kann nicht auf TCP-Port 47624 lauschen` | Der Port ist schon belegt (z. B. läuft das Spiel zweimal), oder eine Firewall blockiert ihn |
| Client: `Suche: ... konnte nicht aufgelöst werden` | Tippfehler im Hostnamen, oder DNS-Problem |
| Client findet nichts, Host-Log zeigt keinen Zugriff | Portweiterleitung, Firewall oder CGNAT beim Host; dann IPv6 versuchen |
| `nicht unterstuetzt: XYZ` | Das Spiel nutzt eine DirectPlay-Funktion, die A3Net noch nicht kann. Bitte das Log aufheben, damit lässt sich das gezielt nachrüsten |

Antiviren-Programme schlagen bei unbekannten, unsignierten DLLs gelegentlich falschen Alarm.
Dann den Spielordner als Ausnahme eintragen.

## 6. Selbst bauen (Visual Studio Code)

Anstoss 3 ist ein **32-Bit-Programm**, deshalb **muss** auch die DLL 32 Bit sein.

### Variante A: Visual Studio Build Tools (empfohlen)

1. Installiere **Visual Studio Code**.
2. Installiere die **Build Tools für Visual Studio 2022** (kostenlos) mit der Arbeitslast **„Desktopentwicklung mit C++“**. CMake ist dort enthalten.
3. Öffne diesen Ordner in VS Code. VS Code schlägt die Erweiterungen **C/C++** und **CMake Tools** vor; installiere beide.
4. Unten in der Statusleiste (oder `Strg+Umschalt+P` → „CMake: Select Configure Preset“) wählst du **„Visual Studio 2022 Build Tools - 32 Bit“**.
5. Build-Preset **„Release (32 Bit, MSVC)“** wählen und **Build** (F7) drücken.
6. Ergebnis: `build/msvc-x86/Release/dinput.dll` (plus `a3net.ini`).

### Variante B: MinGW (MSYS2)

1. Installiere MSYS2 und darin `pacman -S mingw-w64-i686-gcc mingw-w64-i686-cmake mingw-w64-i686-ninja`.
2. Nimm `C:\msys64\mingw32\bin` in den PATH auf.
3. In VS Code das Preset **„MinGW 32 Bit“** wählen und bauen. Das Ergebnis liegt in `build/mingw32/dinput.dll`.

### Direkt in den Spielordner kopieren

Wenn du in `CMakePresets.json` bei `A3_GAME_DIR` deinen Spielordner einträgst
(z. B. `"C:/Spiele/Anstoss3"`), wird `dinput.dll` nach jedem Build automatisch dorthin kopiert.

### Test

Das Ziel `a3net_test` startet Host und Clients im selben Prozess über `127.0.0.1` und spielt
die Aufrufe des Spiels nach. Am Ende muss „ALLE TESTS BESTANDEN“ stehen. Der Test läuft auch
unter Linux (Preset `linux-test`).

## 7. Aufbau des Quellcodes

```
src/dptypes.h          DirectPlay-Definitionen (GUIDs, Strukturen, Fehlercodes) - das SDK hat sie nicht mehr
src/directplay.cpp     IDirectPlay4A + IDirectPlayLobby2A (COM-Vtables, exakt 53 bzw. 19 Methoden)
src/session.cpp/.h     Kern: Host/Client, Spieler, Warteschlange, Suche, Netzwerk-Thread (plattformunabhängig)
src/proto.h            eigenes Protokoll (Frames, Nachrichtentypen)
src/netio.cpp/.h       Sockets (Winsock / BSD), IPv4+IPv6
src/log.cpp/.h         Protokoll
src/win/dinput_proxy.cpp   die Exporte von dinput.dll + DllMain
src/win/platform_win.cpp   Einstellungen, IP-Dialog, UPnP, Umleitung von CoCreateInstance
tests/test_session.cpp     automatischer Test
docs/ANALYSE.md            Ergebnisse der Analyse von anstoss3.exe (Adressen, Abläufe)
```

**Protokoll:** TCP, Sternform über den Host. Jede Nachricht hat den Aufbau `u32 Länge | u8 Typ | Daten`.
Die Spieldaten von Anstoss 3 werden unverändert transportiert. Die Details stehen in `src/proto.h`.

## 8. Bekannte Einschränkungen

- **Kein Host-Wechsel:** Verlässt der Host das Spiel, endet die Session für alle. Das Spiel meldet sich vorher selbst ab.
- Nur die Verbindung „TCP/IP“ wird angeboten; IPX, Modem und Seriell fallen weg.
- Namensänderungen von Spielern nach dem Beitritt werden nicht übertragen (das Spiel nutzt das laut Analyse nicht).
- Gruppen, Chat und Lobby-Start werden nicht unterstützt (das Spiel nutzt sie ebenfalls nicht).

## 9. Rechtliches

A3Net enthält **keinen Code und keine Daten von Anstoss 3**. Für die Schnittstelle wurde nur
analysiert, welche DirectPlay-Funktionen das Spiel aufruft. Das ist eine Analyse zur Herstellung
der Interoperabilität (in Österreich § 40e UrhG). Weitergeben solltest du nur dieses Projekt bzw.
die gebaute `dinput.dll`, keine Spieldateien.
