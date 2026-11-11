# UMAA Component SDK — 최소 사용자 Public API v0.8.4

**기준:** v0.8.3 사용자 API + UMAA ICD 6.0(EO/MO/MM/SA/SEM/SO 및 제공 EXP/CO-EXP) + 제공 IDL 594개  
**개정 목적:** `ServiceKind`(Config/Control/Specs/StatusReport), `Service` 상속, Component의 **Service별 Provider/Consumer 역할**, **역할에 종속된 Operation별 Handler**를 사용자 API에 반영한다.  
**검증 범위:** C++11 Public API 표현성. 실행/실제 DDS 동작/모든 생성 타입의 컴파일·ICD 적합성은 미검증.

## 0. 불변 원칙 — 빙산 구조

- **Public:** Component가 제공·소비하는 Service 선언, 업무 Command/Cancel Handler, 업무 Report Handler, 사용자 입력 데이터, 실제 실행 결과와 데이터 발행/삭제, 업무 복구 의사결정.
- **SDK 내부:** ICD 서비스/Operation 매핑, Queryless 구독 프로토콜, Command ACK/상태 머신, DDS 토픽·인스턴스·Liveliness, Command 갱신·ID 연결, Large Collection 원자적 동기화, Generalization/Specialization 연결. 단 사용자 판단에 필요한 식별·상태·오류 정보는 읽기 전용으로 전달한다.
- **중요:** ServiceKind 네 분류는 ICD §3.4의 일반적인 서비스 명명 규칙이지 사용자 Handler 인터페이스 네 가지를 강제하는 계약이 아니다. 실제 Flow Control은 각 Operation의 메타데이터에서 결정한다. 예외 서비스는 `Other`/명시적 Operation 목록으로 다룬다.
- **Command/Response**는 명령과 관련된 ACK/Status/ExecutionStatus의 흐름, **Queryless Request/Reply**는 일반 Report 구독 및 발행의 흐름이다. `query...` 또는 `cancel...` 이름이 있다고 별도의 Query Payload Handler나 CancelStatus Handler를 생성하지 않는다.

## 1. 사용자에게 보이는 클래스 모델

### 1.1 Service는 생성된 공통 기반 타입을 상속한다

```cpp
namespace umaa {

enum class ServiceKind { Config, Control, Specs, StatusReport, Other };
enum class FlowKind { CommandResponse, RequestReplyNoQueryData };
enum class OperationRole { Command, CommandAck, CommandStatus, ExecutionStatus, Report };

template<class Derived, ServiceKind Kind>
struct Service {
    static constexpr ServiceKind kind = Kind;
};

// Operation 타입 정보는 SDK/codegen이 공급한다. 앱이 구현하는 인터페이스가 아님.
template<class ServiceT, class PayloadT, OperationRole Role, FlowKind Flow>
struct Operation {
    typedef ServiceT ServiceType;
    typedef PayloadT Payload;
    static constexpr OperationRole role = Role;
    static constexpr FlowKind flow = Flow;
};
}
```

이 설계는 **제안된 C++11 공개 타입 형태**다. ICD가 특정 C++ CRTP 상속을 요구하는 것은 아니다. `Service`는 상태나 비즈니스 구현을 소유하지 않는 **생성 타입/메타데이터의 공통 기반**이고, 사용자는 `Service` 서브클래스를 직접 작성하지 않는다. `Component`는 `Service`를 상속하지 않는다.

**MO §6.1.5의 실제 `GlobalVectorControl` Service 구조를 반영한 생성 타입 예시:**

```cpp
namespace mo {
// 아래 Payload 이름은 공개 C++ 이름의 예시. 실제 IDL 원본명은
// GlobalVectorCommandType / GlobalVectorCommandAckReportType /
// GlobalVectorCommandStatusType / GlobalVectorExecutionStatusReportType.
struct GlobalVectorCommand;
struct GlobalVectorCommandAckReport;
struct GlobalVectorCommandStatus;
struct GlobalVectorExecutionStatusReport;

struct GlobalVectorControl
    : umaa::Service<GlobalVectorControl, umaa::ServiceKind::Control> {
    struct SetGlobalVector : umaa::Operation<GlobalVectorControl,
        GlobalVectorCommand, umaa::OperationRole::Command,
        umaa::FlowKind::CommandResponse> {};
    struct CommandAck : umaa::Operation<GlobalVectorControl,
        GlobalVectorCommandAckReport, umaa::OperationRole::CommandAck,
        umaa::FlowKind::CommandResponse> {};
    struct CommandStatus : umaa::Operation<GlobalVectorControl,
        GlobalVectorCommandStatus, umaa::OperationRole::CommandStatus,
        umaa::FlowKind::CommandResponse> {};
    struct ExecutionStatus : umaa::Operation<GlobalVectorControl,
        GlobalVectorExecutionStatusReport, umaa::OperationRole::ExecutionStatus,
        umaa::FlowKind::CommandResponse> {};
    typedef SetGlobalVector Command; // Command가 정확히 하나이므로 축약 허용
};
}
```

`Operation::Payload`, 역할, FlowKind, 소속 Service, 연관 Command(ACK/Status/ExecutionStatus) 메타데이터는 생성기로 일관되게 제공한다. 반드시 `struct`를 사용해야 한다는 뜻은 아니며, C++11 타입 태그/alias로 같은 계약을 만족해도 된다. `ServiceKind`나 `FlowKind`를 사용자에게 런타임 분기시키지 않는다.

### 1.2 Component가 Service별 Provider/Consumer 역할을 선언한다

```cpp
UMAA_COMPONENT(AutoPilot, m) {
    m.provider<mo::GlobalVectorControl>()
      .command<mo::GlobalVectorControl::SetGlobalVector>(&AutoPilot::onVector)
      .cancel<mo::GlobalVectorControl::SetGlobalVector>(&AutoPilot::onCancel);

    m.consumer<sa::GlobalPoseStatus>()
      .report<sa::GlobalPoseStatus::Report>(&AutoPilot::onPose);
}
```

