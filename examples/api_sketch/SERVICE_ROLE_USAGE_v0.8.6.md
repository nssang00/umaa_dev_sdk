# UMAA SDK Public API v0.8.6 — 네 역할 사용 예제 (2026-11-20 설계 갱신)

> **SDK 구현 전 디자인 스케치.** 사용자 기본형은 Generator가 만든 `<ServiceName>Provider/Consumer` 객체를 **Component 멤버로 보유**하고 `onCommand/onReport` Callback을 등록하는 것이다. 추가 사용자 상속 클래스 및 `override`는 필수가 아니다. C++11과 원본 UMAA ICD 6.0 IDL 타입을 대상으로 한다.

실제 [AutoPilot 14 Service 전체 예제](AUTOPILOT_14_SERVICE_USAGE_v0.8.6.cpp)와 [실제 Config의 Command + Report 혼합 예제](CONFIG_MIXED_SERVICE_v0.8.6.cpp)가 기본 사용 예제다. 이전 [상속형 서비스 사용자 클래스 스케치](COMPLETE_FOUR_ROLE_USAGE_v0.8.6.cpp)는 **현재 기본 API가 아닌 이전 대안**이다.

## 1. 생성된 객체 하나가 Service 제공/소비 방향을 표현

```cpp
namespace MO = umaa::services::MO;
namespace SA = umaa::services::SA;
namespace EO = umaa::services::EO;
namespace SO = umaa::services::SO;
namespace MM = umaa::services::MM;

class AutoPilot : public umaa::Component {
public:
    // Provider (실제 AutoPilot Services)
    MO::PrimitiveDriverControlProvider primitiveDriver;
    MO::GlobalVectorControlProvider globalVector;
    MO::GlobalHoverControlProvider globalHover;
    MM::ActiveConstraintsControlProvider activeConstraints;
    EO::UVPlatformSpecsProvider platformSpecs;
    SO::LogReportProvider logReport;
    SO::HealthReportProvider healthReport;

    // Consumer (실제 AutoPilot Services)
    SA::GlobalPoseStatusConsumer globalPose;
    SA::VelocityStatusConsumer velocity;
    SA::SpeedStatusConsumer speed;
    SA::WindStatusConsumer wind;
    SA::WaterCurrentStatusConsumer waterCurrent;
    MM::MissionPlanConstraintControlConsumer planConstraint;
    MM::ConditionalReportConsumer conditionals;
};
```

14개 구성과 `m.add(...)` 등록, 실제 Callback 본문은 [AutoPilot 파일](AUTOPILOT_14_SERVICE_USAGE_v0.8.6.cpp)에 있다. `Config/Control/Specs/Status`는 Service 분류이고 `Provider/Consumer`는 방향이다.

## 2. CommandProvider: onCommand Callback / respond / publish

AutoPilot은 `GlobalVectorControl` 명령을 **받는 Provider**다.

```cpp
using VectorCmd = UMAA::MO::GlobalVectorControl::GlobalVectorCommandType;
using VectorExecution = UMAA::MO::GlobalVectorControl::
    GlobalVectorExecutionStatusReportType;

// AutoPilot 생성자에서
globalVector.onCommand(
    [this](const VectorCmd& cmd, const umaa::CommandInfo& info) {
        // 필수 direction, directionMode, speed는 원본 IDL 타입 그대로
        if (!hardware_.validate(cmd))
            return umaa::CommandDecision::rejected(
                umaa::CommandReason::ValidationFailed);
        hardware_.execute(cmd, info.token());
        return umaa::CommandDecision::accepted();
    });

globalVector.onCancel(
    [this](const umaa::CancelInfo& info) {
        hardware_.requestStop(info.token());
        return umaa::CancelDecision::deferred();
    });

// 실제 하드웨어 이벤트를 확인한 후
globalVector.respond(token, umaa::CommandProgress::executing());
globalVector.publish(token, executionStatus); // VectorExecution
globalVector.respond(token, umaa::CommandProgress::canceled()); // 정지 확인
```

