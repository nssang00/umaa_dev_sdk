# UMAA Component SDK — 사용자 Public API v0.8.5

> **상태:** SDK 설계 계약(제안). 구현 코드·전체 서비스 타입 생성·DDS/ICD 런타임 적합성 인증이 아니다.  
> **갱신:** 2026-11-12 (개선안 A: Service 중심 역할 등록) · **C++:** C++11 · **전 버전:** [v0.8.4](../history/v0.8.4/UMAA_Component_SDK_Public_API_v0.8.4.md)  
> **주요 입력:** 사용자 제공 `UMAA_Public_API_Summary_KO.md`, UMAA ICD 6.0 (§3.8, §3.9, §5.1, §5.2), RTI Starter Kit Hook 목록.

## 0. 이 설계에서 확정할 사용자 관점

- **사용자 등록 API는 2개:** `m.provide<S>()`와 `m.consume<S>()`를 사용한다. Provider/Consumer 역할은 Service 단위로 선언하고, Flow Control 및 Callback 적합성은 등록된 실제 `Op`의 `OperationRole/FlowKind`로 판정한다. 등록 메서드가 네 역할 객체를 직접 반환하는 것으로 오해하지 않는다.
- **2 Flow Control × 2 Role = 4개 런타임 인터페이스:** `CommandProvider<S>`, `CommandConsumer<S>`, `ReportProvider<S>`, `ReportConsumer<S>`. `S`는 생성된 **Service 타입**, `Op`는 그 Service에 속하는 **Operation 타입**.
- **Service는 하나의 정적 정의 타입:** `Service<Derived, ServiceKind>`. 네 런타임 클래스는 Service의 서브클래스가 아니라, 해당 Service에 대해 **Component가 사용 가능한 역할 인터페이스**다. 사용자에게 `ControlServiceProvider`·`SpecsServiceConsumer` 등의 상속은 요구하지 않는다.
- **Action은 업무 의도, Hook은 실제 사용자 이벤트:** `command`/ `cancel`/ `respond`/ `publish`/ `dispose`/ `request`와 `onCommand`/ `onCancel`/ `onStatus`/ `onReport`. `onAck`, `onExecutionStatus`, `onRecoveredCommand`는 선택적.
- **내부 빙산:** DDS Reader/Writer/Topic, ACK·Status Flow, Queryless 구독·Session·Timestamp·Dispose·Liveliness·Collection·Specialization 연결은 SDK 내부. 실제 기계 제어·결과 확인·안전 정지 판단·데이터 값은 앱 책임.
- **ServiceKind는 capability를 단정하지 않는다:** Control/Config/Specs/StatusReport/Other는 분류 메타데이터다. 허용 역할과 `Op`는 **실제 생성 OperationTraits**로 검사한다. 혼합 Config는 Command/Report 양쪽이 있을 수 있다.

## 1. 최종 공개 역할 인터페이스

| Flow | 역할 클래스 | 호출(Action) | Hook 등록 | 기본/선택 |
|---|---|---|---|---|
| Command/Response | `CommandProvider<S>` | `respond(token, progress)`; `publish<ExecutionOp>(token, data)` | `onCommand<CommandOp>(handler)`, `onCancel<CommandOp>(handler)` | onCommand 필수(제공하는 Op마다); onCancel 장기/중단 가능 명령 조건부 필수 |
| Command/Response | `CommandConsumer<S>` | `command<CommandOp>(payload[, opts])`, `cancel(call)` | `onStatus<CommandOp>(handler)`, `onAck<CommandOp>(handler)`, `onExecutionStatus<ExecutionOp>(handler)`, `onRecoveredCommand<CommandOp>(handler)` | onStatus 핵심 관찰 Hook이나 필수 등록은 아님; 나머지 선택 |
| Request/Reply (Queryless) | `ReportProvider<S>` | `publish<ReportOp>(value)`, `publish<ReportOp>(snapshot, ReplaceOwnedInstances)`, `dispose<ReportOp>(key)` | **없음** | 앱이 데이터 제공 |
| Request/Reply (Queryless) | `ReportConsumer<S>` | `request<ReportOp>(handler[, opts])`, 선택적 `getLatest<ReportOp>(key)` | `onReport<ReportOp>(handler)` | onReport 정적 자동 구독 또는 request 동적 구독 **하나만** 선택 |

