# Progress log

## 2026-09-17 — Phase 0 done · Phase 2 drafted · Phase 1 in progress

**Done**
- Phase 0: contracts in `docs/` (decisions, BLE protocol, Firestore schema, theme, hardware, build plan), repo skeleton, base commit on `main`.
- Phase 2 (draft PR `firebase/rules-and-readme`): Firestore rules, the 4 composite indexes, Hosting config, a rules test-suite (needs Java + the emulator to run), project README, `docs/setup-firebase.md`.

**In progress**
- Phase 1 firmware (branch `firmware/band-v0`): `firmware/safelink_band/` Arduino C++ sketch — BLE service per `docs/ble-protocol.md`, button/touch hold, LED patterns, seq persistence, optional GPS + battery. Compile check against ESP32 core 3.3.11 + NimBLE-Arduino 2.5.1 pending. Must ship with a proper "how to connect" guide and diagrams (wiring, state machine, SOS sequence).

**Blocked**
- Firebase project: the Google account is out of Google Cloud project quota ("exceeded your allotted project quota") and has no Firebase projects. Options: another Google account · reuse an existing GCP project (`firebase projects:addfirebase <id>`) · free a slot. Until then `.firebaserc` keeps `safelink-REPLACE_ME` and `firebase deploy --only firestore` (the Phase 2 acceptance check) is untested.

**Next**
1. Finish Phase 1: compile → flash → nRF Connect test → commit + PR.
2. Unblock the Firebase project → set `.firebaserc` → deploy rules + indexes.
3. Config and secrets live in `.env` files that are never committed (`web/.env.local`, app config); commit `.env.example` templates only.
4. Phase 3 (web dashboard), Phase 4 (Flutter app).
