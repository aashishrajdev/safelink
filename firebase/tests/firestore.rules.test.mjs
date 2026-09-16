// SafeLink — Firestore security-rules tests.
//
// Runs against the Firestore emulator (needs Java 11+ on PATH):
//   cd firebase && npm install && npm test
// or, without a globally installed CLI:
//   cd firebase && npm install && npm run test:npx
//
// Every case maps to a line of docs/firestore-schema.md → "Security rules — intent".

import { after, before, beforeEach, describe, test } from 'node:test';
import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

import {
  assertFails,
  assertSucceeds,
  initializeTestEnvironment,
} from '@firebase/rules-unit-testing';
import {
  addDoc,
  collection,
  deleteDoc,
  doc,
  getDoc,
  getDocs,
  orderBy,
  query,
  serverTimestamp,
  setDoc,
  setLogLevel,
  updateDoc,
  where,
} from 'firebase/firestore';

const here = dirname(fileURLToPath(import.meta.url));
const PROJECT_ID = process.env.GCLOUD_PROJECT || 'demo-safelink';
const [EMU_HOST, EMU_PORT] = (process.env.FIRESTORE_EMULATOR_HOST || '127.0.0.1:8080').split(':');

// Actors ---------------------------------------------------------------------
const OWNER = 'uid_owner';
const OWNER_EMAIL = 'anand@example.com';
const GUARDIAN = 'uid_guardian';
const GUARDIAN_EMAIL = 'maa@example.com';
const OTHER_GUARDIAN = 'uid_bhai';
const OTHER_GUARDIAN_EMAIL = 'bhai@example.com';
const STRANGER = 'uid_stranger';
const STRANGER_EMAIL = 'stranger@example.com';

const ALERT_ID = 'alert_1';

let testEnv;

const owner = () => testEnv.authenticatedContext(OWNER, { email: OWNER_EMAIL }).firestore();
const guardian = () => testEnv.authenticatedContext(GUARDIAN, { email: GUARDIAN_EMAIL }).firestore();
const guardianMixedCase = () =>
  testEnv.authenticatedContext(GUARDIAN, { email: 'Maa@Example.COM' }).firestore();
const otherGuardian = () =>
  testEnv.authenticatedContext(OTHER_GUARDIAN, { email: OTHER_GUARDIAN_EMAIL }).firestore();
const stranger = () => testEnv.authenticatedContext(STRANGER, { email: STRANGER_EMAIL }).firestore();
const noEmail = () => testEnv.authenticatedContext('uid_no_email').firestore();
const anon = () => testEnv.unauthenticatedContext().firestore();

// Fixtures -------------------------------------------------------------------
function alertData(overrides = {}) {
  return {
    ownerUid: OWNER,
    ownerName: 'Anand Raj',
    ownerPhone: '+919999999999',
    status: 'active',
    trigger: 'button',
    bandSeq: 12,
    createdAt: serverTimestamp(),
    updatedAt: serverTimestamp(),
    endedAt: null,
    endedBy: null,
    contactEmails: [GUARDIAN_EMAIL, OTHER_GUARDIAN_EMAIL],
    contactPhones: ['+919888888888'],
    contacts: [
      { name: 'Maa', email: GUARDIAN_EMAIL, phone: '+919888888888', priority: 1 },
      { name: 'Bhai', email: OTHER_GUARDIAN_EMAIL, phone: '+919777777777', priority: 2 },
    ],
    locationIntervalMin: 5,
    lastLocation: null,
    mapsUrl: null,
    message: 'SOS! Anand Raj needs help.',
    acks: {},
    ...overrides,
  };
}

/** Seed documents with rules disabled (admin-style). */
async function seed(path, data) {
  await testEnv.withSecurityRulesDisabled(async (ctx) => {
    await setDoc(doc(ctx.firestore(), path), data);
  });
}

const seedAlert = (overrides) => seed(`alerts/${ALERT_ID}`, alertData(overrides));

