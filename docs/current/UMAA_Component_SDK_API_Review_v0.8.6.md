# UMAA Component SDK — Public API Review

> **사용자 설명·설계 검토용 | v0.8.6 기반 | 2026-11-20 | C++11**  
> **문서 성격:** 제품 매뉴얼·ICD 규범 문서가 아닌 **제안된 공개 API의 검토본**입니다. 아래 예제는 실제 UMAA 6.0 IDL 타입명을 사용하지만, `umaa::services::*Provider/Consumer` 및 SDK 런타임은 **생성·구현 예정**입니다. 전체 코드가 RTI 생성 헤더와 컴파일되거나 DDS 상호운용이 검증되었다는 뜻은 아닙니다.

## 이 문서의 목적

이 문서만 보고 응용 개발자가 다음 네 질문에 답할 수 있게 하는 것이 목표입니다.

1. AutoPilot에서 어떤 Service 객체를 선언해야 하나?
2. 명령을 보내거나 받으려면 어떤 API를 호출하나?
3. 상태 Report를 보내거나 받으려면 어떤 API를 호출하나?
4. Config처럼 Command와 Report가 섞인 Service도 동일한 API를 쓸 수 있나?

**한 줄 결론:** **Generator가 만든 `<ServiceName>Provider` 또는 `<ServiceName>Consumer`를 Component에 보유하고, Callback을 등록하거나 `command()`·`publish()`·`respond()`를 호출합니다.**

---

## 1. 30초 사용 예제

아래 네 객체는 **실제 AutoPilot 역할**에서 가져온 예제입니다.

```cpp
namespace MO = umaa::services::MO;
namespace SA = umaa::services::SA;
namespace EO = umaa::services::EO;
namespace MM = umaa::services::MM;

using VectorCmd = UMAA::MO::GlobalVectorControl::GlobalVectorCommandType;
using Pose = UMAA::SA::GlobalPoseStatus::GlobalPoseReportType;
using Specs = UMAA::EO::UVPlatformSpecs::UVPlatformSpecsReportType;
using AddConstraint = UMAA::MM::MissionPlanConstraintControl::
    MissionPlanConstraintAddCommandType;

class AutoPilot : public umaa::Component {
public:
    MO::GlobalVectorControlProvider vector;       // Command를 받는다
    SA::GlobalPoseStatusConsumer pose;             // Report를 받는다
    EO::UVPlatformSpecsProvider specs;             // Report를 보낸다
    MM::MissionPlanConstraintControlConsumer plan; // Command를 보낸다

    AutoPilot() {
        vector.onCommand([this](const VectorCmd& cmd,
                                const umaa::CommandInfo& info) {
            startVectorHardware(cmd, info.token()); // 응용 코드
            return umaa::CommandDecision::accepted();
        });
        pose.onReport([this](const Pose& report) {
            updateNavigation(report); // 응용 코드
        });
    }

    void reportSpecs(const Specs& report) { specs.publish(report); }
    void addConstraint(const AddConstraint& cmd) { plan.command(cmd); }
};

UMAA_COMPONENT(AutoPilot, m) {
    m.add(&AutoPilot::vector);
    m.add(&AutoPilot::pose);
    m.add(&AutoPilot::specs);
    m.add(&AutoPilot::plan);
}
```

> **예제 표기:** `startVectorHardware`, `updateNavigation` 같은 함수는 사용자 업무 로직을 나타내는 자리 표시자입니다. `UMAA_COMPONENT` 및 `m.add()` 역시 SDK에서 구현할 등록 API의 제안입니다.

---

## 2. 설계 원칙과 이름

| 구분 | 의미 | 예 |
|---|---|---|
| **Service 기능 분류** | ICD에서 어떤 기능을 다루는가 | Config / Control / Specs / Status·Report |
| **Component 참여 역할** | 누가 해당 Service를 제공·소비하는가 | `Provider`, `Consumer` |
| **Operation 통신 패턴** | 무엇을 교환하는가 | Command/Response, Queryless Request/Reply |
| **원본 IDL Data Type** | 사용자가 작성·수신하는 업무 메시지 | `UMAA::SA::GlobalPoseStatus::GlobalPoseReportType` |
| **생성 SDK 클래스** | 사용자가 직접 선언하는 Service 객체 | `SA::GlobalPoseStatusConsumer` |

