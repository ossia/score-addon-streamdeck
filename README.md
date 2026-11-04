# score-addon-streamdeck

Elgato Stream Deck support for [ossia score](https://ossia.io).

The deck is talked to directly over HID (through libossia's `ossia::net::hid_device`,
itself built on SDL's HID API): the Elgato software does not need to be installed,
and should not be running at the same time, as both would draw on the keys.

## The Stream Deck panel

*View ▸ Stream Deck* (Ctrl+Shift+J) lays out the decks of the document. The deck
does not need to be plugged: the layout runs in score, the panel shows it, and the
deck picks it up when it is connected. Every edit can be undone, and applies to the
running deck at once.

- **Pages**: add, rename, duplicate, reorder; double-click a page to show it on the
  deck. A *video* page shows the texture sent to the device on its keys.
- **Look** of a key: text (with live values: `$(device:/address)`, `$(page)`),
  colours, text size and position, an image (or drop an image file on the key), and
  a different look while the key is pressed or switched on.
- **Actions**, on press and on release (or when switched on and off for a toggle
  key): send a value, toggle a boolean, step a number within its range, go to a
  page, next / previous page; each can be delayed. Presets for the transport
  (play, pause, stop, back to start).
- **Hold to fire**: the key has to be held for a while before anything happens.
- **Feedbacks**: the key changes its look while a value of the document is on,
  off, equal to, above or below something (e.g. green while `score:/running`).
- **Dials** of the Stream Deck +: an address to turn, a step, a press action, and
  what the touch strip shows above the dial.
- **Test** mode: operate the deck from the panel, without the hardware.
- Drop an address from the Device Explorer on a key to control it: a boolean
  becomes a toggle that lights up while on, an impulse a trigger, anything else a
  key that sends its current value and shows the live one.
- Right-click a key to copy, paste, clear it, or to make it a transport or page key.

## Device tree

| Address | | |
|---|---|---|
| `deck:/` | texture | Shown on the keys of video pages |
| `/page` | int | The page on the deck, from 1; writable |
| `/key/N/pressed` | bool | The physical key, whatever the page; writing it presses the key |
| `/pages/P/key/N/pressed` | bool | Key N, only while page P is shown |
| `/pages/P/key/N/color`, `text`, `text_color`, `image` | | Change a key at run time; an empty text or a transparent colour goes back to the layout |
| `/brightness` | int, 0–100 | |
| `/encoder/N/pressed`, `/encoder/N/turn`, `/encoder/N/position` | | Stream Deck + |
| `/touch/tap`, `/touch/long_press`, `/touch/swipe` | | Stream Deck +, in strip pixels |
| `/touch_key/N/pressed` | bool | Stream Deck Neo |
| `/info/model`, `/info/serial`, `/info/firmware` | string | |

## Remote control mode

Set in the device's settings. The bottom row is the transport (play, stop,
previous / next bank and, with five columns or more, the time). The other keys,
and the dials of the Stream Deck +, show the controls of the selected process: a
press toggles a boolean, fires an impulse, or adds 10 % to a number; a dial turn
moves its control by 1 % per step.

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
