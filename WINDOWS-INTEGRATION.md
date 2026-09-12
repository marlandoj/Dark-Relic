# Windows integration candidate

This source kit was implemented by the primary chat after the Software Factory workers failed to reach their providers. The portable core passed local tests; **Unreal Header Tool, Windows compilation, save files and packaged gameplay are unverified**. Use a disposable clone of the working UE 5.8.1 project first. Do not replace the existing project or its Content folder.

## Install and compile before wiring

1. Close Unreal in the chosen integration clone. Copy `Plugins/DarkRelicCore` into that clone's `Plugins` directory. If a folder already exists, compare it and keep a backup; do not blindly overwrite it.
2. Enable `Dark Relic Core` in the project. The plugin deliberately defaults to disabled. Regenerate project files if needed and build the clone's actual **Development Editor / Win64** target with the installed UE 5.8.1 and VS toolchain. A Blueprint-only project may need an empty C++ class to create a host target. Do not copy Binaries or Intermediate from a different engine build.
3. Restart the editor and confirm `Dark Relic Run Component` appears under Add Component. Add exactly one instance to the player pawn Blueprint. If compilation fails, stop integration and retain the working project; the Blueprint fallback below avoids waiting on this optional module.
4. Keep DirectX 11 in project settings, 1080p, no hardware ray tracing. Disable automatic Player 0 activation on the look-development review camera and use the pawn's Third Person camera.

No compile success is implied by these instructions. Record the actual UHT/compiler output before proceeding.

## Wire the component

`GetSnapshot` is the single source for player HUD/run data. Array indices are 0 Iron, 1 Tallow, 2 Salt, 3 Blackbell. Item names and values are hackathon starting defaults; the bank stores both cumulative collected counts and spendable credits. Buying an upgrade reduces credits, not historical item counts.

| Caller | Component call or event | Required wiring |
|---|---|---|
| Pawn BeginPlay | `LoadBank`, then `StartRun` | First call `Does Save Game Exist` for `SaveSlot`, user index 0. If absent, start fresh. If present but LoadBank fails, show a recovery error and do not overwrite that slot. Bind delegates before starting. |
| Light input Started | `TryAction(Light)` | Play the light montage only if true; use Started rather than every-frame Triggered. |
| Heavy input Started | `TryAction(Heavy)` | Play heavy montage only if true. |
| Dodge input Started | `TryAction(Dodge)` | Accepted action grants the tested invulnerability window; Blueprint supplies movement and montage. Do not independently toggle invulnerability. |
| Heal input Started | `TryAction(Heal)` | Play feedback only on acceptance. Charge is consumed on start; health restores after the configured windup. Death interrupts healing. |
| Enemy hit event | `ReceiveDamage(Amount)` | Call once per valid enemy swing. The core handles invulnerability/death; enemy health and movement remain Blueprint-owned. |
| Loot interaction/overlap | `CollectLoot(Item, Count, PickupId)` | Each placed pickup needs a distinct positive integer ID per run. Destroy/hide it only if the call returns true. Repeated overlaps with the same ID cannot duplicate loot. |
| Extraction overlap | `SetInExtractionZone(Present)` | Filter to player capsule, or count player component overlaps. Pass false only after the last relevant overlap ends. |
| Interact while in ring | `BeginExtraction` | Starts once, defaults to 20 s. Leaving cancels/resets. Re-entry requires another interaction. |
| HUD | `OnStateChanged(Snapshot)` | Display health/max, stamina/max, carried items and extraction countdown. Do not mutate run state from this display callback. |
| Animation/feedback | `OnActionStarted(Action)` | Optional central place for montage and sound, instead of duplicate input wiring. Never play twice from both locations. |
| Results | `OnRunEnded(Escaped)` | Disable combat/input and enemy targeting; show escaped/dead outcome. Call SaveBank before allowing a map reload. |
| Upgrade button | `BuyUpgrade`, then `SaveBank` | One purchase, 100 credits, +20 next-run maximum health. Check both return values. Disable repeat purchase. |
| Restart button | `StartRun` | Same-pawn restart preserves its bank. Recreate enemy/pickup actors and reset world state separately. For map reload: save first, then load bank in the new pawn. |

