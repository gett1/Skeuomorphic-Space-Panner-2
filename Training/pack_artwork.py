"""Embed owner-supplied art in an unsigned test module. Run once per fresh binary."""
import argparse
from pathlib import Path
import struct

files = ("anime.png", "beach.png", "cosmic.png", "graffiti.png", "screw.png", "tree.png",
         "wood_beach.jpg", "wood_cosmic.jpg", "wood_garage.jpg", "wood_kawaii.jpg", "wood_vintage.jpg")
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("module", type=Path)
parser.add_argument("artwork", type=Path)
args = parser.parse_args()
images = [args.artwork.joinpath(name).read_bytes() for name in files]
if not all(0 < len(image) <= 8*1024*1024 for image in images):
    parser.error("Invalid image size")
with args.module.open("r+b") as stream:
    stream.seek(-8, 2)
    if stream.read(8) == b"G1ART210":
        parser.error("This module already contains artwork")
    stream.seek(0, 2)
    entries = []
    for image in images:
        entries.append(struct.pack("<II", stream.tell(), len(image)))
        stream.write(image)
    stream.write(b"".join(entries) + b"G1ART210")
