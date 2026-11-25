# Skeuomorphic Space Panner 2.0 Alpha — DSP Evolution

## Base e compatibilità

Il riferimento sorgente è il commit `3cc948da50ad7abcc44b7d66bf2168bb518113af`, pubblicato come sorgente 1.1.0. Il repository non contiene tag, release o una corrispondenza verificabile tra archivio distribuito e commit. La regressione audio usa quindi questo sorgente come riferimento, compilato con la stessa versione di JUCE del candidato.

Le due configurazioni condividono [processore](../Source/PluginProcessor.cpp), [editor](../Source/PluginEditor.cpp) e [mascotte](../Source/Gett1Cat.h). VST3 Windows mantiene `Manu/Aqip` e `com.gett1.GettoneSpacePanner_Hardware`. AU e VST3 macOS mantengono `Gett/Spnr` e `com.gett1.skeuomorphicspacepanner`. Windows mantiene il solo VST3. macOS mantiene AU, VST3 e Standalone. Pixelify Sans, layout ridimensionabile, cinque skin e colori seguono i sorgenti pubblici, che includono texture e immagini neutre.

Le istanze nuove si aprono in **Neutral**. Finché tutti i parametri sonori corrispondono ai default, il percorso consegna i campioni originali a guadagno unitario. Al primo intervento entra in funzione il renderer esistente. Riportare i controlli ai valori di fabbrica riattiva il passaggio diretto. **Legacy** tiene il renderer sempre attivo e riproduce la risposta della 1.1.0. Il menu sotto il pad permette di selezionare la modalità. Le sessioni e i preset XML privi di `roomMode` vengono migrati a Legacy. Sessioni e preset nuovi registrano `stateVersion=2` e il valore della modalità. I 19 identificativi preesistenti restano nello stesso ordine e il nuovo parametro è in coda.

## Percorso del segnale

1. Guadagno d'ingresso e compensazione timbrica Y. Per canale $s' = s + a(Y)\,HP(s)$, dove $a(Y)$ varia linearmente tra −1 e 1,5. Infatti al centro il contributo dell'high-pass vale $0{,}25\,HP(s)$.
2. Panorama equal-power per scala dei due canali d'ingresso. Con $\theta = \pi(X+1)/4$, il canale sinistro usa $\cos\theta$ e il destro $\sin\theta$. Al centro entrambi i guadagni valgono circa 0,707, ossia −3,01 dB.
3. Quattro tap stereo con interpolazione lineare. Per distanza normalizzata $d_i$ fra sorgente e nodo e controllo SIZE $S$, il ritardo è $5+40Sd_i$ ms. La scala della singola riflessione è $0{,}25\,ER\,[1-\mathrm{clamp}(0{,}7d_i,0,1)]/(1+1{,}5S)$. I nodi mantengono la convenzione geometrica della 1.1.0.
4. Filtro atmosferico low-pass e riverbero JUCE. Il filtro va da 20 kHz alla frequenza minima del preset al crescere di Z, con limite legato a Nyquist. I controlli dry e wet del riverbero sono $1-0{,}85Z^{0{,}8}$ e $0{,}85Z^{1{,}2}$. SEND azzera il dry. I coefficienti interni JUCE applicano una scala dry di 2 e wet di 3 prima dell'elaborazione stereo.
5. Guadagno d'uscita e limiter. Con bypass interno o host stabile il percorso preleva il segnale prima dell'intera catena. La dissolvenza lineare dry/effect di 10 ms opera anche attraverso blocchi di dimensioni variabili. Alla fine del passaggio a dry vengono azzerate le memorie di riverbero, riflessioni e filtri. Meter e Lissajous osservano l'uscita reale.

| Tema | Room size base | Damping | Cutoff minimo |
| --- | ---: | ---: | ---: |
| Kawaii | 0,80 | 0,45 | 16.000 Hz |
| Dark Garage | 0,65 | 0,85 | 9.000 Hz |
| Tropical Beach | 0,90 | 0,35 | 13.000 Hz |
| Cosmic Void | 0,98 | 0,10 | 18.000 Hz |
| Vintage Wood Room | 0,50 | 0,95 | 4.500 Hz |

