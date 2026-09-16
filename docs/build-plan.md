# Build plan

| # | Phase | Deliverable | Done when |
|---|---|---|---|
| 0 | Contracts + skeleton | `docs/` contracts, repo layout, Firebase project | agents can build against the docs |
| 1 | Firmware v0 | `firmware/safelink_band/` — BLE service per `ble-protocol.md`, button / touch hold, LED patterns, seq persistence, optional GPS + battery | nRF Connect shows the service; hold → SOS notify; ACK / CANCEL work |
| 2 | Firebase | rules, indexes, hosting config | rules match `firestore-schema.md`; `firebase deploy --only firestore` succeeds |
| 3 | Web | landing (deck theme) + login + live dashboard + alert detail | a guardian sees an alert appear live with a map and can acknowledge |
| 4 | App | Flutter: auth, contacts, pair band, SOS engine (BLE → Firestore → periodic location), history, settings | phone locked, band pressed → alert on the dashboard in ≤ 5 s |
| 5 | Integration demo | end-to-end on a Wi-Fi hotspot; README demo script | rehearsed twice |
| 6 | Hardening (later) | bonded BLE, background survival on OEM phones, SMS once a SIM exists, enclosure | — |

Later ideas from the design exploration: instant-then-refine location, guardian reply / escalation, duress cancel, journey mode, fall detection, relay through nearby phones.