- `m.provider<Service>()`와 `m.consumer<Service>()`는 **역할을 선언하는 구성 함수**다. 추가 DDS 통신 동사가 아니라 `Component ↔ Service`의 공개 모델이다.
- **한 Component가 같은 Service의 Provider·Consumer 둘 다 선언할 수 있다.** Service 정의 타입 자체에는 역할을 고정하지 않는다.
- `m.provider<Service>().command<CommandOp>(handler)`와 `.cancel<같은 CommandOp>(handler)`만 Provider Command 입력을 받는다.
- `m.consumer<Service>().report<ReportOp>(handler[, ReceiveOptions])`만 Consumer Report/ACK/Status/ExecutionStatus 이벤트를 받는다.
- 동일한 하나의 `Service` 안에 여러 Command/Report Operation이 있다면 **각 Operation을 명시**한다. Service에 정확히 하나의 Command가 있으면 `.command(handler)`, `.cancel(handler)` 생략형 가능. `.report(handler)`는 해당 Service에 구독 가능한 Report Operation이 정확히 하나일 때만 가능.
- `send<CommandOp>`와 `cancel(call)`은 Consumer의 업무 동작, `respond(token, result)`, `publish<ReportOp>`, `dispose<ReportOp>(key)`는 Provider의 업무 동작이다. Report 재시작 시 잔존 인스턴스를 정리하려면 아래 전체 스냅샷 오버로드를 사용한다. Command에 종속된 ExecutionStatus 데이터는 아래 `publish<ExecutionOp>(token, value)` 오버로드를 권장한다. 해당 Service 역할이 선언되어 있어야 한다.
- Provider 역할만 선언해도 된다. 특히 **Specs/StatusReport Provider에는 `command`/`cancel` Handler가 필요하지 않다.** `onStart` 또는 업무 이벤트에서 `publish`로 값을 제공한다.
- 역할은 선언이고 실제 서비스 제공 완료의 증명은 아니다. 등록 시 제공할 필수 Command Handler의 누락·오등록을 검사할 수 있지만, 실제 Report 값을 언제 발행하는지까지 정적 타입 검사로 보증할 수는 없다.

### 1.3 공개 함수의 최소 집합

| 공개 API | 역할 | 사용자 책임 |
|---|---|---|
| `m.provider<S>()` | 구성 | S의 Provider 역할 선언 |
| `m.consumer<S>()` | 구성 | S의 Consumer 역할 선언 |
| `.command<Op>(handler)` | Provider 이벤트 | 새·갱신·복구 명령을 업무적으로 검증·처리 |
| `.cancel<Op>(handler)` | Provider 이벤트 | 기존 Command 업무 중단 판단·처리 |
| `.report<Op>(handler[, options])` | Consumer 이벤트 | 업무 Report/Status 수신 |
| `send<Op>(payload[, options])` | Consumer 동작 | 명령 송신 및 결과 관찰 |
| `cancel(call_or_id)` | Consumer 동작 | 명령 취소 요청 |
| `respond(token, result)` | Provider 동작 | 업무 상태·실제 결과 알림 |
| `publish<Op>(value[, options])` | Provider 동작 | 독립 Report·Specs·상태 데이터 발행 |
| `publish<ExecutionOp>(commandToken, value)` | 같은 동사의 오버로드 | 여러 활성 Command 중 ExecutionStatus가 속한 Command를 지정 |
| `publish<ReportOp>(snapshot, ReplaceOwnedInstances)` | 같은 동사의 오버로드 | 현재 유효한 keyed Report 전체 집합을 선언해 과거 인스턴스 정리 |
| `dispose<Op>(key)` | Provider 동작 | 일반 데이터 인스턴스 삭제 |

**집계:** *업무 동사*는 기존 8개(command/cancel-handler/report/send/cancel-call/respond/publish/dispose)이며, `provider/consumer`라는 **역할 구성 함수 2개를 추가**한다. 따라서 사용자에게 보이는 함수 종류 총합을 8개라고 말하면 부정확하다. `onStart()/onStop()`은 선택적 수명주기 훅이다.

### 1.4 이전 직접 등록 형태와의 관계

기존 `m.command<Op>(handler)`, `m.cancel<Op>(handler)`, `m.report<Op>(handler)`는 v0.8.3 설계의 형태다. **v0.8.4의 정규 표기는 역할 한정 등록**이다. 구형 표기를 지원하는 경우 컴파일·등록 단계에서 해당 Service의 Provider/Consumer로 확실히 귀속하는 **전달용 별칭**으로만 취급하고, 새 설계의 필수 경로는 아니다. 두 형태로 같은 역할/Operation을 중복 등록하면 오류다. **실제 구버전 바이너리 호환성은 검증하지 않았다.**

### 1.5 Provider·Consumer별 허용 행위와 Flow Control

| ServiceKind | Provider가 사용자에게 요구하는 일 | Consumer가 사용자에게 요구하는 일 | Flow 해석 |
|---|---|---|---|
| Control | Command Handler·취소 처리·실제 ExecutionStatus 보고 | Command 송신/취소, 필요한 상태 구독 | Command Op별 Command/Response |
| Config | 각 Config Command Handler·취소 처리·설정 Report 발행 | Config Command 송신/취소, 설정 Report 수신 | Command와 일반 Report 혼합 가능 |
| Specs | 제원 데이터 `publish` | `.report` 등록 | 일반 Report는 Queryless Request/Reply |
| StatusReport | 상태 데이터 `publish`·필요 시 `dispose` | `.report` 등록 | 일반 Report는 Queryless Request/Reply |
| Other | 서비스별 Operation 선언에 따른 업무 | 서비스별 Operation 선언에 따른 업무 | Operation 메타데이터 우선 |