`CommandProvider::publish`는 Command에 연관된 ExecutionStatus Payload를 발행하는 동사로 제한한다. `ReportProvider::publish`는 일반 Report 데이터 발행이다. `respond`는 Command 상태·결과이며 ExecutionStatus 업무 데이터 대신 사용할 수 없다.

## 1.1 실제 ICD/IDL 명칭과 생성 SDK facade의 구분

**실제 UMAA 6.0 명칭:** MO Service `GlobalVectorControl`, Command `setGlobalVector` 및 Payload `UMAA::MO::GlobalVectorControl::GlobalVectorCommandType`, Response Operation `reportGlobalVectorCommandAck` / `reportGlobalVectorCommandStatus` / `reportGlobalVectorExecutionStatus`; SA Service `GlobalPoseStatus`, Report Operation `reportGlobalPose` 및 Payload `UMAA::SA::GlobalPoseStatus::GlobalPoseReportType`.

위 `UMAA::MO::GlobalVectorControl` 및 `UMAA::SA::GlobalPoseStatus`는 IDL의 **module**, 즉 일반적인 C++ 매핑에서는 네임스페이스다. 아래 `umaa::services::MO::GlobalVectorControl`과 `umaa::services::SA::GlobalPoseStatus`는 사용자 편의를 위해 제안한 **SDK 생성 Service facade 클래스 네임스페이스**이며, `::setGlobalVector`와 `::reportGlobalPose` 등 중첩 Operation 타입 태그도 **SDK가 생성하도록 제안한 이름**이다. 원본 IDL에 그 C++ facade 클래스가 정의된 것은 아니다.

**데이터 타입 계약:** `GlobalVectorCommandType`의 `direction`, `directionMode`, `speed`는 실제 ICD 필드이고, `depthChangePitch`, `elevation`, `endTime`는 optional이다. `GlobalPoseReportType`는 `position`, `attitude`, `course`, `navigationSolution` 등을 갖고, 보고 Key는 `source` (`UMAA::Common::IdentifierType`)이다. 이전의 편의용 `VectorSvc`, `VectorOp` 및 `latitude/longitude/sensorId` 최상위 필드는 실제 ICD 형식이 아니어서 통합 AutoPilot 예제에서 제거했다.

**등록 vs 상속:** [실제 ICD 명칭 기반 비교 및 결론](API_REGISTRATION_VS_INHERITANCE_v0.8.5.md)을 참고한다. 범용 Component는 다중 Service/Role이 흔하므로 **Callback 등록 방식을 기본 UX**로 하고, Service별 Provider/Consumer 구현 클래스 상속을 필수로 요구하지 않는다.

## 2. 구성과 실행 객체: Service 중심 등록(개선안 A)

구성 시 사용자는 어떤 Service를 제공/소비하는지 먼저 선언하고, 그 다음 **실제 ICD Operation 이름**으로 Hook을 연결한다. 네 가지 `CommandProvider/Consumer`·`ReportProvider/Consumer`는 여전히 SDK 내부의 **런타임 역할 객체**로 존재한다.

```cpp
namespace MO = umaa::services::MO;
namespace SA = umaa::services::SA;

// 사용자 업무 Callback은 AutoPilot 멤버 함수
UMAA_COMPONENT(AutoPilot, m) {
    m.provide<MO::GlobalVectorControl>()
      .onCommand<MO::GlobalVectorControl::setGlobalVector>(
          &AutoPilot::onSetGlobalVector)
      .onCancel<MO::GlobalVectorControl::setGlobalVector>(
          &AutoPilot::onCancelGlobalVector);

    m.consume<SA::GlobalPoseStatus>()
      .onReport<SA::GlobalPoseStatus::reportGlobalPose>(
          &AutoPilot::onGlobalPoseReport);
}

// 실제 명령 제어·데이터 송수신은 네 런타임 역할 객체가 책임짐
service<MO::GlobalVectorControl>().commandProvider().respond(token, progress);
service<SA::GlobalPoseStatus>().reportConsumer(); // onReport 등록 시 자동 구독
```

**구성 시점:** `Registrar<Component>::provide<S>()`는 `ProviderBinding<S>`을, `consume<S>()`는 `ConsumerBinding<S>`을 반환한다. 각각의 Hook 등록 함수는 같은 바인딩을 반환하여 체이닝 가능하다. 등록기는 Component·Service·Role·Operation 연결을 저장하며 DDS/Flow Control 구현은 직접 노출하지 않는다.

