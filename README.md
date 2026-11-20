# UMAA Component SDK — C++11 User Public API

**Current design baseline: v0.8.6, revised 2026-11-20.** This repository is a **Public API design proposal**; generated Service facades, RTI IDL integration and DDS runtime conformance have **not** been implemented/verified.

## Start here

- **[Public API Review Brief (user/reviewer overview)](docs/current/UMAA_Component_SDK_API_Review_v0.8.6.md)** — recommended starting point; explains the user-facing API without presenting it as an implemented product manual
- **[v0.8.6 Public API contract](docs/current/UMAA_Component_SDK_Public_API_v0.8.6.md)** — current naming, generated facade, Service composition and flow separation
- **[AutoPilot's actual 14 UMAA 6.0 Services — C++11 example](examples/api_sketch/AUTOPILOT_14_SERVICE_USAGE_v0.8.6.cpp)** — 7 Providers and 7 Consumers; default Callback model
- **[Actual mixed Config pattern — GlobalPoseConfig](examples/api_sketch/CONFIG_MIXED_SERVICE_v0.8.6.cpp)** — Command/Response and Queryless Report in one Service Provider/Consumer
- [Four-role user usage guide](examples/api_sketch/SERVICE_ROLE_USAGE_v0.8.6.md)
- [v0.8.6 Changelog](docs/current/CHANGELOG_v0.8.6.md)
- [HANDOFF / design history](HANDOFF.md), [v0.8.5 registration proposal](docs/current/UMAA_Component_SDK_Public_API_v0.8.5.md)
- [Earlier inheritance/override sketch — **not current default**](examples/api_sketch/COMPLETE_FOUR_ROLE_USAGE_v0.8.6.cpp)
- [Source inventory](docs/SOURCE_INVENTORY.md)

## Public naming: Service name + Provider / Consumer

- **Config/Control/Specs/Status**: business **Service category**, never a public role base class.
- **Provider/Consumer**: Component participation direction.
- **Command/Response and Queryless Request/Reply**: capabilities of the Service's **individual Operations**.
- **Generated classes**: `MO::GlobalVectorControlProvider`, `SA::GlobalPoseStatusConsumer`, `EO::UVPlatformSpecsProvider` and others, derived **once** from `umaa::Service`.
- **Internal composition**: generated Service classes **contain/delegate to** `CommandProvider<S>`, `CommandConsumer<S>`, `ReportProvider<S>` and/or `ReportConsumer<S>`. These four implementations do not each inherit `Service`. A mixed Config Service combines both relevant flow roles.

Users normally subclass **only Component**, not every generated Provider/Consumer. They register typed callbacks and invoke common verbs on the generated member objects.

```cpp
namespace MO = umaa::services::MO;
namespace SA = umaa::services::SA;
namespace EO = umaa::services::EO;
namespace MM = umaa::services::MM;

using VectorCmd = UMAA::MO::GlobalVectorControl::GlobalVectorCommandType;
using Pose = UMAA::SA::GlobalPoseStatus::GlobalPoseReportType;

class AutoPilot : public umaa::Component {
public:
    MO::GlobalVectorControlProvider vector;
    SA::GlobalPoseStatusConsumer pose;
    EO::UVPlatformSpecsProvider specs;
    MM::MissionPlanConstraintControlConsumer constraints;

    AutoPilot() {
        vector.onCommand([this](const VectorCmd& cmd,
                                const umaa::CommandInfo& info) {
            startVectorHardware(cmd, info.token());
            return umaa::CommandDecision::accepted();
        });
        pose.onReport([this](const Pose& report) {
            updateNavigation(report);
        });
    }
};

// In AutoPilot implementation:
constraints.command(addConstraint);   // new command, Handle may be ignored
auto handle = constraints.command(addConstraint);
handle.update(revisedConstraint);
handle.cancel();
specs.publish(platformSpecsReport);
```

The code above illustrates the API, not a compiled SDK; `startVectorHardware` and `updateNavigation` are application placeholders. Returning `CommandHandle<Message>` is optional to retain; the Handle does not contain the remote ACK or auto-cancel on destruction.

## Mixed Config services

Real UMAA 6.0 `SA::GlobalPoseConfig` has both `GlobalPoseConfigCommandType` and `GlobalPoseConfigReportType`. Generated `GlobalPoseConfigProvider : Service` combines internal CommandProvider and ReportProvider; a Consumer combines CommandConsumer and ReportConsumer. This is why public `ControlService`/`ConfigService` naming is not used. See [Config example](examples/api_sketch/CONFIG_MIXED_SERVICE_v0.8.6.cpp). It is **not one of the AutoPilot reference component's 14 Service roles**.

## Compatibility and truth in validation

- Actual IDL-generated C++ members may be fields (`cmd.speed = requirement`) or accessors (`cmd.speed(requirement)`). `GlobalVectorCommandType.speed` is `SpeedRequirementVariantType`, not simply `double`.
- Generator must check unique mapping of `(Service, role, IDL message) → Operation`; ambiguous signatures require an explicit Operation tag.
- Existing v0.8.5 mock syntax tests do **not** certify this changed v0.8.6 architecture.
- DDS QoS, Sessions, cancellation, recovery, disposal, Large Collections, Specialization and cross-Topic assembly require implementation and end-to-end verification.