**주의:** 표는 기본적인 사용자 행위의 예시다. `Config`라고 모든 Command/Report가 반드시 존재하는 것은 아니며, 일부 특수 서비스의 Operation 조합은 해당 ICD 정의가 우선한다. ACK/CommandStatus 생성, 구독 활성화, DDS Dispose 취소, Liveliness와 상태 전이는 모두 SDK가 담당하되 업무 데이터·정지 판단은 앱이 제공한다. Command별 ExecutionStatus 발행에는 `CommandToken`을 지정해 관련 Session을 식별한다.

### 1.6 타입 안전성 및 Service 제공 범위 검증

1. `m.provider<S>()`/`m.consumer<S>()`의 `S`는 생성 Service 타입이어야 한다. Command/Report Payload를 Service 타입으로 전달하면 오류다.
2. Provider `.command<Op>`/`.cancel<Op>`는 **해당 Service의 Command Operation**만 허용하고, Consumer `.report<Op>`는 **해당 Service의 출력 Report 계열**만 허용한다. 다른 Service 소속 Operation, 방향 불일치, 중복 등록은 컴파일 또는 등록 오류.
3. `m.provider<Service>().cancel<Op>`에서 Op는 별도의 `cancelGlobalVectorCommand`가 아닌 **원래 `setGlobalVector` Command Op**를 가리킨다. 별도 CancelStatus Handler를 만들지 않는다.
4. `m.provider<S>()`에 Command Handler가 빠지면 서비스별 필수 입력 범위를 검사하여 불완전 선언을 검출한다. 반대로 Specs/StatusReport는 Handler 없이 Provider 선언이 유효하다. Report 발행의 실시간 충분성·업무 정합성은 정적 보장 불가. Report-only Provider의 `publish` 호출 또는 현재 유효한 전체 Snapshot 제출은 별도 업무 책임이다.
5. Command별 Cancel Handler가 필요한지 여부는 장비 중단 요구에 따라 결정한다. **미등록을 취소 성공으로 처리하지 않는다.** Consumer liveliness 상실에 따른 업무 정지가 필요한 경우에는 실제 처리가 가능해야 한다. SDK의 정책 검증과 업무 능력 제약은 별도 명세로 유지한다.
6. 복수 Provider의 목적지 선택과 복수 Consumer의 동시 구독은 역할 선언과 독립적으로 기존 `SendOptions`/Report Handler로 처리한다.
7. 하나의 Component에서 동일 S의 Provider/Consumer 양쪽 등록은 합법이며, 각 역할의 이벤트/출력 범위는 분리한다.

## 2. Command Provider: 사용자 의사결정 (EO §5.1.3–5.1.4)

### 2.1 `CommandInfo` — 원본/문맥 보존

**반드시 사용자 접근 가능:** `id()`(원본 Source+Session 구별), `source()`, `destination()`, `timestamp()`, `isUpdate()`, `isRecovered()`, `previousStatus()`(없으면 Unknown/Optional), `token()`(수신된 갱신 버전의 불변 핸들).

`source`, `sessionID`, `destination`, `timeStamp`는 실제 Command IDL에 있고, 갱신은 **동일 Source/Session + 더 최신 Timestamp**로 판정된다. `CommandToken`이라는 이름/갱신 세대 핸들은 ICD 필드가 아닌 **SDK 비동기 결과 혼선 방지 장치**다. ICD에 존재하지 않는 별도 wire-level Command revision ID를 만들어 보여 주지 않는다.



### 2.1.1 Provider `.command<Op>` Handler 호출 계약 (v0.8.4)

**기본 Signature(의미 기준)**: `CommandDecision onCommand(const Op::Payload& payload, const CommandInfo& info)`; `Op::Payload`는 생성된 Operation의 별칭이다. 등록할 Operation은 *Command 역할*이어야 한다.

- `info.kind()`: `CommandDeliveryKind::{New, Updated, Recovered}`. `isUpdate()` / `isRecovered()`는 이에 대응하는 선택적 별칭으로 유지할 수 있다. `Recovered`는 복구된 Command의 마지막 Payload를 전달하며, 복구 전 이미 한 차례 갱신되었더라도 이번 전달 원인은 `Recovered`다.
- `Updated`: 같은 Source + Session의 **더 새로운 Timestamp**를 가진 활성 Command. 전체 최신 Payload가 전달되며, Provider는 그것을 새로운 실행 판단 대상으로 검증한다. SDK가 자동 병합한 델타로 가정하지 않는다.
- `Recovered`: 기존 Command Payload와 확인 가능한 이전 상태(없으면 `Unknown`)를 전달한다. **무조건 새 실행처럼 재시작하지 않는다.** 사용자가 계속 수행할지 `SERVICE_FAILED`로 종료할지 결정한다.
- `token()`: 수신된 **이번 버전의 Provider 응답 핸들**. `respond()`에만 전달한다. 표준 wire-level revision ID가 아니다.
- Handler가 `Accepted`를 반환했다는 사실은 **업무 검증 완료/시작 준비 완료**를 뜻하지 `EXECUTING` 또는 `COMPLETED`를 뜻하지 않는다.

**동일 Command Operation의 Handler로 세 경우를 모두 다루므로** `m.updateCommand`, `m.recoverCommand` 같은 별도 이벤트는 추가하지 않는다. 갱신·복구 시 비즈니스 처리 방식이 다르면 `info.kind()`로 분기한다. 오래된 비동기 작업이 `respond(staleToken, ...)`을 호출하면 성공으로 보고하지 않는다.

### 2.1.2 최소 Handler 반환 의미

| `CommandDecision` | 사용자 판단 | 연결되는 원격 Command 흐름 |
|---|---|---|
| `accepted()` | 검증·준비 완료 | SDK가 COMMANDED 단계로 진행 가능; 실제 시작 시 `respond(...executing())` |
| `rejected(reason)` | 검증 또는 준비 불가 | 해당 ICD에서 허용하는 `FAILED(reason)` |
| `deferred()` | 검증·준비 판단 자체가 아직 끝나지 않음 | 나중에 `respond(token, commanded()/failed(reason))`; **DEFERRED는 ICD Status 아님** |
| `resume()` | 복구된 명령의 확인된 이전 상태에서 계속 수행 | `Recovered`에만 허용; 새 COMMANDED 상태로 되감지 않음 |

