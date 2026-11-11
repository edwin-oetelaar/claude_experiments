# ICT Speeltuin

Kleine, leerzame projecten voor studenten HBO-ICT, met een direct zichtbaar of
hoorbaar resultaat: graphics, geluid en servers. Er zijn projecten in C, C++
en Python, en elk project bouw en start je met één commando.

## Projecten

| Project | Taal | Wat het doet |
|---|---|---|
| [`graphics/cpp_particles`](projects/graphics/cpp_particles/) | C++ | Deeltjesfontein die je met de muis bestuurt (SDL3) |
| [`sound/cpp_synth`](projects/sound/cpp_synth/) | C++ | Toetsenbord-synthesizer met live golfvorm (SDL3) |
| [`server/c_echo_http`](projects/server/c_echo_http/) | C | HTTP-server op POSIX sockets die je request terugtoont |
| [`server/py_echo_http`](projects/server/py_echo_http/) | Python | Dezelfde server in Python (`asyncio`) |

Elk project heeft een eigen README met uitleg, de leerdoelen en opdrachten om
het uit te breiden.

## Snel starten

Werkt op Linux, en op Windows via WSL2 (Windows 11 met WSLg geeft vensters en
geluid). Haal de repository op:

```bash
git clone https://github.com/edwin-oetelaar/claude_experiments.git
cd claude_experiments
```

Installeer [pixi](https://pixi.sh) één keer:

```bash
curl -fsSL https://pixi.sh/install.sh | sh
```

Ga daarna naar een project en start het. De eerste keer haalt pixi de compiler,
CMake, SDL3 en Python op. Dat duurt even, daarna gaat het snel.

```bash
cd projects/graphics/cpp_particles
pixi run start
```

## Taken

Alle C/C++-projecten hebben dezelfde taken:

| Taak | Wat het doet |
|---|---|
| `pixi run build` | Configureren (eenmalig) en bouwen in `build/` |
| `pixi run test` | Bouwen en de tests draaien |
| `pixi run start` | Bouwen en de app starten |
| `pixi run smoke` | De app kort zonder scherm/geluid draaien, als snelle controle |
| `pixi run build-debug` | Debug-build met AddressSanitizer en UBSan in `build-debug/` |
| `pixi run test-debug` | De tests in de sanitizer-build |

De Python-projecten hebben `start`, `test` (pytest), `lint` en `format` (ruff)
en `smoke`.

`pixi.lock` legt elke versie vast, de compiler ook. Elke checkout bouwt dus
overal hetzelfde.

## Opbouw

```
projects/<categorie>/<taal>_<naam>/   één zelfstandig project
docs/projectvoorstel.md               waarom dit repo bestaat en waar het heen gaat
docs/style-guide.md                   codeconventies voor C, C++, Python, CMake, pixi en git
AGENTS.md                             korte notities voor AI-assistenten
.clang-format                         opmaak voor alle C/C++
```

## Werken via SSH

Graphics en geluid hebben een scherm en een geluidskaart nodig. Via SSH zonder
X-forwarding test je ze zonder scherm:

```bash
pixi run smoke   # SDL_VIDEODRIVER=offscreen, SDL_AUDIODRIVER=dummy, stopt vanzelf
```

## Bijdragen

- Houd je aan de [style guide](docs/style-guide.md).
- Commits volgen [Conventional Commits](https://www.conventionalcommits.org/),
  met de projectmap als scope: `feat(cpp_synth): add square wave`.
- Een nieuw project kopieert de opbouw en de pixi-taken van een bestaand project
  in dezelfde taal.

## Licentie

[MIT](LICENSE).
