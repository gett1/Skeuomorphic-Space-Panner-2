#!/usr/bin/env python3
"""Controllo preventivo porting macOS: non esegue compilazioni e non modifica audio."""
import re
from pathlib import Path
from sys import exit

root = Path(__file__).resolve().parent.parent
files = [
    "anime.png", "beach.png", "cosmic.png", "graffiti.png", "screw.png",
    "tree.png", "PixelifySans.ttf", "wood_beach.jpg", "wood_cosmic.jpg", "wood_garage.jpg",
    "wood_kawaii.jpg", "wood_vintage.jpg",
]
errors = []
for f in files:
    if not (root / "Resources" / f).is_file():
        errors.append("Risorsa mancante: " + f)

src = (root.parent / "Source" / "PluginEditor.cpp").read_text(encoding="utf-8")
cmake = (root.parent / "cmake" / "Artwork.cmake").read_text(encoding="utf-8")
for f in files:
    identifier = f.replace(".", "_")
    if f"BinaryData::{identifier}" not in src:
        errors.append("Immagine non referenziata in Editor: " + f)
    if f not in cmake:
        errors.append("Immagine non inclusa nel CMake: " + f)
for f in ("PluginProcessor.cpp", "PluginProcessor.h", "PluginEditor.cpp", "PluginEditor.h"):
    if not (root.parent / "Source" / f).is_file():
        errors.append("Sorgente mancante: " + f)
if "#include <BinaryData.h>" not in src:
    errors.append("Manca l'include esplicito di BinaryData.h")
if errors:
    print("VERIFICA FALLITA")
    for error in errors:
        print("  -", error)
    exit(1)
print("OK: 4 sorgenti, 11 immagini, font Pixelify Sans e riferimenti BinaryData/CMake coerenti")
print("Nota: non e' una prova di compilazione ne' di funzionamento audio/GUI.")
