/*
 * UMAA Component SDK v0.8.6 — 전체 Public API 사용자 예제 (C++11 설계 스케치)
 *
 * 주의: 실제 SDK 생성 클래스와 Runtime이 아직 없으므로, 본 파일은 실행/컴파일
 * 가능한 샘플이 아닌 API 계약 예제다. 사용자 장비/도메인 타입과 함수도
 * 설명 목적으로 생략하였다.
 *
 * 실제 UMAA IDL 타입: UMAA::<Domain>::<Service>::<Report/Command>Type
 * SDK 생성 예정 클래스: umaa::services::<Domain>::<Service>Provider/Consumer
 * 다른 Service도 동일 command/publish/respond/request 등 상속된 공통 함수 사용.
 *
 * 아래 필드 접근은 공개 멤버 매핑을 기본으로 한다:
 *   cmd.speed = requirement;
 * RTI 생성 타입이 접근자를 사용하면:
 *   cmd.speed(requirement);
 * 수신에서는 pose.position -> pose.position()으로 바꾼다.
 *
 * 이 파일에서는 사용자 하드웨어 데이터 생성을 fillXXX(...)로 표시.
 * GlobalVectorCommandType::speed는 원본 IDL의 SpeedRequirementVariantType;
 * double을 그대로 넣을 수 있다고 가정하지 않는다.
 */

// 실제 구현 때는 SDK 헤더 및 생성된 IDL 헤더를 include해야 한다.
// #include <umaa/component.hpp>
// #include <umaa/generated_services.hpp>
// #include <UMAA/MO/GlobalVectorControl/...>
// ...
#include <vector>

namespace MO = umaa::services::MO;
namespace SA = umaa::services::SA;
namespace SO = umaa::services::SO;
namespace EO = umaa::services::EO;
namespace MM = umaa::services::MM;

using VectorCommand =
    UMAA::MO::GlobalVectorControl::GlobalVectorCommandType;
using VectorAck =
    UMAA::MO::GlobalVectorControl::GlobalVectorCommandAckReportType;
using VectorStatus =
    UMAA::MO::GlobalVectorControl::GlobalVectorCommandStatusType;
using VectorExecution =
    UMAA::MO::GlobalVectorControl::GlobalVectorExecutionStatusReportType;
using PoseReport =
    UMAA::SA::GlobalPoseStatus::GlobalPoseReportType;
using HealthReport =
    UMAA::SO::HealthReport::HealthReportType;
using BatteryReport =
    UMAA::EO::BatteryStatus::BatteryReportType;
using BatteryCell =
    UMAA::EO::BatteryStatus::BatteryCellDataType;
using ConditionalReport =
    UMAA::MM::ConditionalReport::ConditionalReportType;
using ConditionalBase =
    UMAA::MM::Conditional::ConditionalType;
using DepthConditional =
    UMAA::MM::Conditional::DepthConditionalType;

// ==========================================================================
// 1. CommandProvider: 명령 수신 / 취소 / 실행상태 발행
// ==========================================================================
class AutoPilotVector : public MO::GlobalVectorControlProvider {
public:
    explicit AutoPilotVector(Hardware& hardware) : hw_(hardware) {}

    umaa::CommandDecision onCommand(
        const VectorCommand& cmd,
        const umaa::CommandInfo& info) override
    {
        if (info.delivery() == umaa::CommandDelivery::Recovered) {
            // 실제 하드웨어 상태를 확인해야 한다. SDK가 임의로 재실행하지 않음.
            return hw_.isExecuting(info.token())
                ? umaa::CommandDecision::resume()
                : umaa::CommandDecision::rejected(
                    umaa::CommandReason::ServiceFailed);
        }
        if (!hw_.canExecute(cmd))
            return umaa::CommandDecision::rejected(
                umaa::CommandReason::ValidationFailed);

        // cmd.direction / cmd.directionMode / cmd.speed : 원본 IDL 필드
        hw_.start(cmd, info.token());
        return umaa::CommandDecision::accepted(); // 실행 완료가 아님
    }