const ack = (uid = GUARDIAN) => ({
  [`acks.${uid}`]: { name: 'Maa', email: GUARDIAN_EMAIL, at: serverTimestamp() },
  updatedAt: serverTimestamp(),
});

// Lifecycle ------------------------------------------------------------------
before(async () => {
  setLogLevel('error'); // hide the SDK's expected "permission denied" warnings
  testEnv = await initializeTestEnvironment({
    projectId: PROJECT_ID,
    firestore: {
      rules: readFileSync(resolve(here, '../firestore.rules'), 'utf8'),
      host: EMU_HOST,
      port: Number(EMU_PORT),
    },
  });
});

after(async () => {
  await testEnv?.cleanup();
});

beforeEach(async () => {
  await testEnv.clearFirestore();
});

// ============================================================================
describe('users/{uid} and users/{uid}/contacts — owner only', () => {
  test('owner can create and read own profile', async () => {
    await assertSucceeds(
      setDoc(doc(owner(), `users/${OWNER}`), {
        displayName: 'Anand Raj',
        email: OWNER_EMAIL,
        createdAt: serverTimestamp(),
      }),
    );
    await assertSucceeds(getDoc(doc(owner(), `users/${OWNER}`)));
  });

  test('another signed-in user cannot read or write it', async () => {
    await seed(`users/${OWNER}`, { displayName: 'Anand Raj', email: OWNER_EMAIL });
    await assertFails(getDoc(doc(stranger(), `users/${OWNER}`)));
    await assertFails(updateDoc(doc(stranger(), `users/${OWNER}`), { displayName: 'x' }));
  });

  test('signed-out user cannot read it', async () => {
    await seed(`users/${OWNER}`, { displayName: 'Anand Raj', email: OWNER_EMAIL });
    await assertFails(getDoc(doc(anon(), `users/${OWNER}`)));
  });

  test('contacts: owner can write, guardian listed there still cannot read', async () => {
    const contact = { name: 'Maa', email: GUARDIAN_EMAIL, phone: '+919888888888', priority: 1 };
    await assertSucceeds(setDoc(doc(owner(), `users/${OWNER}/contacts/c1`), contact));
    await assertSucceeds(getDoc(doc(owner(), `users/${OWNER}/contacts/c1`)));
    await assertFails(getDoc(doc(guardian(), `users/${OWNER}/contacts/c1`)));
    await assertFails(getDocs(collection(guardian(), `users/${OWNER}/contacts`)));
  });
});

// ============================================================================
describe('alerts — create', () => {
  test('owner can create an active alert for herself', async () => {
    await assertSucceeds(setDoc(doc(owner(), `alerts/${ALERT_ID}`), alertData()));
  });

  test('owner can create with addDoc (auto id)', async () => {
    await assertSucceeds(addDoc(collection(owner(), 'alerts'), alertData()));
  });

  test('cannot create an alert owned by someone else', async () => {
    await assertFails(
      setDoc(doc(stranger(), `alerts/${ALERT_ID}`), alertData({ ownerUid: OWNER })),
    );
  });

  test('cannot create with status other than "active"', async () => {
    await assertFails(setDoc(doc(owner(), `alerts/${ALERT_ID}`), alertData({ status: 'resolved' })));
    await assertFails(setDoc(doc(owner(), `alerts/${ALERT_ID}`), alertData({ status: 'cancelled' })));
  });

  test('cannot create when contactEmails is not a list', async () => {
    await assertFails(
      setDoc(doc(owner(), `alerts/${ALERT_ID}`), alertData({ contactEmails: GUARDIAN_EMAIL })),
    );
  });

  test('cannot create without createdAt', async () => {
    const data = alertData();
    delete data.createdAt;
    await assertFails(setDoc(doc(owner(), `alerts/${ALERT_ID}`), data));
  });

  test('signed-out user cannot create', async () => {
    await assertFails(setDoc(doc(anon(), `alerts/${ALERT_ID}`), alertData()));
  });
});

