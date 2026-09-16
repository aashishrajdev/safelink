# SafeLink — locked decisions (prototype v0)

Last updated: 2026-09-17. Change here first, then in code.

## Scope
- Goal: a **working prototype**: band press → phone → Firestore → web dashboard shows a live alert with location, in ≤ 5 s on the demo Wi-Fi.
- Out of scope for v0: signed BLE messages, automatic calls, duress PIN, journey mode, fall detection, indoor positioning, Cloud Functions, push notifications.
- **No SIM in the test phone** → no SMS in v0. Contacts' phone numbers are stored so SMS can be added later (see `build-plan.md`).
- Alert delivery in v0 = Firestore realtime → web dashboard (guardians log in with their email) + the wearer's own app.

## Hardware (what we own)
ESP32 NodeMCU-32 (38-pin), PBS-11B 12 mm push button, TTP223 touch module, u-blox NEO-6M GPS, TP4056 Type-C charger + 3.7 V 500 mAh LiPo, breadboard, jumpers, berg strips, soldering kit. No buzzer / vibration motor → LED-only feedback. Wiring in `hardware.md`.

## Firmware
- Language: **C++ (Arduino framework)**, edited/flashed with **Arduino IDE 2.x**; `arduino-cli` for compile checks. One sketch folder: `firmware/safelink_band/`.
- BLE stack: **NimBLE-Arduino**. GPS parsing: **TinyGPSPlus** (compile-time optional).
- Trigger: hold push button ≥ 2 s = SOS. TTP223 touch hold ≥ 2 s = SOS too (discreet). Short press = ignored.
- Cancel: only from the app (CANCEL command over BLE).
- GPS on the band: **optional fallback**. Phone GPS is primary (as in the paper). If the NEO-6M is wired and has a fix, the band also streams its fix; the app prefers the phone fix unless the phone has none or it is worse.
- Security v0: plain GATT (`SAFELINK_REQUIRE_ENC 0`). Flip to 1 for a bonded + encrypted link once the flow works.

## Mobile app
- **Flutter**, Android only (iOS untested). Test phones: Android 15/16 — Motorola, Samsung, Nothing.
- Talks to **Firestore directly** (no backend). Firebase Auth email + password.
- Location: immediate fix on SOS, then **every N minutes** until cancelled. N is a user setting: 5 / 10 / 15 / 30, **minimum 5**, default 5.
- Background: a foreground service keeps the BLE link alive with the screen locked. v0 target = app in background; "app swiped away" is a later phase.

## Web
- **Next.js (App Router) + TypeScript + Tailwind**, Firebase Web SDK **client-side only** (no API routes, no server code). Static-export compatible (`output: 'export'`) so it deploys to Firebase Hosting (free) or Vercel.
- Pages: landing (deck theme) → login/register → dashboard (live alerts, map, history, band status, acknowledge) → alert detail.
- Map: Leaflet + OpenStreetMap tiles (free, no API key).

## Firebase
- Spark (free) plan: Auth, Firestore, Hosting. No Cloud Functions.
- Security rules: signed-in only; owner reads/writes own data; guardians (matched by email) read alerts they are listed on and may acknowledge/resolve. See `firestore-schema.md`.

## Repo
- Monorepo: `firmware/` `app/` `web/` `firebase/` `docs/`.
- One branch + PR per module; `main` changes only via PR. No Claude attribution in commits or PRs.
