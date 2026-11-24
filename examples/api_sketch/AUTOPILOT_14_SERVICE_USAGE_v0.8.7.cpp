/*
 * UMAA ICD 6.0 AutoPilot Component — 14-Service Public API usage, C++11
 *
 * Source of the 14 role assignments:
 *   RTI/community docs/UMAA Component Definitions v1.0.md, section 3.1:
 *   Provider: MO PrimitiveDriverControl, GlobalVectorControl, GlobalHoverControl;
 *             MM ActiveConstraintsControl; EO UVPlatformSpecs;
 *             SO LogReport, HealthReport.
 *   Consumer: SA GlobalPoseStatus, VelocityStatus, SpeedStatus,
 *             WindStatus, WaterCurrentStatus;
 *             MM MissionPlanConstraintControl, ConditionalReport.
 *
 * Original IDL message names and Operation names were checked against
 * UMAA 6.0 IDL and per-Service .md definitions.
 *
 * DESIGN SKETCH ONLY — NOT A COMPILED SDK.
 * SDK-generated classes (umaa::services::*Provider/Consumer), m.add(...),
 * Callback registration, CommandHandle, LargeSet, etc. are proposed v0.8.7 APIs.
 * Hardware/Navigation/AutopilotLogic and fill... functions are application
 * placeholders. Include the actual generated SDK/IDL headers in implementation.
 *
 * The source shows a public-member IDL C++ mapping:
 *   cmd.direction = value;    pose.position;
 * If the generated IDL C++ mapping instead uses accessors:
 *   cmd.direction(value);     pose.position();
 * Never mix both mappings in a single build.
 */

namespace MO = umaa::services::MO;
namespace SA = umaa::services::SA;
namespace SO = umaa::services::SO;
namespace EO = umaa::services::EO;
namespace MM = umaa::services::MM;

#include <vector> // SDK snapshot overload (C++11)

// ----------- Original UMAA 6.0 IDL types ----------------------------------
using PrimitiveCmd = UMAA::MO::PrimitiveDriverControl::PrimitiveDriverCommandType;
using VectorCmd = UMAA::MO::GlobalVectorControl::GlobalVectorCommandType;
using VectorExec = UMAA::MO::GlobalVectorControl::GlobalVectorExecutionStatusReportType;
using HoverCmd = UMAA::MO::GlobalHoverControl::GlobalHoverCommandType;
using ActiveCmd = UMAA::MM::ActiveConstraintsControl::ActiveConstraintsCommandType;

using Pose = UMAA::SA::GlobalPoseStatus::GlobalPoseReportType;
using Velocity = UMAA::SA::VelocityStatus::VelocityReportType;
using Speed = UMAA::SA::SpeedStatus::SpeedReportType;
using Wind = UMAA::SA::WindStatus::WindReportType;
using WaterCurrent = UMAA::SA::WaterCurrentStatus::WaterCurrentReportType;

using Specs = UMAA::EO::UVPlatformSpecs::UVPlatformSpecsReportType;
using Capabilities = UMAA::EO::UVPlatformSpecs::UVPlatformCapabilitiesReportType;
using Log = UMAA::SO::LogReport::LogReportType;
using Health = UMAA::SO::HealthReport::HealthReportType;

using ConstraintAdd = UMAA::MM::MissionPlanConstraintControl::
    MissionPlanConstraintAddCommandType;
using ConstraintDelete = UMAA::MM::MissionPlanConstraintControl::
    MissionPlanConstraintDeleteCommandType;
using ConstraintAddStatus = UMAA::MM::MissionPlanConstraintControl::
    MissionPlanConstraintAddCommandStatusType;
using ConstraintDeleteStatus = UMAA::MM::MissionPlanConstraintControl::
    MissionPlanConstraintDeleteCommandStatusType;
using ConstraintAddAck = UMAA::MM::MissionPlanConstraintControl::
    MissionPlanConstraintAddCommandAckReportType;
using ConstraintDeleteAck = UMAA::MM::MissionPlanConstraintControl::
    MissionPlanConstraintDeleteCommandAckReportType;
