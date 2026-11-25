# Skeuomorphic Space Panner

Skeuomorphic Space Panner è un plug-in audio di Giuseppe “Gettone” Ambrosino (gett1), scritto in C++ con JUCE. Il pad controlla panorama stereo, altezza percepita e profondità. Il pannello ADV permette di regolare le prime riflessioni della stanza virtuale.

| Piattaforma | Formati | Progetto |
| --- | --- | --- |
| Windows x64 | VST3 | [windows](windows/) |
| macOS Apple Silicon e Intel | AU, VST3, Standalone | [macos](macos/) |

## 2.0 Alpha — DSP Evolution

Questa build di sviluppo aggiunge il bypass trasparente, la modalità Neutral, un visualizzatore stereo rivisto e il richiamo dei preset 1.1.0 in Legacy. La [documentazione DSP](Documentation/DSP-Evolution.md) descrive implementazione, verifiche e limiti. La 2.0.1 aggiunge il MIX generale. Nome, bundle e identificativi Alpha sono distinti, così la DAW può caricare anche la 1.1.0 nella stessa sessione.

## Risorse per compilare

I progetti includono immagini e texture neutre per consentire la compilazione dai sorgenti. Il font Pixelify Sans è incluso con la propria licenza SIL Open Font License. Per riprodurre la grafica della release puoi sostituire le risorse neutre con le corrispondenti immagini originali da una copia a cui hai accesso.

## Windows

Usa Visual Studio 2026 con Desktop development with C++, Windows SDK, CMake con il generatore Visual Studio 18 2026 e una copia locale dei sorgenti JUCE. Dalla cartella `windows`:

```powershell
.\Windows\Build-Windows.ps1 -JucePath "D:\JUCE" -PluginOnly
```

Lo script produce il VST3 Alpha in `windows/build-vs18-alpha`. Il comando con `-PluginOnly` si ferma prima della fase installer.

## macOS

Usa Xcode, CMake e Python 3. Dalla cartella `macos` esegui:

```sh
cmake -S . -B build-alpha -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO
cmake --build build-alpha --config Release --parallel 2
```

Usa `x86_64` al posto di `arm64` per la build Intel. Il [workflow Alpha](.github/workflows/dsp-alpha.yml) prepara i bundle di test e lancia le verifiche native Windows e macOS.

## Licenze

Il codice del progetto usa la [GNU Affero General Public License, versione 3](LICENSE). JUCE e Pixelify Sans sono distribuiti con i rispettivi termini.
