# UMAA Component SDK — Public API Design

> **Current design baseline:** v0.8.4 (2026-11-11). This repository currently contains **design specifications, review artifacts, and illustrative C++11 API sketches**, not a production UMAA SDK.

## Start here

1. **[HANDOFF.md](HANDOFF.md)** — session-independent project context, decisions, sources, known gaps, and exact resume plan.
2. **[docs/current/UMAA_Component_SDK_Public_API_v0.8.4.md](docs/current/UMAA_Component_SDK_Public_API_v0.8.4.md)** — authoritative current user-facing API proposal.
3. **[docs/current/REVIEW_v0.8.4.md](docs/current/REVIEW_v0.8.4.md)** — user-scenario coverage, counterexamples, and open issues.
4. **[examples/api_sketch/](examples/api_sketch/)** — C++11 syntax sketches; not an SDK implementation.
5. **[docs/history/](docs/history/)** — previous specifications and ICD/IDL static-analysis documents.

## Design objective

Implement UMAA ICD 6.0 internally while exposing only user-relevant business events, decisions and operations externally. **Do not mirror DDS topic/control mechanics into separate public handlers.**

- Service: `umaa::Service<Derived, ServiceKind>`; kinds `Config`, `Control`, `Specs`, `StatusReport`, with `Other` for exceptions.
- Component declares roles: `m.provider<Service>()` / `m.consumer<Service>()`.
- Provider/Consumer handlers are attached per actual ICD operation, not by service-name convention alone.
- Eight business verbs remain: `command`, `cancel` (provider cancel handler), `report`, `send`, `cancel` (consumer action), `respond`, `publish`, `dispose`.
- SDK handles ACK and CommandStatus flow, queryless subscription, IDs, DDS topics, synchronization, and protocol housekeeping. Application supplies business outcomes and actual execution/report data.
- No per-kind handler base class or `m.collection` / `m.specialized` requirement.

## Status

C++11 API sketch syntax and some invalid-registration tests were checked in the design artifacts. Full generated type compilation, all-service user scenario verification, DDS integration, and ICD runtime conformance **have not been established**. See [handoff](HANDOFF.md).

## Source-material policy

The user-supplied ICD PDFs and IDL archive are **not mirrored here**; redistribution permission has not been verified. `HANDOFF.md` records the source inventory and how to reattach the source materials in a new session.

_This is a design/handoff migration, not a claim that an implementation has shipped._
