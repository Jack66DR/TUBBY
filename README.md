# TUBBY

**manners! DSP** dub delay. AU and VST3, macOS universal (Apple Silicon and Intel).

A tape echo on one loop of tape, two heads, ping to pong. Every repeat goes darker, hairier and more lopsided than the one before.
Turn TIME while it's going and the tape changes speed, so the repeats bend. Push FEEDBACK past 100 % and it sings on its own.
Two springs drip under it, and you can kick them. A big desk filter sweeps the returns. Hiss and mains hum come free.

Put it on a send.

## Build

GitHub Actions builds it on every push to `main` (Actions tab, "Build TUBBY"). It also runs the offline engine test and Apple's `auval` check.
Download the `TUBBY-macOS` artifact from the finished run. Inside are `TUBBY-AU.zip` and `TUBBY-VST3.zip`.

## Install

```sh
sudo unzip -o TUBBY-AU.zip -d /Library/Audio/Plug-Ins/Components/
sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/Components/TUBBY.component
```

Then in Logic: put it on an Aux, Audio FX > manners! DSP > TUBBY. If it doesn't appear, Plug-in Manager, find it, Reset & Rescan Selection.
The build is ad hoc signed, not notarised, which is why the `xattr` line is needed.

## Signal flow

```
input ─► IN ─► DRIVE ─► SEND (OPEN, or while DUB is held) ─► tape A ─► repeat ─► tape B ─► repeat ─┐
                 │                                             ▲                                    │
                 │                                             └── saturation, TONE, THIN, hiss ◄───┘  (FEEDBACK)
                 │                                                                    │
                 └──── FEED (dry ◄─► echo) ──► two SPRINGS ◄── KICK / SHAKE           │
                                                    │                                 ▼
                                                    └──────────────► + ─► FILTER (ALTEC) ─► + hiss and hum ─► MIX ─► OUT
```

## Controls

| Control | What it does |
|---|---|
| **ECHO** | |
| TIME | SYNC: 1/16 to 1 bar, dotted and triplets included, on Logic's tempo. FREE: 30 ms to 2 s. Moving it glides, like a tape machine changing speed. |
| FEEDBACK | 0 to 125 %. Past 100 % the loop runs away and sings; the tape saturation keeps it from blowing up. |
| TONE | Low pass inside the loop, 600 Hz to 9 kHz. Each repeat gets darker. Low is properly dark. |
| THIN | High pass inside the loop, 20 to 900 Hz. Each repeat loses a little bottom, so long tails don't turn to mud. |
| WOBBLE | Wow, flutter and drift on the heads. Low is a tired machine, high is seasick. |
| SPREAD | 0 is mono. Up is ping-pong: first repeat on the left, the next on the right. |
| **SPRING** | |
| SPRING | How much spring you hear. |
| DECAY | How long the springs ring. |
| TENSION | Spring length. Low is short and tight, high is long and loose. |
| DRIP | The chirp. Low is a plain reverb, high is the full boing. |
| FEED | What the springs hear. Left: only the dry input. Right: only the echoes. Between: both. |
| SHAKE | Hard hits in the input knock the springs, like a heavy-handed engineer. 0 is off. |
| **DESK** | |
| DRIVE | The preamp before the tape. More grit, more tape compression. |
| NOISE | Tape hiss in the loop (so it builds up with the repeats), plus a hiss and 50 Hz hum floor under everything. 0 is silent. |
| FILTER | The big knob on the returns. Centre is flat. Right is a resonant high pass, 40 Hz to 4 kHz. Left is a resonant low pass, 14 kHz down to 350 Hz. |
| **OUTPUT** | |
| IN, OUT | Input and output gain. |
| MIX | 100 % (wet only) by default, for a send. Lower it if you use TUBBY as an insert. |
| **Switches** | |
| SYNC | Lit: TIME is a note division. Unlit: milliseconds. |
| HOLD | Freezes the loop: nothing new gets on the tape and what's there goes round and round. |
| ALTEC | The FILTER clicks between ten positions, like the old stepped one. |
| OPEN | Lit: everything goes to tape. Unlit: nothing does, except while DUB is held. |
| DUB | Hold to throw the input onto the tape. Automate it for throws on the snare. |
| KICK | Crash the springs. |

## Presets

Eight in the SET selector (and Logic's preset menu). A preset sets the sound and leaves IN, OUT, MIX and the switches you play alone.

KING, SCIENTIST, BLACK ARK, LANKY, SIREN, DRIP, HI-FI, CHANNEL ONE.

## Screen

Yellow phosphor. The monitor shows the last four seconds of tape with the echo heads marked, fading with the feedback,
and the two springs, which shake with what's in them. GLIDE shows while the tape is changing speed; RUNAWAY blinks past 100 %.
A crash knocks the picture. The title has its own echoes, which light up when you throw.
The boot sequence plays the first time the window opens in a session.

## Local tools

Configure with `-DTBY_BUILD_TESTS=ON` to also build:

- `TUBBY_Test`: the offline engine checks (CI runs this): timing, ping-pong, runaway, darkness, throws, hold, springs, crash, shake, filter, noise floor, glide.
- `TUBBY_Tool render out.wav [preset] [open|throw]`: plays a one drop riff at 75 BPM through the plugin.
- `TUBBY_Tool snapshot out.png [crt|runaway|boot]`: renders the editor.

The terminal font is Share Tech Mono, under the SIL Open Font Licence (`Resources/ShareTechMono-OFL.txt`).
