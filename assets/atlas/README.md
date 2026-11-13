# Atlas de tiles default

`tiles.png` es el atlas 128×128 (16 familias × 16 máscaras de 4 vecinos, tiles de 8×8)
que `echo_engine` carga por defecto.

**No editar a mano.** Se genera y se audita con:

```
python3 tools/gen_atlas.py -o assets/atlas/tiles.png
python3 tools/gen_atlas.py --check assets/atlas/tiles.png
```

Layout, paletas y bisel están documentados en el script. Si el archivo falta o no
mide 128×128, el motor cae al atlas procedural.