`accepted`는 접수만 뜻하며 완료가 아니다. Cancel 요청 수락과 실제 CANCELED 상태도 별개다.

## 3. CommandConsumer: command / CommandHandle

AutoPilot이 **명령을 보내는 실제 Service**는 `MissionPlanConstraintControl`이다.

```cpp
using Add = UMAA::MM::MissionPlanConstraintControl::
    MissionPlanConstraintAddCommandType;
using Delete = UMAA::MM::MissionPlanConstraintControl::
    MissionPlanConstraintDeleteCommandType;

Add add;
fillConstraintAdd(add); // 원본 IDL 필드 constraint 등을 채움

planConstraint.command(add); // 일반 전송: Handle 반환값 무시

auto handle = planConstraint.command(add); // 새 별도의 Command Session
Add revised = add;
reviseConstraint(revised);
handle.update(revised);    // 같은 Session의 갱신 전송
auto current = handle.get();
handle.cancel();           // 비동기 취소 요청

Delete remove;
fillConstraintDelete(remove);
planConstraint.command(remove); // 타입에 따라 Delete Operation 추론
```

`planConstraint.onStatus(lambda(const MissionPlanConstraintAddCommandStatusType&))` 등 상태 Hook도 원본 IDL 타입으로 등록한다. `CommandHandle<T>`는 ACK/완료 결과가 아닌 로컬 Source/Session 추적 객체이며, 반환값을 버려도 명령이 자동 취소되지 않는다. 한 예제에서 `command(add)`를 두 번 호출하는 것은 **독립적인 두 명령**이다.

## 4. ReportProvider: publish / dispose

```cpp
using Specs = UMAA::EO::UVPlatformSpecs::UVPlatformSpecsReportType;
using Caps = UMAA::EO::UVPlatformSpecs::UVPlatformCapabilitiesReportType;
using Health = UMAA::SO::HealthReport::HealthReportType;

Specs specs;
Caps capabilities;
Health health;
fillSpecs(specs);
fillCapabilities(capabilities);
fillHealth(health);

platformSpecs.publish(specs);
platformSpecs.publish(capabilities); // 동일 Service의 다른 Operation
healthReport.publish(health);
healthReport.dispose(health);       // 실제 IDL의 source/code/resourceID Key 사용
```

`dispose`는 원본 IDL의 Key 필드를 기반으로 처리하며, 유효하지 않은 Report 데이터까지 채울 필요는 없다. SDK는 자기 소유 Key를 추적하고 DDS Dispose를 처리한다.

## 5. ReportConsumer: onReport / onDispose / 수동 구독

```cpp
using Pose = UMAA::SA::GlobalPoseStatus::GlobalPoseReportType;

// AutoPilot 생성자에서
globalPose.onReport([this](const Pose& pose) {
    navigation_.update(pose);  // event.data() 없이 실제 IDL 객체
});
globalPose.onDispose(
    [this](const UMAA::Common::IdentifierType& source) {
        navigation_.invalidate(source);
    });
```

Callback은 기본적으로 Component 시작부터 자동 구독된다. `ReportInfo`가 필요하면 `onReport(const Pose&, const ReportInfo&)` Callback으로 교체한다. Dispose 수신 시 유효한 `Pose` 객체를 합성해서 넘기지 않는다.

동적 구독이 필요할 때는 **자동 구독을 끈 Service 객체**만 수동 등록한다.

```cpp
UMAA_COMPONENT(PoseProbe, m) {
    m.add(&PoseProbe::pose, umaa::SubscriptionMode::Manual);
}

// Component 시작 이후의 사용자 메서드에서
subscription = pose.request(
    [this](const Pose& sample) { observe(sample); });
subscription.close(); // 더 이상 수신하지 않음
```

