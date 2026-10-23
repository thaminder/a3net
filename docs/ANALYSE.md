# Analyse: Netzwerkcode von anstoss3.exe

Grundlage ist die hochgeladene `anstoss3.exe` (PE32, 4 984 899 Bytes, ImageBase `0x400000`).
Alle Adressen sind virtuelle Adressen. Werkzeug: `objdump -d` und ein paar Hilfsskripte. Für
eigene Untersuchungen eignet sich Ghidra (kostenlos), dort einfach zu diesen Adressen springen.

## Netzwerk-API

- Kein Winsock-Import. DirectPlay wird per **`ole32!CoCreateInstance`** erzeugt
  (IAT-Eintrag `0x80DA48`, Aufrufe bei `0x7CA32A` und `0x7CBCE0`).
- `CLSID_DirectPlay` (`0x8388B0`) mit `IID_IDirectPlay4A` (`0x8388A0`)
- `CLSID_DirectPlayLobby` (`0x838970`) mit `IID_IDirectPlayLobby2A` (`0x838940`), nur für `CreateCompoundAddress`
- GUID-Tabelle der Service Provider ab `0x8388C0`: IPX, TCP/IP, Seriell, Modem
- Globaler Zeiger auf das DirectPlay-Objekt: `0x8B3B98`

## Ablauf

| Adresse | Funktion |
|---|---|
| `0x7CA049` | Init des Netzwerkmoduls: legt 4 Events an (`0x8B3B9C` = Nachrichten-Event für CreatePlayer), startet den Empfangs-Thread |
| `0x7CA226` | Empfangs-Thread: `WaitForMultipleObjects({0x8B3B9C, Quit-Event})`, danach `0x7CAA21` |
| `0x7CA278` | `EnumConnections` (Callback `0x7CA2BA`) |
| `0x7CA2BA` | Connection-Callback: SP-Typ über `0x7D2A9F` (1=IPX, 2=TCP/IP, 3=Seriell, 4=Modem). Für TCP/IP wird die Adresse neu gebaut: **ServiceProvider + INet mit leerem String (1 Byte)**. Danach wird das DP-Objekt **freigegeben und neu erzeugt** (`0x7CBCE7`/`0x7CBCC1`) und `InitializeConnection` getestet |
| `0x7CA4C6` / `0x7CA527` | Verbindung wählen: Objekt neu erzeugen, `InitializeConnection(conn, 0)` |
| `0x7CA595` | `EnumSessions(desc{dwSize=0x50, guidApp}, timeout, 0x7CA600, 0, 0x91)`, also AVAILABLE, ASYNC und RETURNSTATUS |
| `0x7CA600` | Session-Callback: bei `DPESC_TIMEDOUT` FALSE, sonst Filter-Callback (Name, Spieler, max, `dwUser1&1`) und die `guidInstance` merken |
| `0x7CA7F9` | **Host:** `Open(flags=0x2044 [DIRECTPLAYPROTOCOL, KEEPALIVE, MIGRATEHOST], max=[0x8B3BAC], 0x82)`, danach `CreatePlayer(..., event, NULL, 0, 0x100 = SERVERPLAYER)` |
| `0x7CA905` | **Client:** `Open(guidInstance, 0x81)`, danach `CreatePlayer(..., event, NULL, 0, 0)` |
| `0x7CA6CA` | Beenden: eigene Abmeldenachricht `0x80000018/19`, `Close` |
| `0x7CAA21` | Empfangsschleife: `Receive(&from,&to, DPRECEIVE_ALL, buf, &size)`, wiederholt bei `DPERR_BUFFERTOOSMALL`, bis `DPERR_NOMESSAGES`; Nachrichten < 4 Bytes werden ignoriert; `from==0` → System (`0x7CB0E0`), sonst Spiel (`0x7CAB3C`) |
| `0x7CB0E0` | Systemnachrichten: `0x0003` CREATEPLAYER (nur Host, nutzt `dpnName.lpszShortNameA` bei Offset `0x20`), `0x0005` DESTROYPLAYER (nur `dwPlayerType==1`), `0x0031` SESSIONLOST → Beenden, `0x0101` HOST → Host-Übernahme |
| `0x7CB546` | `GetPlayerName` (2-Schritt-Abfrage der Größe) |
| `0x7CB655` | Host: `GetSessionDesc`/`SetSessionDesc` und Bit 0 in `dwUser1` setzen bzw. löschen (Spiel läuft / Lobby offen) |
| `0x7CB912` | `EnumPlayers(app, 0x7CB942, 0, 0)` |
| `0x7CC2AD` | `Send(myId, to, flags, data, size)`, wobei flags 0 oder 1 (GUARANTEED) ist; große Daten zerlegt das Spiel selbst in Blöcke à `0x1FF0` |

## Spielprotokoll (über DirectPlay transportiert)

Das erste DWORD jeder Nachricht ist ein Code `0x800000xx` (z. B. `0x80000004/5/8/9/A/D/10/11/18/19`).
Bit `0x40000000` und `0x20000000` sind Zusatzflags, vermutlich für Anfrage/Antwort bzw. synchrone Aufrufe: Bei `0x20000000` wartet das Spiel per Event `0x1CB8BC4` auf die Antwort. Die
Verteilung steht bei `0x7CAB3C`, die Sprungtabelle bei `0x7CB07C`. Für A3Net sind das nur
undurchsichtige Bytes. Wer später tiefer einsteigen will (z. B. für einen eigenen Server oder
für Desync-Analysen), findet hier den Einstieg.

## Abgeleitete Anforderungen an den Ersatz

1. Vtable-Reihenfolge und `__stdcall`-Signaturen müssen exakt stimmen, auch für unbenutzte Methoden.
2. Das Objekt muss eine Freigabe **während** des eigenen `EnumConnections`-Callbacks überleben.
3. Die DirectPlay-Adresse muss im Originalformat vorliegen (`DPAID_TotalSize` zuerst), denn das Spiel parst sie selbst.
4. Systemnachrichten müssen mit eingebetteten Strings im Empfangspuffer ankommen (Zeiger zeigen in den Puffer).
5. Bei jeder neuen Nachricht muss das an `CreatePlayer` übergebene Event gesetzt werden.
6. Das Spiel übergibt nie eine IP-Adresse. Die Eingabe muss also aus der DLL kommen (Dialog oder ini).
