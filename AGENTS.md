# ICT Speeltuin — Agent Notes

## Repo shape
- Small, independent teaching projects under `projects/<category>/<lang>_<name>/`.
  - `<category>`: `graphics`, `sound`, `server`, ...
  - `<lang>`: `c_`, `cpp_` or `py_`.
- **Every project is its own pixi workspace** (`pixi.toml` + `pixi.lock`). There is no
  root `pixi.toml`; run all commands from the project directory.
- Docs are Dutch (`README.md`, `docs/`); code, identifiers and code comments are English.
- Full conventions: `docs/style-guide.md`. Project rationale: `docs/projectvoorstel.md`.

## Standard pixi tasks (C/C++ projects)
Run from the project directory:
- `pixi run build` — configure (once) + build into `build/` (Release, Ninja, gcc from conda-forge)
- `pixi run test` — build + `ctest --output-on-failure`
- `pixi run start` — build + start the app
- `pixi run build-debug` — Debug build with ASan/UBSan in `build-debug/`
- `pixi run test-debug` — tests in the sanitizer build

Python projects: `pixi run start`, `pixi run test` (pytest), `pixi run lint` (ruff).

## Headless verification (remote over SSH, no DISPLAY)
- SDL3 apps accept `SDL_VIDEODRIVER=offscreen` and `SDL_AUDIODRIVER=dummy`.
- Graphics/sound apps have a `--frames N` option to quit by themselves; `pixi run smoke` uses it.
- Seeing and hearing the result needs a real desktop session on the machine.

## Commit conventions
- Conventional Commits: `type(scope): summary`, scope = project directory name
  (e.g. `feat(cpp_synth): add square wave`). Subject < 50 chars, imperative, no period.
- One logical step per commit. No push unless asked.

## Coding conventions (short)
- C: public symbols take the project prefix (e.g. `http_`), file-local ones `s_` and `static`.
- C++: namespaces per project, no `using namespace` in headers, RAII for SDL handles.
- `assert()` only for programmer errors; operational failures (port in use, no audio
  device, bad request) are reported and handled.
- Pure logic lives in its own files without SDL/socket dependencies, so it can be tested.
- Warnings: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`; new code is warning-free.

## Verification
- Every change includes the exact commands to verify it (`pixi run test`, `pixi run smoke`).