    umaa::CancelDecision onCancel(
        const umaa::CancelInfo& info) override
    {
        if (!hw_.requestSafeStop(info.token()))
            return umaa::CancelDecision::cannotCancel();
        return umaa::CancelDecision::deferred();
    }

    // 아래 함수는 하드웨어 이벤트 수신 후 사용자 업무 코드에서 호출
    void hardwareStarted(const umaa::CommandToken& token) {
        respond(token, umaa::CommandProgress::executing());
    }

    void hardwareProgress(
        const umaa::CommandToken& token,
        const VectorExecution& execution)
    {
        publish(token, execution); // 실행상태 Report 원본 IDL 타입
    }

    void hardwareCompleted(const umaa::CommandToken& token) {
        respond(token, umaa::CommandProgress::completed());
    }

    void hardwareStopConfirmed(const umaa::CommandToken& token) {
        respond(token, umaa::CommandProgress::canceled());
    }

private:
    Hardware& hw_; // 사용자 장비 제어 객체
};

// ==========================================================================
// 2. CommandConsumer: Command 전송 / 갱신 / 취소 / 상태 Hook
// ==========================================================================
class MissionVector : public MO::GlobalVectorControlConsumer {
public:
    void onAck(const VectorAck& ack) override {
        handleCommandAck(ack.command); // 실제 IDL의 command 필드
    }
    void onStatus(const VectorStatus& status) override {
        handleCommandStatus(status.commandStatus,
                            status.commandStatusReason);
    }
    void onExecutionStatus(const VectorExecution& execution) override {
        if (execution.directionAchieved && execution.speedAchieved)
            notifyTargetsAchieved();
    }
};

// 가장 단순한 방법: 반환값을 무시해도 Command는 정상 전송되고 계속 실행될 수 있다.
void sendOnce(MissionVector& consumer) {
    VectorCommand cmd;
    fillVectorCommand(cmd); // 실제 direction / directionMode / speed 요구값
    consumer.command(cmd);
}

// 명령 관리가 필요한 경우: CommandCall(세션 추적 Handle)만 보관.
void sendManageUpdateCancel(MissionVector& consumer) {
    VectorCommand cmd;
    fillVectorCommand(cmd);

    auto handle = consumer.command(cmd);
    // handle은 ACK/완료 결과가 아닌 Source/Session을 가리키는 핸들.

    VectorCommand revised = cmd;
    reviseDesiredSpeed(revised); // 로컬 IDL 메시지 수정만으로 전송되지 않음
    handle.update(revised);      // 여기서 같은 Session으로 갱신 전송

    auto snapshot = handle.get(); // SDK에 최근 관찰된 상태 조회
    auto outcome = handle.wait(umaa::WaitStage::Commanded, 3000);
    (void)snapshot;
    (void)outcome;

    auto cancelRequest = handle.cancel(); // 취소 요청, 실제 정지 완료 아님
    (void)cancelRequest;
    // handle 소멸은 원격 명령의 자동 취소를 의미하지 않음.
}

// 실제 필드를 직접 수정하고 싶다면 다음처럼 사용.
// 공개 필드 타입 매핑 전용; desiredSpeed는 반드시 SpeedRequirementVariantType.
void showPublicFieldVariant(
    MissionVector& consumer,
    const UMAA::Common::Orientation::DirectionRequirementVariantType& direction,
    const UMAA::Common::MaritimeEnumeration::
        DirectionModeEnumModule::DirectionModeEnumType& directionMode,
    const UMAA::Common::Speed::SpeedRequirementVariantType& speed)
{
    VectorCommand cmd;
    cmd.direction = direction;
    cmd.directionMode = directionMode;
    cmd.speed = speed;
    consumer.command(cmd);
}

// 접근자 생성 매핑일 경우 별도 빌드에서 아래 문법을 사용한다.
//   cmd.direction(direction);
//   cmd.directionMode(directionMode);
//   cmd.speed(speed);
//   consumer.command(cmd);

