// Syntax-check example only. This does not include or implement any actual UMAA generated headers.
#include "PUBLIC_API_CXX11_DECLARATION_SKETCH_v0.8.4.hpp"

namespace mo {
struct GlobalVectorCommand {};
struct GlobalVectorCommandAckReport {};
struct GlobalVectorCommandStatus {};
struct GlobalVectorExecutionStatusReport {};

struct GlobalVectorControl
    : umaa::Service<GlobalVectorControl, umaa::ServiceKind::Control> {
    struct SetGlobalVector : umaa::Operation<GlobalVectorControl,
        GlobalVectorCommand, umaa::OperationRole::Command,
        umaa::FlowKind::CommandResponse> {};
    struct CommandAck : umaa::Operation<GlobalVectorControl,
        GlobalVectorCommandAckReport, umaa::OperationRole::CommandAck,
        umaa::FlowKind::CommandResponse> {};
    struct CommandStatus : umaa::Operation<GlobalVectorControl,
        GlobalVectorCommandStatus, umaa::OperationRole::CommandStatus,
        umaa::FlowKind::CommandResponse> {};
    struct ExecutionStatus : umaa::Operation<GlobalVectorControl,
        GlobalVectorExecutionStatusReport, umaa::OperationRole::ExecutionStatus,
        umaa::FlowKind::CommandResponse> {};
    typedef SetGlobalVector Command;
};
}

namespace sa {
struct PoseReport {};
struct GlobalPoseStatus : umaa::Service<GlobalPoseStatus, umaa::ServiceKind::StatusReport> {
    struct Report : umaa::Operation<GlobalPoseStatus,
        PoseReport, umaa::OperationRole::Report,
        umaa::FlowKind::RequestReplyNoQueryData> {};
};
}

class AutoPilot : public umaa::Component {
public:
    umaa::CommandDecision onVector(const mo::GlobalVectorCommand&, const umaa::CommandInfo&);
    umaa::CancelDecision onCancel(const umaa::CancelInfo&);
    void onPose(const sa::PoseReport&);
    void onExecution(const mo::GlobalVectorExecutionStatusReport&);
    void publishExecution(const umaa::CommandToken& token,
                          const mo::GlobalVectorExecutionStatusReport& data) {
        publish<mo::GlobalVectorControl::ExecutionStatus>(token, data);
    }
};

UMAA_COMPONENT(AutoPilot, m) {
    m.provider<mo::GlobalVectorControl>()
      .command<mo::GlobalVectorControl::SetGlobalVector>(&AutoPilot::onVector)
      .cancel<mo::GlobalVectorControl::SetGlobalVector>(&AutoPilot::onCancel);
    m.consumer<sa::GlobalPoseStatus>()
      .report<sa::GlobalPoseStatus::Report>(&AutoPilot::onPose);
    m.consumer<mo::GlobalVectorControl>()
      .report<mo::GlobalVectorControl::ExecutionStatus>(&AutoPilot::onExecution);
}

class MissionManager : public umaa::Component {
public:
    umaa::RecoveryDecision onRecoveredVector(
        const umaa::RecoveredCommand<mo::GlobalVectorCommand>&);
    void onExecution(const mo::GlobalVectorExecutionStatusReport&);
};

UMAA_COMPONENT(MissionManager, m) {
    umaa::ConsumerOptions options;
    options.onRecovered<mo::GlobalVectorControl::SetGlobalVector>(
        &MissionManager::onRecoveredVector);
    m.consumer<mo::GlobalVectorControl>(options)
      .report<mo::GlobalVectorControl::ExecutionStatus>(
          &MissionManager::onExecution);
}

class PoseProvider : public umaa::Component {
public:
    void publishInitialSnapshot(const std::vector<sa::PoseReport>& current) {
        publish<sa::GlobalPoseStatus::Report>(current,
            umaa::PublishMode::ReplaceOwnedInstances);
    }
};
UMAA_COMPONENT(PoseProvider, m) {
    m.provider<sa::GlobalPoseStatus>();
}

static_assert(mo::GlobalVectorControl::kind == umaa::ServiceKind::Control, "service kind");
static_assert(mo::GlobalVectorControl::SetGlobalVector::flow ==
              umaa::FlowKind::CommandResponse, "command flow");