따라서 **`ControlService`·`ConfigService`·`ReportService`를 서로 다른 공개 기반 클래스로 두지 않습니다.** Config Service에도 Command와 Report가 함께 존재할 수 있으므로 **Operation 목록을 보고 기능을 생성**해야 합니다.

### 내부 구조 — 사용자에게는 숨긴다

```text
AutoPilot : umaa::Component
  ├─ GlobalVectorControlProvider : umaa::Service
  │    └─ [내부 합성] CommandProvider<GlobalVectorControl>
  └─ GlobalPoseStatusConsumer : umaa::Service
       └─ [내부 합성] ReportConsumer<GlobalPoseStatus>

GlobalPoseConfigProvider : umaa::Service
  ├─ [내부 합성] CommandProvider<GlobalPoseConfig>
  └─ [내부 합성] ReportProvider<GlobalPoseConfig>
```

모든 생성 Service 객체는 `umaa::Service`를 **한 번 상속**하고, 내부에서는 필요한 역할 객체를 **합성·위임(Composition)** 합니다. 개발자는 일반적으로 `CommandProvider<S>` 등의 공통 템플릿을 직접 선언하거나 Service별 클래스를 추가 상속하지 않습니다.

---

## 3. 사용자에게 보여 줄 Public API 목록

함수명은 **제안된 SDK API**입니다. 모든 Service가 아래의 모든 API를 갖는다는 뜻은 아닙니다. 실제 Operation이 지원하는 기능만 Generator가 노출합니다.

### Component와 Service

| API | 누가 사용하나 | 의미 |
|---|---|---|
| `class MyComponent : public umaa::Component` | 모든 Component 개발자 | 여러 Service 역할 객체의 소유자 |
| `<ServiceName>Provider` | 서비스를 제공할 때 | 실제 Service에 맞춘 생성 객체 |
| `<ServiceName>Consumer` | 서비스를 소비할 때 | 실제 Service에 맞춘 생성 객체 |
| `m.add(&Component::serviceMember)` | Component 구성 | 역할 객체 등록·수명주기 연결 |

### Command/Response

| 객체 / API | 사용 목적 | 중요한 의미 |
|---|---|---|
| Provider `onCommand(handler)` | 신규·갱신·복구 명령 처리 | 원본 IDL Command와 `CommandInfo` 전달 |
| Provider `onCancel(handler)` | 취소 요청 처리 | 요청 수락과 실제 장비 중단은 별개 |
| Provider `respond(token, progress)` | 실행 상태 알림 | 접수/실행/완료/실패/취소의 합법 전이 반영 |
| Provider `publish(token, executionIDL)` | 실행 상세값 발행 | 해당 Service에 ExecutionStatus Operation이 있는 경우 |
| Consumer `command(commandIDL)` | **새 Command 전송** | `CommandHandle<Message>` 반환; 무시 가능 |
| `handle.update(updatedIDL)` | 기존 명령 변경 | **같은 Source/Session**으로 갱신 전송 |
| `handle.cancel()` | 취소 요청 | 완료·중단이 확정된 것은 아님 |
| `handle.get()` | 로컬 상태 확인 | 최종 ACK/결과와 동일하지 않음 |
| `handle.wait(stage, timeout)` | 특정 상태까지 대기 | 블로킹; timeout은 원격 FAILED와 다름 |
| Consumer `onAck(handler)` | ACK 관찰 | 실제 ACK IDL 타입을 받음 |
| Consumer `onStatus(handler)` | 상태 관찰 | 실제 CommandStatus IDL 타입을 받음 |
| Consumer `onExecutionStatus(handler)` | 진행 상세 관찰 | 해당 Operation이 정의한 경우 |
| Consumer `onRecoveredCommand(handler)` | 잔존 명령 복구 정책 | 선택적·구체 서명은 추가 검증 필요 |