Il tempo di coda dichiarato è **18 s** per riservare agli host un margine sulle impostazioni più lente. I comb di JUCE raggiungono feedback 0,98 a room size massimo. Il cammino più lungo è 1640/44100 s, più il ritardo di gruppo a DC dello smorzamento. Con $n=\log(10^{-4})/\log(0{,}98)$ si stimano circa 456 ricircoli per −80 dB per cammino. Si sommano il massimo ritardo early e un margine per la cascata di quattro all-pass con feedback 0,5 prima di arrotondare verso l'alto. È una stima per i parametri correnti del riverbero JUCE, da ricontrollare in caso di cambio della libreria o del renderer.

## Prestazioni e thread

Il callback audio carica i puntatori APVTS già risolti e usa una fotografia per blocco. Se un ripristino dello stato coincide con la lettura si riusa l'ultima fotografia completa. Il lock per serializzare i ripristini appartiene ai chiamanti che sostituiscono lo stato. Le regolazioni DAW ordinarie vengono lette individualmente e X/Y/Z e i nodi conservano il loro smoothing Legacy. I trascinamenti del pad e dei nodi aprono e chiudono un gesto per ciascun parametro. La rotella emette un gesto Z per ogni evento che cambia il valore.

Per la geometria a riposo si calcolano due funzioni di pan e quattro distanze una volta per blocco. Con sorgente o nodi in movimento i calcoli restano per campione. I tap rimangono $O(4N)$ e il riverbero resta invariato. Il filtro atmosferico aggiorna il coefficiente solo a frequenza diversa. La memoria per delay e dissolvenza è riservata durante `prepareToPlay`. Il pass-through stabile esegue solo snapshot parametri, misure e campionamento del visualizzatore. L'audio thread effettua il reset delle linee già allocate una volta per disattivazione completa.

[La FIFO stereo](../Source/DSP/StereoScope.h) collega un singolo produttore audio all'editor sul message thread. Ogni elemento contiene entrambi i canali e l'editor copia i frame in una cronologia privata di 256 punti. Il flusso è decimato a circa 22–24 kHz e il timer visibile ridisegna a 30 Hz. Il timer evita ridisegni e disattiva la scrittura FIFO quando l'editor è nascosto. La saturazione della FIFO scarta nuovi campioni grafici, senza bloccare il callback.

## Verifica riproducibile

Dal repository, creare il riferimento e configurare i test insieme al plugin.

```bash
git worktree add --detach ../panner-legacy 3cc948d
cmake -S macos -B build-alpha -DCMAKE_BUILD_TYPE=Release \
  -DGETT1_BUILD_DSP_TESTS=ON -DGETT1_LEGACY_SOURCE_DIR=../panner-legacy
cmake --build build-alpha --config Release --parallel 2
ctest --test-dir build-alpha -C Release --output-on-failure
```

Per Windows usare `-S windows -G "Visual Studio 18 2026" -A x64 -DGETT1_JUCE_PATH=D:/JUCE`. Per macOS usare `-G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64` o `x86_64`. Per Linux il progetto CMake macOS offre anche VST3 e Standalone di test. `PannerDspTests --benchmark` misura la mediana di tre prove di elaborazione seriale, da 1500 blocchi, a 48 kHz / 128 campioni. Il carico comprende copia del blocco e, nel caso automatizzato, due aggiornamenti periodici di parametro. Va eseguito con la build Release. L'esito VST3 si verifica con pluginval e quello AU con `auval -v aufx Spnr Gett`.

## Limiti di questa Alpha

