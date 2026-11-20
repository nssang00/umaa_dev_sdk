# UMAA Component SDK — C++11 User Public API

**Design baseline: v0.8.6 (2026-11-20)** · **Public API proposal**, not a production DDS implementation or proven UMAA ICD 6.0 conformance.

The SDK goal is to expose actual UMAA IDL business messages while hiding DDS Topics, ACK/CommandStatus mechanics, Queryless subscriptions, Session and Timestamp correlation, liveliness, and collection/specialization transport.

## Read first

- [HANDOFF.md](HANDOFF.md) — project decisions, validation boundaries, outstanding tasks
- **[v0.8.6 Public API — authoritative current proposal](docs/current/UMAA_Component_SDK_Public_API_v0.8.6.md)**
- **[Complete v0.8.6 C++11 API sketch (single file)](examples/api_sketch/COMPLETE_FOUR_ROLE_USAGE_v0.8.6.cpp)**
- **[v0.8.6 usage examples — all four roles](examples/api_sketch/SERVICE_ROLE_USAGE_v0.8.6.md)**
- [v0.8.6 change log](docs/current/CHANGELOG_v0.8.6.md)
- [Previous v0.8.5 API (registration-oriented)](docs/current/UMAA_Component_SDK_Public_API_v0.8.5.md)
- [Previous v0.8.5 example](examples/api_sketch/AUTOPILOT_FOUR_ROLES_v0.8.5.cpp)
- [v0.8.5 review and unverified items](docs/current/REVIEW_v0.8.5.md)
- [Older versions](docs/history/) and [source inventory](docs/SOURCE_INVENTORY.md)

## v0.8.6 core decision: generated Service role objects, inherited common verbs

The SDK generates concrete Service Provider/Consumer types inheriting one or more of these four common role classes:
`CommandProvider<S>`, `CommandConsumer<S>`, `ReportProvider<S>`, `ReportConsumer<S>`.

```cpp
namespace MO = umaa::services::MO;
namespace SA = umaa::services::SA;

MO::GlobalVectorControlConsumer vectorConsumer;  // SDK generated
SA::GlobalPoseStatusProvider poseProvider;        // SDK generated

UMAA::MO::GlobalVectorControl::GlobalVectorCommandType command;
UMAA::SA::GlobalPoseStatus::GlobalPoseReportType pose;

fillCommand(command);
fillPose(pose);

vectorConsumer.command(command);   // ordinary use: send only
poseProvider.publish(pose);

// Only save the returned CommandCall if later update/cancel/status is needed
auto call = vectorConsumer.command(command);
call.update(updatedCommand);
call.cancel();
```

The original `UMAA::...` C++ data types come from the UMAA IDL/RTI generated mapping. Depending on the generated mapping, fields may be public members (`cmd.speed = requirement`) or accessor methods (`cmd.speed(requirement)`). `GlobalVectorCommandType::speed` is a `SpeedRequirementVariantType`, not necessarily a scalar. `CommandCall` is a session-tracking handle, **not** the remote ACK or reply, and discarding it must not cancel the command. The `umaa::services::...` classes are **proposed SDK-generated facades**, not structs in original IDL. Existing v0.8.5 examples use the **superseded** templated `command<Op>` / `publish<Op>` default and `m.provide/consume` registration.

### Hooks: direct original IDL types

```cpp
class MyPoseConsumer : public SA::GlobalPoseStatusConsumer {
public:
    void onReport(
        const UMAA::SA::GlobalPoseStatus::GlobalPoseReportType& pose) override {
        usePosition(pose.position());
    }
    void onDispose(const UMAA::Common::IdentifierType& source) override {
        invalidatePose(source);
    }
};
```

Optional `onReport(const T&, const ReportInfo&)` provides metadata. For CommandProviders, the generated class exposes `onCommand(const CommandIDL&, const CommandInfo&)` and `onCancel(const CancelInfo&)`. The application supplies actual hardware state via inherited `respond(token, progress)` and `publish(token, executionStatusIDL)`.

### Component registration

```cpp
UMAA_COMPONENT(AutoPilot, m) {
    m.add(&AutoPilot::vectorControl);
    m.add(&AutoPilot::poseConsumer);
    m.add(&AutoPilot::healthProvider);
}
```

`m.add(&Component::member)` is a proposed C++11 registration mechanism, *not yet implemented*. It works with the generated objects and avoids requiring one user-side registration callback per Operation.

## Compatibility and validation truth

- **This is a v0.8.6 design change**, not a patch to the current C++11 mock headers or a completed DDS runtime.
- Mapping by `(Service, role, message type)` must be checked for uniqueness. Ambiguous cases require explicit `<Op>` or typed operation tags.
- Large Collection and Specialization remain separate wire Topics; the public proposal passes original IDL Report plus a `LargeList`/`LargeSet` sidecar, not fabricated fields in the original IDL struct.
- Previous [v0.8.5 validation summary](docs/current/VALIDATION_SUMMARY_v0.8.5.json) remains evidence of **what was and was not checked in v0.8.5**, not proof of v0.8.6.
- The original UMAA ICD PDF manuals and IDL source archives are not mirrored in this repository; see [source inventory](docs/SOURCE_INVENTORY.md).