### Queryless Report

| 객체 / API | 사용 목적 | 중요한 의미 |
|---|---|---|
| Provider `publish(reportIDL)` | Report 발행·갱신 | 원본 IDL 타입으로 Operation 추론 |
| Provider `dispose(reportWithKey)` | 유효하지 않은 인스턴스 삭제 | DDS Key를 기준으로 처리 |
| Provider `publish(snapshot, ReplaceOwnedInstances)` | 소유 인스턴스 집합 교체 | 고급/선택적; 타 Provider 소유 인스턴스 삭제 금지 |
| Consumer `onReport(handler)` | 정상 Report 수신 | `const OriginalIDL&` 직접 전달 |
| Consumer `onDispose(handler)` | Report 인스턴스 무효화 알림 | 유효한 Report 객체가 아니라 Key를 받음 |
| Consumer `request(handler)` | 수동 구독 시작 | **일회성 RPC가 아니라 Queryless 구독** |
| `Subscription.close()` | 수동 구독 해제 | 등록한 수신을 종료 |
| Consumer `getLatest(key)` | 최신 유효 Report 조회 | SDK 캐시가 있는 경우 |
| `onReport(const IDL&, const ReportInfo&)` | 수신 메타데이터가 필요할 때 | 일반 `const IDL&` Callback의 선택적 대안 |

### 복합 데이터(해당 Service에만)

| API | 사용자 관점 |
|---|---|
| `LargeList<T>`, `LargeSet<T>` | Element 컬렉션 추가·수정·삭제, 완성 상태 확인 |
| `provider.publish(originalIDL, collection)` | 원본 Report와 분리 Element Topic을 SDK가 동기화하여 전송 |
| `consumer.onReport(handler(const IDL&, const Collection&))` | 조립 완료된 원본 Report와 컬렉션 함께 수신 |
| `Specialized<Base>::set(detail)` | Generalization에 실제 Specialization 지정 |
| `item.is<Detail>()`, `item.as<Detail>()` | 수신한 Specialization을 타입 안전하게 검사·접근 |

---

## 4. 실제 AutoPilot 14개 Service

UMAA Component Definitions v1.0 §3.1 기준의 역할 목록입니다. 이 **방향은 고정 사실**이고, SDK C++ 클래스는 해당 역할에 맞춘 생성물입니다.

| 방향 | 도메인 | 실제 Service | SDK 객체 |
|---|---|---|---|
| Provider | MO | PrimitiveDriverControl | `MO::PrimitiveDriverControlProvider` |
| Provider | MO | GlobalVectorControl | `MO::GlobalVectorControlProvider` |
| Provider | MO | GlobalHoverControl | `MO::GlobalHoverControlProvider` |
| Provider | MM | ActiveConstraintsControl | `MM::ActiveConstraintsControlProvider` |
| Provider | EO | UVPlatformSpecs | `EO::UVPlatformSpecsProvider` |
| Provider | SO | LogReport | `SO::LogReportProvider` |
| Provider | SO | HealthReport | `SO::HealthReportProvider` |
| Consumer | SA | GlobalPoseStatus | `SA::GlobalPoseStatusConsumer` |
| Consumer | SA | VelocityStatus | `SA::VelocityStatusConsumer` |
| Consumer | SA | SpeedStatus | `SA::SpeedStatusConsumer` |
| Consumer | SA | WindStatus | `SA::WindStatusConsumer` |
| Consumer | SA | WaterCurrentStatus | `SA::WaterCurrentStatusConsumer` |
| Consumer | MM | MissionPlanConstraintControl | `MM::MissionPlanConstraintControlConsumer` |
| Consumer | MM | ConditionalReport | `MM::ConditionalReportConsumer` |

**유의:** AutoPilot은 `GlobalVectorControl`의 **Provider**이고, `GlobalPoseStatus`의 **Consumer**입니다. AutoPilot이 `GlobalVectorControlConsumer.command()`를 호출하는 예제는 **외부 Mission Executor 쪽 예제**로 분리해야 합니다.