Legacy e Neutral conservano il layout stereo della 1.1.0. True stereo panning, ingressi mono, smoothing separato di gain/SIZE/WIDTH, early direzionali e revisione SEND richiedono una modalità Modern con test propri. Il renderer attuale mantiene la risposta sonora storica. La validazione delle modalità di freeze, stop e export e della scrittura di automazioni nelle DAW richiede host reali. Nessuna rete neurale viene inclusa in questa Alpha. La ricerca Neural Room e Intelligent Mix dipende dalla verifica del motore e dalla successiva valutazione dei dataset, delle licenze e delle metriche di generalizzazione.

## Risultati Alpha su Linux x86_64

- CMake Release, GCC 14.2, JUCE 8.0.16-security. VST3 e Standalone compilati.
- Null test a identità binaria per Neutral a default, bypass interno stabile e bypass host stabile, con segnale stereo sopra 0 dBFS. Regressione identica per i cinque preset Legacy rispetto al commit di riferimento, incluse variazioni di X/Y/Z, dei nodi e di SEND. Verificati 44,1/48/88,2/96 kHz e buffer 64/128/256/512/1024.
- Superati il ripristino sessione/preset, i blocchi a dimensione variabile, l'uscita silenziosa dopo il rientro dal bypass e il trasferimento stereo coerente al visualizzatore.
- La massima ampiezza assoluta misurata a partire da 18 s per i cinque preset, SIZE massimo, Z massimo e impulso unitario è $1{,}46\times10^{-5}$, circa −96,7 dBFS.
- pluginval 1.0.4 ha superato strictness 10 con i quattro sample rate e i cinque buffer indicati. Verificate le chiamate host di automazione, stato, processi concorrenti sui parametri e fuzzing parametri. L'host di test rileva 0 campioni di latenza. La seconda esecuzione, con display virtuale Xvfb, ha superato anche apertura e riapertura dell'editor VST3.

Tempi mediani in millisecondi sulla stessa macchina Linux (Intel Xeon Platinum 8573C). Editor chiuso. La tabella riporta tempo cumulativo di elaborazione, con tutte le istanze sullo stesso thread.

| Istanze | 1.1.0 statico | 2.0 Alpha statico | 1.1.0 automatizzato | 2.0 Alpha automatizzato |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 32,562 | 24,583 | 32,832 | 29,515 |
| 8 | 262,261 | 207,612 | 269,354 | 230,797 |
| 16 | 531,220 | 413,932 | 527,981 | 477,602 |
| 32 | 1085,180 | 880,603 | 1097,000 | 970,558 |

I risultati riguardano il processore DSP in sequenza. Restano da misurare il costo dell'editor aperto, la RAM e le prestazioni con il routing multithread di una DAW. Le build target Windows e macOS richiedono i runner nativi del workflow Alpha.

## Risultati nativi Windows e macOS

| Piattaforma | Build | Verifiche superate |
| --- | --- | --- |
| Windows x64 | VST3 | Suite DSP con confronto bit per bit, pluginval 5 |
| macOS Apple Silicon | VST3, AU, Standalone | Suite DSP con confronto bit per bit, pluginval 5 per VST3, auval per AU |
| macOS Intel | VST3, AU, Standalone | Suite DSP con confronto bit per bit, pluginval 5 per VST3, auval per AU |

Il [run nativo](https://github.com/gett1/Skeuomorphic-Space-Panner-2/actions/runs/45582023071) contiene log e bundle di test, senza installer. I bundle macOS hanno firma ad hoc e non sono notarizzati. Il controllo AU copia il bundle nella directory di sistema del runner, riavvia la registrazione dei componenti e invoca auval. La prima esecuzione aveva compilato entrambi i target Mac e superato pluginval, ma l'AU appena copiato non era stato registrato.

Su Linux, Valgrind Memcheck 3.24 ha riportato **0 errori e 0 byte ancora allocati** dopo l'intera suite DSP. I controlli in Ableton Live e GarageBand, in particolare automazione registrata, freeze e riproduzione della coda dopo lo stop, restano da eseguire nelle DAW.