using ConditionalReport = UMAA::MM::ConditionalReport::ConditionalReportType;
using ConditionalBase = UMAA::MM::Conditional::ConditionalType;
using DepthConditional = UMAA::MM::Conditional::DepthConditionalType;
using ConditionalItem = umaa::Specialized<ConditionalBase>;
using ConditionalSet = umaa::LargeSet<ConditionalItem>;

// Application-specific type: no assumptions about underlying hardware.
struct AutopilotLogic {
    bool acceptPrimitive(const PrimitiveCmd&) const;
    bool acceptVector(const VectorCmd&) const;
    bool acceptHover(const HoverCmd&) const;
    bool acceptActive(const ActiveCmd&) const;
    void executePrimitive(const PrimitiveCmd&, const umaa::CommandToken&);
    void executeVector(const VectorCmd&, const umaa::CommandToken&);
    void executeHover(const HoverCmd&, const umaa::CommandToken&);
    void executeActive(const ActiveCmd&, const umaa::CommandToken&);
    // Application owns recovery/update decisions and actual stop confirmation.
    umaa::CommandDecision dispatchPrimitive(const PrimitiveCmd&, const umaa::CommandInfo&);
    umaa::CommandDecision dispatchHover(const HoverCmd&, const umaa::CommandInfo&);
    umaa::CommandDecision dispatchActive(const ActiveCmd&, const umaa::CommandInfo&);
    umaa::CommandDecision dispatchVector(const VectorCmd&, const umaa::CommandInfo&);
    umaa::CancelDecision cancelPrimitive(const umaa::CancelInfo&);
    umaa::CancelDecision cancelHover(const umaa::CancelInfo&);
    umaa::CancelDecision cancelActive(const umaa::CancelInfo&);
    bool requestVectorStop(const umaa::CommandToken&);
    bool isExecutingVector(const umaa::CommandToken&) const;
    void receivedPose(const Pose&);
    void receivedVelocity(const Velocity&);
    void receivedSpeed(const Speed&);
    void receivedWind(const Wind&);
    void receivedCurrent(const WaterCurrent&);
    void receivedConditionals(const ConditionalReport&, const ConditionalSet&);
    void poseInvalidated(const UMAA::Common::IdentifierType&);
    void constraintAddStatus(const ConstraintAddStatus&);
    void constraintDeleteStatus(const ConstraintDeleteStatus&);
};

// ==========================================================================
// 1. AutoPilot Component: generated Service role objects as MEMBERS
//    The application does not subclass each Provider/Consumer.
// ==========================================================================

class AutoPilot : public umaa::Component {
public:
    // Official 7 Service Providers
    MO::PrimitiveDriverControlProvider primitiveDriver;
    MO::GlobalVectorControlProvider globalVector;
    MO::GlobalHoverControlProvider globalHover;
    MM::ActiveConstraintsControlProvider activeConstraints;
    EO::UVPlatformSpecsProvider platformSpecs;
    SO::LogReportProvider logReport;
    SO::HealthReportProvider healthReport;

    // Official 7 Service Consumers
    SA::GlobalPoseStatusConsumer globalPose;
    SA::VelocityStatusConsumer velocity;
    SA::SpeedStatusConsumer speed;
    SA::WindStatusConsumer wind;
    SA::WaterCurrentStatusConsumer waterCurrent;
    MM::MissionPlanConstraintControlConsumer planConstraint;
    MM::ConditionalReportConsumer conditionals;

