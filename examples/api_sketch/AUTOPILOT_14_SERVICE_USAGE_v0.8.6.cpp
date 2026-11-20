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
 * virtual Hooks, CommandHandle, LargeSet, etc. are proposed v0.8.6 APIs.
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
// 1. AutoPilot Providers — actual Service role, typed incoming commands
// ==========================================================================

class PrimitiveProvider : public MO::PrimitiveDriverControlProvider {
public:
    explicit PrimitiveProvider(AutopilotLogic& l) : logic_(l) {}
    umaa::CommandDecision onCommand(
        const PrimitiveCmd& cmd, const umaa::CommandInfo& info) override
    {
        // Actual fields: propulsiveLinearEffort, propulsiveRotationalEffort,
        // resistiveLinearEffort, resistiveRotationalEffort.
        if (!logic_.acceptPrimitive(cmd))
            return umaa::CommandDecision::rejected(
                umaa::CommandReason::ValidationFailed);
        logic_.executePrimitive(cmd, info.token());
        return umaa::CommandDecision::accepted();
    }
private:
    AutopilotLogic& logic_;
};

class VectorProvider : public MO::GlobalVectorControlProvider {
public:
    explicit VectorProvider(AutopilotLogic& l) : logic_(l) {}
    umaa::CommandDecision onCommand(
        const VectorCmd& cmd, const umaa::CommandInfo& info) override
    {
        // Real GlobalVectorCommandType fields:
        // direction, directionMode, speed; optional elevation, endTime, etc.
        if (info.delivery() == umaa::CommandDelivery::Recovered) {
            return logic_.isExecutingVector(info.token())
                ? umaa::CommandDecision::resume()
                : umaa::CommandDecision::rejected(
                    umaa::CommandReason::ServiceFailed);
        }
        if (!logic_.acceptVector(cmd))
            return umaa::CommandDecision::rejected(
                umaa::CommandReason::ValidationFailed);
        logic_.executeVector(cmd, info.token());
        return umaa::CommandDecision::accepted(); // not execution complete
    }

    umaa::CancelDecision onCancel(const umaa::CancelInfo& info) override {
        return logic_.requestVectorStop(info.token())
            ? umaa::CancelDecision::deferred()
            : umaa::CancelDecision::cannotCancel();
    }

    void hardwareExecuting(const umaa::CommandToken& token) {
        respond(token, umaa::CommandProgress::executing());
    }
    void hardwareProgress(
        const umaa::CommandToken& token,
        const VectorExec& state)
    {
        // state.directionAchieved, elevationAchieved, speedAchieved.
        publish(token, state);
    }
    void hardwareCompleted(const umaa::CommandToken& token) {
        respond(token, umaa::CommandProgress::completed());
    }
    void hardwareStopped(const umaa::CommandToken& token) {
        respond(token, umaa::CommandProgress::canceled());
    }
private:
    AutopilotLogic& logic_;
};

class HoverProvider : public MO::GlobalHoverControlProvider {
public:
    explicit HoverProvider(AutopilotLogic& l) : logic_(l) {}
    umaa::CommandDecision onCommand(
        const HoverCmd& cmd, const umaa::CommandInfo& info) override
    {
        // Actual fields: position, hoverRadius, controlPriority,
        // transitSpeed, optional heading/elevation/endTime.
        if (!logic_.acceptHover(cmd))
            return umaa::CommandDecision::rejected(
                umaa::CommandReason::ValidationFailed);
        logic_.executeHover(cmd, info.token());
        return umaa::CommandDecision::accepted();
    }
private:
    AutopilotLogic& logic_;
};

class ActiveProvider : public MM::ActiveConstraintsControlProvider {
public:
    explicit ActiveProvider(AutopilotLogic& l) : logic_(l) {}
    umaa::CommandDecision onCommand(
        const ActiveCmd& cmd, const umaa::CommandInfo& info) override
    {
        // Actual field: sequence<NumericGUID,256> constraintConditionalIDs.
        if (!logic_.acceptActive(cmd))
            return umaa::CommandDecision::rejected(
                umaa::CommandReason::ValidationFailed);
        logic_.executeActive(cmd, info.token());
        return umaa::CommandDecision::accepted();
    }
private:
    AutopilotLogic& logic_;
};

// ==========================================================================
// 2. AutoPilot Consumers — actual SA Report types, direct onReport()
// ==========================================================================

class PoseConsumer : public SA::GlobalPoseStatusConsumer {
public:
    explicit PoseConsumer(AutopilotLogic& l) : logic_(l) {}
    void onReport(const Pose& pose) override {
        // Real fields: position, attitude, course, navigationSolution.
        logic_.receivedPose(pose); // no ReportEvent.data() wrapper
    }
    void onDispose(const UMAA::Common::IdentifierType& source) override {
        logic_.poseInvalidated(source);
    }
private:
    AutopilotLogic& logic_;
};