**런타임:** `Component::service<S>().commandProvider()/commandConsumer()/reportProvider()/reportConsumer()`가 네 실제 실행 역할 객체를 돌려준다. **등록기 `m.provide/consume`와 런타임 `service<S>().*`는 서로 다른 책임**이다.

**Operation별 필수 타입 검사:**

- `onCommand<Op>`/`onCancel<Op>`, `onStatus<Op>`/`onAck<Op>`/`onRecoveredCommand<Op>`: `Op::ServiceType == S`, `Op::role == Command`, `Op::flow == CommandResponse`.
- `onExecutionStatus<Op>`: `Op::role == ExecutionStatus`, `Op::flow == CommandResponse`와 동일 Service 소속.
- `onReport<Op>`: `Op::role == Report`, `Op::flow == RequestReplyNoQueryData`와 동일 Service 소속.
- `provide<S>()` 단독 등록은 Report-only Provider에도 유효하며 `onRequest()`를 요구하지 않는다. 반면 Command를 실제로 제공하려면 서비스별 Command Operation에 필요한 비즈니스 Handler가 있어야 한다.
- Config처럼 Command + 일반 Report가 혼합된 Service에서도 `provide<S>()`/ `consume<S>()`는 **각각 한 번만 선언**하고, 그 아래 Operation별 Hook을 조합한다. 일반 Report 발행은 런타임 `reportProvider()`를 이용한다.
- `S`의 지원 capability는 **생성 Operation 메타데이터**로 검증한다. ServiceKind만으로 Command 또는 Report 기능 존재를 추론하지 않는다. 잘못된 Operation/Handler 서명은 실제 구현에서 컴파일 또는 명시적 등록 오류가 되어야 한다.

**비채택:** `m.commandProvider<S>()`, `m.reportConsumer<S>()` 등의 네 가지 등록 함수와 자동 생성 `.onSetGlobalVector()` 같은 편의 메서드를 기본 API에 더하지 않는다. 필요하다면 향후 선택적 편의 계층으로 분리할 수 있지만, v0.8.5 현재 등록 API는 `provide/consume` 둘뿐이다.

## 3. CommandProvider<S>: 명령 실행 책임

```cpp
template<class S> class CommandProvider {
public:
    template<class CommandOp, class Handler> void onCommand(Handler);
    template<class CommandOp, class Handler> void onCancel(Handler);
    void respond(const CommandToken&, const CommandProgress&);
    template<class ExecutionOp> void publish(
        const CommandToken&, const typename ExecutionOp::Payload&);
};
```

- `onCommand`는 신규(New)·갱신(Updated)·복구(Recovered)를 `CommandInfo.delivery()`와 함께 받는다. App은 실제 장치 검증/실행/이어하기 결정. SDK는 원본 Source/Session 및 갱신 Timestamp 비교를 보존하고 ACK/Status 처리.
- `CommandDecision::accepted()`는 **접수**이며 실제 EXECUTING/COMPLETED가 아니다. SDK는 접수에 따른 초기 합법 상태를 단일 책임으로 전이한다. 앱이 중복 `respond(COMMANDED)`를 호출할 필요가 없다. `deferred()`와 `resume()`은 각각 지연 결정과 복구 연속성 표현; SDK 상태 전이 검증 필요.
- `onCancel`은 취소 의사 전달이다. 성공적인 호출만으로 CANCELED를 확정하지 않는다. 실제 중단 확인 전 `deferred()`, 불가하면 `cannotCancel()`. 명령 연결 상실에 따른 필요한 업무 중단도 이 경로로 전달하되 원인이 확정되지 않으면 Unknown으로 표시.
- `respond(token, progress)`로 장치의 실제 executing/completed/failed/canceled를 보고한다. 오래된 갱신 token은 StaleToken 오류 처리. `publish<ExecutionOp>(token, value)`에서 Command-ExecutionStatus 연결을 SDK가 조립한다.

## 4. CommandConsumer<S>: 명령 제출·추적·복구

```cpp
template<class S> class CommandConsumer {
public:
    template<class CommandOp> CommandCall<CommandOp> command(
        const typename CommandOp::Payload&, const CommandOptions& = CommandOptions());
    template<class CommandOp> CancelAttempt cancel(const CommandCall<CommandOp>&);
    template<class CommandOp, class Handler> void onStatus(Handler);
    template<class CommandOp, class Handler> void onAck(Handler);
    template<class ExecutionOp, class Handler> void onExecutionStatus(Handler);
    template<class CommandOp, class Handler> void onRecoveredCommand(Handler);
};
```

