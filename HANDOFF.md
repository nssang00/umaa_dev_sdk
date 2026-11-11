# HANDOFF — UMAA Component SDK v0.8.5 Public API

**Updated:** 2026-11-11 Asia/Seoul  
**Repository:** https://github.com/nssang00/umaa_dev_sdk · branch `main`  
**Authoritative spec:** `docs/current/UMAA_Component_SDK_Public_API_v0.8.5.md`  
**Status:** SDK user-facing **design proposal** only; not the actual working DDS SDK or proof of ICD conformance.

## 0. Session resumption: read this first

The user wants a **minimal, universal C++11 UMAA Component SDK API** that covers the user developer's business tasks, with rigorous UMAA 6.0 protocol implementation hidden inside the SDK (the **iceberg principle**). The service is not a separate user-written subclass; users write a `umaa::Component`, register business callbacks, and call Service runtime role interfaces. The user prefers precise Korean, conclusion first, distinctions between evidence vs API design vs unverified assumptions.

**Most recent design decision v0.8.5:** **2 flows × 2 roles = four classes** with user-level functions and Hooks. No additional ControlServiceProvider/ConfigServiceConsumer/SpecsServiceProvider variants. The class template argument `S` is the generated **Service** type; each individual Operation is a method-level `<Op>`.

```text
Command/Response   CommandProvider<S>  : onCommand, onCancel, respond, publish<ExecutionOp>
                   CommandConsumer<S>  : command, cancel, onStatus, onAck?, onExecutionStatus?, onRecoveredCommand?
Queryless R/R      ReportProvider<S>   : publish, dispose, publish(snapshot, ReplaceOwnedInstances)
                   ReportConsumer<S>   : onReport (auto-subscribe), request(handler) (dynamic), getLatest?
```

Read this file and the newest spec before editing anything. Older v0.8.4 examples use a **different** `m.provider/m.consumer` API and must not be copied verbatim.

## 1. Current essential artifacts

- `README.md` — project entry.
- `docs/current/UMAA_Component_SDK_Public_API_v0.8.5.md` — current contract.
- `docs/current/REVIEW_v0.8.5.md` — scenario matrix + limitations.
- `docs/current/CHANGELOG_v0.8.5.md` — migration mappings from v0.8.4.
- `docs/current/VALIDATION_SUMMARY_v0.8.5.json` — exact evidence of tests / not-run.
- `examples/api_sketch/PUBLIC_API_CXX11_DECLARATION_SKETCH_v0.8.5.hpp` — mock C++11 declarations only.
- `examples/api_sketch/AUTOPILOT_FOUR_ROLES_v0.8.5.cpp` — example across four role classes.
- `examples/api_sketch/README_v0.8.5.md` — scenario and how to syntax-check.
- `docs/history/v0.8.4/` — **archived full v0.8.4 spec**, review, changelog and validation; earlier v0.7/v0.8.x history retained.
- `docs/SOURCE_INVENTORY.md` — source materials intentionally not redistributed.

## 2. User API architecture (keep stable unless evidence demands change)

**Type model:** `Service<Derived, ServiceKind>` generated static Service definition; `ServiceKind::Config/Control/Specs/StatusReport/Other` categorizes but never alone determines flow. Actual generated Operation role/flow/capabilities are authoritative. `CommandProvider<S>` etc. are SDK-owned runtime role interfaces. `Component` owns them through `service<S>().commandProvider()/commandConsumer()/reportProvider()/reportConsumer()`.

**Configuration/Hook syntax:** 
```cpp
UMAA_COMPONENT(AutoPilot, m) {
    m.commandProvider<mo::GlobalVectorControl>()
      .onCommand<mo::GlobalVectorControl::SetGlobalVector>(&AutoPilot::onVectorCommand);
    m.commandProvider<mo::GlobalVectorControl>()
      .onCancel<mo::GlobalVectorControl::SetGlobalVector>(&AutoPilot::onVectorCancel);
    m.reportConsumer<sa::GlobalPoseStatus>()
      .onReport<sa::GlobalPoseStatus::Report>(&AutoPilot::onPoseReport);
}
```

**Actions:** `CommandConsumer<S>::command<CommandOp>(payload[,options])` for a new Source/Session; `CommandCall<Op>::update(newPayload)` for same active Source/Session and newer Timestamp; `CommandConsumer::cancel(call)` submits cancellation, actual CANCELED is later observed. `CommandProvider::respond(token,CommandProgress)` for actual command lifecycle; `CommandProvider::publish<ExecutionOp>(token,payload)` for actual execution telemetry. `ReportProvider::publish<ReportOp>(payload)`, `dispose<ReportOp>(key)`, `publish<ReportOp>(snapshot,ReplaceOwnedInstances)`.