ACK 생성 여부나 `ISSUED` 상태 전환을 Handler에서 직접 호출하게 하지 않는다. `respond`를 통해 요청한 상태 변경은 ICD에서 허용한 전이 및 Reason만 수용한다. 이미 완료된 Command의 새 Update는 독립적인 새 `send()`가 필요하다.

### 2.2 `CommandDecision` / `respond`

- `CommandDecision::accepted()`: 업무 검증/준비 완료. `COMMANDED`에 진행할 수 있음. **COMPLETED 아님**.
- `CommandDecision::rejected(CommandReason)`: ICD 허용 Reason을 가진 `FAILED`.
- `CommandDecision::resume()`: 복구된 명령을 이전 상태와 연속성을 유지하여 이어 실행. `accepted()`와 달리 새 COMMANDED 전이를 강제하지 않음.
- `CommandDecision::deferred()`: 검증/준비가 지연되어 나중에 `respond(token, commanded())` 또는 `respond(token, failed(reason))`를 호출하는 **SDK 비동기 UX**. `DEFERRED`는 UMAA 표준 Status가 아님.
- `respond(token, commanded()/executing()/completed()/failed(reason)/canceled())`: 실제 앱 실행 상태/결과를 전달. ICD의 상태 전이와 Status-Reason 조합을 벗어나면 명시적 오류.
- **`accepted()` 반환 즉시 `executing()`으로 간주하지 않는다.** 하드웨어가 실제 실행하기 전에는 `EXECUTING`을 거짓으로 보고하지 않아야 한다.
- 새 Command 또는 갱신에 대한 ACK/Status 관리 자체는 앱이 개별 Topic 동사로 처리하지 않아도 되게 한다. Command-specific `ExecutionStatus` 값은 앱이 **`publish<ExecutionStatusOp>(token, value)`**로 제공한다. 이는 여러 Command가 동시에 살아 있을 때 어느 실행에 속한 데이터인지 명확히 지정하기 위한 기존 `publish`의 오버로드다. 원본 IDL 전체를 직접 채우고 키/Session을 조립하도록 강제하지 않는다.

**갱신 버전 안전:** `respond(oldToken, ...)`이 이미 새로 갱신된 Session의 상태를 덮어쓰면 안 된다. 오래된 토큰의 응답은 `StaleToken` 등 오류로 처리한다. 다만 wire-level Status에는 별도 갱신 버전 필드가 없으므로 Consumer에게 수신 Status의 버전 귀속이 항상 확정되었다고 말할 수 없다.

## 3. Command Consumer: 상태 관찰, 갱신, 취소, 복구 (EO §5.1.2–5.1.4)

### 3.1 `CommandCall` 및 결과

필수 사용자 경로:

- `CommandCall.id()`: 표준 Source/Session에 해당하는 명령 식별.
- `CommandCall.update(newPayload)`: 동일 활성 Session의 Command를 갱신. 종결 상태에서는 실패; 새 Command로 묵시적 재해석 금지.
- `CommandCall.get()` 또는 전달된 Callback: `SendOptions.responseStage(Ack/Commanded/Terminal)`에서 정한 관찰 조건까지 상태를 얻음. `ResponseStage`는 사용자 대기 조건이고 `CommandStatus`와 다르다.
- `m.consumer<Service>().report<StatusOp>()` 및 필요 시 `.report<AckOp>()`, `.report<ExecutionOp>()`: 명령의 중간 상태/실행 상태를 계속 구독.
- `CommandResponse`: `id`, `source`, `destination`(발신 Command 문맥에 기록된 경우), `status`, `reason`, `timeStamp`, `outcome`, `association`(현재 갱신 귀속 Confirmed/SessionOnly/Unknown), `logMessage`(진단용) 등의 읽기 전용 정보 보존.
- `outcome`: **원격 Terminal 결과**(Completed/Failed/Canceled)와 **관찰 결과 불명/로컬 오류**(Timeout/ProviderLost/TransportError/ProtocolError)를 분리. 로컬 Timeout을 원격 `FAILED(TIMEOUT)`으로 변환하지 않음.

**UX 결정:** v0.8.1의 `get()` 기본 Terminal 대기는 그대로 유지하되, 종료 시간이 없는 지속형 명령은 `responseStage(Commanded)`를 명시하도록 문서에 강하게 안내한다. `wait_for()`는 선택적 편의 함수이며 원격 상태 전이를 의미하지 않는다.

### 3.2 `cancel(call_or_id)`와 `m.cancel`

- `cancel()`의 의미는 *취소 의사 제출*이다. 호출 성공이 원격 `CANCELED`를 보장하지 않는다.
- `CancelAttempt.submission()`은 제출 성공/실패/불명, `CancelAttempt.final()`은 원래 명령의 Terminal 관찰을 나타낸다(제안 결과 타입; 새로운 동사 아님).
- `m.cancel` Handler는 `CancelDecision::canceled()`(실제 중단 확인), `deferred()`(중단 진행 중), `cannotCancel()`(불가능; 원 실행 계속 진행) 중 업무에 맞게 처리할 수 있어야 한다. **이들은 SDK 반환값이지 별도 UMAA Cancel Status 메시지가 아님**.
- 취소 실패 후 원 명령이 `COMPLETED`가 될 수 있다. Liveliness Loss로 인해 취소 처리해야 하는 경우도 있다. Cancel 원인을 알 수 없으면 `Unknown`을 반환해야 하며, 이를 항상 `ConsumerRequest`로 단정하면 안 된다.



### 3.2.1 Provider `.cancel<Op>` Handler 호출 계약 (v0.8.4)