// ============================================================================
describe('alerts — read', () => {
  beforeEach(() => seedAlert());

  test('owner can read her alert', async () => {
    await assertSucceeds(getDoc(doc(owner(), `alerts/${ALERT_ID}`)));
  });

  test('guardian listed in contactEmails can read it', async () => {
    await assertSucceeds(getDoc(doc(guardian(), `alerts/${ALERT_ID}`)));
    await assertSucceeds(getDoc(doc(otherGuardian(), `alerts/${ALERT_ID}`)));
  });

  test('guardian e-mail is matched case-insensitively', async () => {
    await assertSucceeds(getDoc(doc(guardianMixedCase(), `alerts/${ALERT_ID}`)));
  });

  test('a stranger cannot read it', async () => {
    await assertFails(getDoc(doc(stranger(), `alerts/${ALERT_ID}`)));
  });

  test('a signed-in account without an e-mail claim cannot read it', async () => {
    await assertFails(getDoc(doc(noEmail(), `alerts/${ALERT_ID}`)));
  });

  test('a signed-out user cannot read it', async () => {
    await assertFails(getDoc(doc(anon(), `alerts/${ALERT_ID}`)));
  });

  test('owner feed query (ownerUid == uid, createdAt desc) is allowed', async () => {
    const q = query(
      collection(owner(), 'alerts'),
      where('ownerUid', '==', OWNER),
      orderBy('createdAt', 'desc'),
    );
    const snap = await assertSucceeds(getDocs(q));
    if (snap.size !== 1) throw new Error(`expected 1 alert, got ${snap.size}`);
  });

  test('owner active-alert query (ownerUid + status) is allowed', async () => {
    const q = query(
      collection(owner(), 'alerts'),
      where('ownerUid', '==', OWNER),
      where('status', '==', 'active'),
      orderBy('createdAt', 'desc'),
    );
    await assertSucceeds(getDocs(q));
  });

  test('guardian feed query (contactEmails array-contains own e-mail) is allowed', async () => {
    const q = query(
      collection(guardian(), 'alerts'),
      where('contactEmails', 'array-contains', GUARDIAN_EMAIL),
      orderBy('createdAt', 'desc'),
    );
    const snap = await assertSucceeds(getDocs(q));
    if (snap.size !== 1) throw new Error(`expected 1 alert, got ${snap.size}`);
  });

  test('guardian active-alert query (array-contains + status) is allowed', async () => {
    const q = query(
      collection(guardian(), 'alerts'),
      where('contactEmails', 'array-contains', GUARDIAN_EMAIL),
      where('status', '==', 'active'),
      orderBy('createdAt', 'desc'),
    );
    await assertSucceeds(getDocs(q));
  });

  test("a stranger cannot query someone else's guardian feed", async () => {
    const q = query(
      collection(stranger(), 'alerts'),
      where('contactEmails', 'array-contains', GUARDIAN_EMAIL),
      orderBy('createdAt', 'desc'),
    );
    await assertFails(getDocs(q));
  });

  test('an unfiltered collection scan is denied', async () => {
    await assertFails(getDocs(collection(guardian(), 'alerts')));
  });
});