    explicit AutoPilot(AutopilotLogic& logic) : logic_(logic) {
        // Command/Response Provider 1: PrimitiveDriverControl
        primitiveDriver.onCommand(
            [this](const PrimitiveCmd& cmd, const umaa::CommandInfo& info) {
                return logic_.dispatchPrimitive(cmd, info); // New/Updated/Recovered
            });

        // Command/Response Provider 2: GlobalVectorControl
        globalVector.onCommand(
            [this](const VectorCmd& cmd, const umaa::CommandInfo& info) {
                return logic_.dispatchVector(cmd, info); // New/Updated/Recovered
            });
        globalVector.onCancel(
            [this](const umaa::CancelInfo& info) {
                return logic_.requestVectorStop(info.token())
                    ? umaa::CancelDecision::deferred()
                    : umaa::CancelDecision::cannotCancel();
            });

        // Command/Response Provider 3: GlobalHoverControl
        globalHover.onCommand(
            [this](const HoverCmd& cmd, const umaa::CommandInfo& info) {
                return logic_.dispatchHover(cmd, info);
            });

        // Command/Response Provider 4: ActiveConstraintsControl
        activeConstraints.onCommand(
            [this](const ActiveCmd& cmd, const umaa::CommandInfo& info) {
                return logic_.dispatchActive(cmd, info);
            });

        primitiveDriver.onCancel([this](const umaa::CancelInfo& info) {
            return logic_.cancelPrimitive(info);
        });
        globalHover.onCancel([this](const umaa::CancelInfo& info) {
            return logic_.cancelHover(info);
        });
        activeConstraints.onCancel([this](const umaa::CancelInfo& info) {
            return logic_.cancelActive(info);
        });

        // Queryless Report Consumers: five actual SA services
        globalPose.onReport(
            [this](const umaa::ReportEvent<Pose>& event) {
                if (event.kind() == umaa::ReportKind::Received && event.data())
                    logic_.receivedPose(*event.data());
                else if (event.kind() == umaa::ReportKind::Removed)
                    logic_.poseInvalidated(event.key());
            });
        velocity.onReport(
            [this](const Velocity& report) { logic_.receivedVelocity(report); });
        speed.onReport(
            [this](const Speed& report) { logic_.receivedSpeed(report); });
        wind.onReport(
            [this](const Wind& report) { logic_.receivedWind(report); });
        waterCurrent.onReport(
            [this](const WaterCurrent& report) {
                logic_.receivedCurrent(report);
            });

        // Command Consumer: MissionPlanConstraintControl has two Command types
        planConstraint.onResponse(
            [this](const ConstraintAddStatus& status) {
                logic_.constraintAddStatus(status);
            });
        planConstraint.onResponse(
            [this](const ConstraintDeleteStatus& status) {
                logic_.constraintDeleteStatus(status);
            });

        // Queryless Report Consumer with LargeSet + Specialization
        conditionals.onReport(
            [this](const ConditionalReport& report,
                   const ConditionalSet& assembled) {
                if (assembled.complete())
                    logic_.receivedConditionals(report, assembled);
            });
    }

    // ---------- CommandProvider actual hardware events ----------

    void vectorStarted(const umaa::CommandToken& token) {
        globalVector.respond(token, umaa::CommandProgress::executing());
    }

    void vectorProgress(const umaa::CommandToken& token,
                        const VectorExec& data) {
        globalVector.respond(token, data); // ExecutionStatus Report
    }

    void vectorCompleted(const umaa::CommandToken& token) {
        globalVector.respond(token, umaa::CommandProgress::completed());
    }

    void vectorStopped(const umaa::CommandToken& token) {
        // Confirmed physical stop, not simply onCancel() callback return.
        globalVector.respond(token, umaa::CommandProgress::canceled());
    }

    // ---------- ReportProvider publish / remove ----------

    void sendPlatformSpecs(const Specs& specs, const Capabilities& caps) {
        platformSpecs.publish(specs);
        platformSpecs.publish(caps);
    }

    void sendHealth(const Health& data) {
        healthReport.publish(data);
    }

    void disposeHealth(const Health& keyedReport) {
        // Original HealthReportType includes key fields source/code/resourceID.
        healthReport.remove(keyedReport);
    }

    void sendLog(const Log& data) {
        logReport.publish(data);
    }

    // ---------- CommandConsumer command / CommandHandle ----------

    void addConstraintFireAndForget(const ConstraintAdd& cmd) {
        planConstraint.command(cmd); // returned Handle intentionally ignored
    }