class VelocityConsumer : public SA::VelocityStatusConsumer {
public:
    explicit VelocityConsumer(AutopilotLogic& l) : logic_(l) {}
    void onReport(const Velocity& data) override {
        // Real fields: velocity, attitudeRate.
        logic_.receivedVelocity(data);
    }
private:
    AutopilotLogic& logic_;
};

class SpeedConsumer : public SA::SpeedStatusConsumer {
public:
    explicit SpeedConsumer(AutopilotLogic& l) : logic_(l) {}
    void onReport(const Speed& data) override {
        // Real optional fields: speedOverGround, speedThroughWater,
        // speedThroughAir, mode; do not assume every optional is present.
        logic_.receivedSpeed(data);
    }
private:
    AutopilotLogic& logic_;
};

class WindConsumer : public SA::WindStatusConsumer {
public:
    explicit WindConsumer(AutopilotLogic& l) : logic_(l) {}
    void onReport(const Wind& data) override {
        // WindReportType includes optional relative wind measurements.
        logic_.receivedWind(data);
    }
private:
    AutopilotLogic& logic_;
};

class CurrentConsumer : public SA::WaterCurrentStatusConsumer {
public:
    explicit CurrentConsumer(AutopilotLogic& l) : logic_(l) {}
    void onReport(const WaterCurrent& data) override {
        // Real fields: currentDirection, currentSpeed.
        logic_.receivedCurrent(data);
    }
private:
    AutopilotLogic& logic_;
};

// CommandConsumer: AutoPilot sends ADD/DELETE to MissionPlanConstraintControl.
// These are two different actual IDL Command types inside one Service.
class MissionConstraintsConsumer
    : public MM::MissionPlanConstraintControlConsumer {
public:
    explicit MissionConstraintsConsumer(AutopilotLogic& l) : logic_(l) {}

    void onAck(const ConstraintAddAck& ack) override {
        // The application may inspect an actual IDL Ack message here.
        (void)ack;
    }
    void onAck(const ConstraintDeleteAck& ack) override {
        (void)ack;
    }
    void onStatus(const ConstraintAddStatus& status) override {
        logic_.constraintAddStatus(status);
    }
    void onStatus(const ConstraintDeleteStatus& status) override {
        logic_.constraintDeleteStatus(status);
    }
private:
    AutopilotLogic& logic_;
};

// Specialization + LargeSet is a CONSUMED ConditionalReport for AutoPilot.
class ConditionalConsumer : public MM::ConditionalReportConsumer {
public:
    explicit ConditionalConsumer(AutopilotLogic& l) : logic_(l) {}

    void onReport(
        const ConditionalReport& report,
        const ConditionalSet& assembled) override
    {
        // Actual ConditionalReportType has conditionalsSetMetadata,
        // not a synthetic "report.conditionals" vector.
        if (!assembled.complete()) return; // SDK assembly state
        logic_.receivedConditionals(report, assembled);
    }
private:
    AutopilotLogic& logic_;
};

// ==========================================================================
// 3. AutoPilot Component: exactly 14 original Service roles
// ==========================================================================

class AutoPilot : public umaa::Component {
public:
    // Provider 7:
    PrimitiveProvider primitiveDriver;       // MO PrimitiveDriverControl
    VectorProvider globalVector;             // MO GlobalVectorControl
    HoverProvider globalHover;               // MO GlobalHoverControl
    ActiveProvider activeConstraints;        // MM ActiveConstraintsControl
    EO::UVPlatformSpecsProvider platformSpecs;// EO UVPlatformSpecs
    SO::LogReportProvider logReport;         // SO LogReport
    SO::HealthReportProvider healthReport;   // SO HealthReport

    // Consumer 7:
    PoseConsumer globalPose;                 // SA GlobalPoseStatus
    VelocityConsumer velocity;               // SA VelocityStatus
    SpeedConsumer speed;                     // SA SpeedStatus
    WindConsumer wind;                       // SA WindStatus
    CurrentConsumer waterCurrent;            // SA WaterCurrentStatus
    MissionConstraintsConsumer planConstraint;// MM MissionPlanConstraintControl
    ConditionalConsumer conditionals;       // MM ConditionalReport

    explicit AutoPilot(AutopilotLogic& l)
        : primitiveDriver(l),
          globalVector(l),
          globalHover(l),
          activeConstraints(l),
          globalPose(l),
          velocity(l),
          speed(l),
          wind(l),
          waterCurrent(l),
          planConstraint(l),
          conditionals(l) {}

    // ReportProvider: two REAL UVPlatformSpecs Operation data types.
    void publishSpecs(const Specs& specs, const Capabilities& caps) {
        platformSpecs.publish(specs); // reportUVPlatformSpecs
        platformSpecs.publish(caps);  // reportUVPlatformCapabilities
    }

    void publishHealth(const Health& health) {
        // Real HealthReportType: severity, logTime, code, resourceID,
        // optional status, plus SDK-assigned source/timeStamp.
        healthReport.publish(health);
    }

    void disposeHealth(const Health& healthWithKeys) {
        // HealthReportType key = (source, code, resourceID): keyed Report.
        healthReport.dispose(healthWithKeys);
    }

