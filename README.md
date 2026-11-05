# VoxBloom

Ein Vocal-Effekt-Plugin für Delay und Hall: Echos, die nach jeder Wiederholung dunkler werden, und ein breiter Hall, der am Ende einer Zeile aufblüht und ausklingt. Ducking hält den Effekt leise, solange gesungen oder gerappt wird, und lässt ihn in den Pausen hochkommen.

![VoxBloom](docs/screenshot.png)

Formate: **VST3** (Windows, macOS, Linux), **AU** (macOS), **Standalone**. Gebaut mit JUCE 9 und CMake.

## Signalweg

```
Vocal ─┬──────────────────────────────────────────────► Dry ─┐
       │                                                     ├─► Mix ─► Output
       ├─► Delay (Low/High Cut, Drive, Wobble) ─┬─► Level ─┐  │
       │                                        │          ├─► Width ─► Ducking ─┘
       └──────────────► (Delay > Verb) ─► Reverb ─► Level ─┘
```

## Regler

| Bereich | Regler | Wirkung |
|---|---|---|
| Delay | **Sync** / **Time** | Notenwert passend zum DAW-Tempo (1/32 … 1/1, punktiert und triolisch) oder frei in ms |
| | **Mode** | Stereo, Ping-Pong (Echos springen zwischen links und rechts) oder Mono |
| | **Feedback** | Anzahl der Wiederholungen |
| | **Low Cut / High Cut** | Filter, die bei jeder Wiederholung wirken: Echos werden dünner und dunkler |
| | **Drive** | Tape-Sättigung in der Rückkopplung |
| | **Wobble** | Tape-Leiern (Tonhöhenmodulation) |
| Reverb | **Size / Decay / Pre-Delay** | Raumgröße, Nachhallzeit (0,3–20 s), Abstand zwischen Stimme und Hall |
| | **Damping / Low Cut** | Höhen- und Bassanteil im Hall |
| | **Modulation** | Chorus im Hallschweif für einen weicheren Klang |
| | **Delay > Verb** | 0 %: Hall auf der trockenen Stimme, 100 %: Hall auf den Echos |
| Ducking | **Amount / Threshold / Release** | Wie stark der Effekt unter der Stimme abgesenkt wird und wie schnell er danach wiederkommt |
| Output | **Width** | Stereobreite nur des Effekts; die Stimme selbst bleibt in der Mitte |
| | **Mix** | Bis 50 % bleibt die Stimme voll da, nur der Effekt wird lauter. Über 50 % wird die Stimme leiser. |

## Presets

Zwischen den Presets wechselst du mit dem Menü oben oder mit `<` `>`. Mit **Save** speicherst du eigene Presets. Die liegen hier:

- Windows: `%APPDATA%\Drumkidbeatz\VoxBloom\Presets`
- macOS: `~/Library/Drumkidbeatz/VoxBloom/Presets`
- Linux: `~/.config/Drumkidbeatz/VoxBloom/Presets`

Mitgelieferte Presets: Init, Dark Bloom, Melodic Hook, Night Drive Echo, Dotted Dream, Cloud Tail, Ambient Freeze, Halftime Space, Tape Throw, Ad-Lib Throw, Phone Echo, Wide Wash, Slap & Hall, Tight Room.

## Fertige Builds herunterladen

Jeder Push baut das Plugin über GitHub Actions für Windows, macOS und Linux. Unter **Actions → Build → letzter Lauf → Artifacts** findest du `VoxBloom-Windows`, `VoxBloom-macOS` und `VoxBloom-Linux`.

Installation:

- **Windows:** `VoxBloom.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren
- **macOS:** `VoxBloom.vst3` nach `~/Library/Audio/Plug-Ins/VST3/` und `VoxBloom.component` nach `~/Library/Audio/Plug-Ins/Components/` kopieren. Weil das Plugin nicht signiert ist, danach einmal im Terminal ausführen:
  `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/VoxBloom.vst3 ~/Library/Audio/Plug-Ins/Components/VoxBloom.component`
- Anschließend in der DAW nach neuen Plugins suchen lassen.

## Selbst bauen

Benötigt: CMake 3.22 oder neuer und einen C++17-Compiler (Visual Studio 2022, Xcode oder GCC/Clang). JUCE wird beim ersten Konfigurieren automatisch heruntergeladen.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Mit `-DVOXBLOOM_COPY_AFTER_BUILD=ON` wird das Plugin nach dem Build direkt in den Plugin-Ordner kopiert. Mit `-DVOXBLOOM_JUCE_DIR=/pfad/zu/JUCE` nutzt der Build eine lokale JUCE-Kopie.

## Tests

```bash
./build/VoxBloomTests_artefacts/Release/VoxBloomTests                 # Prüfungen
./build/VoxBloomTests_artefacts/Release/VoxBloomTests --render demos  # Hörbeispiele als WAV
```

Geprüft werden: Echo-Timing (ms und Tempo-Sync), Ping-Pong-Verteilung, Echo-Pegel, Nachhallzeit, Stereobreite des Halls, Ducking, Stabilität und Pegel aller Presets, Speichern und Laden (Session und User-Presets), gleiche Ergebnisse bei unterschiedlichen Blockgrößen, Mono-Betrieb und CPU-Last.

## Lizenz

JUCE steht unter AGPLv3 oder der kommerziellen JUCE-Lizenz. Wenn du das Plugin verkaufen oder als Closed Source veröffentlichen willst, brauchst du eine passende JUCE-Lizenz. Für den Eigengebrauch ist das nicht nötig.
