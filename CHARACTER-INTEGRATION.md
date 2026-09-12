# Character integration

The character map uses Greystone for the Warden and Dusk melee, ranged and super minions for Dreg, Hexbound and Bellkeeper. `FDarkRelicCharacterVisuals` stores explicit mesh, sequence, blendspace and speed-axis bindings on the encounter actor, giving the cooker hard references to the required assets.

The importer rejects incompatible skeletons and additive action sequences. Stationary characters use their idle sequence; moving characters use the authored locomotion blendspace, with playback scaling above its maximum speed. Light/heavy attacks, the evasive hop and deaths select matching sequences. The hop uses Greystone's jump-start clip; it is not a bespoke dodge roll.

## Prepared Windows project

- Project: `H:\DarkRelicCharacters-20260912\DarkRelicSmoke.uproject`
- Map: `/Game/WidowfenPrep/LVL_DarkRelicCharacters`
- Packaged output: `H:\DarkRelicCharacterPackage\Windows\DarkRelicSmoke.exe`
- Build receipt: `IntegrationEvidence/character-build.json`
- Stability receipt: `IntegrationEvidence/character-stability.json`

The `integration` scripts operate on this prepared project and its installed packs. They are not installers for a blank Unreal project. Preserve the original project, inspect existing job receipts and confirm no active build before running another attempt. Copy the complete plugin source, header and binding scripts together; a partial update can fail compilation or retain stale map values.

`run_character_build.ps1 -Package` compiles, validates and saves bindings, runs 27 actual-world checks, captures a frame, then cooks and tests the standalone build. `validate_character_package.ps1` adds four packaged relaunches and a short stationary-player performance sample after the first packaged run. Neither script establishes human play/feel approval, traversal performance or final art approval.

The public source contains no marketplace binaries or generated media. Keep Fab acquisition receipts and the existing asset ledger with the private Unreal project. Do not upload the character assets to image or 3D generation services.
