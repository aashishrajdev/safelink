# Firebase setup — click by click

Creates the one Firebase project SafeLink needs (Auth + Firestore + Hosting, all
on the free **Spark** plan), deploys the rules and indexes from `firebase/`, and
wires the Flutter app and the web dashboard to it. Budget: ~20 minutes.

You need: a Google account, Node ≥ 20, and either the Firebase CLI
(`npm i -g firebase-tools`) or `npx -y firebase-tools@latest` in place of every
`firebase` below. The Flutter app step also needs Flutter + the FlutterFire CLI.

---

## 1. Create the project (Spark plan)

1. Open <https://console.firebase.google.com> and sign in.
2. Click **Create a project** (the button may read *Get started with a Firebase project* or *Add project*).
3. **Project name:** `safelink`. The console proposes a **project ID** underneath
   (something like `safelink-4f21a`). You can edit it once here and never again —
   write it down, it is `<project-id>` everywhere below.
4. If the wizard offers **Gemini in Firebase**, leave it off. Click **Continue**.
5. **Google Analytics:** switch **off** (not needed, and it keeps the web config
   smaller). Click **Create project**, wait, then **Continue**.
6. Check the plan: bottom-left of the sidebar shows **Spark** (free). Do **not**
   upgrade to Blaze — nothing in v0 needs it (no Cloud Functions, no outbound
   networking). Spark's daily Firestore quota (50 k reads · 20 k writes · 20 k
   deletes · 1 GiB) is far more than a demo uses.

## 2. Enable Authentication → Email/Password

1. Sidebar → **Build** → **Authentication** → **Get started**.
2. Tab **Sign-in method** → under *Native providers* click **Email/Password**.
3. Toggle **Email/Password** → **Enable**. Leave **Email link (passwordless sign-in)** off. **Save**.
4. Tab **Settings** → **Authorized domains**: `localhost`, `<project-id>.web.app`
   and `<project-id>.firebaseapp.com` are already there. Add another domain only
   if you host the dashboard elsewhere (e.g. a `*.vercel.app` URL — add the exact
   host name).

## 3. Create the Firestore database (production mode, `asia-south1`)

1. Sidebar → **Build** → **Firestore Database** → **Create database**.
2. **Database ID:** keep **`(default)`**. Both SDKs and the CLI target the default
   database; a named database will not work with the config in `firebase/`.
3. **Location:** pick **`asia-south1 (Mumbai)`**. This is permanent — check it before clicking on.
4. **Next** → **Secure rules:** choose **Start in production mode** (deny all;
   our own rules replace it in the next step). **Create**.

## 4. Deploy rules + indexes from `firebase/`

```bash
firebase login                     # opens a browser; once per machine
cd firebase
firebase use --add                 # pick <project-id> from the list, alias: default
                                   # (this rewrites .firebaserc — commit it)
firebase deploy --only firestore   # rules + the four composite indexes
```

Verify in the console: **Firestore Database** → tab **Rules** shows the contents
of `firebase/firestore.rules`; tab **Indexes** → *Composite* lists four `alerts`
indexes, first **Building…**, then **Enabled** (takes 1–5 minutes on an empty
database). Queries that need an index fail with a clear error until then.

To iterate on rules only: `firebase deploy --only firestore:rules`.

## 5. Register the Android app and run `flutterfire configure`

1. Project Overview (gear icon) → **Project settings** → tab **General** → scroll to **Your apps** → **Add app** → the **Android** icon.
2. **Android package name:** `com.safelink.app` (must match `applicationId` in `app/android/app/build.gradle`).
   **App nickname:** `SafeLink Android`. **SHA-1:** leave empty (only needed for Google Sign-In / Dynamic Links).
3. **Register app**. On the next screens you can skip downloading `google-services.json`
   and skip the Gradle instructions — the FlutterFire CLI does both. **Continue to console**.
4. On your machine:

   ```bash
   dart pub global activate flutterfire_cli      # once; make sure ~/.pub-cache/bin is on PATH
   cd app
   flutterfire configure --project=<project-id>
   ```

   At the prompts pick **android** only (space to toggle, enter to confirm). It
   writes `app/lib/firebase_options.dart` and `app/android/app/google-services.json`.
   Both are plain client config guarded by the security rules — fine to commit.
5. `flutter run` on the Android 15/16 test phone. Sign up → you should see the new
   user under **Authentication → Users** and a `users/{uid}` document in Firestore.

## 6. Register the Web app and fill `web/.env.local`

