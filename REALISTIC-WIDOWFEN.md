# Realistic Widowfen environment

The environment candidate adds alder trees, reeds, ferns, wet textured earth, irregular puddles, mossy stone, scattered slate, broken timber, a distant causeway and ruined belfry. Cooler lighting and layered fog give the settlement more depth. It interprets the Widowfen concept in playable 3D geometry; the concept image is not used as a flat backdrop.

The scene contains 650 new decorative actors and 224 deterministic plantings. Decoration has collision disabled. The saved map is compared against the polish baseline for character, animation and voice bindings, Fury assets, player and extraction positions, and retained collision geometry. Combat rules, abilities and input source are unchanged.

![Editor gameplay showing the added Widowfen trees, textured ground and distant belfry during staged combat.](docs/images/dark-relic-realistic-editor.jpg)

Actual rendered editor capture, September 13, 2026. This is staged verification footage from the candidate, not the concept artwork or a human playthrough.

## Build and review

This remains a prepared-project workflow, not a blank-project installer. It requires the verified Polish project with `/Game/WidowfenPrep/LVL_DarkRelicEnhanced`, the existing Poly Haven source folders and materials, and the existing Paragon character/voice bindings. No additional asset purchase or model API is required.

1. Stage the integration scripts in `H:\DarkRelicRealisticDelivery-20260913\integration` and `docs/realistic-source-manifest.json` as the delivery folder's `source-hashes.json`. The manifest describes the committed candidate source; regenerate it from reviewed repository source when changing those files, never from the target merely to make a failed comparison pass. Inspect the hardcoded project, engine and delivery paths before adapting to another machine.
2. `install_realistic_widowfen.ps1` verifies the Polish baseline and protected release hashes, copies it to a separate project, and invokes `run_realistic_build.ps1`.
3. `widowfen_geometry.py` generates five reproducible GLB meshes. `realistic_widowfen.py` assembles `/Game/WidowfenPrep/LVL_DarkRelicRealistic`; `validate_realistic_scene.py` verifies saved gameplay parity.
4. The build runs 102 Unreal gameplay checks and captures eight rendered HUD/gameplay states in DX11. Material compilation failures block visual review. Inspect the actual images before creating a passing `IntegrationEvidence/visual-review.json` receipt.
5. `package_realistic_widowfen.ps1` requires the passing build, parity and visual receipts, selects the new default map in the isolated project, packages into a separate output and reruns the 102 checks.
6. `verify_realistic_release.ps1` captures eight states at both 720p and 1080p, exercises normal Escape exit, verifies source and protected-release hashes, and creates the separate candidate shortcut. Keep desktop input idle during its ordinary-play stage.

Existing destination folders and receipts block accidental reruns. Inspect terminal receipts, logs and any surviving child process before retrying; preserve failed-attempt evidence. Do not overwrite accepted packages or their shortcuts.

For this delivery, the initial source manifest predated the shader fix. `finalize_realistic_release.ps1` handles only that recorded failure after successful package, capture and ordinary-exit checks. It requires a digest-pinned manifest from the committed source, verifies all 18 source files and the 16 reviewed original PNG hashes, rechecks every protected release file, and writes a separate `realistic-finalize.json`. The original failed receipt and manifest remain intact. It does not rebuild the game or waive a source mismatch.

## Verified delivery

Delivered September 13, 2026 at **8:50 AM Arizona**. Open **Dark Relic Realistic Widowfen** on the development desktop, targeting `H:\DarkRelicRealisticPackage\Windows\DarkRelicSmoke.exe` with DX11 at 1920 × 1080. The existing Polish package and earlier releases remain intact.

- 169 portable gameplay checks, two geometry tests and five PowerShell syntax checks pass.
- The scene saves and reloads with 650 decorative actors; 101 retained colliders and gameplay/voice bindings match the baseline.
- 102 editor and 102 packaged runtime checks pass, including actual gameplay ticking, extraction and persistent rewards.
- Sixteen packaged frames at 720p/1080p were inspected for environment rendering, combat warnings, ward prompts and results. Both starting frames are free of the transient editor shader-preparation overlay; packaged capture logs have no material compilation errors.
- Normal keyboard Escape exit returns 0. All 18 expected source files match committed source, 57 protected release files remain unchanged, seven package executable/container hashes are recorded, and the new shortcut passes readback.

The canonical final receipt is `H:\DarkRelicRealistic-20260913\IntegrationEvidence\realistic-finalize.json`. Earlier package-launch timeouts were reconciled from receipts; the initial source-manifest failure is retained alongside the verified finalization. No rebuild or repeat execution is required. This is a playable 3D environment enhancement, not photorealistic parity with the concept image; fresh human playfeel, listening and performance acceptance remain separate.

Run the portable checks with `bash scripts/test-core.sh` and the geometry checks with `python tests/test_widowfen_geometry.py`. These do not substitute for rendered Unreal validation. Specialist routing for this environment pass is shadow-only, not an independent review or human playthrough.
