# Style guide

Deze gids geldt voor alle projecten in ICT Speeltuin. De code is lesmateriaal:
studenten lezen hem vaker dan hij wordt aangepast. Leesbaarheid gaat dus vóór
slimheid, en commentaar legt uit **waarom** iets zo is, niet **wat** er staat.

## 1. Algemeen

| Onderwerp | Afspraak |
|---|---|
| Taal docs | Nederlands (`README.md`, `docs/`) |
| Taal code | Engels: identifiers, commentaar, logberichten, commit-berichten |
| Encoding / regeleinde | UTF-8, LF, één lege regel aan het eind van een bestand |
| Indentatie | 4 spaties, geen tabs (behalve in Makefiles) |
| Regellengte | 100 tekens (C/C++ via `.clang-format`, Python via ruff) |
| Formatter | `clang-format` voor C/C++, `ruff format` voor Python |

### Projectstructuur

Elk project is zelfstandig en kan los gekopieerd worden:

```
projects/<categorie>/<taal>_<naam>/
├── README.md        wat het doet, hoe je het start, wat je ervan leert, opdrachten
├── pixi.toml        toolchain en taken
├── pixi.lock        vastgezette versies (altijd committen)
├── CMakeLists.txt   (C/C++)
├── src/             broncode; pure logica los van SDL/sockets
└── tests/           ctest (C/C++) of pytest (Python)
```

`<taal>` is `c_`, `cpp_` of `py_`. Twee projecten die hetzelfde doen in een
andere taal delen de `<naam>`: `c_echo_http` en `py_echo_http`.

### Scheid logica van I/O

Zet rekenwerk (deeltjes bijwerken, een golfvorm genereren, een HTTP-request
parsen) in eigen bestanden zonder SDL- of socket-afhankelijkheid. Dan kun je
het testen zonder venster, geluidskaart of netwerk, en zien studenten waar het
eigenlijke idee zit.

## 2. Foutafhandeling

Er zijn twee soorten fouten, en die behandel je verschillend:

| Soort | Voorbeeld | Aanpak |
|---|---|---|
| **Programmeerfout** | `NULL` waar dat niet mag, index buiten bereik, onmogelijke toestand | `assert()` aan het begin van de functie |
| **Operationele fout** | poort al in gebruik, geen audio-device, kapot request, bestand ontbreekt | melden (stderr/log) en netjes afhandelen: foutcode, `400 Bad Request`, stoppen met duidelijke melding |

Gebruik `assert()` nooit voor iets wat bij normaal gebruik kan gebeuren. Een
client die onzin stuurt is geen programmeerfout, een server die daarop crasht wel.

## 3. C (C17)

### Naamgeving

| Wat | Stijl | Voorbeeld |
|---|---|---|
| Functies, variabelen | `snake_case` | `http_parse_request()`, `bytes_read` |
| Publieke symbolen | projectprefix | `http_request_t`, `http_parse_request()` |
| Bestandslokaal | `static` + `s_`-prefix | `static int s_listen_fd;`, `static void s_handle_client()` |
| Types | `snake_case_t` | `http_status_t` |
| Macro's, enum-waarden | `UPPER_SNAKE` met prefix | `HTTP_MAX_HEADERS`, `HTTP_METHOD_GET` |

### Regels

- Header guards met `#pragma once`.
- Elk bestand begint met een Doxygen-kop: `@file` en `@brief`, en een alinea
  waarom het bestand bestaat als dat niet vanzelf spreekt.
- Publieke functies krijgen in de header een `@brief`, en `@param`/`@return` waar
  dat iets toevoegt.
- Gebruik `stdint.h`-types (`uint8_t`, `int32_t`) als de breedte ertoe doet, en
  `size_t` voor groottes en indices.
- Geen magische getallen: buffergroottes en limieten als `#define` of `enum`
  bovenaan, en buffers op de stack krijgen hun grootte van die constante.
- Controleer elke return-waarde van systeemaanroepen (`socket`, `bind`,
  `read`, ...) en meld fouten met `perror()` of `strerror(errno)`.
- Gebruik geen `strcpy`/`sprintf`, maar `snprintf` met de buffergrootte.
- State machines volgen het patroon met `current_state` en `next_state`: één
  `switch(current_state)` die alle toestanden afdekt en precies één punt waar de
  overgang wordt vastgelegd (zie het omnibot-patroon).