1. **Project settings** → **Your apps** → **Add app** → the **Web** icon (`</>`).
2. **App nickname:** `SafeLink Dashboard`. Leave **Also set up Firebase Hosting** unticked (step 7 does it). **Register app**.
3. The page shows a `firebaseConfig` object. Copy each value into `web/.env.local`
   (start from `cp web/.env.example web/.env.local`; keep exactly the variable
   names used there — the ones below are the convention):

   ```dotenv
   NEXT_PUBLIC_FIREBASE_API_KEY=AIza...                       # apiKey
   NEXT_PUBLIC_FIREBASE_AUTH_DOMAIN=<project-id>.firebaseapp.com
   NEXT_PUBLIC_FIREBASE_PROJECT_ID=<project-id>
   NEXT_PUBLIC_FIREBASE_STORAGE_BUCKET=<project-id>.firebasestorage.app
   NEXT_PUBLIC_FIREBASE_MESSAGING_SENDER_ID=1234567890          # messagingSenderId
   NEXT_PUBLIC_FIREBASE_APP_ID=1:1234567890:web:abc123          # appId
   ```

   `.env.local` is git-ignored. These values are public by design (they end up in
   the browser bundle); the rules are what protect the data. You can always find
   them again under **Project settings → Your apps → SDK setup and configuration → Config**.
4. **Continue to console**, then `cd web && npm install && npm run dev` →
   <http://localhost:3000> → register a user → it should appear under **Authentication → Users**.

## 7. Enable Hosting and deploy the dashboard

1. Sidebar → **Build** → **Hosting** → **Get started**. The wizard shows CLI steps
   you have already done; click **Next** through it and **Continue to console**.
   Your site is `https://<project-id>.web.app` (and `.firebaseapp.com`).
2. Build the static export and deploy from `firebase/` (its `firebase.json`
   points `public` at `../web/out`):

   ```bash
   cd web && npm run build          # Next.js `output: 'export'` → web/out
   cd ../firebase
   firebase use <project-id>        # only if you skipped step 4
   firebase deploy --only hosting   # or: --only firestore,hosting
   ```

3. Open the printed **Hosting URL**. Because the site is a static export, a
   redeploy is just `npm run build` + `firebase deploy --only hosting`.

## 8. Demo accounts: a wearer and a guardian

The rules only let a guardian see alerts whose `contactEmails` contains her
sign-in e-mail, so the two accounts must be linked through a contact entry.

1. **Wearer** — register from the **Flutter app** on the phone, e.g.
   `anand.wearer@example.com` / a password you will remember. (Registering from
   the app also creates the `users/{uid}` profile the app needs.)
2. **Guardian** — register from the **web dashboard** (local or hosted), e.g.
   `maa.guardian@example.com`. Use a different browser profile or an incognito
   window so both sessions can be open at once during the demo.
3. In the app, signed in as the wearer: **Contacts → Add** → name `Maa`, phone
   `+91…`, **e-mail = the guardian's sign-in e-mail, lower-case**, priority 1.
4. Sanity check before the real band: trigger a **Test SOS** from the app (or
   the band's `TEST_SOS` command). Within a few seconds the guardian's dashboard
   shows the alert; **Firestore Database → Data → `alerts`** shows the document
   with `contactEmails: ["maa.guardian@example.com"]`.
5. On the dashboard tap **I'm coming**: `acks.<guardian-uid>` appears on the
   alert and the wearer's app shows the acknowledgement. Cancel from the app to
   finish (`status: "cancelled"`).

Creating users from **Authentication → Users → Add user** also works, but it
creates only the Auth account — no `users/{uid}` profile — so prefer the app /
dashboard sign-up.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `Missing or insufficient permissions` | Rules not deployed (step 4); guardian e-mail not lower-case / not in `contactEmails`; not signed in; a guardian write touching a key outside `acks · status · updatedAt · endedAt · endedBy`. |
| `The query requires an index` (with a console link) | Indexes still **Building**, or not deployed. Wait, or click the link and confirm it matches `firebase/firestore.indexes.json`. |
| `auth/unauthorized-domain` in the browser | Dashboard served from a host not listed in **Authentication → Settings → Authorized domains**. |
| `flutterfire: command not found` | `~/.pub-cache/bin` is not on `PATH`. |
| Emulator refuses to start | The Firestore emulator needs Java 11+; it is optional — everything above runs against the real project. |
| Firestore location prompt shows only `nam5` / `eur3` | You clicked *Realtime Database* by mistake; use **Firestore Database**. |
