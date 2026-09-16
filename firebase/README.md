# `firebase/` — rules, indexes, hosting

Everything Firebase-side for SafeLink lives here. There is **no backend**: the
Flutter app and the Next.js dashboard talk to Firestore directly, so
`firestore.rules` is the whole authorisation layer.

| File | What it is |
|---|---|
| `firebase.json` | Points the CLI at the rules + indexes and serves the static web export (`../web/out`) on Hosting. Also declares emulator ports. |
| `.firebaserc` | Default project alias. Replace `safelink-REPLACE_ME` (or run `firebase use --add`). |
| `firestore.rules` | Security rules — implements *docs/firestore-schema.md → "Security rules — intent"*. |
| `firestore.indexes.json` | The four composite indexes behind the dashboard / app queries. |
| `tests/firestore.rules.test.mjs` | Rules tests (`@firebase/rules-unit-testing` + `node:test`), run against the emulator. |
| `package.json` | Only for the tests. Not needed to deploy. |

Contract first: change `docs/firestore-schema.md`, then the rules, then the tests.

## Deploy

Prerequisites: Node ≥ 20 and the Firebase CLI (`npm i -g firebase-tools`, or use
`npx -y firebase-tools@latest …` everywhere you see `firebase …` below). Project
creation is click-by-click in [`docs/setup-firebase.md`](../docs/setup-firebase.md).

```bash
cd firebase
firebase login                      # once per machine
firebase use <your-project-id>      # writes .firebaserc; `firebase projects:list` to find it

firebase deploy --only firestore    # rules + indexes (indexes take a few minutes to build)
firebase deploy --only firestore:rules      # rules only — fastest iteration
firebase deploy --only firestore:indexes    # indexes only

(cd ../web && npm run build)        # produces web/out (Next.js static export)
firebase deploy --only hosting      # uploads ../web/out
```

`firebase deploy --only firestore,hosting` does both. Hosting notes:

- `cleanUrls` is **off** and `trailingSlash` is left unset: the Next.js export
  writes `route/index.html`, which Hosting serves for both `/route` and `/route/`.
  If the web app ever switches to `route.html` files, nothing here needs to change.
- No rewrites: the dashboard is fully static and the Firebase Web SDK runs in the browser.
- `_next/static/**` gets a one-year immutable cache header; everything else uses Hosting defaults.

## Emulators (local, no project needed)

The Firestore emulator needs **Java 11+** on `PATH` (`java -version`). Nothing else
here needs Java.

```bash
cd firebase
firebase emulators:start --project demo-safelink
# Firestore  127.0.0.1:8080   Auth 127.0.0.1:9099   Hosting 127.0.0.1:5000   UI http://127.0.0.1:4000
```

A `demo-` project id never touches a real project. Point the clients at it with
`FIRESTORE_EMULATOR_HOST=127.0.0.1:8080` / `connectFirestoreEmulator(db, '127.0.0.1', 8080)`
and `connectAuthEmulator(auth, 'http://127.0.0.1:9099')`.

## Rules tests

```bash
cd firebase
npm install                  # @firebase/rules-unit-testing + firebase (dev only)
npm test                     # = firebase emulators:exec --only firestore … node --test
npm run test:npx             # same, via npx if the CLI is not installed globally
```

`emulators:exec` boots the Firestore emulator, runs the Node test file, and
shuts it down. Each test starts from an empty database; fixtures are written
with rules disabled, then every actor (owner, listed guardian, second guardian,
stranger, e-mail-less account, signed-out) tries the operation the rules should
allow or deny.

> Status: the rules and this test file have been reviewed line-by-line against
> the schema doc but have **not** been executed — the machine they were written
> on has no Java. First person with Java: `npm test`, and fix whatever it says.

## Rules cheat-sheet

Actor legend: **Owner** = signed-in user whose `uid == ownerUid` (or `uid ==
{uid}` under `users/`). **Guardian** = signed-in user whose lower-cased auth
e-mail is in the alert's `contactEmails`. **Other** = any other signed-in user.
Signed-out requests are always denied.

| Path | Owner | Guardian | Other |
|---|---|---|---|
| `users/{uid}` | read · create · update · delete | — | — |
| `users/{uid}/contacts/**` | read · create · update · delete | — | — |
| `alerts/{id}` | read · create¹ · update² | read · update³ | — |
| `alerts/{id}/locations/{loc}` | read · create | read | — |
| `alerts/{id}/events/{ev}` | read · create | read · create⁴ | — |
| anything else | — | — | — |

¹ Create requires `ownerUid == auth.uid`, `status == "active"`, `contactEmails` is a list, `createdAt` is a timestamp (`serverTimestamp()` counts).
² Owner may change any field except `ownerUid`. **Nobody** can delete an alert.
³ Guardian may only change `acks`, `status`, `updatedAt`, `endedAt`, `endedBy`; `status` may stay as it is or become `"resolved"` — never `"cancelled"` or back to `"active"`.
⁴ Guardian may create events only with `type` `guardian_ack` or `resolved`. Locations and events are immutable once written (no update / delete for anyone).

Things clients must get right for the rules to pass:

- Store and query e-mails **lower-case**. The rule compares `request.auth.token.email.lower()`
  with `contactEmails`; a guardian's feed query must be
  `where('contactEmails', 'array-contains', email.toLowerCase())` — otherwise the
  list rule cannot be proven and the whole query is rejected.
- Owner queries must filter `ownerUid == uid`. An unfiltered `alerts` scan is denied.
- Reads of `locations` / `events` do one `get()` of the parent alert (counts
  against the 10-document-access limit per request — fine for our queries).
- Guardian acknowledgement: `updateDoc(ref, { ['acks.' + uid]: {...}, updatedAt: serverTimestamp() })`.
  Any extra top-level key in the same write makes the whole write fail.
