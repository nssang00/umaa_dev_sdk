# v0.8.6 사용자 사용 예제 — Service별 Provider/Consumer 객체 + 공통 함수

> **읽기 전:** [Public API v0.8.6](../../docs/current/UMAA_Component_SDK_Public_API_v0.8.6.md)의 **제안된 SDK facade** 예제다. 원본 UMAA 6.0 IDL의 Data Type 이름은 실제 명칭이지만, `umaa::services::*Provider/Consumer`, `CommandCall`, `m.add` 등은 생성/구현 예정이며 이 파일 자체는 컴파일 검증된 실 SDK가 아니다. RTI C++ 생성 타입의 필드 접근자 `field()` 문법을 가정한다. `fillXXX`/`controller_`/`monitor_`는 사용자 업무 코드 자리다. C++11만 사용한다.

## 0. 사용자가 보게 되는 핵심 다섯 줄

```cpp
vectorConsumer.command(cmd);       // Command 전송
poseProvider.publish(pose);        // Report 전송
healthProvider.publish(health);    // 다른 Service도 동일한 함수 이름
respond(token, umaa::CommandProgress::executing()); // CommandProvider 내부
publish(token, execution);         // CommandProvider 실행 정보
```

`cmd` = `UMAA::MO::GlobalVectorControl::GlobalVectorCommandType`, `pose` = `UMAA::SA::GlobalPoseStatus::GlobalPoseReportType`, `health` = `UMAA::SO::HealthReport::HealthReportType`, `execution` = `UMAA::MO::GlobalVectorControl::GlobalVectorExecutionStatusReportType`.

## 1. 생성된 Service 객체를 Component에 조합

```cpp
namespace MO = umaa::services::MO;
namespace SA = umaa::services::SA;
namespace SO = umaa::services::SO;
namespace EO = umaa::services::EO;
namespace MM = umaa::services::MM;

using VectorCommand =
    UMAA::MO::GlobalVectorControl::GlobalVectorCommandType;
using PoseReport =
    UMAA::SA::GlobalPoseStatus::GlobalPoseReportType;

// 아래 MyVectorProvider / MyPoseConsumer는 2장/4장의 사용자 클래스.
class AutoPilot : public umaa::Component {
public:
    MyVectorProvider vectorControl;       // CommandProvider
    MyPoseConsumer poseSensor;            // ReportConsumer
    SO::HealthReportProvider health;      // ReportProvider

    void publishHealth(const UMAA::SO::HealthReport::HealthReportType& msg) {
        health.publish(msg);
    }
};

// 사용자 정의 Service 객체의 실제 멤버 포인터를 등록
UMAA_COMPONENT(AutoPilot, m) {
    m.add(&AutoPilot::vectorControl);
    m.add(&AutoPilot::poseSensor);
    m.add(&AutoPilot::health);
}
```

Service당 전용 `publishGlobalPose`, `setGlobalVector` 메서드는 생성하지 않는다. 생성된 `*Provider/Consumer`는 공통 기반 클래스를 **상속**한다.

## 2. CommandProvider: onCommand / onCancel / respond / publish

AutoPilot은 실제 `GlobalVectorCommandType`을 받는다. 상속한 `respond()`와 `publish()`를 직접 호출한다.

