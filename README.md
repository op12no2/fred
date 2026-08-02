# Fred

Fred is a fleet of small home robots, f0 to fN, each evolving from the
last. One repo, one directory per robot, each self-contained with its own
README etc..

| Robot | What it is |
|-------|------------|
| [f0](./f0/) | The ancestor. Analog button-bot: press the big red button, he spins; release, he winds down like a music box. No code, nothing digital by rule. |
| [f1](./f1/) | Heat-seeker: spins until his thermal camera sees warmth, then approaches. Thermal eyes, two motors, nothing else. Building now. |
| [f2](./f2/) | f1 evolved: 4WD chassis, face, voice, distance sense, radio, LiPo power. Parts ordered. |
| f3+ | Future. Each inherits the fleet protocol and evolves the body. |

## Fleet comms

With the exception of f0, the fleet talks over **ESP-NOW broadcast** (connectionless 2.4 GHz,
no router, no pairing): every member shouts a small status beacon and
anyone may listen. Received signal strength doubles as a crude "how close
is my sibling" sense.

Protocol v0 (draft, until two members exist to argue about it):

- Broadcast a beacon ~1 Hz: `{ proto_version, robot_id, state, detail }`
  — e.g. f2 sends its mode (scanning / locked / approaching / low
  battery) and the bearing of the warmest thing it sees.
- Sound and light are presentation, not transport: robots *hear* each
  other by radio and *perform* the conversation aloud — chirps and faces
  — for the humans in the room.

## Photos

https://photos.app.goo.gl/y3BawrzCdSZkWBeVA

## License

See [LICENSE](./LICENSE).

