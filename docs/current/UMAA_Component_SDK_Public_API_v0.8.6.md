# UMAA Component SDK — 사용자 Public API v0.8.6

> **상태:** 사용자 지향 **설계 계약/코드 예시**. 아직 구현된 SDK·실제 IDL 바인딩 컴파일·DDS 상호운용·UMAA 적합성 인증을 의미하지 않는다.  
> **개정:** 2026-11-20 · **언어:** C++11 · **이전:** [v0.8.5](UMAA_Component_SDK_Public_API_v0.8.5.md)  
> **기준:** UMAA ICD 6.0의 Service/Operation/IDL Data Type, RTI UMAA Starter Kit의 Service Provider/Consumer 구성, 사용자 결정(상속된 공통 동사 및 원본 IDL 직접 전달).

## 0. 채택한 사용자 경험

**사용자는 Service별 생성된 Provider/Consumer 객체를 보유하거나 상속하고, 공통 동사만 호출한다.**

```cpp
// 실제 UMAA IDL 데이터 타입을 직접 작성
UMAA::MO::GlobalVectorControl::GlobalVectorCommandType cmd;
fillCommand(cmd);

// Service별 Consumer 객체에서 상속된 공통 함수 사용
vectorConsumer.command(cmd);         // 기본: 보내기만 하면 반환값 무시

// 선택적: 나중에 수정·취소·상태 확인이 필요할 때만 핸들 보관
auto call = vectorConsumer.command(cmd);
call.update(updated);
call.cancel();

globalPoseProvider.publish(pose);
healthProvider.publish(health);
specsProvider.publish(specs);
```

**비채택(기본 경로):** `service<S>().commandConsumer().command<Op>(msg)`, `publishGlobalPose(msg)`, `setGlobalVector(msg)`, 일률적인 `Op::Data`, 수신 Hook의 일률적인 `ReportEvent<T>::data()`.  
**고급 예외:** 동일 Service + 역할에서 메시지 C++ 타입만으로 Operation을 유일하게 결정할 수 없으면 `command<Op>(msg)` / `publish<Op>(msg)`를 명시한다. 자동으로 임의 선택하지 않는다.

## 1. 개념 및 실제 타입

| 개념 | 예 | 정의 |
|---|---|---|
| Component | `AutoPilot` | 여러 Service 역할 인스턴스 조합 |
| Service 종류 | Config / Control / Specs / StatusReport / Other | 기능 분류 정보; 실제 Flow/역할 결정자가 아님 |
| SDK 생성 Service 역할 클래스 | `MO::GlobalVectorControlConsumer` | Service에 종속된 구체 클래스, 공통 역할 클래스를 상속 |
| 공통 역할 클래스 | `CommandProvider<S>`, `CommandConsumer<S>`, `ReportProvider<S>`, `ReportConsumer<S>` | 공통 Action 및 런타임 |
| 원본 ICD Operation | `setGlobalVector`, `reportGlobalPose` | ICD Operation 이름; 생성될 Operation tag는 내부/고급 API용 |
| **원본 IDL Data Type** | `UMAA::MO::GlobalVectorControl::GlobalVectorCommandType`, `UMAA::SA::GlobalPoseStatus::GlobalPoseReportType` | 사용자가 작성하고 Hook에서 읽는 실제 메시지 |
| SDK OperationTraits | `(Service, role, message type) → Op/Topic` | 생성기 메타데이터, 기본 사용자 API에 노출하지 않음 |

`UMAA::...`는 원본 IDL의 모듈/생성 C++ 타입이고, `umaa::services::MO::...`는 **SDK 생성 예정 클래스 이름**이다. 실제 C++ 필드는 **생성기·매핑 설정에 따라 공개 멤버 변수 `cmd.speed` 또는 접근자 `cmd.speed()`/`cmd.speed(value)` 형태**일 수 있다. 기본 SDK API는 양쪽 매핑의 원본 타입과 호환되어야 한다. 한 객체에서 두 문법을 동시에 쓸 수 있다는 뜻은 아니다. 예제의 `fillCommand`/`controller_` 등은 사용자 업무 함수이다.

### 상속 계층

```cpp
// 모두 SDK가 생성한다. 사용자는 이 코드를 작성하지 않는다.
class GlobalVectorControlConsumer
    : public umaa::CommandConsumer<GlobalVectorControl> {};

class GlobalVectorControlProvider
    : public umaa::CommandProvider<GlobalVectorControl> {
protected:
    // 생성기는 Operation에 맞춰 실제 메시지 타입별 virtual Hook 선언
    virtual umaa::CommandDecision onCommand(
        const UMAA::MO::GlobalVectorControl::GlobalVectorCommandType&,
        const umaa::CommandInfo&);
    virtual umaa::CancelDecision onCancel(
        const umaa::CancelInfo&);
};

class GlobalPoseStatusProvider
    : public umaa::ReportProvider<GlobalPoseStatus> {};

class GlobalPoseStatusConsumer
    : public umaa::ReportConsumer<GlobalPoseStatus> {
protected:
    virtual void onReport(
        const UMAA::SA::GlobalPoseStatus::GlobalPoseReportType&);
    virtual void onDispose(const UMAA::Common::IdentifierType&);
};
```

이 코드는 계약 설명용 선언 스케치다. **C++11에는 virtual function template이 없으므로** 기반 클래스가 모든 Report/Command 타입에 대해 virtual 템플릿을 정의할 수 없다. 생성기가 실제 메시지 타입별 virtual overload를 만들어야 한다. 단일 Service에 여러 Operation이 있는 경우 해당 메시지 타입별 overload를 생성한다.