- `command<Op>`: **새 Source/Session** 명령 제출. 이전 `send<Op>`에서 이름 변경. 반환하는 `CommandCall<Op>`은 업무 단위 handle.
- `CommandCall<Op>::update(payload)`: 같은 활성 Source/Session 유지 + 새 Timestamp로 **기존 Command 갱신**. 종료된 명령에 대한 갱신은 오류; 새 command()로 묵시적 재생성 금지.
- `CommandCall<Op>::get()/wait(stage, timeout)`: 로컬 관찰. Timestamp/ProviderLost/TransportError/Timeout은 **원격 실패 상태가 아니다**. 장기 명령은 Terminal 대신 Commanded/Executing 등 대기 조건을 지정할 것.
- `cancel(call)`: 취소 **요청** 결과. 최종 CANCELED 여부는 `onStatus` 또는 `CommandCall`의 terminal 결과로 확인. 취소 거절 후 COMPLETED 가능.
- `onStatus<CommandOp>`: COMMANDed/Executing/Completed/Failed/Canceled 같은 상태 전이와 최종 결과를 받는 핵심 관찰 Hook. `onAck`은 별도 전달/접수 확인만 필요한 때, `onExecutionStatus<ExecutionOp>`는 실제 진행 수치가 필요한 때 선택. `onResponse` 단일 Hook은 기본 API에서 채택하지 않는다.
- `onRecoveredCommand<CommandOp>`: 재시작한 Consumer가 **이전 Command payload + 확인된 상태**를 보고 계속 추적/취소 여부 결정. 원본 payload 또는 이전 상태가 불명확하면 불명확함을 전달하고 중복 실행을 임의 추정하지 않는다. 이 Hook을 통한 복구는 onStatus와 별개 사용자 의사결정. 미등록 복구 정책은 구현에서 안전하게 규정해야 한다.
- ACK, Status, ExecutionStatus의 wire Operation을 하나의 `ReportConsumer` 데이터 구독으로 잘못 노출하지 않는다. 각각 `CommandConsumer`의 해당 관찰 Hook으로 전달한다.

## 5. ReportProvider<S>: 일반 데이터 발행·삭제

```cpp
template<class S> class ReportProvider {
public:
    template<class ReportOp> void publish(const typename ReportOp::Payload&);
    template<class ReportOp> PublishResult publish(
        const std::vector<typename ReportOp::Payload>&, PublishMode);
    template<class ReportOp> void dispose(const typename ReportOp::Key&);
};
```

- `publish`: 일반 Report의 현재 값/갱신 전송. `dispose`: Key를 가진 유효하지 않은 인스턴스 제거.
- `publish(snapshot, ReplaceOwnedInstances)`는 기존 `publishSnapshot` 별도 동사 대신 현재 소유 Key 전체 스냅샷을 제공. SDK가 **과거 자기 소유분만** 안전하게 정리할 수 있을 때만 수행; 데이터 소유권/Key 인벤토리가 없으면 실패 상태를 반환하고 임의의 타 Provider 인스턴스를 삭제하지 않는다.
- Query Data가 없는 기본 Request/Reply에서 Provider에게 `onRequest`, `onQuery`, `reply`는 **요구하지 않는다**. 요청마다 응답해야 하는 단발 RPC가 아니며, SDK가 구독과 데이터 배포를 관리한다.

## 6. ReportConsumer<S>: onReport 자동 구독 vs request 동적 구독

```cpp
template<class S> class ReportConsumer {
public:
    // 등록 단계: Component 시작과 함께 자동 구독
    template<class ReportOp, class Handler> void onReport(Handler);

    // 런타임 단계: 그때부터 구독, Handle이 구독을 소유
    template<class ReportOp, class Handler> Subscription request(Handler, const RequestOptions& = RequestOptions());

    template<class ReportOp> Optional<typename ReportOp::Payload> getLatest(
        const typename ReportOp::Key&) const; // 캐싱 사용 시에만
};
```

**구독 수명주기 계약:**