```cpp
class MyVectorProvider : public MO::GlobalVectorControlProvider {
public:
    umaa::CommandDecision onCommand(
        const VectorCommand& cmd,
        const umaa::CommandInfo& info) override
    {
        // New / Updated / Recovered: 사용자 장비 정책에 따라 다르게 판단
        if (info.delivery() == umaa::CommandDelivery::Recovered) {
            return controller_.isAlreadyRunning(info.token())
                ? umaa::CommandDecision::resume()
                : umaa::CommandDecision::rejected(
                    umaa::CommandReason::ServiceFailed);
        }

        // 실제 IDL 데이터: direction(), directionMode(), speed()
        if (!controller_.validate(
                cmd.direction(), cmd.directionMode(), cmd.speed()))
            return umaa::CommandDecision::rejected(
                umaa::CommandReason::ValidationFailed);

        controller_.start(cmd, info.token());   // 실제 하드웨어 실행 요청
        return umaa::CommandDecision::accepted(); // 실행 완료 아님
    }

    umaa::CancelDecision onCancel(
        const umaa::CancelInfo& info) override
    {
        if (!controller_.requestSafeStop(info.token()))
            return umaa::CancelDecision::cannotCancel();

        return umaa::CancelDecision::deferred();
    }

    // 이 세 함수는 장비가 호출하는 일반 업무 메서드다.
    void onHardwareStarted(const umaa::CommandToken& token) {
        respond(token, umaa::CommandProgress::executing());
    }

    void onHardwareProgress(
        const umaa::CommandToken& token,
        const UMAA::MO::GlobalVectorControl::
            GlobalVectorExecutionStatusReportType& execution)
    {
        // 같은 공통 동사 publish, 두 인자이므로 ExecutionStatus
        publish(token, execution);
    }

    void onHardwareStopped(const umaa::CommandToken& token) {
        // 실제 정지 확인 후에만 보고
        respond(token, umaa::CommandProgress::canceled());
    }

    void onHardwareCompleted(const umaa::CommandToken& token) {
        respond(token, umaa::CommandProgress::completed());
    }

private:
    HardwareController controller_; // 앱에 구현
};
```

실제 `GlobalVectorCommandAckReportType`과 `GlobalVectorCommandStatusType` 발행은 SDK가 한다. `GlobalVectorExecutionStatusReportType` 값은 사용자 장비 코드가 생성한다. `onCancel()`만 성공했다고 CANCELED로 확정하지 않는다.

## 3. CommandConsumer: command / update / cancel / get / wait / Hook

### 3.1 소비만 하면 되는 경우: **상속 구현 없이** 생성 객체 사용

```cpp
class MissionManager : public umaa::Component {
public:
    MO::GlobalVectorControlConsumer vector;  // 그대로 사용 가능

    void navigate() {
        VectorCommand cmd;
        fillVectorCommand(cmd); // 실제 direction/mode/speed 포함

        auto call = vector.command(cmd);   // Operation 추론, CommandCall 반환

        VectorCommand updated = cmd;
        setNewSpeed(updated);
        call.update(updated);              // 기존 활성 Session 갱신

        auto observation = call.get();
        auto commanded = call.wait(umaa::WaitStage::Commanded, 3000);

        auto attempt = call.cancel();       // DDS Dispose 요청
        // 실제 CANCELED/COMPLETED/FAILED는 Status로 별도 확인
    }
};

UMAA_COMPONENT(MissionManager, m) {
    m.add(&MissionManager::vector);
}
```

### 3.2 ACK / Status / ExecutionStatus가 필요하면 Consumer 상속

```cpp
class MyVectorConsumer : public MO::GlobalVectorControlConsumer {
public:
    void onAck(
        const UMAA::MO::GlobalVectorControl::
            GlobalVectorCommandAckReportType& ack) override
    {
        traceCommandReceived(ack.command()); // 실제 원본 Command 포함
    }

    void onStatus(
        const UMAA::MO::GlobalVectorControl::
            GlobalVectorCommandStatusType& status) override
    {
        updateMissionState(status.commandStatus(),
                           status.commandStatusReason());
    }

    void onExecutionStatus(
        const UMAA::MO::GlobalVectorControl::
            GlobalVectorExecutionStatusReportType& execution) override
    {
        if (execution.directionAchieved() && execution.speedAchieved())
            monitor_.markTargetAchieved();
    }

    // 복구 Hook은 선택적: SDK가 자동으로 재실행했다고 가정하지 않음
    umaa::RecoveryDecision onRecoveredCommand(
        const VectorCommand& previous,
        const umaa::CommandInfo& info) override
    {
        return shouldContinue(previous, info)
            ? umaa::RecoveryDecision::Continue
            : umaa::RecoveryDecision::Cancel;
    }

private:
    MissionMonitor monitor_;
};
```

같은 Service의 여러 Command Operation이 동일한 메시지 타입을 사용하면 `command(message)` 또는 Hook 인자 타입만으로 구분하지 않고 `command<Op>(message)` / `OperationTag<Op>`로 명시한다.

## 4. ReportProvider: publish / dispose / snapshot