**Hooks:** 
- `CommandProvider::onCommand<CommandOp>(callback)` sees New/Updated/Recovered via `CommandInfo`, returns `CommandDecision` (accepted / rejected / deferred / resume); don't treat accepted as EXECUTING or COMPLETED.
- `CommandProvider::onCancel<CommandOp>(callback)` only for active Commands; cannotCancel/deferred/canceled distinguish actual hardware stopping.
- `CommandConsumer::onStatus<CommandOp>(callback)` is default meaningful status observer; optional `onAck<CommandOp>` (receipt), `onExecutionStatus<ExecutionOp>` (telemetry), `onRecoveredCommand<CommandOp>` (recovery continue/cancel business decision). `onResponse()` umbrella hook is **not part of current baseline**.
- `ReportConsumer::onReport<ReportOp>(callback)` is for Data/Disposed, Key, Source, Timestamp. `onReply()` replaced with `onReport()`.

**Subscriptions:** `m.reportConsumer<S>().onReport<Op>(handler)` creates an automatic subscription on `Component` start and destroys it on stop. Alternatively, without static registration for same S/Op, call `service<S>().reportConsumer().request<Op>(handler)` to get a dynamic RAII `Subscription`. **No implicit duplicate**: both for same S/Op default to `AlreadySubscribed`. `request` is *NOT* single-shot RPC because standard UMAA Request/Reply is Queryless. ReportProvider doesn't require `onRequest`.

**Important exceptions:** `Config` may mix multiple Command and Report operations. Neither ServiceKind suffix nor one-Command shortcut is sufficient for all Services. Use precise `Op` type metadata. Exactly the same Service may be both Provider and Consumer in one Component.

**Data model:** `LargeSet<T>/LargeList<T>` with stable element IDs, `add/set/remove/clear/replaceAll`, List `insert/move`, `state` Complete/Incomplete/Invalid; `Specialized<Base>` with base/set<T>/is<T>/as<T>/state Ready/Incomplete/Unsupported/Invalid. Payload API not new wire communication classes. Generated Optional, union, Key/CompositeKey must preserve IDL semantics.

## 3. AutoPilot four-role scenario

```text
MissionManager        AutoPilot                        PositionSensor
CommandConsumer<S> -> CommandProvider<S>
  command(cmd)          onVectorCommand(cmd, info)
  onStatus(status)   <-  respond(token, progress)
  cancel(call)       ->  onVectorCancel(info)

                      ReportConsumer<PoseSvc>        ReportProvider<PoseSvc>
                      onPoseReport(e)              <- publish(pose)
                      Data/Disposed                <- dispose(key)
```

AutoPilot has **two roles** in its Component; the other two are assigned to MissionManager and PositionSensor. A separate `PoseProbe` demonstrates dynamic `request(handler)` and `Subscription.close()`. Do not register `onReport` and `request` for the same Component/S/Op.

The C++ example has intentionally **mock** `mo::GlobalVectorControl`, `sa::GlobalPoseStatus` and IDL-like Payloads, not real generated headers or device control. Illustrative hardware callbacks are application functions, not extra SDK Hooks.

## 4. Evidence: facts vs design choices vs pending work

**Previously verified ICD facts from user-provided documents (not rechecked this migration):**
- EO §3.4: Config/Control/Specs/Status or Report convention with exceptions.
- EO §5.1 Command: Source/Session correlation, status/ack, update with newer Timestamp, cancel and recovery.
- EO §5.2.1: normal Queryless Request/Reply data subscription/Report/disposal.
- EO §3.8/3.9: Large Collection identity/synchronization; Generalization/Specialization linkage.
- MO §6.1.5: GlobalVectorControl setGlobalVector, CommandAck, CommandStatus, ExecutionStatus and command cancel semantics.

**SDK design choices, NOT mandated C++ ICD APIs:** four template classes, `command()` instead of `send()`, `onReport` auto-subscription, `request(handler)` dynamic subscription and `AlreadySubscribed`, `CommandCall<Op>`, `resume()`, `ReplaceOwnedInstances`, `onRecoveredCommand`, typed `onExecutionStatus`.

**Prior static audit notes:** supplied IDL archive contained 594 IDL files; source audit counted 139 unique Services and 314 unique topic Operations, with 311 exact IDL topic matches. Three experimental `@nested` vs top-level Topic differences were recorded (SEM AcousticSelfNoiseInterferenceStatusType, EO FinsStatusType, EO PropulsorsStatusType). **These are not proven public API gaps**. Standard-only audit: 54 Services, 114 Operations. Source files were not redistributed.

**Validation:** `VALIDATION_SUMMARY_v0.8.5.json` is the truth. A mock C++11 syntax check alone does not certify runtime correctness or SDK codegen coverage.

## 5. Open issues / next-step backlog

**P0:**
1. Report Provider restart: determine persisted owned-key inventory, ensure `publish(snapshot, ReplaceOwnedInstances)` never deletes another Provider's data, and define failure when ownership is unknown.
2. Existing Command recovery: if previous Payload, Status, Session association, timestamp, or token state is unknown, expose Unknown and avoid false successful resume/double execution. Choose safe default continue/cancel policy.
3. C++11 actual generated Service types: compile representative Control/Config/Specs/Status and verify correct Operation/flow role binding (all 139 Services eventually).

