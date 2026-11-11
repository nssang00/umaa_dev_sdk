// UMAA SDK Public API v0.8.5 — AutoPilot four-role example.
// Names of Services, Operations and IDL data types are taken from supplied UMAA 6.0 IDL.
// IMPORTANT: 'umaa::services::*' and the nested operation-tag types are
// PROPOSED SDK-generated wrappers, NOT namespaces/classes in the original IDL.
// The original IDL declares 'UMAA::MO::GlobalVectorControl' and
// 'UMAA::SA::GlobalPoseStatus' as modules (C++ namespaces).
// 2026-11-12: registration API now m.provide<S>() / m.consume<S>().\n// This file uses deliberately INCOMPLETE IDL-shaped mock structs for syntax only;
// actual generated DDS/IDL C++ types must replace them in a real application.
// No actual DDS, ICD state machine or hardware code is implemented here.
#include "PUBLIC_API_CXX11_DECLARATION_SKETCH_v0.8.5.hpp"

namespace UMAA {
namespace Common {
    struct IdentifierType {}; // IDL: IdentifierType includes id and parentID
}
namespace MO {
namespace GlobalVectorControl {
    // Exact IDL type names (members elided; SDK should generate these from IDL):
    // GlobalVectorCommandType has required direction, directionMode, speed,
    // optional depthChangePitch, elevation, endTime, and wire bookkeeping.
    struct GlobalVectorCommandType {};
    struct GlobalVectorCommandAckReportType {};
    struct GlobalVectorCommandStatusType {};
    struct GlobalVectorExecutionStatusReportType {
        bool directionAchieved = false;
        bool elevationAchieved = false;
        bool speedAchieved = false;
    };
}
}
namespace SA {
namespace GlobalPoseStatus {
    // Exact IDL name. Real IDL has 'position' (GeoPosition2D),
    // 'attitude', 'course', 'navigationSolution' and optional fields,
    // not fictitious latitude/longitude/sensorId at the top level.
    struct GlobalPoseReportType {};
}
}
}

// SDK-generated typed service/operation facades; names mirror ICD operations.
// This is an SDK namespace choice, distinct from the IDL's 'UMAA::' modules.
namespace umaa { namespace services {
namespace MO {
struct GlobalVectorControl
    : umaa::Service<GlobalVectorControl, umaa::ServiceKind::Control> {
    struct setGlobalVector : umaa::Operation<
        GlobalVectorControl,
        UMAA::MO::GlobalVectorControl::GlobalVectorCommandType,
        umaa::OperationRole::Command, umaa::FlowKind::CommandResponse> {};
    struct reportGlobalVectorCommandAck : umaa::Operation<
        GlobalVectorControl,
        UMAA::MO::GlobalVectorControl::GlobalVectorCommandAckReportType,
        umaa::OperationRole::CommandAck, umaa::FlowKind::CommandResponse> {};
    struct reportGlobalVectorCommandStatus : umaa::Operation<
        GlobalVectorControl,
        UMAA::MO::GlobalVectorControl::GlobalVectorCommandStatusType,
        umaa::OperationRole::CommandStatus, umaa::FlowKind::CommandResponse> {};
    struct reportGlobalVectorExecutionStatus : umaa::Operation<
        GlobalVectorControl,
        UMAA::MO::GlobalVectorControl::GlobalVectorExecutionStatusReportType,
        umaa::OperationRole::ExecutionStatus, umaa::FlowKind::CommandResponse> {};
};
}
namespace SA {
struct GlobalPoseStatus
    : umaa::Service<GlobalPoseStatus, umaa::ServiceKind::StatusReport> {
    struct reportGlobalPose : umaa::Operation<
        GlobalPoseStatus,
        UMAA::SA::GlobalPoseStatus::GlobalPoseReportType,
        umaa::OperationRole::Report,
        umaa::FlowKind::RequestReplyNoQueryData,
        UMAA::Common::IdentifierType> {}; // GlobalPoseReportType @key source
};
}
}} // namespace umaa::services

// Short namespace aliases preserve exact ICD Service/Operation names.
// These aliases refer to SDK facade types, NOT the original UMAA IDL modules.
namespace MO = umaa::services::MO;
namespace SA = umaa::services::SA;

