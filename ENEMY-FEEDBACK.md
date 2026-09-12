# Enemy hit reactions

Minions now recoil away from landed sword strikes and Relic Burst: 45 cm for light strikes and 75 cm for heavy strikes, Sunder and Burst, over 0.18 seconds. Bellkeeper moves only 12 cm or 22 cm respectively. Swept movement stops at collision; a ground check rejects unsupported destinations. Death cancels recoil. Movement input cannot immediately cancel the shove.

Each enemy role rotates through five recorded pain clips with small pitch variation. Dreg uses pitch 0.90, Hexbound 0.82 and Bellkeeper 0.65. Bellkeeper uses the heavier pain set. Voices are attached spatially to the enemy, attenuate with distance and are limited to one component per enemy with a 0.22-second retrigger cooldown. Restart, run end and shutdown clean up playback.

The recordings are the already installed Paragon Greystone pain SoundWaves, reused with different playback pitch. The installed Paragon Minions pack contains no audio files. No new purchase, generation or third-party source-asset redistribution is included. The source kit references the assets through editable map bindings; their existing marketplace license still applies.

Only confirmed attack contacts invoke reactions. Blocked and missed strikes do not. Bellkeeper's committed area attack retains its timer and original marked center through recoil; damage, enrage thresholds, loot, death animation, Warden voices and Fury are preserved.

## Build and validation

Based on Fury Aura PR #6. All 155 portable gameplay regression checks passed. Delivered September 12, 2026 at 12:44 PM Arizona. Built gameplay source b4d9884; Dark Relic Enemy Feedback desktop shortcut targets H:\DarkRelicEnemyFeedbackPackage\Windows\DarkRelicSmoke.exe. Compilation, bindings, packaging and captures exit 0; all 89 editor and 89 packaged runtime checks pass. Ordinary Escape exit 0, eleven source hashes match, all 42 protected release hashes are unchanged. No game/editor/build remains active. PR #7 stacks on Fury Aura PR #6. Human listening/playfeel acceptance remains pending.

The actual encounter invokes reactions only after confirmed sword or Relic Burst damage. Runtime checks cover spatial audio playback, variant rotation, cooldown, frame-rate-independent travel, wall collision, unsupported ground, boss warning preservation, death and restart cleanup. Specialist routing is shadow-only; no independent review or human listening approval is claimed.

Integration: copy this source into an isolated copy of the verified Fury Aura project; run bind_enemy_feedback.py through Unreal, then run_enemy_feedback_build.ps1. Scripts use the local prepared Windows paths and existing assets; they are not standalone project installers.
