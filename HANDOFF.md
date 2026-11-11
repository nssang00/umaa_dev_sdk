# HANDOFF — UMAA Component SDK Public API

**Updated:** 2026-11-11 (Asia/Seoul)  
**Repository:** https://github.com/nssang00/umaa_dev_sdk  
**Resume branch:** `main`; read this file and `docs/current/UMAA_Component_SDK_Public_API_v0.8.4.md` first.  
**Current baseline:** **v0.8.4 USER-FACING API DESIGN**. It is a design contract, not implemented SDK runtime or proven ICD conformance.

## 0. One-minute resumption brief

The user is designing a **minimal, universal C++11 UMAA Component SDK Public API** that covers the user-relevant duties in UMAA ICD 6.0, while completely hiding internal DDS/Flow-Control implementation (the **iceberg model**). Do **not** treat ICD Topic counts or DDS protocol steps as reasons to expand the Public API. Distinguish: (1) business events and choices the application must see, (2) the business operations the application must invoke, and (3) SDK-internal protocol work that should never leak into user handlers. User prefers objective, evidence-based Korean responses, with conclusions first.

**Latest decision:** Service is a generated lightweight type inheriting `umaa::Service<Derived, ServiceKind>`. The four conventional kinds are `Config`, `Control`, `Specs`, `StatusReport` (plus `Other` fallback). The Component explicitly binds each service as `m.provider<S>()` or `m.consumer<S>()`. Within the binding, handlers are registered **by actual Operation**, not by service kind. Do not introduce four user-facing ServiceType handler base classes. Provider and Consumer are roles of a Component relative to a Service, not subclasses of that Service.

## 1. Authoritative current documents

- `README.md` — quick entry.
- `docs/current/UMAA_Component_SDK_Public_API_v0.8.4.md` — **authoritative current API contract**.
- `docs/current/REVIEW_v0.8.4.md` — review cases and remaining gaps.
- `docs/current/CHANGELOG_v0.8.4.md` — version delta.
- `examples/api_sketch/EXAMPLE_COMPONENT_CXX11_SKETCH_v0.8.4.cpp` — example, NOT runtime implementation.
- `docs/history/v0.8.3/` — prior Command/Cancel design contracts, historical not authoritative.
- `docs/history/v0.8.1/` — static ICD/IDL coverage reviews, historical factual audit, not an API compile test.
- `docs/SOURCE_INVENTORY.md` — provenance, what is intentionally NOT in this public repo.

If any docs disagree, **v0.8.4 full Public API spec wins**; where unimplemented, mark as proposed rather than present as existing.

## 2. Key Public API decisions (keep stable unless counterexample)

### Service / Role / Operation

```cpp
namespace umaa {
enum class ServiceKind { Config, Control, Specs, StatusReport, Other };
template<class Derived, ServiceKind Kind> class Service;
}
namespace mo {
class GlobalVectorControl
    : public umaa::Service<GlobalVectorControl, umaa::ServiceKind::Control> {
public:
    struct SetGlobalVector;
    struct CommandAck;
    struct CommandStatus;
    struct ExecutionStatus;
};
}
UMAA_COMPONENT(AutoPilot, m) {
    m.provider<mo::GlobalVectorControl>()
       .command<mo::GlobalVectorControl::SetGlobalVector>(&AutoPilot::onVector)
       .cancel<mo::GlobalVectorControl::SetGlobalVector>(&AutoPilot::onCancel);
}
```

This is **illustrative interface shape**, not necessarily compiled against actual generated headers. Provider-only Specs/Status services can bind without any `.command` handler. One Component may be both Provider and Consumer for the same Service. Service naming convention can have exceptions: rely on exact ICD Operation metadata, not suffix-only inference. Single-Command Service shorthand `m.command<Service>` can be allowed only when unambiguous; role-scoped form is preferred.

### Eight business verbs (unchanged)

1. Handler registration `command`, `cancel` (Provider), `report` (Consumer).
2. Operations `send`, `cancel` (Consumer action), `respond`, `publish`, `dispose`.

`provider`/`consumer` are **two configuration functions**, not new DDS/business verbs. Avoid `m.config`, `m.control`, `m.collection`, `m.specialized`, `m.execution` as required user-facing verbs. `onStart/onStop` are optional lifecycle hooks.

### Hiding protocol mechanics

SDK handles DDS topics, readers/writers, queryless request/reply subscription, ACK and ordinary CommandStatus protocol transitions, Source/Session/Timestamp correlations, dispose cancellation, large-collection synchronization, specialization linkage, provider/consumer wire state. Business data and real equipment control, actual ExecutionStatus values and user decisions remain application responsibilities.

### Required user-visible data contracts

- `CommandInfo`: New/Updated/Recovered, source, destination, session, timestamp, optional prior status, opaque `CommandToken`. Command update uses same Source+Session and newer timestamp; never invent wire revision ID.
- `CommandDecision`: validate/accept/reject/defer and **resume** a recovered in-progress command without reverting its state. `respond(token,result)` handles async outcome.
- `CancelInfo`/`CancelDecision`: cancellation *request* differs from successful actual stop; `cannotCancel` may continue and later complete. Sender's `cancel(call)` result must not claim CANCELED until observed.
- `CommandCall`: ACK/status/ExecutionStatus, final outcome vs timeout vs unknown; optional raw `m.report<Op>` for detailed reports.
- `ReportEvent<T>`: Data/Disposed, key, source, timestamp, optional completeness; `publish` and `dispose`.
- `LargeSet<T>`/`LargeList<T>`: stable element IDs, atomic update completion, empty, deletion, order.
- `Specialized<T>`: type identity, missing/incomplete/unsupported states, link synchronization and disposal.
- Lossless C++11 representation of IDL optional fields, keys (including composite) and unions.