// === 1) AutoPilot: Command Provider + Report Consumer =====================
class AutoPilot : public umaa::Component {
public:
    umaa::CommandDecision onSetGlobalVector(
        const UMAA::MO::GlobalVectorControl::GlobalVectorCommandType& command,
        const umaa::CommandInfo& info)
    {
        if (info.delivery() == umaa::CommandDelivery::Recovered) {
            // Never resume blindly when previous physical state is unknown.
            return hardwareIsExecutingRecoveredCommand(command)
                ? umaa::CommandDecision::resume()
                : umaa::CommandDecision::rejected(
                      umaa::CommandReason::ServiceFailed);
        }
        if (!isValidForThisVehicle(command))
            return umaa::CommandDecision::rejected(
                umaa::CommandReason::ValidationFailed);

        startGlobalVectorControl(command, info.token());
        return umaa::CommandDecision::accepted(); // NOT EXECUTING/COMPLETED
    }

    umaa::CancelDecision onCancelGlobalVector(
        const umaa::CancelInfo& cancelInfo)
    {
        if (!requestSafeStop(cancelInfo.token()))
            return umaa::CancelDecision::cannotCancel();
        return umaa::CancelDecision::deferred(); // wait until hardware stops
    }

    // Auto-subscribed once Component starts (Queryless Request/Reply).
    void onGlobalPoseReport(
        const umaa::ReportEvent<
            UMAA::SA::GlobalPoseStatus::GlobalPoseReportType>& event)
    {
        if (event.kind() == umaa::ReportKind::Disposed) {
            currentPoseValid_ = false;
            return;
        }
        lastPose_ = event.data();
        currentPoseValid_ = true;
    }

    // These are APPLICATION hardware callbacks, not required SDK Hook names.
    void hardwareStarted(const umaa::CommandToken& commandToken) {
        service<MO::GlobalVectorControl>()
            .commandProvider()
            .respond(commandToken, umaa::CommandProgress::executing());
    }
    void hardwareProgress(
        const umaa::CommandToken& commandToken,
        const UMAA::MO::GlobalVectorControl::
            GlobalVectorExecutionStatusReportType& detail)
    {
        service<MO::GlobalVectorControl>()
            .commandProvider()
            .publish<MO::GlobalVectorControl::
                reportGlobalVectorExecutionStatus>(commandToken, detail);
    }
    void hardwareCompleted(const umaa::CommandToken& commandToken) {
        service<MO::GlobalVectorControl>()
            .commandProvider()
            .respond(commandToken, umaa::CommandProgress::completed());
    }
    void hardwareStopped(const umaa::CommandToken& commandToken) {
        service<MO::GlobalVectorControl>()
            .commandProvider()
            .respond(commandToken, umaa::CommandProgress::canceled());
    }

private:
    bool currentPoseValid_ = false;
    UMAA::SA::GlobalPoseStatus::GlobalPoseReportType lastPose_;
    bool hardwareIsExecutingRecoveredCommand(
        const UMAA::MO::GlobalVectorControl::GlobalVectorCommandType&) const {
        return false;  // mock: do not falsely claim recovered execution
    }
    bool isValidForThisVehicle(
        const UMAA::MO::GlobalVectorControl::GlobalVectorCommandType&) const {
        return true;   // implement real validation in application
    }
    void startGlobalVectorControl(
        const UMAA::MO::GlobalVectorControl::GlobalVectorCommandType&,
        const umaa::CommandToken&) {}
    bool requestSafeStop(const umaa::CommandToken&) { return true; }
};

// Registration uses one Service-level provide/consume builder; Op decides the flow.
UMAA_COMPONENT(AutoPilot, m) {
    m.provide<MO::GlobalVectorControl>()
      .onCommand<MO::GlobalVectorControl::setGlobalVector>(
          &AutoPilot::onSetGlobalVector)
      .onCancel<MO::GlobalVectorControl::setGlobalVector>(
          &AutoPilot::onCancelGlobalVector);

    m.consume<SA::GlobalPoseStatus>()
      .onReport<SA::GlobalPoseStatus::reportGlobalPose>(
          &AutoPilot::onGlobalPoseReport);
}