// ============================================================================
describe('alerts — guardian updates', () => {
  beforeEach(() => seedAlert());

  test('guardian can acknowledge ("I\'m coming")', async () => {
    await assertSucceeds(updateDoc(doc(guardian(), `alerts/${ALERT_ID}`), ack()));
  });

  test('guardian can acknowledge even if the alert has no acks map yet', async () => {
    const data = alertData();
    delete data.acks;
    await seed(`alerts/${ALERT_ID}`, data);
    await assertSucceeds(updateDoc(doc(guardian(), `alerts/${ALERT_ID}`), ack()));
  });

  test('guardian cannot change ownerUid', async () => {
    await assertFails(updateDoc(doc(guardian(), `alerts/${ALERT_ID}`), { ownerUid: GUARDIAN }));
    await assertFails(
      updateDoc(doc(guardian(), `alerts/${ALERT_ID}`), { ...ack(), ownerUid: GUARDIAN }),
    );
  });

  test('guardian cannot touch other keys (message, contactEmails, lastLocation)', async () => {
    const ref = doc(guardian(), `alerts/${ALERT_ID}`);
    await assertFails(updateDoc(ref, { message: 'hacked' }));
    await assertFails(updateDoc(ref, { contactEmails: [GUARDIAN_EMAIL, STRANGER_EMAIL] }));
    await assertFails(updateDoc(ref, { lastLocation: { lat: 0, lng: 0 } }));
  });

  test('guardian can mark the alert resolved', async () => {
    await assertSucceeds(
      updateDoc(doc(guardian(), `alerts/${ALERT_ID}`), {
        status: 'resolved',
        endedAt: serverTimestamp(),
        endedBy: `guardian:${GUARDIAN}`,
        updatedAt: serverTimestamp(),
      }),
    );
  });

  test('guardian cannot set status to "cancelled"', async () => {
    await assertFails(
      updateDoc(doc(guardian(), `alerts/${ALERT_ID}`), {
        status: 'cancelled',
        endedAt: serverTimestamp(),
        endedBy: `guardian:${GUARDIAN}`,
      }),
    );
  });

  test('guardian cannot re-activate a resolved alert', async () => {
    await seedAlert({ status: 'resolved' });
    await assertFails(updateDoc(doc(guardian(), `alerts/${ALERT_ID}`), { status: 'active' }));
  });

  test('stranger cannot acknowledge or resolve', async () => {
    await assertFails(updateDoc(doc(stranger(), `alerts/${ALERT_ID}`), ack(STRANGER)));
    await assertFails(updateDoc(doc(stranger(), `alerts/${ALERT_ID}`), { status: 'resolved' }));
  });
});

// ============================================================================
describe('alerts — owner updates and delete', () => {
  beforeEach(() => seedAlert());

  test('owner can update location fields', async () => {
    await assertSucceeds(
      updateDoc(doc(owner(), `alerts/${ALERT_ID}`), {
        lastLocation: { lat: 30.7691, lng: 76.5754, accuracyM: 8.5, at: serverTimestamp(), source: 'phone' },
        mapsUrl: 'https://maps.google.com/?q=30.7691,76.5754',
        updatedAt: serverTimestamp(),
      }),
    );
  });

  test('owner can cancel', async () => {
    await assertSucceeds(
      updateDoc(doc(owner(), `alerts/${ALERT_ID}`), {
        status: 'cancelled',
        endedAt: serverTimestamp(),
        endedBy: 'owner',
        updatedAt: serverTimestamp(),
      }),
    );
  });

  test('owner cannot hand the alert to another uid', async () => {
    await assertFails(updateDoc(doc(owner(), `alerts/${ALERT_ID}`), { ownerUid: STRANGER }));
  });

  test('nobody can delete an alert', async () => {
    await assertFails(deleteDoc(doc(owner(), `alerts/${ALERT_ID}`)));
    await assertFails(deleteDoc(doc(guardian(), `alerts/${ALERT_ID}`)));
    await assertFails(deleteDoc(doc(stranger(), `alerts/${ALERT_ID}`)));
    await assertFails(deleteDoc(doc(anon(), `alerts/${ALERT_ID}`)));
  });
});