**기본 Signature(의미 기준)**: `CancelDecision onCancel(const CancelInfo& info)`; `Op`는 반드시 **원래 Command Op**를 지정하며, 취소용 별도 `CancelCommandStatus` Operation을 등록하지 않는다. `CancelInfo`는 다음 정보를 제공한다.

| 필드 | 앱이 알아야 하는 이유 | 확정성 |
|---|---|---|
| `commandId()` | 기존 Source+Session 명령 식별 | 원본에서 확보될 때 확정 |
| `token()` | 취소 대상 활성 Command 버전과 비동기 `respond` 연결 | SDK 핸들 |
| `trigger()` | `ExplicitDispose`, `ConsumerLost`, `Unknown` 등 원인 | 원인을 판별할 수 없으면 `Unknown` |
| `previousStatus()` | 직전 확인 상태 | 모르면 `Unknown` |
| `source()`, `destination()` | 명령 소유자/대상 판단 | 원본에서 확보된 범위 |

`CancelDecision`은 **무엇을 보냈는지**가 아니라 **실제 장치에 대해 무엇을 확인했는지** 나타낸다.

- `canceled()` : 사용자가 *실제 취소가 끝났음*을 확인했을 때만 선택. `CANCELED`로 종결할 수 있다.
- `deferred()` : 중단 조치 중이고 아직 결과가 나오지 않았을 때. 안전 중단 결과가 확인되면 `respond(info.token(), CommandResult::canceled())`를 호출한다. 중단되지 않고 실행이 완료되면 실제 최종 결과를 보고한다.
- `cannotCancel()` : 이미 시작된 업무를 중단할 수 없음. **별도의 Cancel 실패 응답을 발행하는 것이 아니라 원래 Command를 계속 추적**한다. 정상 완료되면 `COMPLETED`일 수 있다.

`m.cancel`은 **활성 Command에 대한 취소 요청에서만** 호출한다. 이미 Terminal인 명령에 대한 늦은 취소는 원본 Terminal 의미를 뒤집지 않는다. 갱신된 명령의 이전 토큰으로 현재 명령을 취소했다고 오인해서는 안 된다.

**Handler 미등록 정책 (중요):** `m.provider<Service>().command<Op>`만 등록하고 `m.provider<Service>().cancel<Op>`를 생략해도 *장비가 안전하게 중단되었다고 SDK가 가정해서는 안 된다*. 애플리케이션의 취소/정지가 필요한 비동기·지속 명령은 대응 `m.provider<Service>().cancel<Op>` 등록을 **필수 사용자 계약**으로 한다. 이미 종료되었거나 해당 Operation의 의미상 별도 업무 취소가 없는 경우는 **명시적으로 검증된 정책**으로 생략할 수 있으나, 미등록이 곧 `CANCELED`라는 해석은 금지한다. 누락/충돌은 Provider 서비스 등록 검증에서 경고가 아니라 **서비스 완전성 오류**로 취급한다. (이 마지막 검증 방식은 ICD가 지정한 C++ 요구사항이 아니라 SDK 설계 안전 규칙이다.)

**명령 연결 상실:** EO §5.1.4.5의 Consumer liveliness 상실에 따른 취소 처리는 실제 업무 중단이 필요하다면 동일 `m.cancel` Handler로 전달한다. 연결 상실 원인을 항상 확정할 수는 없으며, 일반적인 Service 발견/소멸 이벤트까지 `m.cancel`에 섞지 않는다.

### 3.2.2 `cancel()`과 `m.cancel`은 다른 측면의 한 흐름

| 주체 | API | 책임 |
|---|---|---|
| Consumer | `cancel(CommandCall 또는 id)` | 기존 명령에 대한 **취소 의사 제출**; 최종 결과와 구분 |
| Provider | `m.provider<Service>().cancel<SameCommandOp>` | 취소 요청을 수신하고 실제 정지 가능성 판단 |
| Provider | `respond(token, canceled()/completed()/failed(...))` | 지연 중단/원래 업무의 **실제 결과** 보고 |
| Consumer | 기존 `CommandCall`/`m.consumer<Service>().report<StatusOp>` | 원격 `CANCELED` vs `COMPLETED` vs 결과 불명 확인 |

ICD는 별도 Cancel Reply를 요구하지 않는다. `m.cancel` 반환 타입을 새 DDS CancelStatus 메시지처럼 간주하지 않는다.

### 3.3 재기동·복구 의사결정

- Provider: Provider `.command<CommandOp>` Handler에 `Recovered`와 `previousStatus`를 전달한다. 사용자는 장치의 실제 상태를 확인하고 **이어 수행하거나 `FAILED(SERVICE_FAILED)`로 처리**한다. `CommandDecision::resume()`은 **기존 상태에서 계속 수행**하겠다는 결과(새로운 SDK 결과 종류)이며, 이전 상태가 EXECUTING인 복구 명령을 무조건 COMMANDED로 되돌리는 일이 없어야 한다. `previousStatus`가 확인되지 않으면 임의의 상태 전이를 만들지 않는다.
- Consumer: ICD §5.1.2.2의 기존 명령 복구/취소 판단은 **Consumer 역할 구성 옵션**인 `m.consumer<S>(ConsumerOptions)`에 속한다. `ConsumerOptions`는 기본 `RecoveryPolicy`(기본값: 명시적인 Cancel/안전 정책) 및 선택적인 **Operation별 복구 판단 Callback**을 지원한다. Callback에는 원래 Command Payload, Source/Session/대상, 확인된 이전 상태가 포함된 `RecoveredCommand<Op::Payload>`를 전달하고 `RecoveryDecision::{Continue, Cancel}`을 받는다. **정확한 Callback 구성 구문은 보조 선언 스케치에서 `ConsumerOptions::onRecovered<Op>(handler)`로 정의한다.** Status Report를 구독했다고 원본 Command Payload가 자동으로 복구된다고 가정하지 않는다.
- 연결 상실: EO §5.1.4.5가 규정하는 **명령 관련** Liveliness 상실에 따른 안전 취소 처리 및 결과 불명 표현을 지원한다. 범용 서비스 사용 가능/불가능 이벤트 `onServiceEvent()`는 여기서 필수로 추가하지 않는다.

