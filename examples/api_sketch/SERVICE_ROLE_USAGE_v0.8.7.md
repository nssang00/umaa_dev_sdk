# v0.8.7 — advanced usage (design sketches)

These are proposed C++11 calls, not compiled SDK examples. See [contract](../../docs/current/UMAA_Component_SDK_Public_API_v0.8.7.md).

## Command

```cpp
vector.onCommand(handler); // distinguish New / Updated / Recovered
vector.onCancel(cancelHandler);
vector.respond(token, umaa::CommandProgress::executing());
vector.respond(token, executionStatus);
vector.respond(token, umaa::CommandProgress::completed());

consumer.onResponse([](const VectorAck& ack) { observe(ack); });
consumer.onResponse([](const VectorStatus& status) { observe(status); });
consumer.onResponse([](const VectorExecution& detail) { observe(detail); });
auto h = consumer.command(cmd, options);
h.update(revised);
h.cancel(); // request only
auto snapshot = h.get();
auto observed = h.wait(umaa::WaitStage::Commanded, timeout);
// Only for a terminal, manually retained command:
h.release();
```

Use ResponseOptions with per-Operation recovery policy (Continue/Cancel or typed recovery decision callback). A recovered payload must be available/validated; missing payload is not a new command.

## Report

```cpp
provider.publish(report);
provider.publish(reports, umaa::PublishOptions(umaa::PublishMode::ReplaceOwnedInstances));
provider.remove(reportWithKey);
consumer.onReport([](const Pose& pose) { use(pose); });
// Alternative event callback:
consumer.onReport([](const umaa::ReportEvent<Pose>& e) {
    if (e.kind() == umaa::ReportKind::Received && e.data()) use(*e.data());
    else if (e.kind() == umaa::ReportKind::Removed) invalidate(e.key());
}, receiveOptions);
auto latest = consumer.getLatest(key);
```

For manual subscription, register member with SubscriptionMode::Manual and call onReport while running. Explicitly close the returned registration; ignoring a return never stops a registration. No separate request/onDispose. Do not register both raw and event callback for the same default observation.

## Attachments (both Command and Report)

```cpp
umaa::LargeList<BatteryCell> cells;
cells.set(cell);
batteryProvider.publish(batteryReport, cells); // unique attachment field
// Generic typed attachment examples, actual generated Op required:
auto h = consumer.command(cmd, commandAttachments, options);
h.update(revised, revisedAttachments);
provider.respond(token, executionStatus, executionAttachments);
```

LargeList insert/move use ListPosition; assign(empty) represents empty collection, not removal of containing Report. A specialization uses Specialized<Base>::set(detail), with state/type/identity retained. Original IDL has metadata rather than invented vector fields. Use typed field tags for ambiguity. CompleteOnly vs AllowPartial is a receive option.

## Actual Service examples

[AutoPilot 14 roles](AUTOPILOT_14_SERVICE_USAGE_v0.8.7.cpp) and [GlobalPoseConfig](CONFIG_MIXED_SERVICE_v0.8.7.cpp) preserve actual original IDL names from v0.8.6. The latter is not one of AutoPilot's 14 roles.
