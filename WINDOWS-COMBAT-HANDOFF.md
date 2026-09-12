# Widowfen: first playable encounter

This is a manual assembly recipe for the separate Windows effort. Coordinates below come from the existing look-development script; that script's completion is still unverified. Tuning values are starting proposals, not measured balance or a performance pass.

## First unblock the playable camera and floor

Open a copy of `/Game/WidowfenPrep/LVL_WidowfenLookdev` only after checking the earlier editor/import receipts. The script creates a `Gameplay Height Review Camera` with Auto Activate for Player 0. Disable that automatic activation for the gameplay map and use the Third Person pawn's camera. Otherwise the character may move under a fixed inspection view.

Set the map's GameMode to the tested Third Person GameMode; confirm its pawn spawns at `Encounter Start` (-650, -450, 120). Keep courtyard collision enabled. Turn off collision on the decorative water surface and tiny extraction-ground marks if they block movement. Rebuild navigation over the actual walkable route and verify that enemies can reach the player across the causeway. Do not mistake an imported/posed character for working navigation or combat.

## Small encounter layout

| Space | Existing scene anchor, cm | Playable purpose |
|---|---|---|
| Entry | Encounter Start (-650, -450, 120) | Safe movement, camera and first interaction |
| Yard | Combat Courtyard centered (0, 500, -15) | Two melee Husks; ordinary loot visibly beyond them |
| Workbench | Blackbell (-100, 800, 160) | Named Relic pickup; clear sound and state change |
| Risk branch | Eastern shed near (1100, 1100, 30) | Pressure enemy plus elite; optional ordinary-loot reward |
| Exit | Ward ring centered (0, 1600, 5) | One extraction overlap; start only on explicit interaction |

Check each spawn capsule is above the ground and outside props. The small look-development footprint may be too short for an 8–12 minute run; use encounter pacing and a return route, not long mandatory waits. Avoid expanding the map before the full loop works.

## First-pass enemy behaviors

| Role | Starting values | Required visible behavior |
|---|---|---|
| Husk | 60 health, 12 damage, 200 cm/s, 0.7 s windup, 1.0 s recovery | Close melee, low/wide silhouette, white face wrap; cannot hit during windup |
| Pressure role | 45 health, 8 damage, 260 cm/s, 1.0 s windup, 1.4 s recovery | Separate spacing/approach behavior and silhouette; use a different tested Minion variant |
| Briar Knight elite | 180 health, 25 damage, 170 cm/s, 1.1 s windup, 1.6 s recovery | Tall shield silhouette; slower punishable attack; no new skeleton pipeline |

These values govern Blueprint/animation wiring; reconcile player combat numbers against the delivered core's editable defaults. Use a single selected animation set per skeleton. Do not force an incompatible montage onto a character. For every swing, open and close the damage window with animation notifies and maintain a set of already-hit actors that resets on the next swing. Count health damage, not just overlap/counter events. Dead actors must immediately stop targeting and causing damage.

## Extraction and visual feedback

Use a 20-second starting extraction countdown that requires remaining inside the ward ring. Leaving cancels and resets it. The factory core is responsible for banking once; Blueprint must not independently award the same loot. The overlap must count only the player, including any multiple overlapping components, so a weapon leaving the volume does not cancel a player still inside.

Preserve the frozen language: ward-white for the exit, amber for loot/player agency, restrained violet for Gloam pressure. Keep important telegraphs visible through fog. Limit the first pressure wave to two active attackers, then tune from measured gameplay. Reuse existing licensed sounds; record their exact sources before submission. No new asset pack is required to establish the loop.

## Five-run check

1. Collect ordinary loot and extract: bank increases exactly once and carried loot clears.
2. Collect Blackbell and die: carried items vanish; earlier bank remains.
3. Start extraction, leave, re-enter and restart it: no early win or duplicated banking.
4. Buy the single upgrade between runs: spend once; persist correctly after closing/reopening the game.
5. Complete the full risk branch and extraction with all intended effects; record native frame timing at 1080p DX11 and exit normally.

Before final handoff, record the source commit, UE build result, exact pack versions/licenses, clean-package result, actual five-run outcomes, screenshot/video paths and mean/p95/p99/worst frame times. A source ZIP, animation preview, static scene or Linux test does not prove these checks passed.
