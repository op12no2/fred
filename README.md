# Fred — a small fleet

Fred is a fleet of small home robots, f0 to fN, each evolving from the
last. One repo, one directory per robot, each self-contained with its own
README and `doc/` (parts list, schematic; firmware where the rule allows).

| Robot | What it is |
|-------|------------|
| [f0](./f0/) | The ancestor. Chipless button-bot: press the big red button, he spins; release, he winds down like a music box. No code, by rule. |
| [f1](./f1/) | Fred himself. Heat-seeker: spins until his thermal camera sees warmth, then approaches. Face, voice, distance sense, radio. Current focus. |
| f2+ | Future. Each inherits the fleet protocol and evolves the body. |

## Common ground

Fleet members (f1 onward) share a pattern rather than a platform:

- An **ESP32-S3** brain; TT gear motors through DRV8833 drivers.
- One shared **I2C bus** of STEMMA QT/Qwiic boards for senses and
  expression — thermal camera, time-of-flight, LED matrix face, buzzer
  voice. Plug-together, address-per-device, no extra GPIOs.
- A single battery through an **XT60 pull-loop key** — no power switches.

## Fleet comms

The fleet talks over **ESP-NOW broadcast** (connectionless 2.4 GHz,
no router, no pairing): every member shouts a small status beacon and
anyone may listen. Received signal strength doubles as a crude "how close
is my sibling" sense.

Protocol v0 (draft, until two members exist to argue about it):

- Broadcast a beacon ~1 Hz: `{ proto_version, robot_id, state, detail }`
  — e.g. f1 sends its mode (scanning / locked / approaching / low
  battery) and the bearing of the warmest thing it sees.
- Sound and light are presentation, not transport: robots *hear* each
  other by radio and *perform* the conversation aloud — chirps and faces
  — for the humans in the room.
- Members may transmit without ever receiving (this is f0's future
  loophole: a taped-on beacon that says hi when his button is pressed,
  while the robot underneath stays proudly analog).

## License

See [LICENSE](./LICENSE).
