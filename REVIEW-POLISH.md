# Dark Relic reviewer polish

This candidate applies the game-development persona recommendations after the operator accepted gameplay and audio in the preceding Enemy Feedback build.

- HUD warnings have dark backing and draw above the hit tint. Enemy labels avoid the status panels and one another, with leader lines for displaced labels and a footer guard. Bellkeeper danger uses radial spikes, while the ward retains its smooth gold ring.
- Extraction prompts require Blackbell. Results show credits earned this run, available bank, Resolve cost, +20 health benefit and ownership. Only restart, eligible upgrade and quit remain on results.
- Hexbound shows a diamond cast tell only while its live windup has range and line of sight. A short cross-shaped impact follows successful damage. There is no projectile and no change to timing, damage or dodge rules.
- Procedural combat sounds use spatial attenuation once. Warning sounds can displace lower-priority cues at the 16-cue limit. Pain and terminal Warden voices survive routine exertion. Death and restart clear old cues.
- The prepared Widowfen map reduces incidental doorway lights, retains neutral hero fill and ward lanterns, moves selected non-colliding decoration away from the main fight, and adds shallow wet ground patches. No new marketplace dependency is introduced.

Sword timing and extraction duration are preserved following operator acceptance. Replay variation, additional weapons, multiplayer and the parked profiler-shutdown fault are outside this pass.

Verified September 12, 2026 at 5:14 PM Arizona: gameplay source a36a252 passed 169 portable checks, 102 editor checks and 102 packaged checks. Compilation, map/bindings, packaging, sixteen presentation captures (eight states each at 720p and 1080p), and normal keyboard Escape exit passed. Eleven plugin source hashes match and fifty protected release files, including the accepted submission ZIP, are unchanged. Four game-specialist source reviews pass. The separate Windows build is H:\DarkRelicPolishPackage; open the Dark Relic Polish Candidate desktop shortcut. No game/editor/build worker remained at final verification.

The operator accepted gameplay and audio in the preceding Enemy Feedback build. These automated checks do not establish a new full human playthrough, headphone/speaker/mono listening comparison, or full performance benchmark. The profiler shutdown fault remains parked; ordinary Escape exit passes. Event-rule and asset-receipt clearance remain separate from this source/candidate delivery. PR #9 contains these improvements.

`integration/polish_review.py` edits only a copy of the already prepared enhanced map. It is not a blank-project installer. The Windows installer rejects existing targets; release verification accepts an already matching candidate shortcut and rejects a conflicting one. Inspect receipts before retrying. The ordinary-input verifier acquires viewport focus before sending keys; avoid concurrent desktop commands during that stage. Presentation capture uses an isolated save and deliberately staged states; those screenshots are not a human full-run recording.
