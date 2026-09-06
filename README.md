# Pebble Calculator++ / MinusPP

A touch-, button-, and voice-driven calculator built first for the **Pebble Time 2** (`emery`).

The aim is simple: make a calculator that feels like it belongs on a Pebble Time 2 rather than a desktop calculator squeezed onto a watch.

## What works

- **Touch calculator** with a 4x4 keypad plus dedicated Clear, Delete, and Voice controls.
- **Physical-button calculator**: Up/Down move through keypad choices, Select activates the focused key, Back deletes, and long Back clears.
- **Voice input** using Pebble's normal Dictation UI. Long-press Select or tap **VOICE**.
- **Deterministic natural-language parser** for common phrases such as:
  - `22 divided by 55 plus 8 equals`
  - `twenty two divided by fifty five plus eight`
  - `10 times 5`
  - `one hundred minus twenty`
  - `fifty point five plus two`
- **Normal calculator precedence**, so `2 + 3 x 4` evaluates to `14`.
- Decimal values, unary negative numbers, backspace/editing, result chaining, invalid-expression handling, and division-by-zero handling.
- **Voice Auto/Edit setting** in the Pebble phone app via an offline Clay configuration page.
  - Auto: recognized speech is calculated immediately.
  - Edit: the recognized expression is placed into the calculator for review/editing first.
- **Voice Debug mode** in the Pebble phone app.
  - When enabled, an unparseable voice result displays the complete raw dictation transcript in a box on the watch.
  - Press Back to dismiss the debug transcript and return to the calculator.

All calculation parsing and evaluation is local and deterministic; there is no LLM involved. Voice transcription itself is supplied by Pebble's standard phone-mediated dictation service.

## Controls

### Touch

The screen contains:

- `C`, `DEL`, `VOICE`
- `7 8 9 /`
- `4 5 6 x`
- `1 2 3 -`
- `0 . = +`

### Physical buttons

- **Up / Down:** move the keypad focus (hold to move quickly)
- **Select:** activate the focused key
- **Hold Select:** start voice input
- **Back:** delete one character; when already empty, leave the app
- **Hold Back:** clear the calculation
- **Back while a debug transcript is displayed:** dismiss the transcript

## Voice vocabulary

The parser understands numeric digits and common English number words, plus phrases including:

- plus / add
- minus / subtract
- times / multiply / multiplied by
- divided by / divide / over
- point / dot
- equals / equal
- negative

Number words support values through millions and common constructions such as `one hundred and five`.

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
- `src/c/parser.c` — deterministic voice-text-to-expression parser
- `src/pkjs/` — phone-side Clay settings page
- `tests/test_logic.c` — host-side parser/evaluator regression tests

## License

MIT License
