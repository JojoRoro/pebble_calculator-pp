# Pebble Calculator++ / MinusPP

A touch-, button-, and voice-driven calculator built first for the **Pebble Time 2** (`emery`).

The aim is simple: make a calculator that feels like it belongs on a Pebble Time 2 rather than a desktop calculator squeezed onto a watch.

## What works

- **Touch calculator** with a 4x4 keypad plus dedicated Clear, Delete, and Voice controls.
- **Physical-button calculator**: Select starts voice immediately when the app opens; Up/Down enter keypad navigation, after which Select activates the focused key. Back deletes and long Back clears.
- **Voice input** using Pebble's normal Dictation UI. Press Select from the default app state, long-press Select, or tap **VOICE**.
- **Selectable voice-calculation language** in the Pebble phone settings: English, German, or French.
- **Deterministic natural-language parser** with language-specific number grammar and operator vocabulary rather than simple one-to-one word substitution.
- **Normal calculator precedence**, so `2 + 3 x 4` evaluates to `14`.
- Decimal values, unary negative numbers, backspace/editing, result chaining, invalid-expression handling, and division-by-zero handling.
- **Voice Auto/Edit setting** in the Pebble phone app via an offline Clay configuration page.
  - Auto: recognized speech is calculated immediately.
  - Edit: the recognized expression is placed into the calculator for review/editing first.
- **Voice Debug mode** in the Pebble phone app.
  - When enabled, an unparseable voice result displays the complete raw dictation transcript in a box on the watch.
  - Press Back to dismiss the debug transcript and return to the calculator.

All calculation parsing and evaluation is local and deterministic; there is no LLM involved. Voice transcription itself is supplied by Pebble's standard phone-mediated dictation service.

> **Dictation language note:** Pebble's Dictation API does not expose a per-app/per-session speech-recognition language selector. The MinusPP language setting selects the parser vocabulary and grammar used after transcription. The recognition language itself is controlled by Pebble/phone-side dictation configuration.

## Controls

### Touch

The screen contains:

- `C`, `DEL`, `VOICE`
- `7 8 9 /`
- `4 5 6 x`
- `1 2 3 -`
- `0 . = +`

### Physical buttons

- **Select on app open / before keypad navigation:** start voice input
- **Up / Down:** enter keypad navigation and move the keypad focus (hold to move quickly)
- **Select after keypad focus is active:** activate the focused key
- **Hold Select:** start voice input
- **Back:** delete one character; when already empty, leave the app
- **Hold Back:** clear the calculation
- **Back while a debug transcript is displayed:** dismiss the transcript

## Voice languages

Choose **Calculation language** in the Pebble phone app and tap **Save settings**. The selected language is persisted on the watch. The small status line shows `EN`, `DE`, or `FR` so you can confirm the parser currently in use.

### English

Examples:

- `twenty two divided by fifty five plus eight`
- `five over ten`
- `fifty point five plus two`
- `negative five plus two`

Common operators and forms include `plus`, `minus`, `times`, `multiplied by`, `divided by`, `over`, `point`, `equals`, and common English number words through millions.

### German

Examples:

- `fünf plus zehn`
- `fünf geteilt durch zehn`
- `fünf mal zehn`
- `fünfundfünfzig geteilt durch sieben`
- `einundzwanzig plus neun`
- `einhundertfünfundzwanzig minus fünfundzwanzig`
- `zwei komma fünf mal vier`
- `minus fünf plus zwei`

The German parser understands German compound-number structure such as `einundzwanzig`, `fünfundfünfzig`, `einhundert...`, and `...tausend...`; it also accepts umlauts/ß and common ASCII transcription variants. Operators include forms such as `plus`, `minus`, `mal`, `multipliziert mit`, `geteilt durch`, `dividiert durch`, and `gleich`.

### French

Examples:

- `cinq plus dix`
- `cinq divisé par dix`
- `cinq sur dix`
- `cinquante-cinq divisé par sept`
- `vingt et un plus neuf`
- `quatre-vingt-dix plus dix`
- `cent vingt-cinq moins vingt-cinq`
- `deux virgule cinq fois quatre`
- `moins cinq plus deux`

The French parser handles hyphenated number words and French-specific constructions such as `soixante-dix`, `quatre-vingts`, and `quatre-vingt-dix` instead of treating them as English-style tens. Operators include `plus`, `moins`, `fois`, `multiplié par`, `divisé par`, `sur`, and `égal`.

For German and French numeric transcripts, comma decimals such as `5,5` are normalized to the calculator's internal decimal representation. Sentence-ending punctuation added by dictation is ignored.

## Target

The current release intentionally targets **Pebble Time 2 only** so the UI can use its 200x228 display, touchscreen, microphone/dictation support, and physical buttons directly. Other Pebble models can be added later with platform-specific layouts and fallbacks.

## Build

The project is intended to build directly in CloudPebble. For local builds with current Pebble tooling installed:

```sh
uv tool install pebble-tool
pebble sdk install latest
npm install
pebble build
```

Install to the Pebble Time 2 emulator:

```sh
pebble install --emulator emery --logs
```

There is no GitHub Actions build workflow; CloudPebble is the primary build path.

## Project structure

- `src/c/main.c` — Pebble Time 2 UI, touch/buttons, dictation, settings transport
- `src/c/calculator.c` — deterministic calculator state and expression evaluator
- `src/c/parser.c` — English deterministic voice parser
- `src/c/parser_languages.c` — German/French token normalization, grammar, and operator parsing
- `src/pkjs/` — phone-side Clay settings page
- `tests/test_logic.c` — host-side parser/evaluator regression tests

## License

MIT License
