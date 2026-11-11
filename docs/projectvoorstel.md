# Projectvoorstel — ICT Speeltuin

## Aanleiding

Studenten HBO-ICT leren programmeren vaak met opdrachten waarvan het resultaat
alleen tekst in een terminal is. Dat werkt, maar het motiveert weinig. Een
programma dat beweegt, klinkt of door een browser wordt aangesproken voelt als
iets echts. Juist in zulke programma's komen de onderwerpen terug die er in de
opleiding toe doen: geheugen, timing, buffers, protocollen en concurrency.

ICT Speeltuin is een verzameling **kleine, afgeronde projecten** met een direct
zichtbaar of hoorbaar effect. Ze zijn klein genoeg om in één keer te
begrijpen en rijk genoeg om op door te bouwen.

## Doel

1. Studenten bouwen werkende programma's met een "wow"-effect: graphics, geluid
   en netwerk.
2. Ze leren onderweg concepten die op het eerste gezicht droog lijken:
   simulatie-loops, sample rates en ringbuffers, sockets en protocollen.
3. Ze werken in een professionele, reproduceerbare omgeving:
   pixi, CMake, gcc, Python, git en tests.
4. Ze vergelijken talen: hetzelfde idee in C, C++ en Python naast elkaar.

## Doelgroep

Studenten HBO-ICT. Kennis van een programmeertaal wordt verondersteld. C/C++
hoeft nog niet bekend te zijn: de projecten zijn daar juist een kennismaking mee.

## Uitgangspunten

| Uitgangspunt | Invulling |
|---|---|
| **Eén commando** | `pixi run start` haalt de toolchain op, bouwt en start. Geen installatiehandleiding van pagina's lang. |
| **Zelfstandig** | Elk project heeft een eigen `pixi.toml` en lockfile en kan los worden uitgedeeld. |
| **Klein** | Een project past in een paar honderd regels, zodat een student het in één keer kan overzien. |
| **Testbaar** | De logica staat los van venster, geluid en netwerk, en heeft tests. |
| **Uitbreidbaar** | Elke README eindigt met opdrachten van makkelijk naar uitdagend. |
| **Eén stijl** | Alle projecten volgen [`style-guide.md`](style-guide.md). |

## Techniek

- **pixi** voor de omgeving. Compiler, CMake, SDL3 en Python komen allemaal van
  conda-forge, met vaste versies in `pixi.lock`.
- **CMake + Ninja + gcc** voor C (C17) en C++ (C++20).
- **SDL3** voor venster, input, 2D-rendering en audio.
- **Python 3.12** voor zelfstandige apps, testclients en tooling. Tests met
  pytest, lint met ruff.
- **Linux** (`linux-64`) als doelplatform.

## Eerste set projecten

| Project | Taal | Effect | Leerstof |
|---|---|---|---|
| `graphics/cpp_particles` | C++ | Een fontein van duizenden gekleurde deeltjes die je met de muis bestuurt | game loop, vaste tijdstap, Euler-integratie, data-oriented arrays, RAII |
| `sound/cpp_synth` | C++ | Het toetsenbord als synthesizer, met de golfvorm live op het scherm | sample rate, oscillatoren (sinus, blok, zaag), envelope, audio-callback vs. hoofdthread |
| `server/c_echo_http` | C | Een webserver die je request terugtoont in de browser | POSIX sockets, `bind`/`listen`/`accept`, HTTP/1.1 parsen, buffergrenzen |
| `server/py_echo_http` | Python | Dezelfde server in Python | `asyncio`, dezelfde protocolregels in minder regels; vergelijking met C |

Eén pytest-suite test beide servers op dezelfde manier. Zo zie je dat twee
implementaties zich aan hetzelfde contract houden.

## Mogelijke vervolgprojecten

Dit zijn ideeën en geen toezeggingen. De keuze maken we samen.

| Categorie | Idee | Leerstof |
|---|---|---|
| Graphics | Mandelbrot-verkenner met zoom | complexe getallen, floating-point-precisie, multithreading (24 cores) |
| Graphics | Game of Life | 2D-arrays, double buffering |
| Graphics | Raycaster in de stijl van een vroege 3D-game | trigonometrie, DDA-algoritme |
| Graphics | Compute shader-deeltjes op de GPU | GPU vs. CPU, parallelisme |
| Geluid | Spectrumanalyzer op de microfoon | FFT, windowing, logaritmische schalen |
| Geluid | Step-sequencer / drumcomputer | timing, sample-nauwkeurige planning |
| Geluid | Delay- en echo-effect | ringbuffers, feedback |
| Server | Chatserver (TCP) | meerdere clients, `poll`/`select` vs. threads vs. `asyncio` |
| Server | Live dashboard via WebSocket | push vs. pull, framing |
| Server | Key-value store met eigen protocol | protocolontwerp, persistentie |
| Server + ESP32 | ESP32 stuurt sensordata naar een server die het live plot | embedded ↔ server, UDP vs. TCP |

## Aanpak

Het werk gaat in logische stappen. Na elke stap volgt een review en daarna een
plan voor de volgende stap:

1. Repository, conventies en documenten (dit voorstel, de style guide, de README).
2. De vier startprojecten, elk in een eigen commit, met tests.
3. Review met docenten en een paar studenten: wat werkt, wat is te groot of te klein?
4. Volgende projecten kiezen uit de lijst hierboven, of nieuwe.

## Open punten

Deze punten zijn nog niet besloten en vragen een keuze van de opdrachtgever:

- In welk(e) vak(ken) of semester worden de projecten ingezet?
- Hoeveel tijd mag een student per project kwijt zijn?
- Worden de opdrachten beoordeeld, en zo ja, hoe?
- Werken studenten op eigen laptops (Linux / WSL2 / macOS?) of op labmachines?
  Dit bepaalt of er naast `linux-64` meer platforms in `pixi.toml` moeten.
- Hoe worden de projecten uitgedeeld (GitLab-groep, zip, template-repo)?
