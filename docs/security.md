# Seguridad

## Fuzzing + sanitizers (CI)

Superficie de entrada no confiable: archivos `.ewmap` (`MapLoader`) y fuentes YAML de mapa (`yaml_lite` en `ewmapc`).

- `tests/fuzz/fuzz_map_loader.cpp` — carga bytes arbitrarios y, si validan, recorre secciones, entidades/strings, links, agua, cuerdas y capas de chunks.
- `tests/fuzz/fuzz_yaml_lite.cpp` — parsea texto arbitrario.
- Corpus semilla: `tests/fuzz/corpus/{ewmap,yaml}` (incluye `hello` con y sin CRC).

Jobs de CI (`.github/workflows/ci.yml`):

| Job | Qué hace |
|---|---|
| `sanitizers` | GCC, `-DEW_SANITIZE=ON` (ASan + UBSan, sin recover). Corre todo `ctest`, incluidos `fuzz_*_smoke` (20 000 mutaciones deterministas con el driver standalone). |
| `fuzz` | clang + libFuzzer + ASan/UBSan, 60 s por target. Si falla, sube `crash-*` como artifact. |

Local:

```sh
# GCC/clang, driver standalone
cmake -S . -B build-asan -DEW_BUILD_ENGINE=OFF -DEW_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-asan -j && ctest --test-dir build-asan --output-on-failure
# libFuzzer (clang)
cmake -S . -B build-fuzz -DEW_BUILD_ENGINE=OFF -DEW_SANITIZE=ON -DEW_LIBFUZZER=ON -DCMAKE_CXX_COMPILER=clang++
cmake --build build-fuzz -j --target fuzz_map_loader
./build-fuzz/fuzz_map_loader -max_total_time=300 tests/fuzz/corpus/ewmap
```

Para reproducir un crash: `./build-fuzz/fuzz_map_loader crash-<hash>`.

## Branch protection (manual, pendiente)

No está activada: requiere permisos de admin del repo (el token de los bots no los tiene). Recomendado en **Settings → Branches → Add rule** para `main`:

- Require a pull request before merging.
- Require status checks to pass: `build`, `sanitizers`, `fuzz`, `engine`.
- Require branches to be up to date before merging.
- Block force pushes y deletions.

Equivalente por API (con token admin): `PUT /repos/{owner}/{repo}/branches/main/protection`.
