# v0.8.7 — 2026-11-24

- Preserve generated Service-named facades, four internal roles, Component ownership and original IDL typed Operation selection.
- Reduce Service surface to nine names. ExecutionStatus publish(token, ...) → respond(token, ...); onAck/onStatus/onExecutionStatus → onResponse typed overloads; onRecoveredCommand → per-Op RecoveryOptions; dispose → remove; onDispose → onReport typed event; request → onReport in manual mode.
- Keep getLatest and CommandHandle update/cancel/get/wait; release retained for manual retention. Explicit subscription close; no implicit cancellation on handle destruction.
- Preserve Command New/Updated/Recovered and separate Cancel callback, reasons and stale-token guards. Preserve source/Key/partial and observation semantics.
- Shared typed attachments cover Command, update, ExecutionStatus and Report, including field tags. Minimize collection methods without dropping stable identity or ordering.
- Update AutoPilot 14-role and mixed Config examples; historical v0.8.6 files remain. This is a design revision, not runtime implementation or newly verified ICD conformance.