## 4. Report: 실제 데이터, Key, Dispose (EO §5.2.1; §6.1)

동일 `m.consumer<Service>().report<Op>`의 Handler 오버로드를 제공한다.

```cpp
void onData(const ReportPayload& value);                // 정상 Data만 필요한 사용자
void onEvent(const umaa::ReportEvent<ReportPayload>& e); // 삭제·Key·Source까지 필요한 사용자
```

`ReportEvent<T>`의 필수 정보 (원본 `source`를 반드시 DDS Writer/Provider 식별자로 간주하지 않음):

- `kind()` = `Data` 또는 `Disposed` (ICD 데이터 관점의 필수 사건). `data()`는 `Data`일 때만 유효.
- `key()` = 원본 타입의 **모든 Key 필드**. 키를 무조건 NumericGUID 하나로 줄이지 않음.
- `source()`, `timestamp()` = 원본 식별과 원본 생성시각을 보존; 수신시각과 구분.
- Data에 포함된 모든 `Optional<T>`, IDL Union 분기, Enum 및 제약 값을 손실 없이 접근 가능.

`ReportEvent::Unavailable/Fault`, `isInitial`, `revision`, `onServiceEvent`는 **기본 ICD 커버리지를 위해 필수라고 주장하지 않음**. 별도의 모니터링 편의 기능으로 제공 가능하나 기본 필수 타입에는 넣지 않는다.

`dispose<Op>(key)`는 **일반 데이터 인스턴스 폐기**이고 Command `cancel()`과 다른 사용자 동사다. 삭제 이벤트가 필요한 소비자는 반드시 `ReportEvent<T>` Handler를 선택한다.

**Report-only Provider 재시작 보완:** 앱이 현재 유효한 keyed Report 집합을 알고 있고 과거 SDK 인스턴스 키는 모를 수 있으므로, 같은 동사 `publish`에 선택적인 **권위 있는 전체 Snapshot 오버로드**를 정의한다.

```cpp
// 초기화 또는 업무상 전체 교체 시: 이 Component가 소유한 동일 ReportOp 인스턴스 집합
publish<ReportOp>(currentValues, umaa::PublishMode::ReplaceOwnedInstances);
```

`currentValues`는 `std::vector<ReportOp::Payload>` 등의 논리적 집합이며, SDK는 **해당 Provider가 소유한 같은 Operation의 이전 keyed 인스턴스 중 새 Snapshot에 없는 것**을 폐기한다. 이 명령은 Report의 전체 집합에 대한 SDK 편의 계약이지 DDS에서 전역 원자적 트랜잭션이 된다는 보장은 아니다. SDK가 과거 소유 키 집합을 신뢰성 있게 확인할 수 없으면 성공인 척하지 않고 명시적 오류를 반환해야 한다. 기존 한 건 `publish<Op>(value)`는 Upsert이고, 필요한 키를 앱이 알고 있을 때는 기존 `dispose<Op>(key)`도 계속 유효하다.

## 5. LargeSet<T>/LargeList<T> (EO §3.8)

새 `m.collection` 또는 `publishCollection` 동사는 불필요. **Command/Report payload 안에서 동일 컬렉션 모델을 사용**한다.

**필수 공개 의미:**

- Collection `id()`, `size()`, `elements()`, `state()`; `state`는 적어도 `Complete/Incomplete/Invalid`를 구분. 빈 완성 Collection과 미수신 Collection을 다르게 나타냄.
- Set: Element `id`, `add`, `set`, `remove`, `clear`, `replaceAll`.
- List: 위 기능 + `insert`, `move`, `elements`의 논리적 순서 유지. 위치와 Element ID는 서로 다름.
- 구독 Handler 기본값은 **완성된 논리적 Snapshot**을 전달한다. 중간/누락 데이터가 관측되면 정상 완성 데이터로 위장하지 않는다. **ICD §3.8.3은 소비자가 부분 Element를 즉시 처리할지, 원자적 업데이트 완료를 기다릴지 선택할 수 있게 한다.** 따라서 `m.consumer<Service>().report<Op>(handler, ReceiveOptions{delivery=CompleteOnly|IncludePartial})`처럼 **기존 등록 함수의 옵션**으로 부분 이벤트를 선택할 수 있어야 한다. `IncludePartial`을 선택한 경우 `CollectionState::Incomplete`와 개별 Element/변경 정보를 관찰할 수 있어야 하며, 완성되지 않은 상태를 `Complete`로 위장하지 않는다. (옵션 구문은 C++11 계약 개념 표기이며 실제 선언부 아님)
- `publish<Op>(containingPayload)` 또는 `send<Op>(containingCommand)`가 논리적 업데이트 단위를 표현한다. 초기 생성, 추가, 수정, 요소 Dispose, 빈 Collection 및 List 순서 변경을 표현할 수 있어야 한다.

`changes()`는 이미 노출한다면 정확히 Added/Updated/Removed/Moved/Reset을 정의해 유지할 수 있지만, **ICD가 사용자에게 변경 이력 API 함수를 요구하는 것은 아니다.** 원자적 Snapshot과 Element ID로 기능 표현은 가능하므로 필수 동사/타입 계약으로 강제하지 않는다.

## 6. Specialized<Base> (EO §3.9)

ICD는 Generalization-Specialization의 **ID / Topic / Timestamp 연결**, 업데이트와 양쪽 폐기를 규정한다. 이를 사용자 데이터 객체로 표현한다.