### AutoPilot의 전체 Service 선언

```cpp
class AutoPilot : public umaa::Component {
public:
    // Provider 7
    MO::PrimitiveDriverControlProvider primitiveDriver;
    MO::GlobalVectorControlProvider globalVector;
    MO::GlobalHoverControlProvider globalHover;
    MM::ActiveConstraintsControlProvider activeConstraints;
    EO::UVPlatformSpecsProvider platformSpecs;
    SO::LogReportProvider logReport;
    SO::HealthReportProvider healthReport;

    // Consumer 7
    SA::GlobalPoseStatusConsumer globalPose;
    SA::VelocityStatusConsumer velocity;
    SA::SpeedStatusConsumer speed;
    SA::WindStatusConsumer wind;
    SA::WaterCurrentStatusConsumer waterCurrent;
    MM::MissionPlanConstraintControlConsumer planConstraint;
    MM::ConditionalReportConsumer conditionals;
};
```

기본 사용자 코드는 **객체 선언 → Callback 등록 → `m.add()` 등록 → 일반 업무 중 Action 호출** 순서입니다. 위 14개에 대한 실제 등록·Callback 전체 스케치는 아래 원본 링크를 참조합니다.

---

## 5. 네 역할을 사용하는 실제 코드

### 5.1 CommandProvider — AutoPilot의 GlobalVectorControl

```cpp
using VectorCmd = UMAA::MO::GlobalVectorControl::GlobalVectorCommandType;
using VectorExecution = UMAA::MO::GlobalVectorControl::
    GlobalVectorExecutionStatusReportType;

// AutoPilot 생성자에서

globalVector.onCommand([this](const VectorCmd& cmd,
                              const umaa::CommandInfo& info) {
    if (!controller_.accept(cmd))
        return umaa::CommandDecision::rejected(
            umaa::CommandReason::ValidationFailed);
    controller_.start(cmd, info.token());
    return umaa::CommandDecision::accepted(); // 원격 실행 완료 아님
});

globalVector.onCancel([this](const umaa::CancelInfo& info) {
    controller_.requestStop(info.token());
    return umaa::CancelDecision::deferred();
});

// 실제 장비 이벤트를 확인한 뒤 (AutoPilot 멤버 함수에서)
globalVector.respond(token, umaa::CommandProgress::executing());
globalVector.publish(token, executionStatus); // VectorExecution
globalVector.respond(token, umaa::CommandProgress::completed());
```

**핵심:** `onCommand()`의 `accepted()`는 **접수 판단**이고, `respond(...completed())`는 **실제 완료 확인**입니다. ACK/Status 송수신·Session 연결은 SDK가 담당하는 설계입니다.

### 5.2 CommandConsumer — AutoPilot의 MissionPlanConstraintControl

```cpp
using AddConstraint = UMAA::MM::MissionPlanConstraintControl::
    MissionPlanConstraintAddCommandType;
using DeleteConstraint = UMAA::MM::MissionPlanConstraintControl::
    MissionPlanConstraintDeleteCommandType;

AddConstraint add;
fillConstraintAdd(add); // 응용 코드: 실제 IDL 필수값 채우기

// 기본형: 새 명령 제출
planConstraint.command(add);

// 관리형: 위 호출과 다른 *새 명령*이며 Handle 보관
AddConstraint managed;
fillConstraintAdd(managed);
auto handle = planConstraint.command(managed);

AddConstraint revised = managed;
reviseConstraint(revised); // 로컬 변수만 바뀜
handle.update(revised);    // 이때 같은 Session으로 실제 갱신
handle.cancel();           // 비동기 취소 요청

DeleteConstraint remove;
fillConstraintDelete(remove);
planConstraint.command(remove);  // 메시지 타입으로 Operation 선택
```

**`CommandHandle`의 의미:** 응답 데이터나 최종 결과가 아니라 **기존 Command Session을 가리키는 로컬 추적 핸들**입니다. `command(cmd);`의 반환값을 무시해도 전송은 동일하며, Handle을 파괴해도 자동 취소되지 않습니다. 기존 명령 취소는 항상 실제 상태 확인과 분리됩니다.