1. `m.consume<S>().onReport<Op>(&App::handler)`는 **정적 자동 구독**: Component가 Start될 때 생성, Stop 때 정리. Hook은 최초 유효 값, 갱신, 삭제(Disposed)에 반응하며 여러 번 호출될 수 있다.
2. `service<S>().reportConsumer().request<Op>(handler)`는 **명시적 동적 구독**: 호출 시 생성, 반환된 `Subscription`의 `close()`/소멸 시 정리. Handler를 함께 전달하므로 별도 onReport 등록은 요구하지 않는다.
3. **한 Component의 동일 S/Op에 정적 onReport와 동적 request를 동시에 등록하는 것은 기본적으로 `AlreadySubscribed` 오류**. 다른 필터/키로 병행 구독이 정말 필요해질 때 별도 옵션 계약을 확장한다. 암묵적 이중 콜백 금지.
4. `onReport` Callback은 `ReportEvent<T>`의 Data/Disposed, Key, Source, Timestamp, Complete/Incomplete 상태를 받는다. `getLatest`는 선택적인 SDK 캐시(키 기반)로서 최신 유효 값만 노출; Disposed된 키는 없는 값으로 처리.
5. 단순 `request<Op>()` 호출만으로 정적 onReport와 연결되거나 새 요청이 반드시 Reply 하나를 낳는다고 가정하지 않는다. 동적 API는 Handler를 명시한다.

`request`는 Queryless Request/Reply를 **데이터 수신 활성화**로 추상화한 SDK 이름이며, 개별 RPC Request Topic 발행이라는 뜻이 아니다.

## 7. 사용자 Payload 타입: 네 클래스 외에는 늘리지 않는다

- `LargeSet<T>`/`LargeList<T>`: 요소 ID 안정성, add/set/remove/clear/replaceAll, List insert/move, elements/size/state(Complete/Incomplete/Invalid). 빈 완성 컬렉션과 미수신/미완성 구분; 기본 수신 CompleteOnly, 특별한 경우 IncludePartial. **Command/Report Payload의 속성**으로 사용한다.
- `Specialized<Base>`: base/set<T>/is<T>/as<T>/state(Ready/Incomplete/Unsupported/Invalid), 선택적 type/identity. Base와 실제 Topic specialization의 Type/ID/Timestamp 연결 및 Dispose는 SDK 내부. `LargeList<Specialized<Base>>` 등 중첩 지원.
- 생성 `Optional<T>`, union, key(composite 포함)는 IDL 의미를 손실 없이 제공한다. Report의 full payload에 불필요한 DDS bookkeeping을 사용자에게 강제하지 않는다.

## 8. AutoPilot 중심 4 역할 예제

전체 C++11 문법 스케치는 [`../../examples/api_sketch/AUTOPILOT_FOUR_ROLES_v0.8.5.cpp`](../../examples/api_sketch/AUTOPILOT_FOUR_ROLES_v0.8.5.cpp)를 참조한다. 실제 응용 관계는 아래와 같다.

```text
MissionManager                          AutoPilot                         PositionSensor
CommandConsumer<GlobalVectorControl>    CommandProvider<GlobalVectorControl>
 command(cmd) ──────────────────────────> onCommand(cmd, info)
 onStatus(status) <────────────────────── respond(token, progress)
 cancel(call) ───────────────────────────> onCancel(info)

                                        ReportConsumer<GlobalPoseStatus>   ReportProvider<GlobalPoseStatus>
                                        onReport(pose) <────────────────── publish(pose)
                                        (자동 구독은 Component Start 시)
```

단일 AutoPilot Component가 **Command Provider와 Report Consumer 두 역할**을 맡고, 통합 예제의 주변 MissionManager·PositionSensor가 나머지 **Command Consumer와 Report Provider**를 맡아 네 클래스를 모두 실제 사용한다.

**대표 등록 (실제 ICD 명칭, 개선안 A):**

```cpp
namespace MO = umaa::services::MO;
namespace SA = umaa::services::SA;

UMAA_COMPONENT(AutoPilot, m) {
    m.provide<MO::GlobalVectorControl>()
      .onCommand<MO::GlobalVectorControl::setGlobalVector>(&AutoPilot::onSetGlobalVector)
      .onCancel<MO::GlobalVectorControl::setGlobalVector>(&AutoPilot::onCancelGlobalVector);
    m.consume<SA::GlobalPoseStatus>()
      .onReport<SA::GlobalPoseStatus::reportGlobalPose>(&AutoPilot::onGlobalPoseReport);
}
```

