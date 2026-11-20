# UMAA Component SDK — 사용자 Public API v0.8.6

> **상태:** 사용자 지향 **설계 계약/코드 예시**. 아직 구현된 SDK·실제 IDL 바인딩 컴파일·DDS 상호운용·UMAA 적합성 인증을 의미하지 않는다.  
> **개정:** 2026-11-20 · **언어:** C++11 · **이전:** [v0.8.5](UMAA_Component_SDK_Public_API_v0.8.5.md)  
> **기준:** UMAA ICD 6.0의 Service/Operation/IDL Data Type, RTI UMAA Starter Kit의 Service Provider/Consumer 구성, 사용자 결정(상속된 공통 동사 및 원본 IDL 직접 전달).

## 0. 최종 공개 API: 생성 Service별 Provider/Consumer

**사용자는 Generator가 만든 `<ServiceName>Provider`와 `<ServiceName>Consumer` 객체를 Component 멤버로 사용한다.** 별도의 사용자 Service 하위 클래스나 `CommandProvider<S>` 등 네 역할 템플릿을 직접 선언할 필요는 없다.

`Config / Control / Specs / StatusReport`는 **Service 기능 분류**, `Provider / Consumer`는 **참여 방향**, `Command/Response / Queryless Request/Reply`는 **Operation 통신 패턴**이다. 따라서 사용자용 `ControlService`, `ConfigService`, `ReportService`를 패턴별 기반 클래스로 만들지 않는다.

```cpp
namespace MO = umaa::services::MO;
namespace SA = umaa::services::SA;
namespace EO = umaa::services::EO;
namespace MM = umaa::services::MM;

using VectorCmd = UMAA::MO::GlobalVectorControl::GlobalVectorCommandType;
using Pose = UMAA::SA::GlobalPoseStatus::GlobalPoseReportType;

class AutoPilot : public umaa::Component {
public:
    MO::GlobalVectorControlProvider vector;            // Provider
    SA::GlobalPoseStatusConsumer pose;                  // Consumer
    EO::UVPlatformSpecsProvider specs;                  // Provider
    MM::MissionPlanConstraintControlConsumer constraints; // Consumer

    AutoPilot() {
        vector.onCommand([this](const VectorCmd& cmd,
                                const umaa::CommandInfo& info) {
            startVectorHardware(cmd, info.token()); // 사용자 업무 함수
            return umaa::CommandDecision::accepted();
        });
        pose.onReport([this](const Pose& report) {
            updateNavigation(report);
        });
    }
};

UMAA_COMPONENT(AutoPilot, m) {
    m.add(&AutoPilot::vector);
    m.add(&AutoPilot::pose);
    m.add(&AutoPilot::specs);
    m.add(&AutoPilot::constraints);
}
```

위 코드는 공식 AutoPilot Service 14개 중 4개를 추린 것이다. [실제 14개 전체 예제](../../examples/api_sketch/AUTOPILOT_14_SERVICE_USAGE_v0.8.6.cpp)를 참조한다.

## 1. umaa::Service와 네 내부 역할의 관계

생성된 Provider/Consumer 클래스 **하나가 `umaa::Service`를 상속하고**, 지원하는 통신 패턴의 구현 객체를 **합성·위임(Composition)** 한다. 네 역할 구현 클래스가 각각 `Service`를 상속하도록 해 Diamond 상속을 만들지 않는다.

```text
AutoPilot : umaa::Component
 ├─ GlobalVectorControlProvider : umaa::Service
 │   └─ 내부 CommandProvider<GlobalVectorControl>
 └─ GlobalPoseStatusConsumer : umaa::Service
     └─ 내부 ReportConsumer<GlobalPoseStatus>

별도 Config 서비스:
 GlobalPoseConfigProvider : umaa::Service
  ├─ 내부 CommandProvider<GlobalPoseConfig>
  └─ 내부 ReportProvider<GlobalPoseConfig>
 GlobalPoseConfigConsumer : umaa::Service
  ├─ 내부 CommandConsumer<GlobalPoseConfig>
  └─ 내부 ReportConsumer<GlobalPoseConfig>
```

