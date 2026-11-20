/*
 * UMAA 6.0 Config Service: SAME Provider/Consumer facade can support both
 * Command/Response and Queryless Request/Reply.
 *
 * Original Service: UMAA::SA::GlobalPoseConfig (NOT in official AutoPilot 14).
 * Verified original IDL:
 *  - GlobalPoseConfigCommandType: attitude, elevation, position,
 *    optional attitudeCovariance/positionCovariance, timeStamp,
 *    source, destination, sessionID.
 *  - GlobalPoseConfigReportType: timeStamp, source. Do NOT invent
 *    position/attitude fields on this report.
 *
 * Source:
 *  datamodel/umaa/idl/UMAA/SA/GlobalPoseConfig/GlobalPoseConfig.md
 *  .../GlobalPoseConfigCommandType.idl (Version 6.0)
 *  .../GlobalPoseConfigReportType.idl (Version 6.0)
 *
 * API DESIGN SKETCH ONLY, not SDK/DDC-compiled C++11. Required real generated
 * SDK headers and application PoseInitializer/ConfigObserver are omitted.
 */
namespace SA = umaa::services::SA;

using ConfigCommand =
    UMAA::SA::GlobalPoseConfig::GlobalPoseConfigCommandType;
using ConfigReport =
    UMAA::SA::GlobalPoseConfig::GlobalPoseConfigReportType;

// =========================================================================
// Provider: one generated Service role object offers TWO communication flows
// =========================================================================
class PoseConfigurationProvider : public umaa::Component {
public:
    SA::GlobalPoseConfigProvider poseConfig; // not ConfigService or ReportService

    explicit PoseConfigurationProvider(PoseInitializer& device)
        : device_(device)
    {
        // Command/Response pattern of the Config Service
        poseConfig.onCommand(
            [this](const ConfigCommand& cmd,
                   const umaa::CommandInfo& info) {
                // Real fields: position, attitude, elevation.
                if (!device_.validate(cmd))
                    return umaa::CommandDecision::rejected(
                        umaa::CommandReason::ValidationFailed);

                device_.applyInitialPose(cmd, info.token());
                return umaa::CommandDecision::accepted(); // asynchronous
            });

        poseConfig.onCancel(
            [this](const umaa::CancelInfo& info) {
                device_.requestCancel(info.token());
                return umaa::CancelDecision::deferred();
            });
    }

    void hardwareApplied(const umaa::CommandToken& token) {
        poseConfig.respond(token, umaa::CommandProgress::completed());
    }

    // Queryless Report pattern of the SAME Config Service
    void reportCurrentConfiguration(const ConfigReport& report) {
        poseConfig.publish(report);  // distinct report Operation
    }

    void disposeConfiguration(const ConfigReport& reportWithKey) {
        poseConfig.dispose(reportWithKey); // key: source
    }

private:
    PoseInitializer& device_;
};

UMAA_COMPONENT(PoseConfigurationProvider, m) {
    m.add(&PoseConfigurationProvider::poseConfig); // one Service object
}

// =========================================================================
// Consumer: one generated Service role object can send commands AND subscribe
// =========================================================================
class PoseConfigurationConsumer : public umaa::Component {
public:
    SA::GlobalPoseConfigConsumer poseConfig;

    explicit PoseConfigurationConsumer(ConfigObserver& observer)
        : observer_(observer)
    {
        // The Queryless Report event carries original ConfigReportType.
        poseConfig.onReport([this](const ConfigReport& report) {
            // Actual IDL Report only has source/timeStamp;
            // this is NOT equivalent to reading the Command's pose fields.
            observer_.receivedConfigState(report);
        });
        poseConfig.onDispose(
            [this](const UMAA::Common::IdentifierType& source) {
                observer_.configurationDisposed(source);
            });
    }

    // Command/Response flow: simple send
    void initializePose(const ConfigCommand& cmd) {
        poseConfig.command(cmd);
    }

    // Command/Response flow: tracked send
    umaa::CommandHandle<ConfigCommand>
    initializePoseTracked(const ConfigCommand& cmd) {
        return poseConfig.command(cmd); // not remote ACK/completion
    }

    void revisePose(umaa::CommandHandle<ConfigCommand>& h,
                    const ConfigCommand& updated) {
        h.update(updated); // same Command Session, not a new command
    }

    void cancelPose(umaa::CommandHandle<ConfigCommand>& h) {
        h.cancel(); // async cancellation REQUEST
    }

private:
    ConfigObserver& observer_;
};

UMAA_COMPONENT(PoseConfigurationConsumer, m) {
    m.add(&PoseConfigurationConsumer::poseConfig); // one Service object
}

/*
 * Generator sketch: generated SA::GlobalPoseConfigProvider : umaa::Service
 * privately COMPOSES CommandProvider<GlobalPoseConfig> plus
 * ReportProvider<GlobalPoseConfig>. Generated ...Consumer privately COMPOSES
 * CommandConsumer<GlobalPoseConfig> plus ReportConsumer<GlobalPoseConfig>.
 *
 * UMAA Config/Control/Specs/Status suffix determines the business category,
 * NOT the precise communication flow. An actual Operation inventory drives
 * generator capabilities; CommandStatus/Ack are Command-role protocol reports,
 * while reportGlobalPoseConfig is the Queryless Report-role operation.
 *
 * Same Service can also be used by both Provider and Consumer in the SAME
 * Component, if application/platform architecture intentionally requires it.
 */