Default actions: light costs 15 stamina / 0.5 s recovery; heavy 30 / 0.9 s; dodge 25 / 0.55 s with 0.3 s invulnerability; heal restores 30 after 0.8 s, two charges per run; stamina regenerates 20/s. Recovery and invulnerability run on game delta time, so ordinary pause stops them. Tune the component between runs to match actual animation contact and recovery. No movement, damage traces, montages or AI are supplied by these timers.

## Contact, enemies and loss state

Use animation notifies to open/close the sword trace window. Keep a per-swing set of hit actors; clear it only on a newly accepted swing. Ignore targets already in that set. Apply damage to each living enemy's Blueprint health once, and clear all trace windows when the player dies or extracts. Never let hit-count/VFX counters substitute for health changes.

The component is single-player only and is not replicated. One authoritative player component owns the run; do not add one to every enemy. Enemy role tuning and concrete Widowfen layout are in `WINDOWS-COMBAT-HANDOFF.md`. Implement the two common roles and elite with tested compatible animations and navigation. The plugin does not author characters, levels, collision or AI.

Extraction completes at the component's tick. On a frame where a fatal hit and countdown completion compete, Unreal callback/tick order decides which event reaches the core first. After either terminal outcome, the other is rejected. Test this boundary in the actual game; do not promise frame-order independence.

## Save failure handling

SaveBank is explicit and only succeeds between runs. It saves version 1, bank counts, credits and upgrade through Unreal's SaveGame system; carried loot is deliberately omitted. The synchronous operation is small and intended for a results/menu transition, not per-frame autosaving.

If SaveBank returns false, keep the current pawn and bank in memory, display a retry action, and do not reload/quit automatically. If an existing slot fails LoadBank validation, retain it for recovery; require an explicit new-game decision before replacing it. Local saves are not an anti-cheat or tamper-proof economy.

## Blueprint-only fallback

If plugin compilation would derail the event, use the working Third Person Blueprint project and implement this small state contract there. **The C++ test results do not validate a separate Blueprint rewrite.** Run the five packaged acceptance scenarios again.

- Variables: Phase = Ready/Running/Extracting/Escaped/Dead; carried and banked four-item arrays; bank credits; upgrade 0/1; extraction remaining; player health/stamina; two heal charges; action recovery and dodge-window timers; collected pickup-ID set.
- Start from Ready/Escaped/Dead only. Clear carried/collected IDs/timers; restore health/stamina/charges; keep bank and upgrade. Reset level actors separately.
- Collect only while Running/Extracting, once per pickup ID. Reject invalid or negative counts. Allow one Blackbell per run. Ordinary values: Iron 10, Tallow 15, Salt 20; Blackbell 100.
- Begin extraction only while Running and inside the ring. Leaving resets it to Running with zero remaining. At zero: change phase to Escaped FIRST, bank carried items/value ONCE, clear carried, and show results.
- Fatal damage while Running/Extracting changes phase to Dead, clears carried and combat/extraction timers, and preserves bank. Invulnerability blocks incoming damage only during the dodge window.
- Buy the upgrade only between runs with at least 100 credits and upgrade == 0. Spend 100 exactly once; next run gains 20 maximum health. Save bank/upgrade after success and purchase, load before the next run.

## Verification before claiming game readiness

Pass the five scenarios in `WINDOWS-COMBAT-HANDOFF.md`, inspect real save/reload behavior, and package from a clean source clone. Capture 1080p DX11 gameplay with representative peak enemies and effects, frame timing, successful extraction, death, restart and normal process exit. The existing smoke/static-prop evidence does not replace these tests.

API references checked against Epic's documentation: [Blueprint exposure](https://dev.epicgames.com/documentation/unreal-engine/exposing-gameplay-elements-to-blueprints-visual-scripting-in-unreal-engine?lang=en-US), [SaveGame](https://dev.epicgames.com/documentation/unreal-engine/saving-and-loading-your-game-in-unreal-engine). Runtime compatibility remains pending the Windows build.