// ==========================================================================
// 3. ReportProvider: publish() / dispose() / 다중 Report / snapshot
// ==========================================================================
class PositionSensor : public umaa::Component {
public:
    SA::GlobalPoseStatusProvider pose;

    void sampleAndSend() {
        PoseReport data;
        fillPoseReport(data); // position, attitude, course, navigationSolution
        pose.publish(data);   // reportGlobalPose를 자동 선택
    }

    void disposePriorInstance(const PoseReport& priorKeyedReport) {
        // 이 객체의 @key source를 사용; 일반 필드는 Dispose에 필수 아님
        pose.dispose(priorKeyedReport);
    }

    void replaceOwnInstances(const std::vector<PoseReport>& reports) {
        pose.publish(reports, umaa::PublishMode::ReplaceOwnedInstances);
    }
};

UMAA_COMPONENT(PositionSensor, m) {
    m.add(&PositionSensor::pose);
}

// 한 Service에서 두 종류 Report 발행: 공통 publish()를 그대로 사용
void publishUVSpecs(EO::UVPlatformSpecsProvider& provider) {
    UMAA::EO::UVPlatformSpecs::UVPlatformSpecsReportType specs;
    UMAA::EO::UVPlatformSpecs::UVPlatformCapabilitiesReportType capabilities;
    fillUVPlatformSpecs(specs);
    fillUVPlatformCapabilities(capabilities);
    provider.publish(specs);
    provider.publish(capabilities);
}

void publishHealth(SO::HealthReportProvider& provider) {
    HealthReport report;
    fillHealthReport(report); // 필수 key인 code/resourceID 등도 설정
    provider.publish(report);
}

// ==========================================================================
// 4. ReportConsumer: 원본 IDL 직접 Hook / Dispose / 추가 ReportInfo
// ==========================================================================
class AutoPilotPose : public SA::GlobalPoseStatusConsumer {
public:
    explicit AutoPilotPose(Navigation& nav) : nav_(nav) {}

    void onReport(const PoseReport& pose) override {
        nav_.setPosition(pose.position); // event.data() 필요 없음
        nav_.setAttitude(pose.attitude);
        nav_.setCourse(pose.course);
    }

    void onDispose(
        const UMAA::Common::IdentifierType& source) override {
        nav_.invalidateSource(source); // 유효한 PoseReport를 가정하지 않음
    }

private:
    Navigation& nav_;
};

// 메타데이터가 필요하다면 위 onReport() 대신 다음 서명을 선택
class DiagnosticPose : public SA::GlobalPoseStatusConsumer {
public:
    void onReport(
        const PoseReport& pose,
        const umaa::ReportInfo& info) override
    {
        observePosition(pose.position);
        traceReceiveTime(info.receivedAt());
    }
};

// 명시적인 임시 구독: Component 등록 시 Manual 모드가 필요
class ManualPoseProbe : public umaa::Component {
public:
    SA::GlobalPoseStatusConsumer pose;
    umaa::Subscription subscription;

    void startProbe() {
        subscription = pose.request(
            [this](const PoseReport& data) {
                showPosition(data.position);
            });
    }
    void stopProbe() { subscription.close(); }

    void inspect(const UMAA::Common::IdentifierType& key) {
        auto latest = pose.getLatest(key); // SDK Optional<PoseReport>
        if (latest.hasValue())
            showPosition(latest.value().position);
    }
};

UMAA_COMPONENT(ManualPoseProbe, m) {
    m.add(&ManualPoseProbe::pose, umaa::SubscriptionMode::Manual);
}

// ==========================================================================
// 5. LargeList: Battery Report + 별도 Element Topic을 SDK가 함께 처리
// ==========================================================================
class BatteryPublisher : public umaa::Component {
public:
    EO::BatteryStatusProvider battery;

    void sendCells() {
        BatteryReport report;
        fillBatteryReport(report); // state 필수, cellsListMetadata SDK 관리

        umaa::LargeList<BatteryCell> cells;
        BatteryCell cell;
        fillBatteryCell(cell);     // voltage/current/temperature 원본 타입
        auto id = cells.add(cell);
        reviseBatteryCell(cell);
        cells.set(id, cell);

        battery.publish(report, cells); // 공통 publish() 오버로드
    }
};

