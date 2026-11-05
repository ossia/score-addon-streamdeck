# score-addon-streamdeck

Elgato Stream Deck and Mirabox / Ajazz Stream Dock support for [ossia score](https://ossia.io): pages of keys with actions, feedbacks and scripts, addresses to map along the score, key faces from a texture, and a remote control mode.

Documentation: [Stream Deck device](https://ossia.io/score-docs/devices/streamdeck-device.html) in the score manual.

## Building

Build it as part of score: clone it into `score/src/addons`. It needs score's HID support (`OSSIA_PROTOCOL_HID`).

With `SCORE_TESTING`, `score_addon_streamdeck_models` checks every model of the table in `StreamDeck/Model.cpp`.

## Adding a model

A model is an entry of the table in `StreamDeck/Model.cpp`: USB ids, key grid and image size, image transform, report size, and the key maps of Mirabox devices. If a deck shows its images rotated, or reports the wrong keys, its entry needs fixing.
