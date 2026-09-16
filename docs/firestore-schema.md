# Firestore schema (v0)

Single source of truth for `app/` (Flutter), `web/` (Next.js) and `firebase/firestore.rules`.
Both clients talk to Firestore **directly** — there is no backend. Denormalise instead of joining.

Conventions: timestamps are Firestore `Timestamp`s; `createdAt` / `updatedAt` use `serverTimestamp()`; emails are stored **lower-case**; phone numbers in E.164 (`+91…`).

## `users/{uid}` — one per account (wearer or guardian; same doc shape)
```jsonc
{
  "displayName": "Anand Raj",
  "email": "anand@example.com",          // lower-case, mirrors Auth
  "phone": "+919999999999",              // optional
  "createdAt": Timestamp,
  "settings": {
    "locationIntervalMin": 5,            // 5 | 10 | 15 | 30 — minimum 5
    "sosHoldMs": 2000                    // informational; the band decides
  },
  "band": {                              // written by the app; read by the owner's dashboard
    "deviceId": "3C:71:BF:12:3F:2A",     // BLE address / remoteId, null if not paired
    "name": "SafeLink-3F2A",
    "connected": true,
    "batteryPct": 87,                    // null if unknown
    "firmwareVersion": "0.1.0",
    "lastSeenAt": Timestamp
  }
}
```

## `users/{uid}/contacts/{contactId}` — up to 5 trusted contacts
```jsonc
{
  "name": "Maa",
  "phone": "+919888888888",   // kept for the future SMS phase
  "email": "maa@example.com", // lower-case; the guardian logs in to the web dashboard with this email
  "relation": "mother",       // free text, optional
  "priority": 1,              // 1..5, order of escalation
  "createdAt": Timestamp
}
```

## `alerts/{alertId}` — top-level, so guardians can query across owners
```jsonc
{
  "ownerUid": "uid_abc",
  "ownerName": "Anand Raj",
  "ownerPhone": "+919999999999",
  "status": "active",              // "active" | "cancelled" | "resolved"
  "trigger": "button",             // "button" | "touch" | "app" | "test"
  "bandSeq": 12,                   // press counter from the band; null if app-triggered
  "createdAt": Timestamp,          // serverTimestamp
  "updatedAt": Timestamp,
  "endedAt": null,                 // Timestamp when cancelled / resolved
  "endedBy": null,                 // "owner" | "guardian:<uid>"
  "contactEmails": ["maa@example.com", "bhai@example.com"],   // denormalised from contacts — rules use this
  "contactPhones": ["+919888888888"],
  "contacts": [ { "name": "Maa", "email": "maa@example.com", "phone": "+91…", "priority": 1 } ],
  "locationIntervalMin": 5,
  "lastLocation": {                // null until the first fix
    "lat": 30.7691, "lng": 76.5754, "accuracyM": 8.5,
    "at": Timestamp, "source": "phone"   // "phone" | "band"
  },
  "mapsUrl": "https://maps.google.com/?q=30.7691,76.5754",
  "message": "SOS! Anand Raj needs help.",
  "acks": {                        // guardian acknowledgements, keyed by guardian uid
    "uid_guardian": { "name": "Maa", "email": "maa@example.com", "at": Timestamp }
  }
}
```

### `alerts/{alertId}/locations/{locId}` — trail (one doc per fix)
```jsonc
{ "lat": 30.7691, "lng": 76.5754, "accuracyM": 8.5, "speedMps": 0.4, "at": Timestamp, "source": "phone" }
```

### `alerts/{alertId}/events/{eventId}` — timeline
```jsonc
{ "type": "sos_received", "at": Timestamp, "byUid": "uid_abc", "note": "seq 12 via button" }
```
`type` ∈ `sos_received` · `acked_band` · `location_fix` · `location_update` · `pressed_again` · `band_disconnected` · `band_reconnected` · `guardian_ack` · `cancelled` · `resolved` · `test`

## Who reads / writes what
| Actor | Reads | Writes |
|---|---|---|
| Wearer (Flutter app) | own `users/{uid}` + contacts; own alerts (+ subcollections) | own user / contacts; creates alerts; appends locations / events; cancels (`status: "cancelled"`) |
| Wearer (web dashboard) | same as above | cancel / resolve own alert |
| Guardian (web dashboard) | alerts whose `contactEmails` contains their email (+ subcollections) | `acks.<uid>` on those alerts; may set `status: "resolved"`; appends `guardian_ack` / `resolved` events |

## Queries (→ composite indexes in `firebase/firestore.indexes.json`)
1. Own alerts: `alerts` where `ownerUid == uid` orderBy `createdAt desc` → index (ownerUid ASC, createdAt DESC)
2. Own active alert: `alerts` where `ownerUid == uid` and `status == "active"` orderBy `createdAt desc` → index (ownerUid ASC, status ASC, createdAt DESC)
3. Guardian feed: `alerts` where `contactEmails array-contains email` orderBy `createdAt desc` → index (contactEmails CONTAINS, createdAt DESC)
4. Guardian active: `alerts` where `contactEmails array-contains email` and `status == "active"` orderBy `createdAt desc` → index (contactEmails CONTAINS, status ASC, createdAt DESC)
5. Trail: `alerts/{id}/locations` orderBy `at asc` (single-field, automatic)
6. Timeline: `alerts/{id}/events` orderBy `at asc` (automatic)

## Security rules — intent (implemented in `firebase/firestore.rules`)
- Everything requires sign-in.
- `users/{uid}` and its `contacts`: owner only.
- `alerts`: create only with `ownerUid == request.auth.uid`; read if owner **or** `request.auth.token.email` (lower-cased) is in `contactEmails`; the owner may update anything; a guardian may update only `acks`, `status` (only to `"resolved"`), `updatedAt`, `endedAt`, `endedBy`; nobody deletes.
- `locations`: read = owner or guardian of the parent alert; create = owner only.
- `events`: read = owner or guardian; create = owner, or guardian for `guardian_ack` / `resolved` only.

## Alert lifecycle (who sets what)
1. App receives SOS → `alerts` create (`status: active`, contacts denormalised, `lastLocation: null`) + event `sos_received` → ACK to band → event `acked_band`.
2. First fix → update `lastLocation` + `mapsUrl`, add a `locations` doc, event `location_fix`.
3. Every `locationIntervalMin` minutes → new `locations` doc, update `lastLocation`, event `location_update`.
4. Guardian taps "I'm coming" → `acks.<uid>` + event `guardian_ack`.
5. Wearer cancels → `status: cancelled`, `endedAt`, `endedBy: "owner"` + event. Guardian marks safe → `status: resolved`, `endedBy: "guardian:<uid>"` + event.
