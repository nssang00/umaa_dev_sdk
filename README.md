# UMAA Component SDK — C++11 User Public API

**Design baseline: v0.8.5 (revised 2026-11-12; Improvement A)** · Status: **public API proposal**, not production implementation or proven ICD conformity.

The goal is a high-level UMAA ICD 6.0 SDK built around the application's **business events and actions**, with DDS Topics, ACK/CommandStatus state machinery, Queryless subscriptions, correlation, liveliness and collection/specialization internals hidden.

## Read first

- [HANDOFF.md](HANDOFF.md) — durable context, decisions, open issues and exact session-resume checklist
- [v0.8.5 Public API](docs/current/UMAA_Component_SDK_Public_API_v0.8.5.md) — **authoritative current spec**
- [Review and limitations](docs/current/REVIEW_v0.8.5.md)
- [ICD-exact naming and registration vs inheritance](docs/current/API_REGISTRATION_VS_INHERITANCE_v0.8.5.md)
- [Change log](docs/current/CHANGELOG_v0.8.5.md)
- [AutoPilot four-role example](examples/api_sketch/AUTOPILOT_FOUR_ROLES_v0.8.5.cpp)
- [C++11 declaration sketch](examples/api_sketch/PUBLIC_API_CXX11_DECLARATION_SKETCH_v0.8.5.hpp)
- [Prior versions](docs/history/) — v0.7 through v0.8.4; do not treat as current API
- [Source inventory](docs/SOURCE_INVENTORY.md)

## Service-first Component registration (Improvement A)

Register Service Provider/Consumer intent with **only two methods**: `m.provide<S>()` and `m.consume<S>()`. The specific ICD Operation `<Op>` determines its Command/Response or Queryless Report handling. Four role classes remain SDK **runtime interfaces**, not four separate registration methods.

```cpp
namespace MO = umaa::services::MO;
namespace SA = umaa::services::SA;

UMAA_COMPONENT(AutoPilot, m) {
    m.provide<MO::GlobalVectorControl>()
      .onCommand<MO::GlobalVectorControl::setGlobalVector>(&AutoPilot::onSetGlobalVector)
      .onCancel<MO::GlobalVectorControl::setGlobalVector>(&AutoPilot::onCancelGlobalVector);
    m.consume<SA::GlobalPoseStatus>()
      .onReport<SA::GlobalPoseStatus::reportGlobalPose>(&AutoPilot::onGlobalPoseReport);
}
```

The `umaa::services` namespace is a **proposed SDK-generated facade**, not original IDL's `UMAA::...` namespace.

## Four role classes

| Flow | Role API | User actions | Hooks |
|---|---|---|---|
| Command/Response | `CommandProvider<S>` | `respond`, `publish<ExecutionOp>` | `onCommand`, `onCancel` |
| Command/Response | `CommandConsumer<S>` | `command`, `cancel`; `CommandCall<Op>::update/get/wait` | `onStatus`, optional `onAck`, `onExecutionStatus`, `onRecoveredCommand` |
| Queryless Request/Reply | `ReportProvider<S>` | `publish`, `dispose`, snapshot `publish` | none by default |
| Queryless Request/Reply | `ReportConsumer<S>` | optional dynamic `request<Op>(handler)`, `getLatest` | `onReport<Op>(handler)` (starts auto-subscription at Component start) |

`S` is a generated **Service definition** (e.g., `mo::GlobalVectorControl`), `Op` is a generated **Operation type**. The four classes are **runtime role interfaces**, not four different Service subclasses. For Config services, Command and Report role APIs can coexist. Actual supported roles are determined by ICD Operation metadata, not Service name suffixes.

**Subscription decision:** `onReport<Op>(handler)` is static auto-subscription; `request<Op>(handler)` is dynamic subscription with a `Subscription` handle. The same Component must not use both for identical S/Op without an explicit future extension; default result is `AlreadySubscribed`.

## AutoPilot example structure

```text
MissionManager (CommandConsumer) --> AutoPilot (CommandProvider)
                                 --> AutoPilot (ReportConsumer) <-- PositionSensor (ReportProvider)
```

The example is **illustrative only**. Its mock IDL payloads, declarations and empty hardware methods do **not** implement a real SDK or DDS runtime.

## Validation truth

See `docs/current/VALIDATION_SUMMARY_v0.8.5.json` for exact verified vs unverified checks. A C++11 syntax-only sketch is not evidence of full UMAA ICD compliance. Full generated Service compile coverage, actual DDS data exchange, provider restart cleanup and command recovery remain open.

## Source material / rights

The earlier user-provided UMAA ICD PDF manuals and the expanded 594-IDL ZIP are **not included** in this public repository because their redistribution rights have not been checked. See [source inventory](docs/SOURCE_INVENTORY.md); new sessions should use attached originals when exact primary-source validation is needed.
