# Warden Fury transformation

Press **R** during play to activate Warden Fury. A short Greystone challenge gesture accompanies the existing power-up voice. A warm crimson-and-gold rim aura, rising embers and local light build around Warden, remain visible while Fury is active, and fade smoothly during its final two seconds. Fury still lasts six seconds by default.

The flourish lasts up to 1.1 seconds but does not extend the existing 0.35-second action recovery. Attacks and dodges can replace the animation after their normal eligibility checks. Damage, stamina, cooldowns, hit recoil, healing and saves retain their existing rules. **M** disables camera shake and ember orbit/rise while keeping the glow, material shimmer and duration fade visible.

The aura reads the same remaining-power timer as combat. It disappears on expiry, death, extraction completion and restart. Nine reusable, collision-free meshes and one shadowless light bound its runtime cost; no new marketplace assets or paid generation are required.

## Prepared-project integration

This remains a source kit. `integration/bind_warden_aura.py` requires the prepared enhanced map and installed Paragon Greystone assets. It verifies the animation skeleton and full-pose type, creates the project-owned additive rim material, persists the map bindings and reads them back. No third-party source assets are included in this repository.

`integration/install_aura.ps1` preserves the feedback project and earlier packages, creates `H:\DarkRelicAura-20260912`, and invokes the compile/bind/test/package runner. These scripts have project-specific paths; inspect them before use elsewhere. Read `H:\DarkRelicAuraDelivery-20260912\job.json` and the candidate's `IntegrationEvidence/aura-build.json` before retrying an interrupted worker.

The `-DarkRelicAuraCapture` flag exercises the real Fury action and captures charge, peak, fade and off frames before exiting. It is for verification only; omit it for normal play.

## Validation

Delivered September 12, 2026 at 11:13 AM Arizona through the **Dark Relic Fury Aura** desktop shortcut, targeting `H:\DarkRelicAuraPackage\Windows\DarkRelicSmoke.exe`.

- 155 portable checks pass, including an AddressSanitizer/UndefinedBehaviorSanitizer run.
- Unreal compilation, persisted animation/material bindings and packaging pass.
- 70 editor and 70 packaged runtime checks pass, including active aura intensity, animation playback and complete expiry shutdown.
- Fresh packaged charge, peak, fade and off captures were inspected. The initial uniform shield was replaced with softer procedural wisps.
- Ordinary R input played the Fury voice; Escape exited with code 0.
- Eleven installed plugin files match source hashes; all 35 protected executable/container hashes across five earlier releases remain unchanged.

The live encounter consumes the bound animation/material through R and its normal tick. No game/editor/build process remained at final verification. Human aesthetic/playfeel approval and broader submission acceptance remain pending; this is a delivered candidate, not a claim of AAA quality or a performance benchmark. Specialist advice/review routing ran in shadow mode with no independent model approval claimed.
