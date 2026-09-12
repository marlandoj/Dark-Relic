# Warden Fury transformation

Press **R** during play to activate Warden Fury. A short Greystone challenge gesture accompanies the existing power-up voice. A warm crimson-and-gold rim aura, rising embers and local light build around Warden, remain visible while Fury is active, and fade smoothly during its final two seconds. Fury still lasts six seconds by default.

The flourish lasts up to 1.1 seconds but does not extend the existing 0.35-second action recovery. Attacks and dodges can replace the animation after their normal eligibility checks. Damage, stamina, cooldowns, hit recoil, healing and saves retain their existing rules. **M** disables camera shake and freezes decorative aura motion while keeping the duration fade visible.

The aura reads the same remaining-power timer as combat. It disappears on expiry, death, extraction completion and restart. Nine reusable, collision-free meshes and one shadowless light bound its runtime cost; no new marketplace assets or paid generation are required.

## Prepared-project integration

This remains a source kit. `integration/bind_warden_aura.py` requires the prepared enhanced map and installed Paragon Greystone assets. It verifies the animation skeleton and full-pose type, creates the project-owned additive rim material, persists the map bindings and reads them back. No third-party source assets are included in this repository.

`integration/install_aura.ps1` preserves the feedback project and earlier packages, creates `H:\DarkRelicAura-20260912`, and invokes the compile/bind/test/package runner. These scripts have project-specific paths; inspect them before use elsewhere. Read `H:\DarkRelicAuraDelivery-20260912\job.json` and the candidate's `IntegrationEvidence/aura-build.json` before retrying an interrupted worker.

The `-DarkRelicAuraCapture` flag exercises the real Fury action and captures charge, peak, fade and off frames before exiting. It is for verification only; omit it for normal play.

## Validation

155 portable checks pass, including smooth fade, expiry, invalid timer handling and run-end suppression. Windows compilation, cooked runtime and visual evidence are pending. Human aesthetic/playfeel approval remains pending.