// ============================================================================
describe('alerts/{id}/locations — trail', () => {
  const fix = () => ({
    lat: 30.7691, lng: 76.5754, accuracyM: 8.5, speedMps: 0.4, at: serverTimestamp(), source: 'phone',
  });
  const LOC = `alerts/${ALERT_ID}/locations/l1`;

  beforeEach(() => seedAlert());

  test('owner can append a fix', async () => {
    await assertSucceeds(setDoc(doc(owner(), LOC), fix()));
    await assertSucceeds(addDoc(collection(owner(), `alerts/${ALERT_ID}/locations`), fix()));
  });

  test('guardian and stranger cannot append a fix', async () => {
    await assertFails(setDoc(doc(guardian(), LOC), fix()));
    await assertFails(setDoc(doc(stranger(), LOC), fix()));
  });

  test('owner and guardian can read the trail; stranger cannot', async () => {
    await seed(LOC, fix());
    await assertSucceeds(getDoc(doc(owner(), LOC)));
    await assertSucceeds(getDoc(doc(guardian(), LOC)));
    await assertSucceeds(
      getDocs(query(collection(guardian(), `alerts/${ALERT_ID}/locations`), orderBy('at', 'asc'))),
    );
    await assertFails(getDoc(doc(stranger(), LOC)));
    await assertFails(getDocs(collection(stranger(), `alerts/${ALERT_ID}/locations`)));
  });

  test('fixes are immutable (no update / delete, even by the owner)', async () => {
    await seed(LOC, fix());
    await assertFails(updateDoc(doc(owner(), LOC), { lat: 0 }));
    await assertFails(deleteDoc(doc(owner(), LOC)));
    await assertFails(deleteDoc(doc(guardian(), LOC)));
  });
});

// ============================================================================
describe('alerts/{id}/events — timeline', () => {
  const EVENTS = `alerts/${ALERT_ID}/events`;
  const event = (type, byUid, note = '') => ({ type, at: serverTimestamp(), byUid, note });

  beforeEach(() => seedAlert());

  test('owner can append any event type', async () => {
    await assertSucceeds(addDoc(collection(owner(), EVENTS), event('sos_received', OWNER, 'seq 12')));
    await assertSucceeds(addDoc(collection(owner(), EVENTS), event('location_fix', OWNER)));
    await assertSucceeds(addDoc(collection(owner(), EVENTS), event('cancelled', OWNER)));
  });

  test('guardian can append guardian_ack and resolved only', async () => {
    await assertSucceeds(addDoc(collection(guardian(), EVENTS), event('guardian_ack', GUARDIAN)));
    await assertSucceeds(addDoc(collection(guardian(), EVENTS), event('resolved', GUARDIAN)));
    await assertFails(addDoc(collection(guardian(), EVENTS), event('sos_received', GUARDIAN)));
    await assertFails(addDoc(collection(guardian(), EVENTS), event('cancelled', GUARDIAN)));
    await assertFails(addDoc(collection(guardian(), EVENTS), event('location_update', GUARDIAN)));
  });

  test('stranger cannot append events', async () => {
    await assertFails(addDoc(collection(stranger(), EVENTS), event('guardian_ack', STRANGER)));
  });

  test('owner and guardian can read the timeline; stranger cannot', async () => {
    await seed(`${EVENTS}/e1`, event('sos_received', OWNER));
    await assertSucceeds(getDocs(query(collection(owner(), EVENTS), orderBy('at', 'asc'))));
    await assertSucceeds(getDocs(query(collection(guardian(), EVENTS), orderBy('at', 'asc'))));
    await assertFails(getDocs(collection(stranger(), EVENTS)));
  });

  test('events are immutable', async () => {
    await seed(`${EVENTS}/e1`, event('sos_received', OWNER));
    await assertFails(updateDoc(doc(owner(), `${EVENTS}/e1`), { note: 'edited' }));
    await assertFails(deleteDoc(doc(owner(), `${EVENTS}/e1`)));
  });
});

// ============================================================================
describe('everything else', () => {
  test('unknown collections are denied even when signed in', async () => {
    await assertFails(getDoc(doc(owner(), 'misc/anything')));
    await assertFails(setDoc(doc(owner(), 'misc/anything'), { x: 1 }));
  });

  test('subcollection of a non-existent alert is denied', async () => {
    await assertFails(getDoc(doc(owner(), 'alerts/nope/locations/l1')));
    await assertFails(setDoc(doc(owner(), 'alerts/nope/locations/l1'), { lat: 1 }));
  });
});