- `base()`: 공통 필드.
- `is<T>()`, `as<T>()`: 실제 타입 확인·타입 안전 접근.
- `set(const T&)`: 해당 특수 타입 생성·갱신.
- `type()`/`identity()`: Specialization 종류와 원본 ID/Timestamp 등 연결 정보 보존.
- `state()`: 적어도 `Ready/Incomplete/Unsupported/Invalid`를 구분. `is<T>()`가 true여도 아직 완성되지 않으면 `as<T>()`는 유효 데이터처럼 반환하지 않는다.
- 제거는 **해당 Report/Containing Instance의 `dispose<Op>(key)`**, 또는 Collection Element `remove/replaceAll` 등의 기존 행위로 표현한다. 별도 최상위 `disposeSpecialization()`을 만들지 않는다.

`Unsupported` 같은 구체 열거형은 SDK 설계지만, 불완전/미지원 값을 정상 타입으로 오인하지 않게 하는 **정보 보존 선택**이다. 구체 타입을 이해하지 못하는 경우 원래 타입 식별자를 잃지 않는다. 원격 확장 타입 Payload의 무제한 파싱·포워딩까지 보장하지 않는다.

## 7. 타입 생성의 공개 의미 (각 ICD의 Service Interfaces, IDL)

- `@key` → 1개 이상의 원본 Key 구성요소를 모두 보존하는 typed key.
- `@optional` → 미설정과 값의 기본값을 구분할 C++11 `Optional<T>` 또는 동등 타입.
- IDL Union → discriminator와 선택된 branch를 보존하는 타입; 임의의 일반 base 타입으로 평탄화하지 않음.
- 식별자, 원본 timestamp, enum 값, 제약 숫자값 및 중첩 타입을 보존한다.
- 서비스에 여러 Operation이 있을 때 `Service::Operation`을 구체 선택해 등록/송신/발행한다.
- 별도의 Topic/Metadata/Elements를 사용하더라도 사용자 객체는 단일 논리적 Collection/Specialization으로 접근할 수 있어야 한다.

## 8. C++11 사용자 코드 — Role 기반 정규 예시

> `mo::GlobalVectorControl`, `sa::GlobalPoseStatus` 등의 공개 타입명은 SDK 코드 생성에 맞춰 확정한다. 아래 코드는 사용자 UX의 **설계 예시**이며 실제 SDK 구현 코드가 아니다.

```cpp
class AutoPilot : public umaa::Component {
public:
    umaa::CommandDecision onVector(
        const mo::GlobalVectorCommand& cmd,
        const umaa::CommandInfo& info) {
        if (info.kind() == umaa::CommandDeliveryKind::Recovered) {
            if (canResumeHardware(info))
                return umaa::CommandDecision::resume();
            return umaa::CommandDecision::rejected(
                umaa::CommandReason::ServiceFailed);
        }
        if (!validate(cmd))
            return umaa::CommandDecision::rejected(
                umaa::CommandReason::ValidationFailed);
        startHardwareAsync(cmd, info.token());
        return umaa::CommandDecision::accepted();
    }
    umaa::CancelDecision onCancel(const umaa::CancelInfo& info) {
        return requestSafeStop(info.token())
            ? umaa::CancelDecision::deferred()
            : umaa::CancelDecision::cannotCancel();
    }
    void onPose(const sa::GlobalPoseReport& pose);
    void onHardwareStopped(const umaa::CommandToken& token) {
        respond(token, umaa::CommandResult::canceled());
    }
};

UMAA_COMPONENT(AutoPilot, m) {
    m.provider<mo::GlobalVectorControl>()
      .command<mo::GlobalVectorControl::SetGlobalVector>(&AutoPilot::onVector)
      .cancel<mo::GlobalVectorControl::SetGlobalVector>(&AutoPilot::onCancel);

    m.consumer<sa::GlobalPoseStatus>()
      .report<sa::GlobalPoseStatus::Report>(&AutoPilot::onPose);
}
```

**Report Provider가 현재 유효한 키 집합 전체를 제공:**

```cpp
publish<sa::GlobalPoseStatus::Report>(
    currentPoseReports, umaa::PublishMode::ReplaceOwnedInstances);
```

**Control Provider의 실행 중 업무 데이터 발행:**

```cpp
// 복수 Command가 진행 중이더라도 session 연결을 SDK가 책임지도록 지정
publish<mo::GlobalVectorControl::ExecutionStatus>(token, currentVectorStatus);
```

**Control Consumer의 선택적 진행 보고:**

```cpp
UMAA_COMPONENT(MissionManager, m) {
    m.consumer<mo::GlobalVectorControl>()
      .report<mo::GlobalVectorControl::ExecutionStatus>(
          &MissionManager::onVectorExecution);
}

// Consumer role이 선언된 Component의 업무 메서드에서 사용
const auto call = send<mo::GlobalVectorControl::SetGlobalVector>(cmd);
// cancel(call);  // 실행을 끝내야 할 때. 성공 제출 ≠ 원격 CANCELED.
```

Consumer 재기동 시 원래 Command Payload를 보고 복구 여부를 판단해야 하는 경우:

```cpp
UMAA_COMPONENT(MissionManager, m) {
    umaa::ConsumerOptions opts;
    opts.onRecovered<mo::GlobalVectorControl::SetGlobalVector>(
        &MissionManager::onRecoveredVector);
    m.consumer<mo::GlobalVectorControl>(opts)
      .report<mo::GlobalVectorControl::ExecutionStatus>(
          &MissionManager::onVectorExecution);
}
// onRecoveredVector(const RecoveredCommand<mo::GlobalVectorCommand>&)
// 는 RecoveryDecision::Continue 또는 ::Cancel을 반환.
```

### 8.1 Specs/StatusReport의 Provider와 Consumer

```cpp
UMAA_COMPONENT(Sensor, m) {
    m.provider<sa::SomeSensorSpecs>();  // 이 서비스에는 업무 Command Handler 없음
    m.provider<sa::SomeSensorStatus>();
}
// onStart() 또는 센서 갱신 시:
publish<sa::SomeSensorSpecs::Report>(specs);
publish<sa::SomeSensorStatus::Report>(status);

UMAA_COMPONENT(Monitor, m) {
    m.consumer<sa::SomeSensorStatus>()
      .report<sa::SomeSensorStatus::Report>(&Monitor::onStatus);
}
```

