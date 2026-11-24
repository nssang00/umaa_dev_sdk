# v0.8.7 — API review and acceptance gates

Status: proposed C++11 contract only. The nine facade names are counted once across all roles; overloads, handles and value types are separate. All runtime rows below remain pending. Historical audit evidence: [coverage review](../history/v0.8.1/UMAA_ICD6_IDL_Coverage_Review_v0.8.1.md), [ICD requirements](../history/v0.8.2/ICD_REQUIREMENTS_v0.8.2.csv).

| Capability | v0.8.7 public path | Acceptance scenario (pending) |
|---|---|---|
| New / update / recovery | command, handle.update, onCommand(info) | same session vs new, newer timestamp, recover or fail without unsafe replay |
| Provider decision and status | onCommand return, respond | accepted != completion; all legal states/reasons; reject invalid transition |
| Cancellation | handle.cancel, onCancel, respond | deferred/cannotCancel/confirmed stop, liveliness loss, no fake CANCELED |
| Ack/status/execution | onResponse, respond | distinct original IDL types, optional execution only where defined, local errors separate |
| Consumer restart | onResponse options | continue/cancel recovered command; missing payload separately handled |
| Terminal cleanup | retention + handle.release | manual vs auto, no active release, no destruction cancel |
| Report lifecycle | publish/remove/onReport | composite key, multi-source, remove without payload, source lost != dispose |
| Subscription | onReport options + close | pre-start auto/runtime manual, ignored return survives, explicit close/drain |
| Queryless cache | getLatest | no RPC; missing/removed/partial/stale/no-cache distinct |
| Multiple operations | typed overload / explicit tag | all 139 services, ambiguity and wrong role reject |
| Collections (18) | shared attachments, set/remove/assign/insert/move | Command and Report; IDs/order/gaps/empty/partial/recovery |
| Specialization (48) | Specialized + attachments | known/unknown/partial/nested, metadata consistency |
| Mixed services | generated facade composition | GlobalPoseConfig with command and report, single Service base |
| C++11 / IDL | typed callback, generated headers | actual RTI field/accessor mappings; no C++14 features |
| Concurrency / plugin lifecycle | token and runtime contract | reentrancy, late result, callback exception, stop/drain/unload |
| Protocol interop | internal engine | all regular/expanded flows; QoS/session/recovery/disposal cross-vendor where applicable |

Blockers: original source archives not in repo; 3 Experimental nested/Topic mismatches (AcousticInterferenceStatus, FinsStatus, PropulsorsStatus). Historical 114/114 regular and 311/314 expanded unique Topic alignments are not v0.8.7 implementation coverage. Existing CI tests v0.8.5 only. New actual IDL codegen and DDS tests required.

[Public API contract](UMAA_Component_SDK_Public_API_v0.8.7.md) · [14 roles example](../../examples/api_sketch/AUTOPILOT_14_SERVICE_USAGE_v0.8.7.cpp) · [mixed Config](../../examples/api_sketch/CONFIG_MIXED_SERVICE_v0.8.7.cpp)
