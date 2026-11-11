// UMAA SDK Public API v0.8.5 — four roles, AutoPilot-centered C++11 example.
// Compiles for C++11 *syntax* using mock generated types below. Does NOT run DDS.
// Source operation names are illustrative wrappers, NOT generated UMAA headers.
#include "PUBLIC_API_CXX11_DECLARATION_SKETCH_v0.8.5.hpp"

namespace mo {
struct GlobalVectorCommand {
    double speed = 0.0;       // demo-only payload
    double direction = 0.0;   // real ICD has additional required fields
};
struct GlobalVectorCommandAckReport {};
struct GlobalVectorCommandStatus {};
struct GlobalVectorExecutionStatusReport {
    bool directionAchieved = false;
    bool elevationAchieved = false;
    bool speedAchieved = false;
};

struct GlobalVectorControl
    : umaa::Service<GlobalVectorControl, umaa::ServiceKind::Control> {
    struct SetGlobalVector : umaa::Operation<
        GlobalVectorControl, GlobalVectorCommand,
        umaa::OperationRole::Command, umaa::FlowKind::CommandResponse> {};
    struct CommandAck : umaa::Operation<
        GlobalVectorControl, GlobalVectorCommandAckReport,
        umaa::OperationRole::CommandAck, umaa::FlowKind::CommandResponse> {};
    struct CommandStatus : umaa::Operation<
        GlobalVectorControl, GlobalVectorCommandStatus,
        umaa::OperationRole::CommandStatus, umaa::FlowKind::CommandResponse> {};
    struct ExecutionStatus : umaa::Operation<
        GlobalVectorControl, GlobalVectorExecutionStatusReport,
        umaa::OperationRole::ExecutionStatus, umaa::FlowKind::CommandResponse> {};
};
}  // namespace mo

namespace sa {
struct GlobalPoseReport {
    unsigned int sensorId = 0;
    double latitude = 0.0;   // demo fields, not guaranteed IDL members
    double longitude = 0.0;
};
struct GlobalPoseStatus
    : umaa::Service<GlobalPoseStatus, umaa::ServiceKind::StatusReport> {
    struct Report : umaa::Operation<
        GlobalPoseStatus, GlobalPoseReport,
        umaa::OperationRole::Report,
        umaa::FlowKind::RequestReplyNoQueryData,
        unsigned int> {};
};
}

typedef mo::GlobalVectorControl VectorSvc;
typedef VectorSvc::SetGlobalVector VectorOp;
typedef VectorSvc::ExecutionStatus VectorExecutionOp;
typedef sa::GlobalPoseStatus PoseSvc;
typedef PoseSvc::Report PoseOp;

// === 1) AutoPilot: COMMAND PROVIDER + REPORT CONSUMER ======================
class AutoPilot : public umaa::Component {
public:
    // SDK invokes when a Command is new, updated, or recovered.
    umaa::CommandDecision onVectorCommand(
        const VectorOp::Payload& cmd,
        const umaa::CommandInfo& info)
    {
        if (info.delivery() == umaa::CommandDelivery::Recovered) {
            // Must check actual device state before choosing resume.
            return deviceAlreadyExecuting(cmd)
                ? umaa::CommandDecision::resume()
                : umaa::CommandDecision::rejected(
                      umaa::CommandReason::ServiceFailed);
        }

        if (cmd.speed < 0.0)
            return umaa::CommandDecision::rejected(
                umaa::CommandReason::ValidationFailed);

        startDeviceAsync(cmd, info.token());
        return umaa::CommandDecision::accepted(); // NOT EXECUTING/COMPLETED
    }

    // SDK invokes when the original Command is requested to cancel.
    umaa::CancelDecision onVectorCancel(const umaa::CancelInfo& info) {
        if (!beginSafeStop(info.token()))
            return umaa::CancelDecision::cannotCancel();
        return umaa::CancelDecision::deferred(); // waiting for actual stop
    }

    // SDK invokes automatically after startup, on every Data/Disposed report.
    void onPoseReport(const umaa::ReportEvent<PoseOp::Payload>& event) {
        if (event.kind() == umaa::ReportKind::Disposed) {
            hasPose_ = false;
            return;
        }
        lastPose_ = event.data();
        hasPose_ = true;
    }

    // Application hardware callbacks, NOT UMAA SDK hooks:
    void hardwareExecutionStarted(const umaa::CommandToken& token) {
        service<VectorSvc>().commandProvider().respond(
            token, umaa::CommandProgress::executing());
    }

    void hardwareProgress(const umaa::CommandToken& token,
                          const VectorExecutionOp::Payload& detail) {
        service<VectorSvc>().commandProvider()
            .publish<VectorExecutionOp>(token, detail);
    }

    void hardwareCompleted(const umaa::CommandToken& token) {
        service<VectorSvc>().commandProvider().respond(
            token, umaa::CommandProgress::completed());
    }