**P1:**
4. Command cancellation: onCancel omission vs mandatory safety stop for long-running commands, liveliness loss, cancel refusal and terminal outcome.
5. Subscription semantics: RAII/close, auto vs dynamic duplicate error, callback lifetime, retries, thread-affinity, shutdown.
6. Completion/status error model: ACK vs CommandStatus, old token updates, timeout vs remote FAILED, unsolicited Report disposed.
7. C++11 negative static compilation tests and DDS integration scenarios.
8. Large Collection and Specialized<T> nested data behavior against real generated IDL.

## 6. Sources and publication policy

The user attached `UMAA_Public_API_Summary_KO.md` and `umaa_starter_kit_hooks.md` as design inputs to the November 11 conversation; core decisions are **recorded in current spec and this Handoff**. Original UMAA ICD 6.0 PDFs (EO/MO/MM/SA/SEM/SO, EXP and CO-EXP) and the `umaa-6.0-idl-with-exp-services.zip` source bundle (~594 files) are **not in this public GitHub repo**: redistribution rights were not checked. Their former `/mnt/data` paths are ephemeral and not valid across sessions. Request original attachments again to verify exact clauses or regenerate real IDL code.

## 7. Exact next-session resumption steps

1. Open `https://github.com/nssang00/umaa_dev_sdk`, read **`HANDOFF.md`**, **v0.8.5 Public API**, **REVIEW**, **VALIDATION** and AutoPilot example.
2. Do not assume v0.8.4 method names still apply; consult the migration table in the newest spec.
3. If discussing ICD facts, use originals if attached or flag them as earlier cross-checks; no invented new service names or wire topics.
4. Work on P0s; update current spec, changelog/review, example and HANDOFF together, then make a GitHub commit with its link.
5. On any interruption, verify current `main` and recent commits before repeating changes; do not blindly overwrite or duplicate.

**Copy-paste prompt for a new session:**

`https://github.com/nssang00/umaa_dev_sdk 저장소의 HANDOFF.md와 v0.8.5 Public API·AutoPilot 4-role 예제를 읽어줘. 네 역할 클래스(CommandProvider/CommandConsumer/ReportProvider/ReportConsumer), onReport 자동 구독/request 동적 구독 계약을 기준으로 설계를 이어가고, ICD 근거와 구현 검증 여부를 구분해줘.`

## 8. 2026-11-11 ICD naming correction and API usability decision

**Source-verified names (user-provided UMAA 6.0 IDL ZIP):**
- MO Service: `GlobalVectorControl`; operations `setGlobalVector`, `reportGlobalVectorCommandAck`, `reportGlobalVectorCommandStatus`, `reportGlobalVectorExecutionStatus`.
- MO IDL data structs: `UMAA::MO::GlobalVectorControl::GlobalVectorCommandType`, `GlobalVectorCommandAckReportType`, `GlobalVectorCommandStatusType`, `GlobalVectorExecutionStatusReportType`.
- SA Service: `GlobalPoseStatus`; operation `reportGlobalPose`; IDL data type `UMAA::SA::GlobalPoseStatus::GlobalPoseReportType`.
- `GlobalVectorCommandType` has required `direction` (variant), `directionMode`, `speed` (variant), and optional `depthChangePitch`, `elevation`, `endTime`. `GlobalPoseReportType` has `position`, `attitude`, `course`, `navigationSolution` and key `source` (`UMAA::Common::IdentifierType`), not fictional top-level `latitude/longitude/sensorId`.

**Code generation boundary:** `UMAA::MO::GlobalVectorControl` and `UMAA::SA::GlobalPoseStatus` are IDL *modules* (C++ namespaces). The new example's `umaa::services::MO::GlobalVectorControl` and nested `setGlobalVector` operation **type tag** are a **proposed SDK-generated facade convention**, NOT names/types present in the original IDL. Do not claim they are pre-existing RTI-generated C++ class names.

**Decision:** Keep **Callback registration** (`UMAA_COMPONENT(AutoPilot, m)` plus typed `.onCommand<...>(handler)` / `.onReport<...>(handler)`) as the default API. Requiring users to subclass each `CommandProvider/ReportConsumer` and override virtual hooks creates multiple inheritance or extra service classes for a multi-service AutoPilot. A subclass/override adapter may be considered optional for *single* Service apps, but must not become the default.

The original aliases `VectorSvc`, `VectorOp`, `PoseSvc`, `PoseOp` and invented payload fields have been eliminated from `examples/api_sketch/AUTOPILOT_FOUR_ROLES_v0.8.5.cpp`. Previous archived versions retain historical names.

Read `docs/current/API_REGISTRATION_VS_INHERITANCE_v0.8.5.md` for an exact-name map, sourced file inventory, and the alternative designs comparison. v0.8.5 spec section 1.1 explains the namespace separation.

**Validation boundary:** The exact names above were checked against ZIP contents. Reworked examples are still a **C++11 declaration/mock sketch**, not actual generated DDS headers or running hardware code. CI for latest edited example has not been confirmed to pass; compiler/runtime conformance remains pending.