### Component 구성

```cpp
class AutoPilot : public umaa::Component {
public:
    MyVectorProvider vector;
    MyPoseConsumer pose;
    MyHealthProvider health;
};

UMAA_COMPONENT(AutoPilot, m) {
    m.add(&AutoPilot::vector);
    m.add(&AutoPilot::pose);
    m.add(&AutoPilot::health);
}
```

`m.add(&Component::member)`는 **v0.8.6의 제안 등록 API**다. C++11의 멤버 포인터를 사용하므로 현재 v0.8.5 매크로의 범위 밖 지역 변수를 요구하지 않는다. SDK가 Component 인스턴스의 멤버를 등록하고 수명·시작/종료를 관리한다. 객체를 직접 생성했으나 등록하지 않거나, Component 시작 전에 전송하는 행위는 오류로 정의한다.

## 2. 네 역할의 공통 Action / Hook

| 역할 | 상속한 Action | 생성된 역할 클래스의 Hook |
|---|---|---|
| `CommandProvider<S>` | `respond(token, progress)`, `publish(token, executionData)` | `onCommand(const CommandIDL&, CommandInfo)`, `onCancel(CancelInfo)` |
| `CommandConsumer<S>` | `command(commandIDL)` → `CommandCall`, `call.update(message)`, `call.cancel()`, `call.get()/wait()` | `onAck(const AckIDL&)`, `onStatus(const StatusIDL&)`, `onExecutionStatus(const ExecutionIDL&)`, 선택적 복구 Hook |
| `ReportProvider<S>` | `publish(reportIDL)`, `dispose(reportWithKey)`, `publish(snapshot, ReplaceOwnedInstances)` | 없음 |
| `ReportConsumer<S>` | 명시적 `request(handler)`, `Subscription.close()`, `getLatest(key)` | `onReport(const ReportIDL&)`, 선택적 `onReport(const ReportIDL&, ReportInfo)`, `onDispose(key)` |

**메시지 타입 추론:** `(Service, 역할, Message)` 조합이 하나의 Operation만 가리키면 컴파일 타임에 자동 선택한다. Operation과 다른 서비스 타입 전달, 중복 매핑, 유효하지 않은 Flow는 컴파일 오류 또는 명시적 진단이다.

**중복/서명 충돌:** 같은 Service의 두 Operation이 같은 메시지 타입이거나 동일한 Key 타입을 사용하면 C++ overload만으로 Hook을 구분할 수 없다. 생성기는 `OperationTag<Op>` 또는 `Disposed<Payload>` 등의 **타입화된 식별 인자**를 제공하고, 필요한 때만 이를 사용한다. 모든 Service가 인자 타입만으로 구별된다고 가정하지 않는다.

**Config 혼합 Service:** 같은 Service에서 Command와 일반 Report가 모두 존재할 수 있다. 생성 Provider/Consumer는 두 역할 기반 클래스를 조합하며, `publish(token, executionData)`와 `publish(reportData)`의 오버로드 집합을 `using` 선언 등으로 명확히 병합한다. 일반 Report/Command 역할을 ServiceKind로 추측하지 않는다.

## 3. CommandConsumer: 간단히 command(cmd), 필요할 때만 CommandCall

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

### 3.2 반환값 `CommandCall`은 무엇인가?

`command(cmd)`는 전송 동작이면서, **해당 명령의 Source/Session을 추적하는 로컬 핸들 `CommandCall<Cmd>`(개념 타입)를 반환**한다. 이 핸들은 **원격 ACK, 실제 응답 메시지, 명령 수락 또는 실행 완료를 뜻하지 않는다**. 사용자가 단지 명령을 보낼 때는 반환값을 무시하면 된다.

```cpp
// 명령 갱신·취소를 해야 하는 경우에만 핸들 저장
Cmd initial;
fillCommand(initial);

auto call = vectorConsumer.command(initial); // 새 Command Session

Cmd updated = initial;
// 실제 생성 매핑 중 하나만 사용한다.
updated.speed = revisedSpeedRequirement;       // 공개 멤버 방식
// updated.speed(revisedSpeedRequirement);     // 접근자 방식

call.update(updated);  // ***여기서 갱신 명령을 전송함***
auto state = call.get();
auto observed = call.wait(umaa::WaitStage::Commanded, 3000);
auto attempt = call.cancel(); // 취소 요청; CANCELED 확정 아님
```

**중요:** `initial.speed = revisedSpeedRequirement;` 또는 `initial.speed(revisedSpeedRequirement);`만 실행하면 **로컬 객체만 바뀌며 네트워크 전송은 일어나지 않는다**. 기존 명령에 실제로 반영하려면 `call.update(updated)`를 호출해야 한다. `update`는 새 `command()`가 아니므로 같은 Source/Session과 더 최신 Command Timestamp를 사용한다.

**핸들 수명 계약:** `CommandCall`은 명령을 소유하는 RAII 취소 객체가 아닌 **명령 식별/추적 핸들**이다. `vectorConsumer.command(cmd);`처럼 반환값을 무시하거나 핸들이 소멸하더라도 **원격 Command가 자동 취소되어서는 안 된다**. 필요한 경우에만 명시적으로 `call.cancel()`을 요청한다. 전송 제출 실패와 원격 Provider의 FAILED는 구분하며, 전송 오류를 예외/오류 결과로 전달하는 세부 계약은 실제 구현에서 확정한다.

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