    umaa::CommandHandle<ConstraintAdd>
    addConstraint(const ConstraintAdd& cmd) {
        return planConstraint.command(cmd);
    }

    void updateConstraint(umaa::CommandHandle<ConstraintAdd>& handle,
                          const ConstraintAdd& revised) {
        handle.update(revised);
    }

    void cancelConstraint(umaa::CommandHandle<ConstraintAdd>& handle) {
        handle.cancel(); // cancel REQUEST only
    }

    void deleteConstraint(const ConstraintDelete& cmd) {
        planConstraint.command(cmd); // distinct operation, inferred from type
    }

    // ---------- Optional latest valid Report cache ----------

    void useCachedPose(const UMAA::Common::IdentifierType& source) {
        auto latest = globalPose.getLatest(source);
        if (latest.hasValue())
            logic_.receivedPose(latest.value());
    }

private:
    static umaa::CommandDecision rejectInvalid() {
        return umaa::CommandDecision::rejected(
            umaa::CommandReason::ValidationFailed);
    }

    AutopilotLogic& logic_;
};

UMAA_COMPONENT(AutoPilot, m) {
    // Providers: 7
    m.add(&AutoPilot::primitiveDriver);
    m.add(&AutoPilot::globalVector);
    m.add(&AutoPilot::globalHover);
    m.add(&AutoPilot::activeConstraints);
    m.add(&AutoPilot::platformSpecs);
    m.add(&AutoPilot::logReport);
    m.add(&AutoPilot::healthReport);

    // Consumers: 7
    m.add(&AutoPilot::globalPose);
    m.add(&AutoPilot::velocity);
    m.add(&AutoPilot::speed);
    m.add(&AutoPilot::wind);
    m.add(&AutoPilot::waterCurrent);
    m.add(&AutoPilot::planConstraint);
    m.add(&AutoPilot::conditionals);
}

// ==========================================================================
// 2. Other Components use the SAME generated Service-role naming scheme
//    These are NOT extra AutoPilot Service roles.
// ==========================================================================

void missionExecutorSendsVector(
    MO::GlobalVectorControlConsumer& remoteConsumer)
{
    VectorCmd cmd;
    fillGlobalVectorCommand(cmd);    // builds real IDL variant fields

    remoteConsumer.command(cmd);      // simple new Command
}

void missionExecutorTracksVector(
    MO::GlobalVectorControlConsumer& remoteConsumer)
{
    VectorCmd cmd;
    fillGlobalVectorCommand(cmd);

    auto handle = remoteConsumer.command(cmd);
    VectorCmd revised = cmd;
    reviseGlobalVectorSpeed(revised); // local change only
    handle.update(revised);          // same Source/Session update
    handle.cancel();                 // async request, not canceled result
}

void navigationPublishesPose(
    SA::GlobalPoseStatusProvider& navigationProvider)
{
    Pose pose;
    fillGlobalPose(pose);             // original IDL data
    navigationProvider.publish(pose);
}

/*
 * DESIGN STATUS
 * - 14 registered Service roles are the official AutoPilot definition,
 *   not a claim that all 14 are implemented by actual DDS in this repository.
 * - Generator creates <ServiceName>Provider / <ServiceName>Consumer facades,
 *   each deriving once from umaa::Service and COMPOSING the supported
 *   CommandProvider/Consumer<S> or ReportProvider/Consumer<S> roles.
 * - Callback registration and all facade Actions above are proposed C++11
 *   interfaces. There is no implemented SDK/RTI-generated header here.
 * - In C++ generated IDL mapping, fields may be public or accessors.
 * - ServiceKind suffix cannot alone determine Command/Report pattern.
 * - Config mixed Command + Report example is in
 *   CONFIG_MIXED_SERVICE_v0.8.7.cpp (not one of AutoPilot's 14 Services).
 * - Command conflict policy across GlobalVector/GlobalHover/PrimitiveDriver
 *   and multiple senders is the application/platform's responsibility.
 * - SDK hides DDS source/destination/session/timestamps, QoS and
 *   large collection/specialization wire topics; runtime validation pending.
 */