**동적 요청은 별도 대안:** `onReport<Op>`를 등록하지 않은 Component에서만 `auto sub = service<SA::GlobalPoseStatus>().reportConsumer().request<SA::GlobalPoseStatus::reportGlobalPose>(handler);` 를 사용한다. 기본 자동 구독과 중복 등록 금지 정책은 유지한다.

## 9. 이행표: 이전 등록 표현 → 개선안 A

| 이전 등록 표현 | 개선안 A (현재 v0.8.5) | 비고 |
|---|---|---|
| `m.provider<S>().command<Op>(handler)` (v0.8.4) | `m.provide<S>().onCommand<Op>(handler)` | Command Handler |
| `m.provider<S>().cancel<Op>(handler)` (v0.8.4) | `m.provide<S>().onCancel<Op>(handler)` | 취소 처리 |
| `m.commandProvider<S>().onCommand<Op>(handler)` (구 v0.8.5) | `m.provide<S>().onCommand<Op>(handler)` | 역할 등록을 2개로 통일 |
| `m.commandConsumer<S>().onStatus<CmdOp>(handler)` | `m.consume<S>().onStatus<CmdOp>(handler)` | Command 상태 |
| `m.commandConsumer<S>().onAck<CmdOp>(handler)` | `m.consume<S>().onAck<CmdOp>(handler)` | ACK 선택 |
| `m.commandConsumer<S>().onExecutionStatus<ExecOp>(handler)` | `m.consume<S>().onExecutionStatus<ExecOp>(handler)` | 실행 상세 선택 |
| `m.commandConsumer<S>().onRecoveredCommand<CmdOp>(handler)` | `m.consume<S>().onRecoveredCommand<CmdOp>(handler)` | 복구 판단 선택 |
| `m.reportConsumer<S>().onReport<ReportOp>(handler)` | `m.consume<S>().onReport<ReportOp>(handler)` | Queryless 자동 구독 |
| `m.reportProvider<S>()` | `m.provide<S>()` | Report Provider는 Hook 불필요 |
| `Component::send<CmdOp>(payload)` (v0.8.4) | `service<S>().commandConsumer().command<CmdOp>(payload)` | 런타임 Action |
| `Component::publish/respond/dispose` (v0.8.4) | `service<S>().reportProvider()/commandProvider()`의 Action | 런타임 위치 유지 |
| `CommandCall.update/get/wait` | `CommandCall<CmdOp>::update/get/wait` | 이번 개선에서 변경 없음 |

**소스 호환성:** 개선안 A는 **등록 소스 API 변경**이다. 네 가지 기존 등록 메서드가 영구 호환 alias로 제공된다고 가정하면 안 된다. 네 런타임 역할 클래스와 Action·Hook 이름 자체는 유지한다. 기존 v0.8.4는 history에 보존되며, 본 명세는 v0.8.5의 **2026-11-12 개정본**이다.

## 10. 검증 상태와 미해결 문제

| 항목 | 상태 |
|---|---|
| 네 클래스/Operation별 역할 타입·C++11 선언 스케치 | 예제 빌드 단계에서 확인 예정; 체크 결과는 별도 VALIDATION 기록을 따른다 |
| 원본 생성 SDK 헤더 / 전체 139 고유 Service 컴파일 | 미검증 |
| DDS 런타임 / UMAA ICD 적합성 | 미검증 |
| 이전 owned Report Key의 신뢰성 있는 재기동 정리 | P0: 실행·실패·타 Provider 격리 미검증 |
| 이전 상태/원본 Payload가 없을 때 Consumer/Provider 복구 | P0: 안전한 기본 정책 구현·검증 필요 |
| 등록 누락 onCancel, Liveliness-loss 업무 정지 | P1: Operation별 정책 및 검증 |
| C++11 Callback 소유권, Subscription 자동/동적 병행 범위, getLatest cache policy | P1: 런타임 contract/테스트 필요 |
| 동시 명령 ExecutionStatus 연관 / Old Token Stale 처리 | P1: 런타임 검증 필요 |

**참고:** ServiceType별로 ControlServiceProvider/SpecsServiceConsumer 등의 추가 기반 클래스를 생성하지 않는다. 실제 지원하는 `CommandProvider`/`ReportProvider` 조합은 생성 Operation 메타데이터가 결정한다.
