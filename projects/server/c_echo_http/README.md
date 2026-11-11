# Echo-webserver (C, POSIX sockets)

Een webserver van een paar honderd regels C, zonder bibliotheken: alleen
`socket`, `bind`, `listen`, `accept`, `recv` en `send`. Open hem in je browser
en je ziet precies wat je browser stuurt.

Dezelfde server bestaat in Python: [`../py_echo_http`](../py_echo_http/).
Beide moeten slagen voor dezelfde contract-tests.

## Starten

```bash
pixi run start
```

Open daarna <http://127.0.0.1:8080> en <http://127.0.0.1:8080/raw>, of gebruik de terminal:

```bash
curl -v http://127.0.0.1:8080/raw
```

```bash
printf 'GET / HTTP/1.1\r\nHost: ik\r\n\r\n' | nc 127.0.0.1 8080
```

Werk je via SSH op een andere machine? Stuur de poort door met
`ssh -L 8080:127.0.0.1:8080 pc` en open de link in de browser op je eigen laptop.

| Optie | Betekenis |
|---|---|
| `--port N` | TCP-poort (standaard 8080; `0` = een vrije poort, die de server meldt) |
| `--host ADDR` | Adres om op te luisteren (standaard `127.0.0.1`; `0.0.0.0` = vanaf het netwerk bereikbaar) |

| Taak | Wat het doet |
|---|---|
| `pixi run start` | Bouwen en starten op poort 8080 |
| `pixi run test` | Unittests van parser en antwoorden (ctest) en contract-tests tegen de draaiende server (pytest) |
| `pixi run smoke` | Starten, twee pagina's ophalen, stoppen |
| `pixi run build-debug` / `test-debug` | Alles nog eens met AddressSanitizer en UBSan, de contract-tests ook |

## Wat de server doet

| Request | Antwoord |
|---|---|
| `GET /` | `200`: HTML-pagina met je request-regel en alle headers |
| `GET /raw` | `200`: de request precies zoals ontvangen, als `text/plain` |
| `HEAD /`, `HEAD /raw` | Dezelfde headers als `GET`, zonder body |
| Onbekend pad | `404 Not Found` |
| Andere methode (`POST`, ...) | `405 Method Not Allowed` met `Allow: GET, HEAD` |
| Onleesbare request | `400 Bad Request` |
| Ander HTTP-versie dan 1.0/1.1 | `505 HTTP Version Not Supported` |
| Head > 8 KiB of > 32 headers | `431 Request Header Fields Too Large` |

Elk antwoord heeft `Content-Length`, `Connection: close` en `X-Request-Count`
(het hoeveelste antwoord sinds de start).

## Hoe het werkt

| Bestand | Inhoud |
|---|---|
| `src/http.h/.c` | De request-head parsen, HTML-escaping. Geen sockets. |
| `src/app.h/.c` | Routing: van request-bytes naar response-bytes. Geen sockets. |
| `src/main.c` | Het netwerkdeel: luisteren, verbindingen aannemen, lezen, schrijven |
| `tests/http_test.c` | Unittests (C) voor parser en antwoorden |
| `tests/test_contract.py` | Contract-tests (Python), identiek aan die in `py_echo_http` |

**TCP is een stroom, geen berichtenkanaal.** Eén `recv()` geeft niet per se de
hele request. De server leest door tot de lege regel die de head afsluit.
`test_request_in_pieces` stuurt de request expres in drie stukken.

**Geen kopieën.** De parser kopieert geen strings. Elk veld is een pointer naar
de ontvangstbuffer plus een lengte (`http_str_t`). Dat is snel en heeft geen
`malloc` nodig, maar zo'n veld is alleen geldig zolang die buffer bestaat.

**Begrensde buffers.** Elke buffer heeft een vaste maximale grootte
(`HTTP_MAX_HEAD`, `APP_MAX_BODY`) en de code controleert die overal. Een
client die te veel stuurt, krijgt `431` en geen buffer overflow.

**Escaping.** Alles wat de client stuurt, wordt ge-escaped voordat het in de
HTML-pagina komt. Stuur maar eens `X-Naam: <script>alert(1)</script>` mee.
Zonder escaping is dat een XSS-lek.

**Netjes afsluiten.** Sluit je een socket terwijl er nog ongelezen data
binnenstaat, dan stuurt de kernel een TCP-reset en kan de client je antwoord
kwijtraken. Daarom doet `s_drain()` eerst `shutdown(SHUT_WR)` en leest dan de
rest weg.

**Ctrl+C.** Een signal-handler zet een vlag, en `accept()` wordt onderbroken met
`EINTR`. De lus stopt daarna netjes. De contract-tests controleren dat de
server met exitcode 0 stopt.

## Leerdoelen

- De socket-API: `socket` → `bind` → `listen` → `accept` → `recv`/`send` → `close`
- HTTP/1.1 als tekstprotocol: request-regel, headers, statuscodes
- Veilig omgaan met invoer: begrensde buffers, geen `strcpy`, escaping
- Signals en `EINTR`
- Testen op twee niveaus: unittests van de logica en contract-tests van buitenaf

## Opdrachten

1. **Opwarmen:** voeg een route `GET /time` toe die de tijd van de server teruggeeft.
   Schrijf eerst de test.
2. **Bestanden serveren:** laat `GET /static/<naam>` een bestand uit een map
   `www/` sturen, met het juiste `Content-Type`. Pas op voor `../`: probeer
   zelf eerst `/static/../../etc/passwd`.
3. **Traag is gevaarlijk:** open met `nc` een verbinding en typ niets. Probeer
   in een tweede terminal de pagina te laden. Wat gebeurt er, en waarom?
4. **Gelijktijdig:** maak de server concurrent, op drie manieren: `fork()` per
   client, een thread per client, en één thread met `poll()`. Wat zijn de voor-
   en nadelen? (Let op: `request_count` wordt dan gedeeld.)
5. **Benchmark:** meet met `ab` of `wrk` hoeveel requests per seconde de C- en
   de Python-server halen, iteratief en concurrent.
6. **Uitdaging:** ondersteun `Connection: keep-alive`, zodat een browser meer
   requests over dezelfde verbinding stuurt. Wat moet er dan veranderen aan het
   lezen?
