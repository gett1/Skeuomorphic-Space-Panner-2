# Skeuomorphic Space Panner — porting macOS (gett1)

> Questo documento descrive il porting 1.1.0 e conserva lo storico. Per la 2.0 Alpha usa la [guida di compilazione](../README.md) e la [documentazione DSP Evolution](../Documentation/DSP-Evolution.md). Il codice condiviso si trova in `../Source/`.

## Stato reale di questa consegna

- Preparato un progetto **CMake/JUCE** portabile a partire dai 4 file originali.
- Copiate tutte le **11 immagini originali** in `Resources/`: saranno incorporate con `juce_add_binary_data`.
- Unica modifica al codice sorgente: aggiunta `#include <BinaryData.h>` in `Source/PluginEditor.cpp`, necessaria per la generazione CMake delle risorse.
- Impostato correttamente **effetto AUDIO stereo, non MIDI effect**.
- Workflow GitHub Actions preparato per **macOS Intel x86_64** e **Apple Silicon arm64**; genera **VST3, AU e Standalone**.
- Test risorse locali eseguibili; `pluginval` previsto nella build Intel (strictness 5, test GUI esclusi sui runner senza desktop).
- **Non ancora compilato/testato su macOS**: la prima esecuzione del workflow confermera' la compatibilita' e potra' evidenziare errori.
- La build di prova e' **non firmata e non notarizzata**: non distribuire pubblicamente prima delle verifiche.

## Contenuto

- `CMakeLists.txt`: impostazione CMake che scarica JUCE 8.0.16-security da GitHub.
- `Source/`: i quattro file C++ e .h.
- `Resources/`: undici file grafici per i preset.
- `.github/workflows/macos.yml`: automatismo compilazione Intel + Apple Silicon.
- `scripts/verifica_risorse.py`: test rapido di presenza/coerenza delle risorse.
- `Archivio/NewProject333_originale.jucer`: il progetto Projucer originario **solo per riferimento**; contiene vecchi percorsi Windows e non deve essere usato per questa build.

## Primo passo: creare un repository privato (evita di pubblicare il sorgente)

1. Entra in <https://github.com/new> e crea un repository chiamato `SkeuomorphicSpacePanner-macOS`.
2. Seleziona **Private**. NON selezionare template, README o licenze automatiche per il repository (li caricherai con questo progetto).
3. Carica sul repository **il contenuto della cartella di questo ZIP**, non lo ZIP da solo. La struttura `.github/workflows/macos.yml` deve essere conservata.
4. L'interfaccia web di GitHub non e' sempre comoda per caricare directory nascoste come `.github/`: consigliato `git` oppure **GitHub Desktop** (gratuito). I comandi semplici sono sotto.
5. In `Settings > Billing` controlla **Actions usage** e configura un limite di spesa che impedisca addebiti. I minuti inclusi in GitHub Free sono limitati, e i runner macOS possono consumarli piu' velocemente rispetto a Linux.
6. Il workflow parte **solo se premi Run workflow**: nessuna compilazione automatica al caricamento del codice. In `Actions` apri **Compila e verifica macOS (gratis entro quota Actions)** e scegli **Run workflow**.
7. Apri l'esecuzione: troverai due job (`Intel-x86_64` e `AppleSilicon-arm64`).
8. In fondo alla pagina, sotto **Artifacts**, scarica i pacchetti. Contengono VST3, AU e una app Standalone NON firmata.
9. Se compare una X rossa, apri il job e copia il **primo errore**. La presenza di una build non equivale a un test audio completo.

### Caricamento con Git (dal terminale nella cartella estratta)

```bash
git init
git add .
git commit -m "Porting macOS iniziale: JUCE CMake, risorse, CI"
git branch -M main
git remote add origin https://github.com/TUO-ACCOUNT/SkeuomorphicSpacePanner-macOS.git
git push -u origin main
```

Installa Git per Windows o GitHub Desktop. Sostituisci `TUO-ACCOUNT` con il tuo username e segui l'autenticazione del client GitHub. Per un repository vuoto puoi anche pubblicare la cartella direttamente tramite GitHub Desktop.

**Attenzione:** nello ZIP trovi una cartella nascosta `.github`. Abilita la visualizzazione dei file nascosti, perche' e' indispensabile per il workflow.

## Compilazione manuale su un Mac con Xcode (alternativa)

```bash
cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO
cmake --build build --config Release --parallel 3
```

`cmake` scarica JUCE da GitHub: serve accesso Internet. Per un binario Universal 2 usa entrambe le architetture sopra; nel workflow CI si fanno due build separate (una per architettura).

## Test da completare prima di poterlo dichiarare compatibile

- Verificare completamento **build Intel e Apple Silicon**.
- Analizzare `pluginval-report.txt` e correggere eventuali errori; il test GUI automatico e' disattivato.
- Su Mac reale aprire **Standalone**, muovere pad X/Y, scroll Z, ADV/4 nodi e verificare texture per tutti i preset.
- Inserire VST3 in **Ableton Live per macOS**, verificare audio, preset, parametri, automazioni e chiusura/riapertura progetto.
- Su Mac con Logic/GarageBand provare la versione **AU** e l'Audio Unit validation (`auval`).
- Test a 44.1/48/96 kHz, buffer 64/128/512/1024, bypass, send, input di livello basso/alto e riverbero di coda.
- Controllare eventuale aspetto differente di font **Impact/Courier New** e dimensione GUI Retina: i font Windows potrebbero mancare e venire sostituiti.
- Verificare firma/notarizzazione per la distribuzione: le build di CI sono **per test**, non installer pronti per la vendita.

## Attenzioni tecniche individuate nell'originale (NON cambiate in questo porting)

1. `PluginProcessor.cpp` dice di avere 0 secondi di coda (`getTailLengthSeconds()`) pur contenendo una riverberazione: alcuni host potrebbero troncare la coda.
2. In `PluginProcessor.h` ci sono gli array `scopeL/scopeR` aggiornati dal thread audio e letti dal thread GUI: accessi senza sincronizzazione esplicita (potenziale **data race**).
3. In `PluginEditor.cpp`, `DynamicKnobLookAndFeel` e' statico e condiviso tra istanze del plugin: potrebbe causare problemi quando si aprono piu' finestre/preset.
4. I font `Impact` e `Courier New` non hanno garanzia di identico rendering su macOS.
5. Il comportamento di `SEND` e del bypass deve essere misurato e verificato. Non e' sufficiente compilare.
6. `getName()` restituisce `Gettone Space Panner` mentre il nome di progetto e' diverso: scegliere un nome definitivo prima del rilascio.
7. L'originale `.jucer` non dichiara esplicitamente gli identificativi plugin. Il CMake introduce codici `Gett` / `Spnr`: **NON possiamo garantire compatibilita' con progetti DAW salvati usando il vecchio VST3** finche' non confrontiamo i reali ID e non facciamo una migrazione.

## Costi e licenze

Il setup utilizza strumenti gratuiti. GitHub Actions Free ha una **quota**, non tempo macOS illimitato: se il consumo la supera, non e' possibile promettere costo zero senza limiti; evita l'addebito impostando i controlli di spesa prima del primo workflow. JUCE e' soggetto alla sua licenza, da verificare in base all'uso commerciale. Firma/notarizzazione per macOS richiedono normalmente Apple Developer Program: non necessari per iniziare la compilazione, ma importanti prima di distribuire il prodotto a terzi.