```cpp
class PositionSensor : public umaa::Component {
public:
    SA::GlobalPoseStatusProvider pose; // 상속한 publish/dispose 사용

    void sample() {
        PoseReport report;
        report.position(sensor_.position());
        report.attitude(sensor_.attitude());
        report.course(sensor_.course());
        report.navigationSolution(sensor_.solution());

        pose.publish(report);              // 일반 Report 발행
    }

    void invalidateSource(const UMAA::Common::IdentifierType& source) {
        PoseReport keyed;
        keyed.source(source);              // key 필드만 필요
        pose.dispose(keyed);               // 같은 메시지 타입으로 Topic/Key 식별
    }

    void replaceOwnedPoses(const std::vector<PoseReport>& current) {
        pose.publish(current,
            umaa::PublishMode::ReplaceOwnedInstances);
        // 자기 소유 인스턴스 외의 Key를 Dispose하지 않음
    }

private:
    PositionHardware sensor_;
};

UMAA_COMPONENT(PositionSensor, m) {
    m.add(&PositionSensor::pose);
}
```

하나의 Service가 여러 Report를 제공하는 경우에도 함수 이름은 `publish` 그대로다.

```cpp
EO::UVPlatformSpecsProvider platform;
UMAA::EO::UVPlatformSpecs::UVPlatformSpecsReportType specs;
UMAA::EO::UVPlatformSpecs::UVPlatformCapabilitiesReportType capabilities;
fillSpecs(specs);
fillCapabilities(capabilities);

platform.publish(specs);
platform.publish(capabilities);
```

## 5. ReportConsumer: onReport 원본 타입 / onDispose / ReportInfo

### 5.1 기본형

```cpp
class MyPoseConsumer : public SA::GlobalPoseStatusConsumer {
public:
    void onReport(const PoseReport& pose) override {
        controller_.updatePosition(pose.position()); // event.data() 불필요
        controller_.updateAttitude(pose.attitude());
    }

    void onDispose(const UMAA::Common::IdentifierType& source) override {
        controller_.invalidatePose(source);
    }
private:
    NavigationController controller_;
};
```

정상 Report의 `onReport`만 데이터 타입을 직접 받고, Dispose의 `onDispose`에는 Key만 전달한다. 유효하지 않은 DDS sample에 존재하지 않는 데이터를 만들지 않는다.

### 5.2 메타데이터가 필요하면 두 인자 Hook을 대신 사용

```cpp
class DiagnosticPoseConsumer : public SA::GlobalPoseStatusConsumer {
public:
    void onReport(
        const PoseReport& pose,
        const umaa::ReportInfo& info) override
    {
        show(pose.position());
        logReceiveTime(info.receivedAt());
    }
};
```

데이터만 받는 `onReport(const T&)`와 메타데이터까지 받는 `onReport(const T&, const ReportInfo&)`는 **한 Operation에서 하나를 선택**한다.

### 5.3 필요할 때만 동적으로 request

```cpp
class PoseProbe : public umaa::Component {
public:
    SA::GlobalPoseStatusConsumer pose;
    umaa::Subscription sub;

    void startMonitoring() {
        sub = pose.request(
            [this](const PoseReport& report) {
                show(report.position());
            });
    }
    void stopMonitoring() { sub.close(); }
};

// 이 객체만 수동 구독 모드로 등록한다.
UMAA_COMPONENT(PoseProbe, m) {
    m.add(&PoseProbe::pose, umaa::SubscriptionMode::Manual);
}
```

일반 Report는 Queryless Request/Reply이며 `request()`는 즉시 RPC 응답 한 번을 요청하는 함수가 아니다. 구독을 시작하고 종료한다.

### 5.4 최신값 캐시

```cpp
auto latest = poseConsumer.getLatest(sourceKey); // C++11 SDK Optional<PoseReport>
if (latest.hasValue())
    usePosition(latest.value().position());
```

캐시 사용은 선택적이며 Dispose된 Key는 최신 유효 데이터로 반환하지 않는다.

## 6. LargeList: EO BatteryStatus

원본 `BatteryReportType`에는 `cellsListMetadata`만 존재한다. Cell Element는 `BatteryReportTypeCellsListElement`라는 **별도 Topic**이다. 따라서 실제 IDL Report는 수정하지 않는다.