## 3. Verified ICD facts and design distinctions

- EO ICD §3.4/Table 3 — Config, Control, Specs, Status/Report are a **convention**; not every Service has every standard Operation. Config can mix command and report.
- MO ICD §6.1.5/Table 22–26 — `GlobalVectorControl` has `setGlobalVector`, CommandAck, CommandStatus, ExecutionStatus; cancel is DDS disposal model, not a separate reply message.
- EO ICD §5.1.2.1–2 — Provider/Consumer must handle existing commands on restart. Business decision to resume, fail, continue observing, or cancel is user-visible.
- EO ICD §5.1.4.2 — updated Command same source+session, newer timestamp; active command restarts flow.
- EO ICD §5.1.4.5 — cancellation is a request; may end CANCELED or continue to COMPLETED; connection/liveliness loss has cancellation requirements.
- EO ICD §5.2.1 — queryless Report service is publish/subscribe; Provider restarts by refreshing or disposing stale reports.
- EO ICD §3.8–3.9 — Large Collections and Specializations have identity and atomic synchronization requirements; surface stable typed data, not DDS machinery.
- SDK `Service<Derived,Kind>`, Provider/Consumer builders, `CommandToken`, `resume()`, `publish<ExecOp>(token,payload)`, `ReplaceOwnedInstances` are **SDK UX design proposals**, NOT C++ APIs required by ICD.

## 4. Review snapshot / validation truth

Latest v0.8.4 review evaluated 20 user scenarios (covered by proposed design or explicitly outstanding). Representative C++11 **declaration sketch** was syntax-checked and bad role / wrong service / bad service type registration examples rejected. **Not done:** compile with production SDK/generated real IDL headers, all-service compile coverage, real DDS behavior, true ICD conformance.

Previous static source audit, including experimental ICDs: **139 unique Services**, **314 unique Topic Operations**, **311 exact IDL Topic matches**. Three experimental ICD/IDL discrepancies were documented:
- `UMAA::SEM::AcousticInterferenceStatus::AcousticSelfNoiseInterferenceStatusType`;
- `UMAA::EO::FinsStatus::FinsStatusType`;
- `UMAA::EO::PropulsorsStatus::PropulsorsStatusType`.
They are `@nested` in supplied IDL without corresponding top-level Topic constants. These are **source mapping discrepancies, not demonstrated Public API gaps**. Standard 54 Services / 114 Operations matched in earlier audit. These are static counts, not runtime conformance evidence.

## 5. Open issues — next session backlog

**P0**: Validate `publish<ReportOp>(snapshot, ReplaceOwnedInstances)` can clean only owned pre-restart keyed instances safely; handle absence of owned-key inventory and failure result.

**P0**: Define safe user-visible semantics when recovered Command's previous state, payload, or version association is unavailable. Never turn Unknown into success or double-execute a command.

**P1**: Cancellation policy when handler missing/noninterruptible, especially on Consumer liveliness loss. Preserve business safety decision without falsely claiming cancel.

**P1**: Validate ServiceKind/roles/actual Operations against all 139 Service definitions (including odd Config multi-Command cases) using generated type metadata; evaluate whether bare Provider binding suffices for Report-only service and clear service completeness reporting.

**P1**: Compile representative real generated C++11 API examples and negative checks, then run DDS conformance/integration separately.

**P1**: Reconcile the three experimental @nested/Topic discrepancies against authoritative sources without guessing missing DDS topics.

Do **not** add a new user-facing top-level verb unless a real user scenario cannot be represented with existing eight + role declarations + typed options. Each proposed API addition must cite the user activity it enables and relevant ICD clause.

## 6. Source inventory and restoration

The original session used a user-supplied `umaa-6.0-idl-with-exp-services.zip` (~594 IDL files) and UMAA 6.0 ICD PDFs for EO, MO, MM, SA, SEM, SO, EXP, CO-EXP. **These source binaries are not committed to this public repository** because redistribution permission was not established. Their former local runtime path (`/mnt/data/...`) **is not portable across sessions**. Request the user to reattach when necessary; do not assume paths survive.

The local design bundle previously produced is named `UMAA_Component_SDK_v0.8.4_Service_Roles_User_API_2026-11-11.zip`. The repository's `docs/current` is intended to be the persistent canonical copy of its text contents.

## 7. Exact steps to resume work after session interruption

1. Open `https://github.com/nssang00/umaa_dev_sdk` and read **`HANDOFF.md`**, then `docs/current/UMAA_Component_SDK_Public_API_v0.8.4.md` and `docs/current/REVIEW_v0.8.4.md`.
2. Read `docs/SOURCE_INVENTORY.md` to know which ICD/IDL source files are not mirrored.
3. Distinguish **existing verified docs** from **proposed public API** and **unimplemented runtime**.
4. Continue at the P0 items above. Prefer narrow edits to latest spec and update `HANDOFF.md`, `CHANGELOG`, and review simultaneously.
5. Only advance version after updating example source, acceptance checklist, explicit verification results, and Handoff.
6. Link the GitHub commit/PR on completion. Never assume a prior ephemeral attachment path exists.

**Copy/paste resume prompt:**  
`nssang00/umaa_dev_sdk 저장소의 HANDOFF.md와 docs/current v0.8.4 명세를 읽고, 이전 대화의 UMAA Component SDK 사용자 API 설계를 이어가자. 구현 내부는 숨기고 사용자 시나리오/ICD 근거 중심으로 P0 보완 사항부터 검토해줘. 검증하지 않은 것은 검증했다고 쓰지 말아줘.`
