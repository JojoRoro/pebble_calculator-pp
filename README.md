# Pebble Calculator++

A modern calculator app for Pebble smartwatches, starting with the **Pebble Time 2**.

## Goals

Pebble Calculator++ aims to provide a fast calculator experience using all available watch inputs:

- Touchscreen input on Pebble Time 2
- Physical button input
- Voice input through the existing Pebble Android app voice flow

## Voice input

The goal is to allow natural expressions such as:

> "22 divided by 55 plus 8 equals"

The Pebble Android app handles speech-to-text. The resulting text is then converted into an equation using a deterministic parser.

No LLM is required. The parser uses normal logic to recognize operators such as:

- plus
- minus / subtract
- multiplied by / times
- divided by
- equals

The same calculation pipeline is used regardless of whether input came from touch, buttons, or voice.

## Planned features

- [ ] Basic calculator UI
- [ ] Pebble Time 2 touchscreen support
- [ ] Button input support
- [ ] Voice expression parsing
- [ ] Configurable voice behavior:
  - Automatically calculate after voice input
  - Allow editing before calculating
- [ ] Support additional Pebble models later

## Development

This project is initially optimized for Pebble Time 2.

## License

MIT License