### 5.3 ReportProvider — AutoPilot의 UVPlatformSpecs / HealthReport

```cpp
using Specs = UMAA::EO::UVPlatformSpecs::UVPlatformSpecsReportType;
using Capabilities = UMAA::EO::UVPlatformSpecs::
    UVPlatformCapabilitiesReportType;
using Health = UMAA::SO::HealthReport::HealthReportType;

Specs specs;
Capabilities capabilities;
Health health;
fillPlatformSpecs(specs);
fillCapabilities(capabilities);
fillHealth(health);

platformSpecs.publish(specs);
platformSpecs.publish(capabilities); // 같은 Service, 다른 Report 타입
healthReport.publish(health);
healthReport.dispose(health);        // Key를 사용해 Dispose
```

### 5.4 ReportConsumer — AutoPilot의 GlobalPoseStatus

```cpp
using Pose = UMAA::SA::GlobalPoseStatus::GlobalPoseReportType;

// AutoPilot 생성자에서
globalPose.onReport([this](const Pose& pose) {
    navigation_.update(pose);  // 원본 IDL 타입을 직접 받음
});

globalPose.onDispose(
    [this](const UMAA::Common::IdentifierType& source) {
        navigation_.invalidate(source);
    });
```

Report는 Queryless 구독입니다. 기본 `onReport()` Callback 등록은 Component 생명주기에 묶인 구독을 의미하며 `onRequest()`/`onQuery()` Hook을 기본 노출하지 않습니다. 메타데이터가 필요하면 `onReport([](const Pose&, const ReportInfo&){...})`형을 선택할 수 있습니다.

---

## 6. Config는 두 패턴을 함께 지원한다

실제 UMAA 6.0 `SA::GlobalPoseConfig`에는 `GlobalPoseConfigCommandType`과 `GlobalPoseConfigReportType`이 있습니다. **하나의 생성 Provider가 Command 처리와 Report 발행을 모두 제공**하고, 하나의 Consumer가 Command 전송과 Report 수신을 모두 제공합니다.

> **중요:** `GlobalPoseConfig`는 공식 AutoPilot 14개에 포함되지 않는 **별도 Config 사례**입니다. 또한 실제 `GlobalPoseConfigReportType`은 `source` 및 `timeStamp`만 정의하며 `position`·`attitude`를 임의로 포함시키지 않습니다.

```cpp
namespace SA = umaa::services::SA;
using ConfigCmd = UMAA::SA::GlobalPoseConfig::GlobalPoseConfigCommandType;
using ConfigReport = UMAA::SA::GlobalPoseConfig::GlobalPoseConfigReportType;

// Config Provider — 하나의 객체, 두 역할
SA::GlobalPoseConfigProvider provider;
provider.onCommand([](const ConfigCmd& cmd,
                      const umaa::CommandInfo& info) {
    applyInitialPose(cmd, info.token()); // 사용자 업무 로직
    return umaa::CommandDecision::accepted();
});
ConfigReport configReport;
fillConfigReport(configReport);      // source/timeStamp 등 실제 IDL 필드
provider.publish(configReport); // 일반 Report 발행

// Config Consumer — 하나의 객체, 두 역할
SA::GlobalPoseConfigConsumer consumer;
consumer.onReport([](const ConfigReport& report) {
    observeConfig(report);
});
ConfigCmd configCmd;
fillConfigCmd(configCmd);
auto handle = consumer.command(configCmd);
handle.cancel();
```

**내부 구현 방향(공개 API 아님):** `GlobalPoseConfigProvider : umaa::Service`가 내부에 `CommandProvider<GlobalPoseConfig>`와 `ReportProvider<GlobalPoseConfig>`를 합성합니다. Consumer도 두 내부 역할을 합성합니다. 사용자가 다중 상속을 처리하지 않습니다.

---

## 7. 원본 IDL 데이터 타입과 복합 데이터

### 원본 IDL 타입 그대로 사용