네 종류 `CommandProvider<S>` / `CommandConsumer<S>` / `ReportProvider<S>` / `ReportConsumer<S>`는 **SDK 내부 동작·DDS 상태 관리 객체**이고, `ServiceTraits<S>`가 Operation/IDL/Topic 매핑을 제공한다. 사용자용 전용 클래스는 역할 객체의 지원 Action만 전달한다.

### Generator 내부 구성 예시 (설계 스케치)

```cpp
// SDK 생성: 실제 C++ IDL 타입/패턴에 맞는 메서드만 출력
class GlobalPoseConfigProvider : public umaa::Service {
    using S = GlobalPoseConfig;
    umaa::detail::CommandProvider<S> commandRole_;
    umaa::detail::ReportProvider<S> reportRole_;
public:
    template<class Handler> void onCommand(Handler cb) {
        commandRole_.onCommand(cb);
    }
    void respond(const umaa::CommandToken& t,
                 const umaa::CommandProgress& status) {
        commandRole_.respond(t, status);
    }
    void publish(
        const UMAA::SA::GlobalPoseConfig::GlobalPoseConfigReportType& report) {
        reportRole_.publish(report);
    }
};
```

이 클래스의 정확한 생성자/런타임 바인딩 및 타입 제한은 생성기 구현 대상이다. C++11 가상 함수 템플릿 없이 Callback을 등록할 수 있다. Config Consumer는 `CommandConsumer<S> + ReportConsumer<S>`를 내부 합성하여 `command()`와 `onReport()`를 모두 제공한다.

## 2. 역할별 공개 Action과 실제 Config의 혼합 패턴

| Service 객체에 노출할 기능 | 내부 구현 | 사용자 코드 |
|---|---|---|
| Command Provider | `CommandProvider<S>` | `onCommand(handler)`, `onCancel(handler)`, `respond(token, progress)`, `publish(token, executionIDL)` |
| Command Consumer | `CommandConsumer<S>` | `command(cmd)` → `CommandHandle<Message>`, `handle.update/cancel/get/wait`, `onAck/onStatus/onExecutionStatus` |
| Report Provider | `ReportProvider<S>` | `publish(report)`, `dispose(keyedReport)` |
| Report Consumer | `ReportConsumer<S>` | `onReport(handler)`, `onDispose(handler)`, `request(handler)`, `getLatest(key)` |

