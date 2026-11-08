# SchuWebRosol

Webová stránka jako živý video zdroj pro **Resolume** (a cokoliv dalšího, co umí
**Spout** na Windows nebo **Syphon** na macOS).

SchuWebRosol vyrenderuje stránku v Chromiu mimo obrazovku (CEF off-screen
rendering) a každý snímek pošle jako Spout/Syphon zdroj. V Resolume se pak
objeví v panelu **Sources → Spout** (Windows) / **Sources → Syphon** (macOS)
pod názvem `SchuWebRosol` a dá se použít jako jakýkoliv jiný klip – včetně
průhlednosti (alpha).

Inspirováno projektem [SpoutBrowser](https://github.com/bntre/SpoutBrowser).

## Instalace

Hotové balíčky jsou v GitHub Actions (artefakty buildu) a u tagů `v*` v Releases:

| Systém | Soubor |
| --- | --- |
| Windows 10/11 x64 | `SchuWebRosol-windows-x64.zip` – rozbalit a spustit `SchuWebRosol.exe` |
| macOS 12+ Apple Silicon | `SchuWebRosol-macos-arm64.zip` |
| macOS 12+ Intel | `SchuWebRosol-macos-x86_64.zip` |

macOS: aplikace není notarizovaná. Po prvním spuštění otevři
*Nastavení systému → Soukromí a zabezpečení* a klikni na *Přesto otevřít*
(nebo `xattr -dr com.apple.quarantine SchuWebRosol.app`).

## Použití

1. Spusť SchuWebRosol – otevře se ovládací okno.
2. Zadej URL a klikni na **Načíst**.
3. V Resolume přetáhni zdroj `SchuWebRosol` ze *Sources* do kompozice.

V ovládacím okně je živý náhled výstupu. Kliknutím do náhledu klikneš do
stránky, kolečkem scrolluješ a přes pole *Napsat text* lze psát do formulářů
(např. přihlášení). Dále lze nastavit:

- **Název zdroje** – jak se zdroj jmenuje v Resolume,
- **Rozlišení** (16–8192 px) a **max FPS** (1–240),
- **Zoom** stránky, **průhledné pozadí** (stránka s `background: transparent`
  pak má v Resolume alpha kanál), **ztlumení zvuku**, **skrytí posuvníků**,
- **auto reload** po N sekundách,
- **vlastní CSS a JavaScript**, které se aplikují po každém načtení
  (např. schování cookie lišty nebo zprůhlednění pozadí).

Nastavení se ukládá automaticky (`Použít a uložit`) do:

- Windows: `%APPDATA%\SchuWebRosol\<instance>.json`
- macOS: `~/Library/Application Support/SchuWebRosol/<instance>.json`

### Více zdrojů najednou

Každá instance je samostatný zdroj s vlastním nastavením, cookies a cache:

```
SchuWebRosol.exe --instance=scoreboard
SchuWebRosol.exe --instance=titulky
```

Zdroje se pak jmenují `SchuWebRosol scoreboard`, `SchuWebRosol titulky` atd.
(název jde v okně změnit).

### Parametry příkazové řádky

Parametry přepíšou (a uloží) hodnoty z konfigurace:

| Parametr | Význam |
| --- | --- |
| `--instance=<jméno>` | Samostatná instance (konfigurace, cache, cookies) |
| `--config=<soubor.json>` | Jiný konfigurační soubor |
| `--url=<url>` | Stránka |
| `--name=<název>` | Název Spout/Syphon zdroje |
| `--width=`, `--height=` | Rozlišení výstupu |
| `--fps=` | Maximální snímková frekvence |
| `--zoom=` | Zoom stránky v % |
| `--reload-interval=` | Auto reload v sekundách (0 = vypnuto) |
| `--transparent[=true/false]` | Průhledné pozadí |
| `--mute[=true/false]` | Ztlumit zvuk |
| `--hide-scrollbars[=true/false]` | Skrýt posuvníky |
| `--no-ui` | Bez ovládacího okna (např. pro autostart) |
| `--verbose` | Podrobný log (`<instance>.log` vedle konfigurace) |

Na macOS se parametry předávají takto:
`open -n SchuWebRosol.app --args --instance=scoreboard`.

## Jak to funguje

```
 CEF (Chromium, off-screen)  ──OnPaint(BGRA)──▶  FrameSender
                                                  ├─ Windows: Spout2 (SpoutDX, sdílená D3D11 textura)
                                                  ├─ macOS:   Syphon (SyphonMetalServer)
                                                  └─ Linux:   jen vývoj/test (dump do PPM)
```

- `src/controller.cc` – nastavení, výstupní (off-screen) prohlížeč, posílání snímků
- `src/osr_client.cc` – CEF klient výstupního prohlížeče (OnPaint → sender)
- `src/control_client.cc`, `src/control.html` – ovládací okno a jeho lokální API
- `src/frame_sender_*.{cc,mm}` – Spout / Syphon / Linux backend

Stránka se renderuje jen když se mění (statická stránka = 0 fps), Spout/Syphon
přitom drží poslední snímek, takže v Resolume obraz nezmizí.

## Build ze zdrojáků

Potřeba: CMake 3.21+, Python 3, a

- Windows: Visual Studio 2022 (C++),
- macOS: Xcode 15+ (Syphon se stáhne a zkompiluje automaticky),
- Linux (jen pro vývoj): GCC 11+, Ninja, `libx11-dev`.

CEF (Chromium Embedded Framework) se při konfiguraci stáhne automaticky do
`third_party/cef`, Spout2 a Syphon také.

```bash
# Windows
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target SchuWebRosol
# → build\src\Release\SchuWebRosol.exe

# macOS (arm64 nebo x86_64)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPROJECT_ARCH=arm64
cmake --build build --target SchuWebRosol
# → build/src/Release/SchuWebRosol.app

# Linux (vývoj; snímky lze ukládat do souboru)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target SchuWebRosol
SCHUWEBROSOL_DUMP=/tmp/frame.ppm build/src/Release/SchuWebRosol --url=https://example.com
```

Verze CEF je nastavená v `CMakeLists.txt` (`CEF_VERSION`).
