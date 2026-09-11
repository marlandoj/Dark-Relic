# Dark Relic factory source lane

status: complete
watchdog: off

- [x] Recover game scope and separate Windows ownership
- [x] Resolve Unreal game specialists in shadow mode
- [x] Submit a validated factory ticket and confirm actual execution
- [x] Implement and test portable gameplay rules and Blueprint adapter source
- [x] Review source, integration instructions and evidence boundaries
- [x] Deliver a versioned source archive and Windows handoff

Windows compilation, scene assembly and packaged gameplay remain operator-owned. Full game readiness remains Amber. No game runtime is deployed by this lane.

Factory ticket: ZOU-1632. Dispatcher: SWARM 0.54, medium-risk gate passed. Execution exec-486ec88a failed before source generation due to provider connectivity; stopped with zero surviving workers. The primary chat separately implemented this source candidate; 81 core checks and sanitized execution pass. Invocation/stop receipts: ../dark-fantasy-extraction-rpg/hackathon/evaluations/. Private source repository: https://github.com/marlandoj/dark-relic-hackathon-kit. Windows remains separately owned by the operator.

Delivery: ../dark-fantasy-extraction-rpg/hackathon/delivery/dark-relic-source-kit-d96d9a7.zip (source commit d96d9a76c395588714e8f15903a3093acb4796f4). ZIP manifest/hash verification and tests from a fresh archive extraction pass. Draft review: https://github.com/marlandoj/dark-relic-hackathon-kit/pull/1. This completes only the local source-candidate delivery, not factory recovery or Windows integration.
