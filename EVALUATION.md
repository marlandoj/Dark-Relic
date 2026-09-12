# Runtime verification

September 12, 2026: the character candidate compiles with Unreal 5.8 on Windows and passes 90 portable checks plus 27 actual-world character/gameplay checks. BuildCookRun completed successfully; the cooked standalone executable also passed all 27 checks in 1080p DX11 and exited 0. Editor and packaged captures exited 0. Nine plugin source hashes match between Zo and Windows. See `CHARACTER-INTEGRATION.md` for the prepared project, scripts, bindings and evidence locations. Stability and performance results are recorded with the release evidence.

The packaged default map contains the encounter actor, which consumes character bindings and drives the same gameplay component tested by the smoke. Required skeletal assets are installed and referenced by the map. Test save slots are isolated and cleaned up. No existing asset or project was deleted or renamed. Human input/feel, long traversal performance, final art quality and submission readiness are separate acceptance items. No real specialist review was invoked; routing remains shadow-only.

## Historical source candidate evaluation

The following records the original source-only candidate, before factory approval and Windows integration. Its pending statements do not describe the current playable character build.

September 11, 2026. Source candidate only; full game and visual readiness remain Amber.

## Proven locally

- `bash scripts/test-core.sh`: 81 checks pass with g++ C++17, Wall/Wextra/Werror/pedantic; command-line demo extracts Blackbell for 100 credits, buys one upgrade, and starts the next run with 120 health.
- The same 81 checks pass under AddressSanitizer and UndefinedBehaviorSanitizer, without reported errors.
- Checks cover death preserving earlier bank while removing carried items, restart, duplicate pickup IDs, one Relic per run, leaving/re-entering extraction, exactly-once banking, insufficient stamina, action recovery, dodge window, heal windup/charge/cap, invalid clock/damage/tuning, save version/count/value limits, rejected-save nonmutation, bank overflow rejection and partitioned-time healing.
- Unreal adapter source delegates to `Portable/DarkRelicRules.h`, the actual tested header. It does not reimplement inventory/combat/extraction arithmetic.
- Source delivery tooling passed archive readback and deliberately altered-file rejection. No marketplace mesh, texture, character, animation, audio or generated art is included.
- TypeScript: not applicable; this repository contains no TypeScript project. No JS toolchain was added.

## Not proven

Unreal Header Tool, Windows compiler/linker, plugin discovery, Blueprint calls/events, actual SaveGame file I/O, controller/mouse input, animation contact, enemy navigation/damage, UMG, scene assembly, 1080p DX11 performance, clean packaging and five consecutive packaged runs have not been tested for this candidate. Prior smoke/static-prop results do not cover the new code. The adapter's engine-specific API declarations require the Windows compilation gate.

The default timers/numbers are tuning proposals. The source contains no executable map, enemy AI, montage, melee trace, dodge movement, sound or HUD asset. Blueprint callbacks supply these. SaveBank is explicit; the integration guide requires handling save errors before a map reload. A Blueprint-only rewrite is a fallback contract, not covered by the C++ tests.

## Factory provenance

ZOU-1632 passed the five-field contract and dispatched as SWARM (0.54). Medium-risk admission passed. Execution exec-486ec88a used a separate worktree from the initial source scaffold. Claude Code timed out before generating source; OpenCode failed; Codex repeatedly reported provider DNS/transport failures. The bounded attempt was stopped with no surviving worker processes and was recorded as failed with target_reached=false. No source changes were produced by those workers. The primary chat implemented this candidate separately on work/hackathon-local-source.

No global factory flags, containment policies, promotion controls or Windows processes were changed. No factory-ready label was added for repeated automatic retries. The task has a local stop receipt and factory lifecycle/journal evidence in the hackathon project. Specialist routing resolved five game roles in shadow mode; this is not independent human or model approval.

## Five-part gap audit

| Check | Finding |
|---|---|
| Reachability | Command-line demo and tests call the portable core. Plugin source calls that same core. The production consumer is the operator's Third Person player component after manual compilation and wiring; it is not yet installed. |
| Data prerequisites | Four item defaults, health/stamina/action tuning, extraction time and bank schema are present. Unreal scene actors, asset bindings and save slot contents are still operator prerequisites. |
| Cross-process state | Portable bank payload validation is tested. Actual UE SaveGame persistence across game restarts is pending Windows evidence. |
| Test/production parity | Core rules are shared directly. UE wrapper, event order, save I/O and gameplay timing are explicitly outside local coverage. |
| Dangling identifiers | No existing identifier, Windows source, asset, service or public route was deleted or renamed. New package names are internally consistent in descriptor, Build.cs and module. |

Verdict: portable source rules pass; Unreal integration candidate requires Windows validation. This is not a Green game-readiness verdict.