사용자 업무 인자는 SDK 래퍼의 `Op::Data`가 아닌 원본 `UMAA::...` 타입입니다. C++ IDL 생성 결과는 **공개 멤버 변수** 또는 **접근자 함수** 방식일 수 있어 한 방식으로 단정하지 않습니다.

```cpp
// 공개 멤버를 생성한 경우
cmd.speed = speedRequirement;

// 접근자를 생성한 경우 — 위 코드와 같은 빌드에서 함께 사용하지 않음
cmd.speed(speedRequirement);
```

`GlobalVectorCommandType.speed`는 실제 IDL의 `SpeedRequirementVariantType`입니다. 단순 `double` 값을 곧바로 대입할 수 있다고 가정하지 않습니다. SDK 관리 `source`·`destination`·`sessionID`·`timeStamp`는 별도 정책 및 전송 복사본에서 처리하도록 설계합니다.

### Large Collection

원본 Report에 없는 필드를 만들지 않습니다. 예를 들어 `EO::BatteryStatus`는 **AutoPilot 14개 외부 사례**입니다.

```cpp
using Battery = UMAA::EO::BatteryStatus::BatteryReportType;
using Cell = UMAA::EO::BatteryStatus::BatteryCellDataType;

Battery battery;
umaa::LargeList<Cell> cells;
Cell cell;
fillCell(cell);
cells.add(cell);
batteryProvider.publish(battery, cells); // 원본 Report + 별도 Element
```

AutoPilot의 실제 `MM::ConditionalReport`는 `conditionalsSetMetadata`와 별도 Element·Specialization Topic을 사용합니다.

```cpp
using ConditionalReport = UMAA::MM::ConditionalReport::ConditionalReportType;
using Base = UMAA::MM::Conditional::ConditionalType;
using Items = umaa::LargeSet<umaa::Specialized<Base>>;

conditionals.onReport([this](const ConditionalReport& report,
                              const Items& items) {
    if (!items.complete()) return;
    logic_.receivedConditionals(report, items);
});
```

`LargeList`·`LargeSet`의 Element ID, 순서, Dispose, Complete/Incomplete 상태 및 Specialization 연결은 SDK가 관리할 기능입니다. **여러 DDS Topic의 원자적 전송을 보장한다고 가정하지 않습니다.**

---

## 8. API 동작에서 오해하기 쉬운 점

| 코드 또는 이벤트 | 의미 | 의미하지 않는 것 |
|---|---|---|
| `consumer.command(cmd)` 반환 | 로컬 Command 제출 후 Handle 생성 | 원격 ACK·명령 수락·실행 완료 |
| `handle.update(updated)` | 기존 Session에 변경 전송 | 새로운 독립 명령 생성 |
| `handle.cancel()` | 취소 요청 제출 | 실제 기계 정지·`CANCELED` 확정 |
| `onCommand()`의 `accepted()` | Provider의 접수 결정 | 작업 완료 |
| `handle.wait(stage, timeout)` | 호출 스레드에서 상태 관찰 대기 | C++11 `await`, 원격 FAILED 판정 |
| `onReport(const T&)` | 유효한 Report 데이터 수신 | Dispose된 인스턴스 데이터 보장 |
| `request(handler)` | Queryless 수동 구독 생성 | Query RPC 단발성 호출 |
| `publish(original, collection)` | 복합 데이터 발행 요청 | DDS 다중 Topic 원자성 보장 |

**C++11에는 `co_await`가 없습니다.** 이 API는 Callback과 필요 시 `wait()`를 기본으로 하고, 다른 언어 또는 후속 C++ 버전에서는 비동기 대기 어댑터를 별도로 제공할 수 있습니다.

---

## 9. 구현 전 검토가 필요한 결정

아래 항목은 공개 API 확정 전에 검토가 필요합니다. 이 문서는 해결된 것처럼 표시하지 않습니다.

