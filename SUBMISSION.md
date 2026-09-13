# Dark Relic — Hackyard submission preparation

status: in_progress
watchdog: paused

## Event and deadline

Target: [Hackyard Yard #2 — Backlog](https://hackyard.tech/yards/yard-2). The public builder roster lists **@marlandoj**. The build window is September 11–13, 2026, **11:00 AM Arizona** on both days (18:00Z). Submit before **September 13 at 11:00 AM Arizona**; entries lock when judging starts.

The [general FAQ](https://hackyard.tech/faq) requires a public repository, a writeup of at most 500 characters and an AI model declaration. It calls video optional, but the Yard #2 page specifically asks for a demo video: prepare one. A screenshot is optional and appears on the entry card. A claimed spot is required; the roster supports participation, but authenticated submission has not been verified.

## Copy-ready fields

**Repository:** https://github.com/marlandoj/Dark-Relic

**Title:** Dark Relic

**Writeup — under 500 characters:**

> Dark Relic turns my dark-fantasy game backlog into a solo Unreal extraction adventure. Enter Widowfen as the Warden: chain sword strikes, unleash Relic Burst and Fury, defeat the Bellkeeper, recover Blackbell, then survive the ward countdown. Read enemy tells, dodge, heal and bank your haul; death costs this run's loot. Built with AI assistance and premade assets. Windows gameplay is verified locally; this public repo contains the source kit.

**Model declaration draft:**

> OpenAI GPT-6 Astra via Codex on Zo Computer for development and iteration; Moonshot models for specialist source reviews. fal.ai and Tripo supported visual/asset preparation. One human builder directed the work and accepted baseline gameplay/audio.

This names models supported by the conversation and saved review receipts. Exact Moonshot and asset-generation model versions are not established here; do not invent them. Review this declaration for any additional models used before posting. AI role personas are software assistants, not additional human teammates. No private session logs, credentials or asset receipts are included.

**Screenshot:** [Existing gameplay hero](docs/images/dark-relic-hero.jpg), a real enhanced-map capture from September 12. It predates the newest HUD polish; do not label it as the final polish capture.

**Demo video:** no approved hosted URL is available yet. Use an unobscured gameplay recording with game audio. Suggested 60–90-second sequence: entering Widowfen; sword hit/recoil and Hexbound warning; Relic Burst and Fury glow/fade; Bellkeeper attack; Blackbell pickup; ward extraction and banked reward. Use normal gameplay or clearly label edited/staged excerpts. Do not reuse the rejected recording with the Windows Security overlay.

## Timing and provenance

- GitHub reports repository creation on September 11, 2026 at **1:49:32 PM Arizona** (20:49:32Z).
- Earliest recorded source-kit commit `e9d3960`: September 11 at **1:49:24 PM Arizona** (20:49:24Z), after the 11:00 AM kickoff. It initializes the source lane; `d96d9a7` adds the tested gameplay core.
- Latest gameplay polish revision `a36a252` and final source/documentation `c7aa421` were merged in PR #9 as `02d8576` on September 12 at **6:02 PM Arizona**.
- Planning, visual prototypes, asset acquisition and preparation preceded the gameplay build. Unreal, premade character/environment assets and generated art are declared inputs. This is not a claim that every input was created during the event.
- Commit and repository dates are useful evidence, not proof of original creation times. Preserve the history. The event asks for original project code during its window; any earlier custom code or scene scripts used as inputs must be disclosed and reconciled before claiming full compliance.

## Build and release boundaries

The latest **Dark Relic Polish Candidate** is installed locally on Windows. Recorded evidence: **169 portable checks, 102 editor checks and 102 packaged checks**, successful compilation/packaging, sixteen staged 720p/1080p captures, normal Escape exit 0 and matching source hashes. See [REVIEW-POLISH.md](REVIEW-POLISH.md). These checks do not replace a human full run or listening comparison.

The operator accepted baseline gameplay/audio in the earlier Enemy Feedback build. The separately prepared **1,000,232,678-byte Enemy Feedback ZIP** restores 48 matching files and passes 89 runtime checks; **it does not contain the latest polish**. No public playable GitHub Release or video upload is recorded. A source-only repository clone cannot reproduce the scene without Unreal and the prepared assets/project; see [README.md](README.md#build-with-unreal-engine).

## Submission checklist

- [x] Confirm event, theme, deadline and public builder listing
- [x] Verify public repository and merge gameplay polish PR #9
- [x] Prepare short writeup, model declaration draft and screenshot link
- [x] Document timing, premade assets and latest/older package differences
- [ ] Choose and add an open-source license for original code; exclude third-party assets
- [ ] Retain accepted Fab acquisition/license records
- [ ] Record, inspect and host an unobscured gameplay video with audio
- [ ] Confirm final model declaration and any pre-event custom-code inputs
- [ ] Report a fresh full run of the latest polish if claiming that acceptance
- [ ] Enter final fields in Hackyard and verify the resulting submission page

Public visibility alone does not grant an open-source license. The original-source license is awaiting the owner's selection. Third-party assets and their source libraries remain under their own terms and are excluded from this repository's redistribution grant.

Release/submission preparation is tracked in **ZOU-1648**. The profiler shutdown issue remains parked in ZOU-1633; ordinary Escape exits cleanly. Replay variation, new weapons and multiplayer remain outside the submission scope. This document prepares an entry; it does not claim a completed submission.