일반 UMAA Report의 `request()`는 단발성 RPC가 아니라 Queryless Subscription 시작이다.

## 6. LargeSet + Specialization (실제 AutoPilot Consumer)

원본 `ConditionalReportType`에는 `conditionalsSetMetadata`가 있으며, Condition Element와 Specialization은 별도 Topic을 사용한다. 원본 Report 구조체에 가상의 `conditionals` 필드를 추가하지 않는다.

```cpp
using Raw = UMAA::MM::ConditionalReport::ConditionalReportType;
using Base = UMAA::MM::Conditional::ConditionalType;
using Items = umaa::LargeSet<umaa::Specialized<Base>>;

conditionals.onReport(
    [this](const Raw& report, const Items& items) {
        if (!items.complete()) return;
        logic_.receivedConditionals(report, items);
    });
```

SDK는 Topic을 조립하고 불완전한 상태·삭제·시간 차이를 구분해야 한다. DDS가 여러 Topic의 원자적 전달을 제공한다고 가정하지 않는다.

별도 일반 ReportProvider의 LargeList 예로는 `EO::BatteryStatus::BatteryReportType`와 `LargeList<BatteryCellDataType>`을 `batteryProvider.publish(report, cells)`로 발행하는 API가 있다. 이는 **AutoPilot 공식 14개 Service에 포함되지 않는 확장 사례**다.

## 7. 실제 Config Service: Command/Response + Queryless Report

`SA::GlobalPoseConfig`의 실제 UMAA 6.0 IDL에는 `GlobalPoseConfigCommandType`과 `GlobalPoseConfigReportType`이 둘 다 있다. 생성된 `SA::GlobalPoseConfigProvider` 한 객체는 `onCommand()`와 `publish(configReport)`를 모두 지원하며, 생성된 `SA::GlobalPoseConfigConsumer` 한 객체는 `command(configCommand)`와 `onReport()`를 함께 지원한다.

```cpp
using ConfigCmd = UMAA::SA::GlobalPoseConfig::GlobalPoseConfigCommandType;
using ConfigReport = UMAA::SA::GlobalPoseConfig::GlobalPoseConfigReportType;

SA::GlobalPoseConfigProvider provider; // 실제 원본 Config Service
provider.onCommand([](const ConfigCmd& cmd,
                      const umaa::CommandInfo& info) {
    processInitialPose(cmd, info.token());
    return umaa::CommandDecision::accepted();
});
provider.publish(configReport); // Queryless Report, CommandStatus와 별도

SA::GlobalPoseConfigConsumer consumer;
consumer.onReport([](const ConfigReport& report) {
    observeConfig(report);
});
auto handle = consumer.command(configCmd);
handle.cancel();
```

주의: 실제 `GlobalPoseConfigReportType`에는 `source`와 `timeStamp`가 있으며, `position`이나 `attitude`를 갖는다고 가정하지 않는다. AutoPilot 공식 14개에 없는 Service다. [정확한 타입과 전체 사용 예제](CONFIG_MIXED_SERVICE_v0.8.6.cpp).

## 8. 내부 구조와 검증 범위

`umaa::Service`가 Component 바인딩/생명주기를 갖고, 생성된 `<ServiceName>Provider/Consumer`가 이를 한 번 상속한다. 내부의 `CommandProvider<S>`, `CommandConsumer<S>`, `ReportProvider<S>`, `ReportConsumer<S>`는 필요한 만큼 **멤버로 합성**한다. Config처럼 두 패턴이 있으면 둘 다 포함한다. 사용자 API에는 공통 동사만 위임한다.

**생성 및 실환경 검증 전:** 위 파일은 SDK 설계 스케치이며 실제 RTI 생성 헤더 C++11 컴파일, DDS 통신, Command 상태/취소/복구, Large Collection 조립은 아직 검증되지 않았다. 원본 IDL 멤버 접근은 공개 변수 또는 접근자 문법 중 실제 생성 매핑을 따른다.