| 우선 | 검토 주제 | 확인·결정할 내용 |
|---|---|---|
| P0 | **Generator의 타입 매핑** | Service별 실제 Operation과 IDL C++ 타입이 유일하게 대응하는가? 중복 시 `<Op>` 태그 계약 |
| P0 | **Service 객체의 혼합 역할** | Config의 Command/Report 구현·등록·Session/Topic 공유가 중복 없이 되는가? |
| P0 | **CommandHandle 계약** | 예외/오류 반환, Move/Copy 가능 여부, Consumer 종료 뒤 Handle 동작, 세션 복구 시 재연결 정책 |
| P0 | **취소·복구와 상태 전이** | ACK, Commanded, Executing, terminal, recovered 처리 및 장비 실제 정지 확인 |
| P1 | **Callback 수명/스레딩** | `this` 캡처 수명, 호출 스레드, 재진입, 예외, 종료 중 Callback 금지 |
| P1 | **Report Dispose·캐시** | Key 추출, NoWriters와 Dispose 구분, `getLatest`의 무효화 정책 |
| P1 | **복합 Topic 조립** | LargeList/Set 완성 상태, Element 순서·삭제, 미지원 Specialization 처리 |
| P1 | **실행 환경 구성** | Domain/Partition/QoS/Source·Destination GUID를 코드 변경 없이 설정할 수 있는가? |
| P2 | **API 확장성** | 선택적 `ReportInfo`, 수동 구독, snapshot API, 고급 Operation 태그 노출 정도 |

### 현재 상태

- **확인한 출처의 사실:** AutoPilot의 Provider 7 + Consumer 7 구성, 예제에 인용한 주요 UMAA Service/IDL 타입의 존재, 실제 `GlobalPoseConfig`의 Command/Report 타입.
- **확정된 설계 방향:** Service별 생성 Provider/Consumer, `umaa::Service` 단일 상속 + 내부 역할 합성, Callback 등록 기본, `CommandHandle` 명명, 원본 IDL 직접 사용.
- **아직 검증하지 않은 구현:** Generator 결과, 실제 RTI 생성 C++11 헤더 빌드, DDS 교환·QoS·상태 전이, 취소/복구, Large Collection과 Specialization의 종단 간 동작.

---

## 10. 검토 문서 배포 방식

**권장: Markdown 원본 1개 + HTML 검토본 1개.**

- **Markdown:** Git으로 버전 관리·차이 비교·문구 수정하는 **단일 원본(SSOT)**.
- **HTML:** 목차·코드·표를 읽고 팀에서 의견을 모으는 **주 배포 형식**. 한 파일로 만들어 오프라인에서도 볼 수 있게 한다.
- **PDF:** 리뷰가 끝난 뒤 **고정 버전**을 서명·승인·공식 보관할 때 별도로 출력한다. 편집·코드 복사·링크 확인에는 HTML보다 불편할 수 있다.

### 출처 및 자세한 예제

- [현재 Public API 명세 (GitHub)](https://github.com/nssang00/umaa_dev_sdk/blob/main/docs/current/UMAA_Component_SDK_Public_API_v0.8.6.md)
- [실제 AutoPilot 14 Service 예제 (GitHub)](https://github.com/nssang00/umaa_dev_sdk/blob/main/examples/api_sketch/AUTOPILOT_14_SERVICE_USAGE_v0.8.6.cpp)
- [실제 Config 혼합 패턴 예제 (GitHub)](https://github.com/nssang00/umaa_dev_sdk/blob/main/examples/api_sketch/CONFIG_MIXED_SERVICE_v0.8.6.cpp)
- [역할별 코드 예제 (GitHub)](https://github.com/nssang00/umaa_dev_sdk/blob/main/examples/api_sketch/SERVICE_ROLE_USAGE_v0.8.6.md)
- [UMAA Component Definitions v1.0 — §3.1 AutoPilot](https://github.com/rticommunity/rticonnextdds-usecases-umaa/blob/master/docs/UMAA%20Component%20Definitions%20v1.0.md)
- [원본 `GlobalPoseConfig` Service 명세](https://github.com/rticommunity/rticonnextdds-usecases-umaa/blob/main/datamodel/umaa/idl/UMAA/SA/GlobalPoseConfig/GlobalPoseConfig.md)