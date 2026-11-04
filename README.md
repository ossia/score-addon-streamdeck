# score-addon-streamdeck

Elgato Stream Deck support for [ossia score](https://ossia.io).

The deck is talked to directly over HID (through libossia's `ossia::net::hid_device`,
itself built on SDL's HID API): the Elgato software does not need to be installed,
and should not be running at the same time, as both would draw on the keys.

## The Stream Deck editor

*Windows ▸ Stream Deck…* (Ctrl+Shift+J), or *Edit the layout…* in the context menu
of a deck in the Device Explorer, opens the editor of the decks of the document. The
deck does not need to be plugged in: the layout runs in score, the editor shows it,
and the deck picks it up when it is connected. Every edit can be undone, and applies
to the running deck at once.

- **Pages**, on the left: add, duplicate, delete, reorder, double-click to rename.
  The selected page is the one on the deck, both ways: selecting a page shows it,
  and the list follows when the deck changes page. A *video page* shows the texture
  sent to the device on its keys.
- **Look** of a key: text, colours, text size and position, an image (or drop an
  image file on the key), and a different look while the key is pressed or on.
- **Actions**, on press and on release (or when switched on and off for a toggle
  key): a *trigger*, send a value, toggle a boolean, step a number within its
  range, go to a page, next / previous page, or run a script; each can be delayed.
  A trigger writes to an address the deck creates for it, `deck:/actions/<name>`:
  an impulse, or a boolean each press switches (the key then looks pressed while it
  is on). Map it to whatever it should drive, differently along the score. Presets for
  the transport (play, pause, stop, back to start).
- **Hold to fire**: the key has to be held for a while before anything happens.
- **Feedbacks**: the key changes its look while an address is on, off, equal to,
  above or below something (e.g. green while `score:/running`), or while a script
  is true. The address can be one of the score, or an *input of the deck*,
  `deck:/feedbacks/<name>`, that the deck creates to be mapped to along the score.
- **Dials** (Stream Deck +, Mirabox N3 / N4): an address to turn, a step, a press
  action, and what the strip shows above the dial.
- **Try it**: operate the deck from the editor, without the hardware.
- Drop an address from the Device Explorer on a key to control it: a boolean
  becomes a toggle that lights up while on, an impulse a trigger, anything else a
  key that sends its current value and shows the live one.
- Right-click a key to copy, paste, clear it, or to make it a transport or page key.

## Scripts

Scripts use score's JavaScript API, in the same engine as the console and
`--script`: `Device.read()`, `Device.write()`, `Score.*`, `Util.*`. They also get
`deck` (this deck) and `key` (the key's number, from 1).

- In the text of a key, parts written `${…}` are JavaScript:
  `Level\n${Number(Device.read("osc:/level")).toFixed(2)}`,
  `${deck.pageName()} ${deck.page()}/${deck.pageCount()}`.
- A *Script* action runs a function body:
  `Device.write("osc:/light", 1); if(deck.page() == 1) deck.nextPage();`
- A *script is true* feedback is an expression: `Device.read("osc:/level") > 0.5`.

`deck` has `page()`, `pageName()`, `pageCount()`, `setPage(n)`, `nextPage()`,
`previousPage()`, `brightness()`, `setBrightness(percent)`. A script that fails
shows ⚠ on its key, and its error in the editor. Text and feedback scripts are run often and
must not change anything: there, `deck.setPage()` and the like do nothing. Like
the console, scripts see the devices of the document in front.

## Device tree

| Address | | |
|---|---|---|
| `deck:/` | texture | Shown on the keys of video pages |
| `/page` | int | The page on the deck, from 1; writable |
| `/key/N/pressed` | bool | The physical key, whatever the page; writing it presses the key |
| `/pages/P/key/N/pressed` | bool | Key N, only while page P is shown |
| `/pages/P/key/N/color`, `text`, `text_color`, `image` | | Change a key at run time; an empty text or a transparent colour goes back to the layout |
| `/brightness` | int, 0–100 | |
| `/actions/<name>` | impulse or bool | Written by the keys' triggers |
| `/feedbacks/<name>` | bool or float | Read by the keys' feedbacks |
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

Any deck can be laid out without being plugged in: pick *Any <model>* in the
device's settings.

| Model | USB id | Status |
|---|---|---|
| Stream Deck MK.2 | `0fd9:0080` | tested |
| Stream Deck MK.2 (scissor keys), (v2), MK.2 modules | `0fd9:00a5`, `006d`, `00b9`, `00e4` | same protocol as the MK.2 |
| Stream Deck XL, XL (v2), XL module | `0fd9:006c`, `008f`, `00ba` | untested |
| Stream Deck + | `0fd9:0084` | untested |
| Stream Deck Neo | `0fd9:009a` | untested; the info screen is not driven |
| Stream Deck Mini, Mini (v2), Discord edition, Mini module | `0fd9:0063`, `0090`, `00b3`, `00b8` | untested |
| Stream Deck Pedal | `0fd9:0086` | untested |
| Ajazz AKP153 / AKP153E / AKP153R, Mirabox 293S | `5548:6674`, `0300:1010`, `0300:1020`, `5548:6670` | untested; presses only, no releases |
| Mirabox Stream Dock 293, Redragon SS-550 | `5500:1001`, `0200:1000` | untested; presses only |
| Mirabox Stream Dock 293 V3 | `6603:1005`, `6603:1006` | untested |
| Mirabox N3, Ajazz AKP03 / AKP03R, Soomfon SE, Redragon SS-551 | `6603:1002`, `6603:1003`, `0300:1001`, `0300:1003`, `0300:3003`, `1500:3001`, `0200:2000` | untested; the 3 side buttons are `/touch_key/N` |
| Mirabox N4, Ajazz AKP05 / AKP05E, Soomfon CN003, Redragon SS-552 | `6602:1001`, `6603:1007`, `0300:3006`, `0300:3004`, `1500:3002`, `0200:3001` | untested |

Not supported: the original 2017 Stream Deck (`0fd9:0060`), Stream Deck + XL and
Studio (the references disagree on their key sizes), the Mirabox models whose
references disagree (AKP153 rev. 2, M18, N1, K1 Pro, XL), the Ulanzi D200 (another
protocol, with images sent as zip files) and the Loupedeck / Razer Stream
Controller (serial, not HID).

Mirabox devices are written from the vendor SDK and from the open-source drivers
that agree with it (mirajazz, ajazz-sdk, the Companion and OpenDeck plug-ins); a
model that shows its images rotated, or reports the wrong key, needs its table
entry in `StreamDeck/Model.cpp` adjusted.

## Linux permissions

The user running score needs read / write access to the deck's `/dev/hidraw*` node,
for instance with `/etc/udev/rules.d/70-streamdeck.rules` (one line per vendor id of
the table above; `0fd9` is Elgato):

```
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="0fd9", TAG+="uaccess"
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="5548", TAG+="uaccess"
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="6603", TAG+="uaccess"
```

then `sudo udevadm control --reload-rules && sudo udevadm trigger`, and replug the deck.
