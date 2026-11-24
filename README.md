# UMAA Component SDK — C++11 User Public API

**Current design baseline: v0.8.7 (2026-11-24).** This repository is a Public API design proposal, not an implemented DDS SDK. Actual RTI-generated headers, codegen, runtime and UMAA conformance are not verified.

## Start here

- [v0.8.7 Public API contract](docs/current/UMAA_Component_SDK_Public_API_v0.8.7.md) — nine Service-level verbs, typed options, events, handles and full-flow requirements.
- [v0.8.7 review and acceptance matrix](docs/current/UMAA_Component_SDK_API_Review_v0.8.7.md)
- [AutoPilot 14 actual Service roles](examples/api_sketch/AUTOPILOT_14_SERVICE_USAGE_v0.8.7.cpp)
- [Mixed GlobalPoseConfig example](examples/api_sketch/CONFIG_MIXED_SERVICE_v0.8.7.cpp)
- [Advanced usage](examples/api_sketch/SERVICE_ROLE_USAGE_v0.8.7.md)
- [Changelog](docs/current/CHANGELOG_v0.8.7.md), [handoff](HANDOFF.md), [source inventory](docs/SOURCE_INVENTORY.md)
- [Prior v0.8.6 contract](docs/current/UMAA_Component_SDK_Public_API_v0.8.6.md) (superseded, retained for reference).

## Nine Service API names

| Internal role | Generated facade exposes |
|---|---|
| CommandProvider | `onCommand`, `onCancel`, `respond` |
| CommandConsumer | `command`, `onResponse` |
| ReportProvider | `publish`, `remove` |
| ReportConsumer | `onReport`, `getLatest` |

Generated `<ServiceName>Provider/Consumer` derives once from `umaa::Service` and privately composes the applicable internal role(s). Mixed services have the union of the applicable verbs; users subclass Component only. The original IDL payload or explicit Operation tag selects the operation. No Query/RPC class is required for ICD 6.0 queryless Report.

```cpp
vector.onCommand(handler);       // New / Updated / Recovered
vector.onCancel(cancelHandler);
vector.respond(token, umaa::CommandProgress::executing());
vector.respond(token, executionStatus);
auto h = constraints.command(command);
h.update(revised);
h.cancel();
pose.onReport(reportHandler);   // original IDL or typed event
specs.publish(specsReport);
health.remove(healthWithKey);
```

CommandHandle has update/cancel/get/wait and release for manual retention. Subscription.close explicitly stops a registration; ignoring a handle does not cancel a command or registration. Large Collection and Specialization use shared typed attachments for Command, update, response and Report; see the contract.

## Validation boundary

Historical static audit: 114/114 regular Operation topics; 311/314 unique expanded topics, 139 unique Services, 18 Large Collection Element topics, 48 Specialization topics. Three experimental nested-vs-topic discrepancies remain. These are historical declaration results, not implemented coverage. All earlier sketches and their CI are historical, not proof of v0.8.7 compilation or runtime conformance.
