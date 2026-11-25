# Neural Spatial Engine 2.1.0 Alpha

Modern è il motore predefinito. Al centro la copia del segnale ai valori di fabbrica è esatta. La curva del panorama stereo conserva entrambi i canali al centro e reindirizza parte del canale opposto quando si sposta il pad. Il guadagno diretto diminuisce con la profondità. Il disegno ADV usa la convenzione verticale del pad in modalità Modern. La modalità Legacy resta disponibile per il confronto e il richiamo delle impostazioni 1.x. Il codice e gli ID Alpha rimangono distinti dalla release.

## Neural Room

MLP ReLU 6→32→10 con pesi addestrati tramite regressione. Gli ingressi sono SIZE, assorbimento, diffusione, X, Y e Z. SIZE modifica insieme le dimensioni di una stanza rettangolare virtuale. La rete predice un RT60, il livello di riverbero e ritardo/ampiezza delle quattro riflessioni. Il dataset genera stanze indipendenti con impostazioni campionate, quattro sorgenti immagine e decadimento Eyring. Le riflessioni sono lette da delay line a ritardo frazionario, i parametri inferiti cambiano con smoothing e il riverbero algoritmico realizza la coda. Il pannello ADV mostra la posizione nel volume e i controlli di assorbimento e diffusione. I valori predetti sono parametri per il renderer, quindi il RT60 acustico dell'uscita può discostarsi dal target Eyring. Il motore legge una previsione nuova al massimo 50 volte/s durante i movimenti, senza I/O o allocazioni nel callback.

## Spatial Network e Global Mix

Apri GLOBAL MIX, crea un codice e copialo sulle istanze della stessa sessione, poi riproduci il materiale e premi ANALYZE MIX. Il registro condiviso usa il codice esplicito, non interpreta il nome del progetto fornito dalla DAW. Ogni istanza registra feature compatte a 12 kHz quando aderisce a un gruppo. La classificazione fornisce una probabilità per sei categorie sintetiche, con override manuale e soglia di visualizzazione al 55%. L'analisi legge le finestre recenti. Evita di giudicare una previsione su una singola battuta. Le tracce silenziose senza override restano escluse.

Il modello di scena prende ruolo, RMS, stereo e un aggregato delle tracce. Addestramento su scene sintetiche con etichette euristiche. I punti pieni mostrano le posizioni, i cerchi vuoti i suggerimenti. Il menu seleziona tutte le tracce o una traccia. APPLY usa la percentuale di intervento, UNDO ripristina l'ultima applicazione sulla selezione, Lock track esclude l'istanza e SHARE ROOM copia SIZE, assorbimento, diffusione e modalità Neural. Le azioni su X/Y/Z inviano gesti al DAW. Predizione e applicazione si eseguono dal thread grafico. I gruppi si persistono nello stato. Se apri due sessioni con lo stesso codice nello stesso processo, crea un nuovo codice per una delle due. AU e VST3 usano registri distinti e non scambiano dati tra processi.

## Riprodurre l'addestramento

```bash
OPENBLAS_NUM_THREADS=1 python Training/train.py
```

Richiede NumPy. Seed fisso. Esporta l'header C++ e il JSON delle metriche su set indipendenti. La rete acustica scende a circa 63 ms di MAE sul target RT60 sintetico, e circa 1,8–2,3 ms sui quattro ritardi. La classificazione misura l'88,5% sulle forme d'onda sintetiche, dopo separazione delle serie simulate. Questi numeri misurano il simulatore. I modelli non sono ancora addestrati o validati con sale registrate o decisioni di fonici.

Per usare dati propri, il trainer accetta `--room`, `--instrument` e `--scene` con file NPZ contenenti matrici normalizzate `x`, `y` e un vettore `group`. Il gruppo identifica la stanza, la registrazione sorgente o il brano, rispettivamente. Il trainer separa i gruppi prima dell'addestramento. Le colonne e la normalizzazione sono quelle descritte nelle funzioni `rooms`, `instruments` e `scenes`. `--output /percorso` consente di confrontare pesi e metriche senza modificare il progetto.

Esempi di risorse da valutare per una fase successiva: [BUT ReverbDB](https://ieeexplore.ieee.org/abstract/document/8717722) per RIR misurate, [Slakh](https://www.slakh.com/) per stem e classi strumentali, e [Learning Neural Acoustic Fields](https://papers.neurips.cc/paper_files/paper/2022/hash/151f4dfc71f025ae387e2d7a4ea1639b-Abstract-Conference.html) per la generazione di risposte spaziali. Nessuna di queste risorse è stata usata per addestrare i pesi distribuiti qui.

Il benchmark DSP Legacy a 48 kHz, 128 campioni e 32 istanze su Linux ha misurato 781 ms rispetto a 1000 ms con controlli fermi e 928 ms rispetto a 1064 ms con automazione. I tempi si riferiscono allo stesso carico di elaborazione del benchmark 1.1.0 e non misurano il costo dei modelli Neural e Global Mix.

Con lo stesso segnale e 32 istanze in un secondo benchmark, Modern ha misurato **793 ms contro 1094 ms** con parametri fermi e **981 ms contro 1064 ms** con automazione. Neural Room ha misurato rispettivamente **953 ms** e **993 ms**. In questa prova non erano attivi il visualizzatore e il gruppo di analisi. I tempi riguardano il PC di build Linux e non stimano la CPU di Ableton su un altro computer.

I pacchetti di prova personalizzati includono le grafiche. Windows carica i byte incorporati nel modulo VST3, Linux incorpora le immagini con JUCE BinaryData e macOS le firma come risorse del bundle. Le immagini originali restano fuori dalla repository pubblica. In compilazioni proprie impostare `GETT1_ARTWORK_PATH` sulla cartella di immagini autorizzate. Il VST3 Alpha mantiene il codice della 2.0.1 e resta una voce distinta dalla 1.1.0.
