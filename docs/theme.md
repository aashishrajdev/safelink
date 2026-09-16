# SafeLink visual theme (from the symposium deck)

Light theme only. Source: `docs/assets/hero.jpg` (deck cover) and the slide colours.

## Tokens
| Token | Value | Use |
|---|---|---|
| `--sl-bg` | `#FFFFFF` | page background |
| `--sl-surface` | `#F7F8FA` | cards, section bands |
| `--sl-line` | `#DCDFE4` | borders, dividers |
| `--sl-ink` | `#1C1E22` | headings, body |
| `--sl-muted` | `#62676F` | secondary text, captions |
| `--sl-red` | `#C64646` | primary actions, SOS, active alert, icons |
| `--sl-red-deep` | `#B3262A` | wordmark / hero gradient end, hover |
| `--sl-red-tint` | `#FDF3F3` | icon circles, active-alert background |
| `--sl-navy` | `#3C4E75` | section labels, links, "acknowledged" |
| `--sl-navy-2` | `#486394` | link hover, secondary buttons |
| `--sl-navy-tint` | `#F2F6FC` | info backgrounds |
| `--sl-green` | `#437D5B` | connected, resolved, success |
| `--sl-green-tint` | `#F2F8F4` | success backgrounds |

Status mapping: **active** alert → red · **acknowledged** → navy · **resolved / cancelled / connected** → green · **disconnected / unknown** → muted.

## Type
- The deck uses Calibri. Web: **Inter** via `next/font/google`, fallback `system-ui, -apple-system, "Segoe UI", Roboto, sans-serif`.
- Display (hero wordmark "SafeLink:"): weight 800, tight tracking (`-0.03em`), red → red-deep gradient text.
- H1 48–64 px · H2 32–40 px · H3 20–24 px, weight 700, ink. Body 16–18 px; muted for secondary.
- **Kicker** label: 12 px, uppercase, letter-spacing `0.22em`, ink, followed by a 32 × 3 px red rule. This replaces underlines — never underline headings.

## Shape & motifs (repeat these; don't invent others)
1. **Icon chip**: 48 px circle, `--sl-red-tint` background, red 22 px icon (lucide). Used for feature rows and step markers.
2. **Card**: white, 16 px radius, 1 px `--sl-line` border, shadow `0 8px 30px rgba(28,30,34,0.06)`. Section bands use `--sl-surface`.
3. **Swoosh**: soft red radial-gradient blob (`--sl-red` at 18 % → transparent) behind the hero's right side and the footer, clipped with `overflow-hidden`. Pure CSS, no image.
4. **Numbered steps**: red number in a chip + label, connected by a thin `--sl-line` rule (the deck's 8-step flow).
5. Hero photo: `docs/assets/hero.jpg` may be used on the landing hero (right half, 24 px radius) — it is the team's own deck cover.

Buttons: primary = red background, white text, 12 px radius, 44 px tall, hover `--sl-red-deep`; secondary = white background, ink text, `--sl-line` border. Focus ring 2 px navy.

Avoid: decorative side stripes / header bars, underlined headings, beige backgrounds, dark mode (not needed in v0).

## Landing page content (from the deck)
- Kicker: TECHNOLOGY FOR A SAFER TOMORROW
- H1: **SafeLink:** IoT-Based Secure Wearable-Mobile System for Women's Safety
- Sub: A smart, connected and reliable emergency response solution for a safer society.
- 4 feature chips: SOS at a touch · Real-time location · Alerts to trusted contacts · Quick response
- How it works (8 steps): SOS button pressed → BLE signal sent → App receives signal → GPS position obtained → Maps link generated → Alert reaches trusted contacts (live dashboard; SMS coming) → Alert stored in Firestore → Status updated
- Four layers: Wearable sensing · Communication (BLE) · Mobile application · Cloud / backend
- Measured (from the paper): 98 % reconnect at 0–5 m · 1.2 s reconnect · 3.2 m GPS error outdoors · 2.8 s alert latency
- Footer: Department of Computer Science and Engineering, Chandigarh University, Mohali · Student Research Symposium – Build Summit 2026 · team: Rhitam Roy Choudhuri, Anand Raj, Garvit Yadav, Devansh Kumar, Satyam Kumar, Daulat Sihag
- CTA: "Open dashboard" (login) and "Are you a trusted contact? Sign in with the email that was added for you."
