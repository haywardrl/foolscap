# Foolscap

A distraction-free e-ink writing device. Open firmware for ESP32-S3, written in C, with a desktop simulator. Early development.

## Status

In active early development. Not yet usable. Building toward a v0.1 simulator-only milestone first; hardware port follows.

## Design goals

- Distraction-free writing experience on e-ink
- Open firmware, hackable, well-documented
- Same source builds for ESP32-S3 hardware and a desktop simulator
- Plain text and Markdown only — no proprietary formats
- Sub-100ms keystroke-to-pixel latency on hardware

## Targets

- ESP32-S3 (LilyGo T5 4.7" V2.3, parallel e-ink panel)
- Desktop simulator (Linux, SDL2)

## Building

See [BUILDING.md](BUILDING.md).

## Changelog

See [CHANGELOG.md](CHANGELOG.md)

## License

MIT — see [LICENSE](LICENSE).

## Acknowledgement

Bundled with IBM Plex Mono, copyright 2017 IBM Corp, licensed under the SIL Open Font License v1.1