Config는 **Command/Response와 Report 패턴을 함께 가질 수 있다**. 예를 들어 실제 UMAA 6.0 `SA::GlobalPoseConfig`에는 `GlobalPoseConfigCommandType`과 `GlobalPoseConfigReportType`이 정의되어 있으며, 생성 Provider 하나에서 `onCommand(...)`와 `publish(configReport)`를 모두 제공할 수 있다. 다만 `GlobalPoseConfig`는 AutoPilot의 공식 14개 Service에 포함되지 않는다. [원본 Service 설명](https://github.com/rticommunity/rticonnextdds-usecases-umaa/blob/main/datamodel/umaa/idl/UMAA/SA/GlobalPoseConfig/GlobalPoseConfig.md), [Command IDL](https://github.com/rticommunity/rticonnextdds-usecases-umaa/blob/main/datamodel/umaa/idl/UMAA/SA/GlobalPoseConfig/GlobalPoseConfigCommandType.idl), [Report IDL](https://github.com/rticommunity/rticonnextdds-usecases-umaa/blob/main/datamodel/umaa/idl/UMAA/SA/GlobalPoseConfig/GlobalPoseConfigReportType.idl).

**Operation 선택:** `(Service, 역할, 원본 IDL 타입)`이 유일할 때는 `command(data)`, `publish(data)` 및 타입을 가진 Callback으로 자동 추론한다. 동일 타입 다중 Operation이나 Key가 같은 Dispose Hook이면 `<Op>` 명시 또는 타입 태그를 사용하며, 임의 Operation을 선택하지 않는다.

**기본 Hook은 Callback 등록**이다. 생성된 클래스를 사용자가 다시 상속하고 `override`하는 방식을 필수화하지 않는다. 특수 어댑터로 별도 허용할 수 있지만 동일 Operation에 중복 Callback은 호출하지 않는다.

**CommandHandle:** 원격 ACK/결과가 아니라 Source/Session을 추적하는 핸들. 반환값을 무시해도 명령 전송 동작은 같고 소멸 시 자동 취소되지 않는다. 원본 IDL 변수를 수정하는 것만으로 전송되지 않으며 `handle.update(updated)`로 갱신한다.

**원본 IDL:** 생성 C++ 매핑은 공개 변수 `cmd.speed = value` 또는 접근자 `cmd.speed(value)` 중 실제 매핑에 맞는 문법을 사용한다. `GlobalVectorCommandType.speed`는 단순 `double`이 아니라 `SpeedRequirementVariantType`이다. SDK는 전달된 원본 IDL 객체를 변경하지 않는다.


## 3. CommandConsumer: 간단히 command(cmd), 필요할 때만 CommandHandle

### 3.1 기본 사용: Command를 보낼 뿐이라면 반환값은 받지 않는다

```cpp
namespace MO = umaa::services::MO;
using Cmd = UMAA::MO::GlobalVectorControl::GlobalVectorCommandType;

MO::GlobalVectorControlConsumer vectorConsumer; // Component에 등록된 객체
Cmd cmd; // 실제 IDL 생성 타입
fillCommand(cmd); // 실제 direction / directionMode / speed 구성

vectorConsumer.command(cmd); // 끝. 반환되는 핸들은 무시
```

원본 `GlobalVectorCommandType.idl`에는 `direction: DirectionRequirementVariantType`, `directionMode: DirectionModeEnumType`, `speed: SpeedRequirementVariantType`가 정의되어 있다. **`speed`는 단순한 숫자 필드가 아니라 Variant 타입**이므로 `newSpeed` 같은 임의 `double`을 직접 대입할 수 있다고 가정하지 않는다.

**공개 멤버를 사용하는 C++ 생성 매핑이라면:**

```cpp
Cmd cmd;
cmd.direction     = desiredDirection;        // 실제 DirectionRequirementVariantType
cmd.directionMode = desiredMode;             // 실제 DirectionModeEnumType
cmd.speed         = desiredSpeedRequirement; // 실제 SpeedRequirementVariantType
vectorConsumer.command(cmd);
```

**접근자 함수를 생성하는 C++ 매핑이라면:**

```cpp
Cmd cmd;
cmd.direction(desiredDirection);
cmd.directionMode(desiredMode);
cmd.speed(desiredSpeedRequirement);
vectorConsumer.command(cmd);
```

두 방식은 **같은 메시지의 서로 다른 C++ 생성 매핑 예**이며, 사용 중인 타입이 제공하는 하나의 문법을 선택한다. `desiredSpeedRequirement`는 IDL Variant 값이 이미 적법하게 구성되어 있다고 가정한다.

### 3.2 반환값 `CommandHandle`은 무엇인가?

설계된 반환 타입은 `CommandHandle<Message>`이다. 실제 C++11 타입 선언은 생성된 Service/Operation 매핑을 통해 결정하며, 기본 호출은 아래 두 경우 모두 동일한 명령을 전송한다.

```cpp
vectorConsumer.command(cmd);                 // Handle을 저장하지 않는 기본 호출
auto handle = vectorConsumer.command(cmd);   // Handle 저장: 나중에 update/cancel/get/wait
```

`command(cmd)`는 전송 동작이면서, **해당 명령의 Source/Session을 추적하는 로컬 핸들 `CommandHandle<Cmd>`(개념 타입)를 반환**한다. 이 핸들은 **원격 ACK, 실제 응답 메시지, 명령 수락 또는 실행 완료를 뜻하지 않는다**. 사용자가 단지 명령을 보낼 때는 반환값을 무시하면 된다.

```cpp
// 명령 갱신·취소를 해야 하는 경우에만 핸들 저장
Cmd initial;
fillCommand(initial);

auto handle = vectorConsumer.command(initial); // 새 Command Session

Cmd updated = initial;
// 실제 생성 매핑 중 하나만 사용한다.
updated.speed = revisedSpeedRequirement;       // 공개 멤버 방식
// updated.speed(revisedSpeedRequirement);     // 접근자 방식

handle.update(updated);  // ***여기서 갱신 명령을 전송함***
auto state = handle.get();
auto observed = handle.wait(umaa::WaitStage::Commanded, 3000);
auto attempt = handle.cancel(); // 취소 요청; CANCELED 확정 아님
```

**중요:** `initial.speed = revisedSpeedRequirement;` 또는 `initial.speed(revisedSpeedRequirement);`만 실행하면 **로컬 객체만 바뀌며 네트워크 전송은 일어나지 않는다**. 기존 명령에 실제로 반영하려면 `handle.update(updated)`를 호출해야 한다. `update`는 새 `command()`가 아니므로 같은 Source/Session과 더 최신 Command Timestamp를 사용한다.

**핸들 수명 계약:** `CommandHandle`은 명령을 소유하는 RAII 취소 객체가 아닌 **명령 식별/추적 핸들**이다. `vectorConsumer.command(cmd);`처럼 반환값을 무시하거나 핸들이 소멸하더라도 **원격 Command가 자동 취소되어서는 안 된다**. 필요한 경우에만 명시적으로 `handle.cancel()`을 요청한다. 전송 제출 실패와 원격 Provider의 FAILED는 구분하며, 전송 오류를 예외/오류 결과로 전달하는 세부 계약은 실제 구현에서 확정한다.

선택적 CommandConsumer Hook은 원본 `GlobalVectorCommandAckReportType`, `GlobalVectorCommandStatusType`, `GlobalVectorExecutionStatusReportType`를 **직접 인자로** 받는다. 동일 Service의 다수 Operation/Session을 처리하려면 `CommandInfo`를 선택적 두 번째 인자로 받는다. `wait` timeout 및 transport failure와 원격 FAILED는 다르다. `setGlobalVector`는 `endTime`이 없으면 무기한 실행될 수 있으므로 무조건 Terminal까지 대기하는 예제는 피한다.

## 4. CommandProvider: onCommand / onCancel / respond / publish

```cpp
class MyVectorProvider : public MO::GlobalVectorControlProvider {
public:
    umaa::CommandDecision onCommand(
        const UMAA::MO::GlobalVectorControl::GlobalVectorCommandType& cmd,
        const umaa::CommandInfo& info) override
    {
        if (!hardwareAccepts(cmd))
            return umaa::CommandDecision::rejected(
                umaa::CommandReason::ValidationFailed);
        hardwareStart(cmd, info.token());
        return umaa::CommandDecision::accepted(); // 완료 아님
    }
    umaa::CancelDecision onCancel(const umaa::CancelInfo& info) override {
        requestHardwareStop(info.token());
        return umaa::CancelDecision::deferred();
    }
    void hardwareStarted(const umaa::CommandToken& token) {
        respond(token, umaa::CommandProgress::executing());
    }
    void hardwareStatus(
        const umaa::CommandToken& token,
        const UMAA::MO::GlobalVectorControl::GlobalVectorExecutionStatusReportType& s) {
        publish(token, s); // 공통 publish: ExecutionStatus Operation 자동 선택
    }
    void hardwareStopped(const umaa::CommandToken& token) {
        respond(token, umaa::CommandProgress::canceled());
    }
};
```

`onCommand`는 New/Updated/Recovered 구분을 `info.delivery()`로 받는다. Recovered는 실제 장비 상태를 재확인한 뒤 resume/reject를 결정하며, SDK가 임의로 재시작하지 않는다. 취소 요청과 실제 장비 정지 완료는 구분한다. ACK/CommandStatus 합법 상태 전이와 Session 매핑은 SDK 책임이다.

## 5. ReportProvider: publish / dispose

```cpp
namespace SA = umaa::services::SA;
using Pose = UMAA::SA::GlobalPoseStatus::GlobalPoseReportType;

SA::GlobalPoseStatusProvider globalPoseProvider; // Component에 등록됨
Pose pose;
fillPose(pose); // position, attitude, course, navigationSolution
globalPoseProvider.publish(pose);                // 실제 Report 타입 직접 전달
globalPoseProvider.dispose(pose);                // 같은 타입의 @key source 추출
```

`dispose(reportWithKey)`는 **새 사용자 API 제안**이다. 원본 IDL 메시지의 Key를 사용해 Dispose하며, 유효한 비-Key 데이터까지 채울 필요는 없다. Key-only 대안 `dispose<ReportOp>(key)`는 중복 Key·컴포지트 Key 등의 고급 경로로 보유한다. `publish(snapshot, ReplaceOwnedInstances)`는 SDK가 자신이 소유한 Key만 정확히 추적할 때 허용한다. 타 Provider 소유의 인스턴스를 지우지 않는다.

다수 Report를 가진 한 Service도 메시지 인자 타입으로 발행 Topic을 구분한다.

```cpp
EO::UVPlatformSpecsProvider specsProvider;
specsProvider.publish(specsReport);             // UVPlatformSpecsReportType
specsProvider.publish(capabilitiesReport);      // UVPlatformCapabilitiesReportType
```

## 6. ReportConsumer: 실제 IDL 타입 직접 Hook

```cpp
class MyPoseConsumer : public SA::GlobalPoseStatusConsumer {
public:
    void onReport(const Pose& pose) override {
        usePosition(pose.position()); // event.data() 불필요
    }
    void onDispose(const UMAA::Common::IdentifierType& source) override {
        invalidatePose(source);
    }
};
```

메타데이터가 필요하면 `onReport(const Pose&, const umaa::ReportInfo&)` 오버로드를 사용한다. 동일 Operation에서 둘을 **동시에 호출하지 않으며**, 사용자 구현이 선택한 서명 하나만 정상 데이터 Callback으로 등록된다. 실제 삭제/invalid sample에는 유효한 `Pose`가 없을 수 있어 `onReport(const Pose&)`를 호출하지 않는다. SDK 캐시 무효화와 Dispose/NoWriters 구분은 내부 책임이고, 필요하면 `onUnavailable` 등 추가 Hook으로 원인을 드러낸다.

**구독 계약:** `m.add(&MyComponent::poseConsumer)`의 기본 동작은 Component 생명주기에 묶인 자동 구독. 임시 구독은 별도의 ReportConsumer 객체를 `m.add(..., SubscriptionMode::Manual)`로 등록하고 `request(handler)`를 호출해 `Subscription` RAII 핸들로 관리한다. 한 Service/Operation에 자동·수동을 섞어 중복 Callback을 일으키지 않는다.

## 7. LargeSet / LargeList 및 Specialization: 원본 IDL 유지

원본 `BatteryReportType`에는 `cells` 벡터가 **없고** `cellsListMetadata`만 있다. `ConditionalReportType`에도 `conditionalsSetMetadata`만 있다. 따라서 원본 IDL 객체에 존재하지 않는 `report.cells.add()` 형태를 기본 API로 제안하지 않는다.

```cpp
// ReportProvider는 같은 공통 publish를 사용하되, 분리된 요소를 함께 전달
UMAA::EO::BatteryStatus::BatteryReportType battery;
umaa::LargeList<UMAA::EO::BatteryStatus::BatteryCellDataType> cells;
cells.add(cell);                    // 안정적인 Element ID 유지
batteryProvider.publish(battery, cells);

// Generalization + Specialization + LargeSet 결합
umaa::Specialized<UMAA::MM::Conditional::ConditionalType> condition;
condition.base().conditionalID(conditionalId);
condition.set(depthConditional);   // 실제 DepthConditionalType

umaa::LargeSet<umaa::Specialized<
    UMAA::MM::Conditional::ConditionalType>> conditionals;
conditionals.add(condition);
conditionalProvider.publish(conditionalReport, conditionals);
```

이는 `publish(const ReportIDL&, const CollectionView&)`라는 **생성된 Service별 오버로드**의 제안이다. SDK는 원본 Report의 Metadata를 채운 전송용 복사본, Collection Element Topic, Specialization Topic을 모두 동기화한다. 같은 요소 타입으로 여러 컬렉션을 가진 메시지는 컬렉션 필드 태그가 추가로 필요할 수 있다.

Consumer는 `onReport(const BatteryReportType&, const LargeList<BatteryCellDataType>&)` 또는 `onReport(const ConditionalReportType&, const LargeSet<Specialized<ConditionalType>>&)`라는 **생성된 전체 조립형 오버로드**를 선택할 수 있다. 기본 정책은 CompleteOnly이며, 일부 미수신/원소 순서/삭제/Unsupported specialization을 정상 데이터와 구분한다. **Cross-Topic 원자적 가시성을 DDS가 제공한다고 가정하지 않는다.**

## 8. C++11 및 사용자 계약

- Service 클래스와 메서드 템플릿/타입 판별은 C++11에 맞춰 생성한다. `auto` NTTP, `std::optional`, C++14 generic lambda, virtual function template은 사용하지 않는다.
- 모든 원본 IDL Message는 사용자가 직접 채우거나 직접 읽는다. `source`, `destination`, `sessionID`, `timeStamp`의 SDK 관리 및 앱 제공 값의 우선순위는 명시적 옵션/검증 정책으로 정한다. 원본 메시지 참조를 몰래 수정하지 않는다.
- Hook의 `const T&` 참조는 Callback 수행 중에만 유효하다. 장기 보관 시 앱이 복사한다. Callback 예외/스레드/순서/재진입·Component 종료 후 호출 금지 정책은 구현 시 명시한다.
- Service 컴포넌트 조합 및 DDS QoS, 도메인, Partitions, Source/Destination ID는 프로그램 소스 재컴파일 없이 바꿀 수 있어야 하는 컴포넌트/플랫폼 구성 요구사항에 맞춰 별도 구성한다.
- 모든 Service가 동일한 Operation 수·메시지 조합이라고 추론하지 않는다. 정규 ICD 및 Experimental 부속의 Topic/IDL 불일치, 다중 Operation/Key 충돌을 생성기에서 검사해야 한다.

## 9. 코드 예제 및 이전 버전 변경

- **[AutoPilot 실제 14개 Service를 사용한 C++11 예제](../../examples/api_sketch/AUTOPILOT_14_SERVICE_USAGE_v0.8.6.cpp)** — UMAA Component Definitions v1.0 §3.1에 따른 Provider 7 + Consumer 7, 실제 MO/SA/MM/EO/SO IDL 메시지 타입.
- **[v0.8.6 통합 C++11 API 코드](../../examples/api_sketch/COMPLETE_FOUR_ROLE_USAGE_v0.8.6.cpp)** — 하나의 파일에서 Command/Report 4역할, 명령 추적, Dispose, Request, Large Collection, Specialization, Component 등록을 확인한다.
- **[v0.8.6 전체 사용 예제](../../examples/api_sketch/SERVICE_ROLE_USAGE_v0.8.6.md)** — 네 역할, Command 갱신·취소·상태, Report 삭제, 동적 구독, Multiple Report, Large Collection, Specialization.
- **[기존 v0.8.5 명세](UMAA_Component_SDK_Public_API_v0.8.5.md)** — 등록형 API를 사용한 과거 설계이며 이 문서의 기본 UX와 다르다.
- `m.provide<S>().onCommand<Op>(...)` → `m.add(&Component::providerMember)` + 생성된 Provider 가상 Hook `onCommand(IDL&, Info)`.
- `service<S>().commandConsumer().command<Op>(cmd)` → `serviceConsumerMember.command(cmd)`.
- `service<S>().reportProvider().publish<Op>(report)` → `serviceProviderMember.publish(report)`.
- `onReport(ReportEvent<T>)` → 기본 `onReport(const T&)` + 선택적 `onDispose(key)` / `ReportInfo`.

## 10. 검증 상태 / 남은 게이트

**검증된 것:** 현재 저장소의 기존 v0.8.5 API는 모형/문서 수준이고, 사용자 제공 ICD 6.0 및 확장 IDL 정적 검토에서는 정규 114/114 Operation 및 전체 고유 311/314 Topic 선언이 일치한다고 보고되었다. 나머지 3개는 Experimental `@nested`/Topic 불일치다.

**아직 검증되지 않은 것:** v0.8.6의 실제 SDK 생성 클래스, 메시지 기반 Operation 유일성 전체 조사, C++11 + 실제 RTI 생성 헤더 컴파일, Command Session/취소/복구 런타임, Report dispose/구독, Large 18개 Element/48개 Specialization의 런타임 조립, DDS 상호운용 및 규격 인증. **구조적으로 표현 가능하다는 주장과 구현 완료는 구분한다.**

> 구현 전 우선 검사: (1) 139개 고유 Service별 생성 클래스/Hook 중복·충돌 검사, (2) Command/Report 실제 메시지 매핑 유일성, (3) C++11 컴파일, (4) DDS 실환경 상태 전이·메타데이터·Large Collection 회복 시나리오.