```cpp
using Battery = UMAA::EO::BatteryStatus::BatteryReportType;
using Cell = UMAA::EO::BatteryStatus::BatteryCellDataType;

EO::BatteryStatusProvider batteryProvider;

Battery report;
fillBatteryState(report);
umaa::LargeList<Cell> cells;

Cell cell;
cell.voltage(measuredVoltage);
cell.current(measuredCurrent);
cell.temperature(measuredTemperature);
auto elementId = cells.add(cell);

// 식별자 유지, 갱신
cell.voltage(newMeasuredVoltage);
cells.set(elementId, cell);

// 같은 공통 publish. 생성기는 BatteryReportType + CellsList에 매핑.
batteryProvider.publish(report, cells);
```

Consumer의 **복합 메시지용 생성 오버로드**:

```cpp
class BatteryMonitor : public EO::BatteryStatusConsumer {
public:
    void onReport(const Battery& raw, const umaa::LargeList<Cell>& cells) override {
        if (!cells.complete()) return; // Partial과 Complete 구분
        for (const auto& element : cells.elements())
            inspectCell(element.voltage());
    }
};
```

대량 요소에는 동기화/부분 수신/삭제/순서·Element ID 유지 규칙이 필요하다. 두 Topic 이상을 DDS가 원자적으로 전달한다고 가정하지 않는다.

## 7. LargeSet + Specialization: MM ConditionalReport

원본 `ConditionalReportType`은 `conditionalsSetMetadata`를 포함한다. 실제 `ConditionalType` Generalization과 `DepthConditionalType` Specialization은 각각 별도 Topic과 ID/Timestamp 연결을 사용한다.

```cpp
using ConditionalReport = UMAA::MM::ConditionalReport::ConditionalReportType;
using BaseConditional = UMAA::MM::Conditional::ConditionalType;
using DepthConditional = UMAA::MM::Conditional::DepthConditionalType;
using Item = umaa::Specialized<BaseConditional>;

MM::ConditionalReportProvider conditionalProvider;

ConditionalReport report;
Item condition;
condition.base().conditionalID(conditionalId);
condition.base().name(conditionalName);

DepthConditional depth;
depth.conditionalOp(depthComparisonOp);
depth.depth(targetDepth);
condition.set(depth);  // template type deduction: DepthConditional

umaa::LargeSet<Item> items;
items.add(condition);

conditionalProvider.publish(report, items);
```

Consumer의 조립된 요소 Hook:

```cpp
class ConditionalMonitor : public MM::ConditionalReportConsumer {
public:
    void onReport(
        const ConditionalReport& report,
        const umaa::LargeSet<Item>& items) override
    {
        if (!items.complete()) return;
        for (const auto& item : items.elements()) {
            if (!item.ready()) continue;
            if (item.is<DepthConditional>()) {
                const DepthConditional& depth = item.as<DepthConditional>();
                evaluateDepth(depth.conditionalOp(), depth.depth());
            }
        }
    }
};
```

SDK는 세 종류 Topic을 합치고 `listID/setID`, `elementID`, `specializationReferenceID`, Timestamp의 일관성을 관리한다. Metadata만 수신한 상태와 완성된 객체는 구분해야 한다.

## 8. 적용 범위와 미검증 사항

이 예제는 **사용자 API의 원하는 사용감**을 정의한다. `m.add` 등록 방식, Service별 자동 생성 virtual Hook, type→Operation 자동 매핑, 동적 구독, 복합 Report 오버로드의 실제 C++11 컴파일 검증 및 DDS 실행은 아직 하지 않았다. 원본 IDL 필드와 Operation 명칭의 매핑에는 ICD/IDL 기반 생성기를 적용해야 하며, 일부 타입 충돌 시 생성기는 추가 tag 인자를 요구할 수 있다. 원본 IDL/생성 클래스는 아직 repository에 완전 구현된 것이 아니다.

> 각 Component 인스턴스의 Service 멤버는 Component보다 오래 살아 있으면 안 된다. `const T&`의 보존 수명은 Callback 동안으로 제한하며 필요 시 복사한다.
