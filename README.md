# Dark Relic hackathon source kit

## Playable Widowfen integration

The `hackathon/playable-widowfen` branch adds a real Unreal encounter actor and HUD on top of the approved 90-check portable core. In the prepared Windows project, it drives light/heavy contact, enemy windups, dodge/heal, three enemy roles, four loot types, the elite relic lock, timed extraction, bank persistence, one upgrade, and restart. It preserves the Third Person template's movement and camera.

Controls: WASD and mouse; left/right mouse for light/heavy attacks; Shift to dodge; Q to heal; E to collect or extract. After a run, R restarts and U buys the permanent upgrade. Escape quits.

`integration/build_playable_map.py` duplicates the imported Widowfen look-development map and wires the encounter. `integration/polish_playable_map.py` adjusts exposure and adds surrounding deadwood. `integration/run_playable_build.ps1` compiles, assembles, runs actual-world checks, captures DX11 gameplay, and packages the prepared project. These scripts target the existing isolated `H:\DarkRelicIntegration-97ff125` project; they are not a clean-project installer. The repository does not distribute Unreal, marketplace assets, or binary maps.

Character integration now replaces the template mannequin with Greystone and three distinct minion roles. Explicit bindings validate skeletons, idle/locomotion and action clips. See [CHARACTER-INTEGRATION.md](CHARACTER-INTEGRATION.md) for the isolated Windows project and build procedure. Final art and human play/feel approval remain separate from technical acceptance; no AAA-quality claim is made.

Run the executable with `-DarkRelicSmoke -NullRHI -nosound` to exercise the same encounter methods and component tick used in normal play. The smoke uses an isolated save slot and removes it. `-DarkRelicCapture -dx11 -windowed -ResX=1920 -ResY=1080` records a frame and exits. Ensure the project's `IntegrationEvidence` directory exists before running these modes.

The operator's later authorization gives Zo ownership of Windows integration; the earlier operator-only source-lane notes below are historical.

Source-only companion to the Unreal 5.8.1 solo PvE extraction game. The operator owns all Windows editor operations and final packaging. This repository does not contain a playable Unreal project or marketplace assets.

Canonical game brief: ../dark-fantasy-extraction-rpg/hackathon/TASK.md. Frozen Widowfen art and acquired assets remain in that project. Preserve third-person solo, one weapon, light/heavy attack, dodge/heal, two common enemy roles, one elite, three ordinary loot items, Blackbell Relic, extraction countdown, bank-on-success/lose-on-death, one upgrade and restart. No multiplayer, blockchain, new art purchases, or desktop automation.

The primary chat supplied a portable C++ gameplay core and a thin Blueprint-callable Unreal plugin after the factory workers failed to reach their providers. The core passes 81 checks and a command-line demo, including an AddressSanitizer/UndefinedBehaviorSanitizer run. This is an optional integration candidate until Windows UHT/compiler and packaged playtesting pass. Blueprint scene assembly, controls, animation notifies, collision traces, audio and UMG remain in the operator lane. Never silently enable a plugin in an existing project.

Do not mark full game or visual readiness Green from Linux-only tests.

Start with [WINDOWS-INTEGRATION.md](WINDOWS-INTEGRATION.md), then [WINDOWS-COMBAT-HANDOFF.md](WINDOWS-COMBAT-HANDOFF.md). A Blueprint-only fallback contract is included if the plugin cannot be compiled quickly. Run portable checks with `bash scripts/test-core.sh`; the test and demo binaries are built in disposable /tmp directories. See [EVALUATION.md](EVALUATION.md) for the exact evidence boundary.