    void hardwareStopped(const umaa::CommandToken& token) {
        service<VectorSvc>().commandProvider().respond(
            token, umaa::CommandProgress::canceled());
    }

private:
    bool hasPose_ = false;
    PoseOp::Payload lastPose_;
    bool deviceAlreadyExecuting(const VectorOp::Payload&) const { return false; }
    void startDeviceAsync(const VectorOp::Payload&, const umaa::CommandToken&) {}
    bool beginSafeStop(const umaa::CommandToken&) { return true; }
};

UMAA_COMPONENT(AutoPilot, m) {
    // Registration attaches callbacks to role objects for this Component.
    m.commandProvider<VectorSvc>()
      .onCommand<VectorOp>(&AutoPilot::onVectorCommand);
    m.commandProvider<VectorSvc>()
      .onCancel<VectorOp>(&AutoPilot::onVectorCancel);

    // Default: onReport binding AUTOMATICALLY subscribes at Component start.
    m.reportConsumer<PoseSvc>()
      .onReport<PoseOp>(&AutoPilot::onPoseReport);
}

// === 2) MissionManager: COMMAND CONSUMER ================================
class MissionManager : public umaa::Component {
public:
    void startMission() {
        VectorOp::Payload cmd;
        cmd.speed = 2.0;
        cmd.direction = 90.0;
        call_ = service<VectorSvc>().commandConsumer()
                    .command<VectorOp>(cmd);
    }

    void changeSpeed(double speed) {
        VectorOp::Payload update;
        update.speed = speed;
        update.direction = 90.0;
        call_.update(update); // same session; no new CommandCall
    }

    void stopMission() {
        // Submission does not mean actual CANCELED.
        service<VectorSvc>().commandConsumer().cancel(call_);
    }

    void onVectorStatus(const umaa::StatusEvent& status) {
        if (status.isTerminal()) {
            // COMPLETED / FAILED / CANCELED or observation error:
            // inspect the real SDK's typed status/result.
        }
    }

    void onVectorAck(const umaa::AckEvent&) {}
    void onVectorExecution(
        const umaa::ExecutionStatusEvent<VectorExecutionOp::Payload>& e) {
        const bool reached = e.data().directionAchieved && e.data().speedAchieved;
        (void)reached;
    }

    umaa::RecoveryDecision onRecoveredCommand(
        const umaa::RecoveredCommand<VectorOp::Payload>&) {
        // Real SDK must represent unknown previous payload/state.
        return umaa::RecoveryDecision::Unknown;
    }

private:
    umaa::CommandCall<VectorOp> call_;
};

UMAA_COMPONENT(MissionManager, m) {
    m.commandConsumer<VectorSvc>()
      .onStatus<VectorOp>(&MissionManager::onVectorStatus);
    m.commandConsumer<VectorSvc>()
      .onAck<VectorOp>(&MissionManager::onVectorAck); // optional
    m.commandConsumer<VectorSvc>()
      .onExecutionStatus<VectorExecutionOp>(&MissionManager::onVectorExecution); // optional
    m.commandConsumer<VectorSvc>()
      .onRecoveredCommand<VectorOp>(&MissionManager::onRecoveredCommand); // optional
}

// === 3) PositionSensor: REPORT PROVIDER =================================
class PositionSensor : public umaa::Component {
public:
    void newPoseMeasurement(const PoseOp::Payload& pose) {
        service<PoseSvc>().reportProvider().publish<PoseOp>(pose);
    }

    void sensorRemoved(unsigned int sensorId) {
        service<PoseSvc>().reportProvider().dispose<PoseOp>(sensorId);
    }

    umaa::PublishResult publishCurrentSnapshot(
        const std::vector<PoseOp::Payload>& current) {
        // On restart, SDK must only delete stale instances owned by this Provider.
        return service<PoseSvc>().reportProvider()
            .publish<PoseOp>(current, umaa::PublishMode::ReplaceOwnedInstances);
    }
};

UMAA_COMPONENT(PositionSensor, m) {
    m.reportProvider<PoseSvc>(); // no onRequest: queryless Request/Reply
}

// === 4) Dynamic request alternative: another REPORT CONSUMER ============
class PoseProbe : public umaa::Component {
public:
    void beginTemporaryMonitoring() {
        // Do NOT also register onReport<PoseOp> on the same Component.
        subscription_ = service<PoseSvc>().reportConsumer().request<PoseOp>(
            [this](const umaa::ReportEvent<PoseOp::Payload>& event) {
                this->onPose(event);
            });
    }

    void endTemporaryMonitoring() { subscription_.close(); }

private:
    umaa::Subscription subscription_;
    void onPose(const umaa::ReportEvent<PoseOp::Payload>&) {}
};
UMAA_COMPONENT(PoseProbe, m) { m.reportConsumer<PoseSvc>(); }

// === static type checks =================================================
static_assert(VectorSvc::kind == umaa::ServiceKind::Control, "Control Service");
static_assert(PoseSvc::kind == umaa::ServiceKind::StatusReport, "Status Report Service");
static_assert(VectorOp::role == umaa::OperationRole::Command, "Command Op");
static_assert(PoseOp::role == umaa::OperationRole::Report, "Report Op");
