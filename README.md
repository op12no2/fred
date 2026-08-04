# Fred

Fred is a fleet of small home robots, f0 to fN, each evolving from the
last. One repo, one directory per robot, each self-contained with its own
README etc..

| Robot | Function | 
|-------|----------|
| [f0](./f0/) | Analog | 
| [f1](./f1/) | Shy but curious |
| [f2](./f2/) | WIP |

## Fleet communications

With the exception of f0, the fleet talks over ESP-NOW broadcasts (connectionless 2.4 GHz,
no router, no pairing). Every member shouts a small status beacon and
anyone may listen. Received signal strength doubles as a crude "how close
is my sibling" sense.

Protocol v0 (draft, until two members exist to argue about it):

- Broadcast a beacon at ~1 Hz: `{ proto_version, robot_id, state, detail }`.
  For example f2 sends its mode (scanning / locked / approaching / low
  battery) and the bearing of the warmest thing it sees.
- Sound and light are presentation, not transport: robots hear each
  other by radio and perform the conversation aloud - chirps and faces
  for the humans in the room.

## Photos

See https://photos.app.goo.gl/y3BawrzCdSZkWBeVA

## License

See [LICENSE](./LICENSE).

