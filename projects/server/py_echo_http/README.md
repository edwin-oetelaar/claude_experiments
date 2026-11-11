# Echo-webserver (Python, asyncio)

Dezelfde webserver als [`../c_echo_http`](../c_echo_http/), maar in Python met
`asyncio` en alleen de standaardbibliotheek. Leg de twee naast elkaar: welke
regels zijn verdwenen, en welke zorgen zijn gebleven?

## Starten

```bash
pixi run start
```

Open daarna <http://127.0.0.1:8080> en <http://127.0.0.1:8080/raw>, of:

```bash
curl -v http://127.0.0.1:8080/raw
```

Via SSH: `ssh -L 8080:127.0.0.1:8080 pc` en open de link op je eigen laptop.
Opties zoals bij de C-versie: `python -m echo_http --host 0.0.0.0 --port 9000`.

| Taak | Wat het doet |
|---|---|
| `pixi run start` | Starten op poort 8080 |
| `pixi run test` | Unittests (`test_http.py`) en contract-tests (`test_contract.py`) |
| `pixi run lint` | ruff: stijl- en foutcontrole, en of alles geformatteerd is |
| `pixi run format` | Alles formatteren met ruff |
| `pixi run smoke` | Starten, twee pagina's ophalen, stoppen |

## Wat de server doet

Precies hetzelfde als de C-versie: zie de tabel in
[`c_echo_http/README.md`](../c_echo_http/README.md#wat-de-server-doet).
`tests/test_contract.py` is in beide projecten hetzelfde bestand. Alleen
`tests/conftest.py` verschilt: dat zegt hoe de server gestart wordt.

## Hoe het werkt

| Bestand | Inhoud | In C |
|---|---|---|
| `echo_http/http.py` | De request-head parsen | `src/http.c` |
| `echo_http/app.py` | Routing: van request-bytes naar response-bytes | `src/app.c` |
| `echo_http/server.py` | Het netwerkdeel, met `asyncio` | `src/main.c` |
| `echo_http/__main__.py` | Opdrachtregel (`argparse`) | `main()` in `src/main.c` |

**Wat Python je uit handen neemt:** geheugenbeheer en buffergroottes,
strings, HTML-escaping (`html.escape`) en het parsen van opties (`argparse`).

**Wat je nog steeds zelf moet doen:** doorlezen tot de head compleet is (TCP
blijft een stroom), een maximum op de grootte van de head, escaping van invoer,
een timeout voor stille clients en het netjes afsluiten van de verbinding
(`_drain`).

**Foutafhandeling.** C geeft een resultaatcode terug
(`HTTP_PARSE_BAD_REQUEST`), Python gooit een exceptie met de status erin
(`HttpError(400)`).

**Gelijktijdig zonder threads.** De C-server helpt één client tegelijk. Deze
server start voor elke verbinding een *coroutine*. Wacht die op data
(`await reader.read(...)`), dan draait `asyncio` intussen de andere. Alles
gebeurt op één thread, dus `request_count` heeft geen lock nodig: een
coroutine geeft alleen bij een `await` de beurt door, en in `respond()` zit
geen `await`.

## Leerdoelen

- Hetzelfde protocol in twee talen: wat hangt af van de taal en wat niet?
- `asyncio`: coroutines, `await` en een event loop
- Contract-tests: één testsuite die twee implementaties aan dezelfde afspraken houdt
- Professionele Python-gewoontes: type hints, ruff, pytest, een package met `__main__.py`

## Opdrachten

1. **Opwarmen:** voeg `GET /time` toe, net als bij de C-versie. Zet de test in
   `test_contract.py` en kopieer die naar `c_echo_http`. Slagen beide?
2. **Stille clients:** open met `nc` een verbinding en typ niets. Kun je de
   pagina in je browser nu wél laden? Probeer hetzelfde met de C-server.
3. **JSON:** voeg `GET /json` toe die de request als JSON teruggeeft
   (`json.dumps`). Hoeveel werk is dit in C?
4. **Benchmark:** vergelijk met `ab -n 10000 -c 50` de requests per seconde
   van beide servers. Verklaar het verschil.
5. **Uitdaging:** maak een variant die niet `asyncio` gebruikt maar
   `socketserver.ThreadingTCPServer`. Wat moet er nu wél met een lock?
