# score-addon-streamdeck

Elgato Stream Deck support for [ossia score](https://ossia.io).

The deck is talked to directly over HID (through libossia's `ossia::net::hid_device`,
itself built on SDL's HID API): the Elgato software does not need to be installed,
and should not be running at the same time, as both would draw on the keys.

## Device tree

| Address | Type | |
|---|---|---|
| `deck:/` | texture | Connect a video output here: it is cut into the key images |
| `/key/N/pressed` | bool | |
| `/key/N/color` | rgba | Key background |
| `/key/N/text`, `/key/N/text_color`, `/key/N/text_size` | | `text_size` 0 fits the text to the key |
| `/key/N/image` | string | Path to an image, scaled to cover the key |
| `/brightness` | int, 0–100 | |
| `/clear` | impulse | Blanks every key |
| `/encoder/N/pressed`, `/encoder/N/turn`, `/encoder/N/position` | | Stream Deck + |
| `/lcd/N/...` | | Stream Deck +: one face per dial, on the touch strip |
| `/touch/tap`, `/touch/long_press`, `/touch/swipe` | | Stream Deck +, in strip pixels |
| `/touch_key/N/pressed` | bool | Stream Deck Neo |
| `/info/model`, `/info/serial`, `/info/firmware` | string | |

A key shows whatever changed last: a parameter write, or a change in its part of
the texture. A texture that does not change does not overwrite the parameters.

The texture is the keys laid out on their grid, with a gap of a quarter of a key
between them (and, on the Stream Deck +, the touch strip below). Its size is shown
in the device's settings.

## Remote control mode

The bottom row is the transport (play, stop, previous / next bank and, with five
columns or more, the time). The other keys, and the dials of the Stream Deck +,
show the controls of the selected process: a press toggles a boolean, fires an
impulse, or adds 10 % to a number; a dial turn moves its control by 1 % per step.

## Models

| Model | Product id | Status |
|---|---|---|
| Stream Deck MK.2 | `0x0080` | tested |
| Stream Deck MK.2 (scissor keys), Stream Deck (v2) | `0x00a5`, `0x006d` | same protocol as the MK.2 |
| Stream Deck XL, XL (v2) | `0x006c`, `0x008f` | untested |
| Stream Deck + | `0x0084` | untested |
| Stream Deck Neo | `0x009a` | untested; the info screen is not driven |
| Stream Deck Mini, Mini (v2) | `0x0063`, `0x0090` | untested |
| Stream Deck Pedal | `0x0086` | untested |

The original 2017 Stream Deck (`0x0060`) is not supported.

## Linux permissions

The user running score needs read / write access to the deck's `/dev/hidraw*` node,
for instance with `/etc/udev/rules.d/70-streamdeck.rules`:

```
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="0fd9", TAG+="uaccess"
```

then `sudo udevadm control --reload-rules && sudo udevadm trigger`, and replug the deck.