// === 2) MissionManager: Command Consumer =================================
class MissionManager : public umaa::Component {
public:
    void commandGlobalVector(
        const UMAA::MO::GlobalVectorControl::GlobalVectorCommandType& command)
    {
        activeCommand_ =
            service<MO::GlobalVectorControl>()
                .commandConsumer()
                .command<MO::GlobalVectorControl::
                    setGlobalVector>(command);
    }
    void updateGlobalVector(
        const UMAA::MO::GlobalVectorControl::GlobalVectorCommandType& updated)
    {
        activeCommand_.update(updated); // same Source+Session, new timestamp
    }
    void cancelGlobalVector() {
        // Submission does not imply the actual Command became CANCELED.
        service<MO::GlobalVectorControl>()
            .commandConsumer().cancel(activeCommand_);
    }
    void onGlobalVectorCommandStatus(const umaa::StatusEvent& status) {
        if (status.isTerminal()) { /* inspect completed / failed / canceled */ }
    }
    void onGlobalVectorCommandAck(const umaa::AckEvent&) {}
    void onGlobalVectorExecutionStatus(
        const umaa::ExecutionStatusEvent<
            UMAA::MO::GlobalVectorControl::
                GlobalVectorExecutionStatusReportType>& e)
    {
        const bool achieved = e.data().directionAchieved &&
                              e.data().elevationAchieved &&
                              e.data().speedAchieved;
        (void)achieved;
    }
    umaa::RecoveryDecision onRecoveredGlobalVector(
        const umaa::RecoveredCommand<
            UMAA::MO::GlobalVectorControl::GlobalVectorCommandType>&)
    {
        // With insufficient evidence, do not pretend an outcome is known.
        return umaa::RecoveryDecision::Unknown;
    }
private:
    umaa::CommandCall<
        MO::GlobalVectorControl::setGlobalVector>
            activeCommand_;
};

UMAA_COMPONENT(MissionManager, m) {
    m.consume<MO::GlobalVectorControl>()
      .onStatus<MO::GlobalVectorControl::setGlobalVector>(
          &MissionManager::onGlobalVectorCommandStatus)
      .onAck<MO::GlobalVectorControl::setGlobalVector>(
          &MissionManager::onGlobalVectorCommandAck)
      .onExecutionStatus<MO::GlobalVectorControl::
          reportGlobalVectorExecutionStatus>(
              &MissionManager::onGlobalVectorExecutionStatus)
      .onRecoveredCommand<
          MO::GlobalVectorControl::setGlobalVector>(
              &MissionManager::onRecoveredGlobalVector);
}

// === 3) PositionSensor: Report Provider ==================================
class PositionSensor : public umaa::Component {
public:
    void newGlobalPose(
        const UMAA::SA::GlobalPoseStatus::GlobalPoseReportType& report)
    {
        service<SA::GlobalPoseStatus>()
            .reportProvider()
            .publish<SA::GlobalPoseStatus::
                reportGlobalPose>(report);
    }

    void disposeGlobalPose(const UMAA::Common::IdentifierType& source) {
        // In GlobalPoseReportType, 'source' is the actual @key.
        service<SA::GlobalPoseStatus>()
            .reportProvider()
            .dispose<SA::GlobalPoseStatus::
                reportGlobalPose>(source);
    }

    umaa::PublishResult publishCurrentGlobalPoses(
        const std::vector<
            UMAA::SA::GlobalPoseStatus::GlobalPoseReportType>& current)
    {
        return service<SA::GlobalPoseStatus>()
            .reportProvider()
            .publish<SA::GlobalPoseStatus::
                reportGlobalPose>(
                    current, umaa::PublishMode::ReplaceOwnedInstances);
    }
};
UMAA_COMPONENT(PositionSensor, m) {
    m.provide<SA::GlobalPoseStatus>();
}

// === 4) PoseProbe: optional dynamic Report Consumer ======================
class PoseProbe : public umaa::Component {
public:
    void startTemporaryMonitoring() {
        // Must not ALSO register an onReport for the same Service/Op here.
        subscription_ =
            service<SA::GlobalPoseStatus>()
                .reportConsumer()
                .request<SA::GlobalPoseStatus::
                    reportGlobalPose>(
                    [this](const umaa::ReportEvent<
                        UMAA::SA::GlobalPoseStatus::GlobalPoseReportType>& e) {
                        onTemporaryGlobalPose(e);
                    });
    }
    void stopTemporaryMonitoring() { subscription_.close(); }
private:
    umaa::Subscription subscription_;
    void onTemporaryGlobalPose(
        const umaa::ReportEvent<
            UMAA::SA::GlobalPoseStatus::GlobalPoseReportType>&) {}
};
UMAA_COMPONENT(PoseProbe, m) {
    m.consume<SA::GlobalPoseStatus>();
}

// The original UMAA IDL contains modules and data structs; it does NOT
// define these generated SDK Service facades or method registration APIs.
static_assert(
    MO::GlobalVectorControl::setGlobalVector::role ==
        umaa::OperationRole::Command, "setGlobalVector is a Command");
static_assert(
    SA::GlobalPoseStatus::reportGlobalPose::role ==
        umaa::OperationRole::Report, "reportGlobalPose is a Report");
