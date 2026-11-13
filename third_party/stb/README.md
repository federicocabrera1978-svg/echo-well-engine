# stb_image (vendorizado)

- Archivo: `stb_image.h` **v2.32** (2026-11-12), public domain.
- Upstream: https://github.com/nothings/stb — commit `9f755b604db3492df2cbeb39bf84b81e3e3932e3`.
- SHA-256: `deb4f18c9ca21b751a1323019031c94c0bf5ca1d2929ef5913cc56f00aab6e91`.
- Lo usa el loader del atlas (`engine/render/tile_atlas.cpp`) con `STBI_ONLY_PNG` y `STBI_MAX_DIMENSIONS 256`. Si el PNG falla o no mide 128×128, se cae al atlas procedural.