```c
/**
 * @brief Parse the request line ("GET /path HTTP/1.1") into @p req.
 * @return 0 on success, -1 if the line is malformed.
 */
int http_parse_request_line(const char *line, size_t len, http_request_t *req);
```

## 4. C++ (C++20)

De C++ in dit repo is modern, maar blijft dicht bij C: geen diepe
klassehiërarchieën en geen template-trucs. Studenten moeten kunnen zien wat er
in het geheugen gebeurt.

### Naamgeving

| Wat | Stijl | Voorbeeld |
|---|---|---|
| Namespaces | `snake_case`, één per project | `namespace particles { ... }` |
| Types (struct/class/enum) | `PascalCase` | `ParticleSystem`, `Waveform` |
| Functies, variabelen | `snake_case` | `spawn_burst()`, `sample_rate` |
| Constanten | `k` + `PascalCase` | `constexpr int kMaxParticles = 20000;` |
| Private members | suffix `_` | `rng_`, `phase_` |

### Regels

- `#pragma once` en een Doxygen-kop zoals bij C.
- Gebruik geen `using namespace` in headers. In `.cpp`-bestanden mag het alleen
  voor de eigen namespace.
- **RAII voor SDL-handles**: gebruik `std::unique_ptr` met een deleter, of een
  kleine wrapper, zodat een vroege `return` nooit een venster of audio-stream
  laat lekken.
- Gebruik geen kale `new`/`delete`. Kies `std::vector`, `std::array` en
  `std::unique_ptr`.
- Pas `struct` toe voor data zonder invarianten en `class` als er iets te
  bewaken valt.
- Gebruik `enum class` in plaats van kale enums.
- Kies `float` voor graphics en audio. Wees expliciet met conversies
  (`static_cast<float>(i)`), want we bouwen met `-Wconversion`.
- Gebruik geen exceptions over de grens met SDL-callbacks. Fouten bij het
  opstarten meld je in `main` en eindig je met een exitcode.

## 5. Python (3.12)

- Volg PEP 8, met ruff als linter en formatter: `pixi run lint`.
- Type hints op alle publieke functies.
- Gebruik de standaardbibliotheek waar dat kan. Een extra package komt alleen
  in `pixi.toml` als het iets toevoegt wat studenten moeten leren.
- `if __name__ == "__main__":` roept alleen `main()` aan.
- Tests gebruiken pytest en staan in `tests/test_*.py`.
- Docstrings in Google-stijl op modules en publieke functies.

## 6. CMake

- `cmake_minimum_required(VERSION 3.20)` en een `project()` met expliciete
  talen (`C` of `CXX`).
- Zet standaard Release als `CMAKE_BUILD_TYPE` leeg is.
- `CMAKE_EXPORT_COMPILE_COMMANDS ON`, voor clangd en de editor.
- Een library-target voor de pure logica (`<project>_core`), en daarnaast de
  app en de tests die daartegen linken.
- Waarschuwingen per target, niet globaal:
  `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`. Nieuwe code bouwt zonder
  waarschuwingen.
- Optie `SANITIZE` (ASan + UBSan) voor de debug-build.
- Tests via `enable_testing()` en `add_test()`, uitgevoerd met `ctest`.

## 7. pixi

- `channels = ["https://prefix.dev/conda-forge"]` en `platforms = ["linux-64"]`.
- Compiler via `c-compiler` / `cxx-compiler`: dat is gcc van conda-forge, dus
  iedereen bouwt met dezelfde versie.
- Elke dependency krijgt een versiebereik en, als het niet vanzelf spreekt, een
  regel commentaar waarom hij erin zit.
- Iedere C/C++-workspace heeft dezelfde taken: `configure`, `build`, `test`,
  `start`, `smoke`, `build-debug` en `test-debug`.
- `pixi.lock` wordt altijd gecommit.

## 8. Git

- [Conventional Commits](https://www.conventionalcommits.org/):
  `type(scope): samenvatting`. De scope is de projectmap, bijvoorbeeld
  `feat(cpp_synth): add square wave`.
- Types: `feat`, `fix`, `docs`, `refactor`, `test`, `build`, `chore`.
- Onderwerpregel korter dan 50 tekens, gebiedende wijs, geen punt aan het eind,
  in het Engels.
- Eén logische stap per commit, en de build en tests slagen op elke commit.