`SomeSensorSpecs`/`SomeSensorStatus`는 **구조 설명용 가상 타입명**이다. Report Provider가 키/잔존 데이터를 갱신·폐기해야 한다는 ICD 의무는 유지되지만, 사용자에게 DDS Reader/Writer 또는 `onQuery`를 요구하지 않는다.

### 8.2 다중 Command Config + 일반 Report 혼합

```cpp
// CO-EXP CommsChannelConfig를 단순화한 타입명 예시 (모든 Operation 나열 아님)
UMAA_COMPONENT(CommsManager, m) {
    m.provider<co::CommsChannelConfig>()
      .command<co::CommsChannelConfig::AddMessage>(&CommsManager::onAdd)
      .cancel<co::CommsChannelConfig::AddMessage>(&CommsManager::onCancelAdd)
      .command<co::CommsChannelConfig::DeleteMessage>(&CommsManager::onDelete)
      .cancel<co::CommsChannelConfig::DeleteMessage>(&CommsManager::onCancelDelete);

    m.consumer<co::CommsChannelConfig>()
      .report<co::CommsChannelConfig::ConfigReport>(&CommsManager::onConfig);
}
```

Service 하나에 Command Operation이 여러 개이므로 `.command(handler)` 축약은 오류다. Config의 일반 Report 수신은 `.report`, 발행은 `publish<ConfigReport>`다. **위 예시만으로 CommsChannelConfig의 모든 실제 Operation 구현이 완료되었다는 뜻은 아니다.**

## 9. 재검토 결과 — 공개 API 표현성

| 사용자 시나리오 | 이번 판정 | 경계/남은 검증 |
|---|---|---|
| Control Provider의 Command/Cancel 업무 | **표현 가능** | 실제 장비 중단 결과는 사용자 판단 |
| 여러 동시 Command에 대한 ExecutionStatus 데이터 | **`publish<ExecOp>(token, value)`로 보완** | 실제 생성 IDL 필드와 연결 방식 검증 필요 |
| Control Consumer의 Send/Cancel/ACK/Status 관찰 | **표현 가능** | 원격 취소 성공을 보장하지 않음 |
| Config의 여러 Command와 일반 Report 동시 사용 | **표현 가능** | 실제 Service별 Operation 목록 검증 필요 |
| Specs/StatusReport의 Report-only Provider | **표현 가능** | 역할 선언만으로 유효한 데이터가 발행되지는 않음 |
| 동일 Service Provider+Consumer 병행 | **표현 가능** | 두 역할 간 등록 충돌 검사 필요 |
| ServiceType별 Flow Control 자동 선택 | **부적절; Operation 단위로 결정** | 예외 Service 매핑 필요 |
| Recovered Command를 진행 중 상태로 이어 수행 | **`resume()` 계약으로 보완** | 이전 상태 미확정 시 안전 실패 경로 필요 |
| Consumer 재기동 후 Command 복구·취소 판단 | **역할 설정으로 이동** | `ConsumerOptions` 코드 생성·실제 복구는 미검증 |
| Report Provider 초기화/재기동·과거 keyed 데이터 정리 | **전체 Snapshot `publish` 오버로드로 보완** | 실제 SDK가 소유 인스턴스 키를 확인할 수 있는지 검증 필요 |
| LargeSet/List와 Specialization의 앱 데이터 표현 | **기존 v0.8.3 계약 유지** | 생성 데이터 타입과 매핑 검증 필요 |
| 139개 고유 Service 전체의 사용자 경로 | **미검증** | 대표/예외 Service별 인터페이스·사용 예 생성 필요 |
| 생성 SDK 헤더 실제 C++11 컴파일/ICD 런타임 적합성 | **미검증** | 본 사용자 API 설계 검토 범위 밖 |

**재검토 결론:** 4종 ServiceType을 사용자용 별도 추상 Handler 클래스로 나누지 않는다. Service는 `umaa::Service<Derived, ServiceKind>` 기반 생성 타입, 역할은 Component의 `.provider/.consumer`, 실제 업무 이벤트는 역할별 `.command/.cancel/.report`로 명확하게 분리한다. **업무 동사 8개 + 역할 선언 2개**는 현재 최소성/표현성의 균형점이다. Command/Report 실제 데이터와 삭제/재시작 정책, 복구 문맥에 대한 미검증 사항을 내부 절차가 해결되었다는 이유로 숨기거나 완료로 표시하지 않는다.

## 10. 대표 ICD 근거 및 보완의 출처

| ICD 근거 | 사용자 API 설계에 반영한 사항 |
|---|---|
| EO §3.4 Table 3 | 4종 Service 명명 관례, 실제 Operation 유무의 예외; Config Command/일반 Report 혼합 가능 |
| MO §6.1.5 Table 22–26 | GlobalVectorControl의 setGlobalVector/ACK/CommandStatus/ExecutionStatus/Cancel Operation |
| EO §5.1.2.1–2 | Provider/Consumer 기존 Command 복구·실패·취소 판단 |
| EO §5.1.4.1–5 | ACK/Status, 갱신, 취소, 실행 완료·실패·결과 구분 |
| EO §5.2.1.1–4 | Queryless Report Provider/Consumer 등록·발행·삭제/재시작 |
| EO §3.8/§3.9 | Large Collection 및 Specialization 사용자 데이터·완전성 |
| CO-EXP §5.1.1 | 다중 Config Command + Report 실제 사례 |

`Service` 기반 클래스, `Provider/Consumer` 등록 함수, `Operation` 타입 계층, `CommandDecision::resume()`은 **ICD의 C++ 인터페이스가 아니라 SDK가 선택한 사용자 UX 계약**이다.