    void replaceOwnedHealth(const std::vector<Health>& current) {
        healthReport.publish(current, umaa::PublishMode::ReplaceOwnedInstances);
    }

    void publishLog(const Log& log) {
        // Real LogReportType: entry, level; SDK manages source/timeStamp.
        logReport.publish(log);
    }

    // CommandConsumer: AutoPilot sends actual two MM Command types.
    void addMissionConstraint(const ConstraintAdd& add) {
        planConstraint.command(add);
    }

    umaa::CommandHandle<ConstraintAdd> addConstraintWithTracking(
        const ConstraintAdd& add)
    {
        return planConstraint.command(add);
    }

    void deleteMissionConstraint(const ConstraintDelete& remove) {
        planConstraint.command(remove);
    }

    void updateConstraint(
        umaa::CommandHandle<ConstraintAdd>& handle,
        const ConstraintAdd& modified)
    {
        handle.update(modified);
    }

    void cancelConstraint(umaa::CommandHandle<ConstraintAdd>& handle) {
        handle.cancel(); // cancellation request, not terminal status
    }

    // ReportConsumer cache is optional, not a subscription RPC.
    void inspectPose(
        const UMAA::Common::IdentifierType& source,
        AutopilotLogic& logic)
    {
        auto latest = globalPose.getLatest(source);
        if (latest.hasValue())
            logic.receivedPose(latest.value());
    }
};

UMAA_COMPONENT(AutoPilot, m) {
    // Seven Service Providers:
    m.add(&AutoPilot::primitiveDriver);
    m.add(&AutoPilot::globalVector);
    m.add(&AutoPilot::globalHover);
    m.add(&AutoPilot::activeConstraints);
    m.add(&AutoPilot::platformSpecs);
    m.add(&AutoPilot::logReport);
    m.add(&AutoPilot::healthReport);

    // Seven Service Consumers:
    m.add(&AutoPilot::globalPose);
    m.add(&AutoPilot::velocity);
    m.add(&AutoPilot::speed);
    m.add(&AutoPilot::wind);
    m.add(&AutoPilot::waterCurrent);
    m.add(&AutoPilot::planConstraint);
    m.add(&AutoPilot::conditionals);
}

// ==========================================================================
// 4. External caller (e.g. Mission Executor) → AutoPilot Provider
//    IMPORTANT: GlobalVectorControlConsumer is NOT one of AutoPilot's 14 roles!
// ==========================================================================

void missionExecutorCommandsAutoPilot(
    MO::GlobalVectorControlConsumer& remoteVectorConsumer)
{
    VectorCmd cmd;
    fillGlobalVectorCommand(cmd);
    // Actual required fields: direction, directionMode, speed;
    // speed is SpeedRequirementVariantType, NOT an untyped double.

    // Simple fire-and-track-by-system-state usage.
    remoteVectorConsumer.command(cmd);

    // A separate NEW command instance for a managed session:
    VectorCmd managed;
    fillGlobalVectorCommand(managed);
    auto handle = remoteVectorConsumer.command(managed);
    VectorCmd changed = managed;
    reviseGlobalVectorSpeed(changed); // local change only
    handle.update(changed);          // same Command Session update
    auto state = handle.wait(umaa::WaitStage::Commanded, 3000);
    (void)state;
    handle.cancel();                 // request, not confirmed stop
}

// ==========================================================================
// 5. External Provider → AutoPilot ReportConsumer (illustration only)
//    Navigation provides GlobalPoseStatus; AutoPilot consumes it.
// ==========================================================================

void navigationPublishesPose(
    SA::GlobalPoseStatusProvider& navigationPoseProvider)
{
    Pose pose;
    fillGlobalPose(pose);
    // Real pose fields: position, attitude, course, navigationSolution.
    navigationPoseProvider.publish(pose);
}

/*
 * IMPORTANT LIMITATIONS
 *
 * - The 14 members above match actual AutoPilot Component service roles.
 *   The outside references in sections 4-5 are NOT additional AutoPilot roles.
 * - Operation→Topic resolution by message type must be unique per Service/role;
 *   ambiguous cases require the advanced <Op> form.
 * - Component Conflict Policy: the official AutoPilot definition requires a
 *   deliberate policy for conflicts among PrimitiveDriver, GlobalVector,
 *   GlobalHover and among multiple command senders; the SDK cannot invent it.
 * - SDK-proposed Service facade virtual Hooks and m.add registration are not
 *   real compiled classes yet.
 * - The source demonstrates public-field IDL C++ mapping. Accessor mapping is
 *   a different form, e.g. pose.position() and cmd.speed(requirement).
 * - For optional fields, check generated optional representation before read.
 * - CommandHandle tracks source/session; it is not an ACK, remote result or
 *   RAII auto-cancel. Destruction must not stop the remote Command.
 * - DDS cancel, recovery, Queryless subscription, dispose and cross-Topic
 *   LargeSet specialization assembly need real runtime verification.
 */
