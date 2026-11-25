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