UMAA_COMPONENT(BatteryPublisher, m) {
    m.add(&BatteryPublisher::battery);
}

class BatteryMonitor : public EO::BatteryStatusConsumer {
public:
    void onReport(
        const BatteryReport& report,
        const umaa::LargeList<BatteryCell>& cells) override
    {
        if (!cells.complete()) return;
        inspectBattery(report, cells);
    }
};

// ==========================================================================
// 6. LargeSet + Specialization: ConditionalReport
// ==========================================================================
class ConditionalPublisher : public umaa::Component {
public:
    MM::ConditionalReportProvider conditional;

    void sendDepthCondition() {
        ConditionalReport report;
        fillConditionalReport(report);

        umaa::Specialized<ConditionalBase> item;
        fillConditionalBase(item.base()); // conditionalID / name

        DepthConditional depth;
        fillDepthConditional(depth);      // depth / conditionalOp
        item.set(depth);                  // 실제 DepthConditionalType

        umaa::LargeSet<umaa::Specialized<ConditionalBase>> items;
        items.add(item);
        conditional.publish(report, items);
    }
};

UMAA_COMPONENT(ConditionalPublisher, m) {
    m.add(&ConditionalPublisher::conditional);
}

class ConditionalMonitor : public MM::ConditionalReportConsumer {
public:
    void onReport(
        const ConditionalReport& report,
        const umaa::LargeSet<umaa::Specialized<ConditionalBase>>& items) override
    {
        if (!items.complete()) return;
        for (const auto& item : items.elements()) {
            if (item.is<DepthConditional>()) {
                const DepthConditional& d = item.as<DepthConditional>();
                evaluateDepth(d.depth, d.conditionalOp);
            }
        }
    }
};

// ==========================================================================
// 7. Component에 여러 Service/Role 조합 (AutoPilot 예)
// ==========================================================================
class AutoPilot : public umaa::Component {
public:
    AutoPilotVector vectorControl; // CommandProvider
    AutoPilotPose globalPose;      // ReportConsumer
    SO::HealthReportProvider health;   // ReportProvider
    EO::UVPlatformSpecsProvider specs; // ReportProvider

    AutoPilot(Hardware& hw, Navigation& nav)
        : vectorControl(hw), globalPose(nav) {}
};

UMAA_COMPONENT(AutoPilot, m) {
    m.add(&AutoPilot::vectorControl);
    m.add(&AutoPilot::globalPose);
    m.add(&AutoPilot::health);
    m.add(&AutoPilot::specs);
}

class MissionManager : public umaa::Component {
public:
    MissionVector vector; // CommandConsumer
};

UMAA_COMPONENT(MissionManager, m) {
    m.add(&MissionManager::vector);
}

/*
 * 8. 예외 규칙과 구현 전 검사
 *
 *  - (Service, Role, 실제 IDL Message Type)으로 유일한 Operation을 정한다.
 *    동일 타입이 둘 이상에 매핑되면 command<Op>()/publish<Op>()가 필요.
 *  - 여러 Report의 onDispose(Key)가 동일 C++ 서명이면 OperationTag가 필요.
 *  - Config 같이 Command/Report가 섞인 Service는 둘 이상의 role 기반 클래스를
 *    상속할 수 있고 publish() overload 이름 충돌을 해결해야 한다.
 *  - Request는 일반 Report 구독이며 일회성 RPC가 아니다.
 *  - Report Data와 DDS Dispose/NoWriters는 다르며 onReport(T&)에 유효하지
 *    않은 데이터 객체를 넘기지 않는다.
 *  - CommandHandle(권장 이름) vs CommandCall(현재 v0.8.6 명세의 타입명):
 *    본 예제에서는 'auto handle' 로 받아 명칭 확정 전 소스 차이를 피했다.
 *  - 실제 runtime, generated C++11 headers, DDS 전송, Large Collection
 *    2~3 Topic 조립/원자성 및 recovery 검증은 완료되지 않았다.
 */
