# UMAA Component SDK — 최소 사용자 Public API v0.8.3

**날짜:** 2026-11-10  
**근거:** 라이브러리 UMAA ICD 6.0 (EO/MM/MO/SA/SEM/SO/CO-EXP/EXP), `umaa-6.0-idl-with-exp-services.zip` (594 IDL), v0.8.1 API 및 검증표, v0.8.2 사용자 API 계약; Service/Operation 등록 세부 계약은 v0.8.3 보완.  
**목적:** SDK 내부 구현을 설계하는 것이 아니라 **ICD에서 애플리케이션 사용자에게 필요한 모든 판단·행위·데이터를 적은 수의 동사로 표현**한다.  
**상태:** 규격 기반 **API 계약 설계안**. 생성된 C++ 헤더의 컴파일, 구현·상호운용성·규격 적합성 완료 증명 아님.

## 0. v0.8.2에서 유지하는 결정

1. **최상위 등록/동작 API 8개를 유지**한다. 새로운 필수 등록/발행/호출 동사는 제안하지 않는다.
2. ICD에서 요구하는 정보(명령 상태·사유·갱신·취소·복구, 키/Optional, Collection 동기화/순서/삭제, Specialization 연결/완전성)를 **기존 함수의 인자/결과 또는 데이터 타입으로 노출**한다.
3. `onServiceEvent`, `m.discover`, `latest`, `next`, `onStatus`, `m.collection`, `m.specialized`, `m.execution`, `sendAsync`, `publishCollection`, `unpublish`는 **ICD 충족을 위한 필수 공개 API에서 제외**한다. 필요 시 별도 SDK 편의 확장으로 취급한다.
4. `request/m.request`는 UMAA 6.0 정규 Queryless Report의 동사가 아니다. 실제 Query Payload를 가진 확장 서비스가 별도 정의된 경우에만 재검토한다.
5. Report의 `Unavailable/Fault`, `CommandCall::wait_for` 및 Collection `changes`는 기본 프로토콜의 별도 규범 사건이 아니라 SDK 편의 기능이다. 일부는 선택 오버로드로 제공해도 되지만 **필수 커버리지 근거에 포함하지 않는다**.
6. 앞선 실험적 ICD 3개의 `@nested` Topic 불일치는 **IDL/바인딩 소스 적합성** 쟁점이며, 이번 **범용 사용자 API 동사 부족 여부 판정에서는 제외**한다. 지원 여부 자체는 별도 게이트로 남긴다.

## 1. 공통 최소 동사 — 확정 제안

| 분류 | 함수 | 사용자 행위 |
|---|---|---|
| 이벤트 등록 | `m.command<Op>(handler)` | Command 최초·갱신·복구 수신 및 수락/거절/지연 판단 |
| 이벤트 등록 | `m.cancel<Op>(handler)` | 진행 중 Command의 취소 의사 수신, 실제 안전 중단 가능 여부 판단 |
| 이벤트 등록 | `m.report<Op>(handler[, ReceiveOptions])` | Report/CommandStatus/Ack/ExecutionStatus 이벤트 수신 |
| 동작 | `send<Op>(payload[, SendOptions])` | 목적지를 지정해 Command 송신 및 결과 관찰 시작 |
| 동작 | `cancel(call_or_id)` | Command 취소 요청 제출; 원래 Command의 최종 결과 관찰 |
| 동작 | `respond(commandToken, result)` | Provider 명령 진행/종결 결과 통지 |
| 동작 | `publish<Op>(payload[, PublishOptions])` | 사용자 데이터/ExecutionStatus/Specs/Report 발행 |
| 동작 | `dispose<Op>(key)` | keyed Report/데이터 인스턴스 삭제 |

`onStart()/onStop()`은 이미 존재하는 선택 Component 수명주기 훅으로 유지한다(핵심 동사 8개에 포함하지 않음). ICD에서 **유일한 C++ 함수명이나 클래스명을 지정하지는 않는다**. 위는 SDK가 채택하는 UX 계약이다.

`<Op>`는 생성된 **개별 Operation 토큰**이다. 한 Service에 다수의 Command/Report Operation이 있다면 반드시 `Service::Op`를 지정한다. Operation이 유일할 경우에만 `Service` 단축 표기를 허용한다. 사용자 입력·출력 방향과 일치하지 않는 등록은 오류여야 한다. (각 ICD의 Service Interfaces)



### 1.1 Service/Operation/Flow Control을 타입으로 분리 (v0.8.3 필수 보완)

ICD EO §3.4에서 `Config`, `Control`, `Specs`, `Status`/`Report`는 **서비스 명명/용도 분류**다. 이름을 만드는 *convention*이며 모든 서비스가 정확히 같은 Operation 집합을 갖는다고 강제하지 않는다. **한 서비스가 네 종류 모두에 속한다는 뜻도 아니다.** 실제 서비스는 여러 Command Operation, Ack, CommandStatus, ExecutionStatus, 일반 Report를 조합할 수 있다.

- `Service`: 생성된 **논리적 서비스 타입 토큰**. 네 종류 중 하나의 주된 분류를 가지되 예외도 허용하며, 실제 Operation 집합이 우선한다.
- `Operation`: 생성된 **구체적인 입출력 Operation 타입 토큰**. Command, CommandAck, CommandStatus, ExecutionStatus, Report 등 역할 및 어떤 Command에 연관되는지가 정의된다.
- `FlowControl`: 명령 계열은 Command/Response, 일반 보고 계열은 Request/Reply **without Query Data**. **Config는 Command와 Report 두 계열을 동시에 포함할 수 있다.** 따라서 FlowControl을 Service 이름 끝부분에서만 추론해서는 안 된다.
- `ServiceTraits<S>` / `OperationTraits<Op>`: 읽기 전용 생성 타입 메타데이터 계약. 서비스 분류/소속 Operation, Operation 역할/FlowControl/입출력 방향, Command와 연관된 Ack·Status·ExecutionStatus를 타입 수준에서 식별한다. 사용자가 이 Traits를 직접 호출할 필요는 없고, **별도의 `m.config`, `m.control`, `m.specs`, `m.status`는 만들지 않는다.** 정확한 C++ traits 구현 형식은 코드 생성 시 결정한다.

| 사용자 등록/동작 | 허용 Operation 역할 | 허용되지 않는 예 |
|---|---|---|
| `m.command<Op>(handler)` | Provider 입력 **Command** (`Config` 또는 `Control`) | Report/Specs/ACK 타입 등록 |
| `m.cancel<Op>(handler)` | **같은 Command Op**의 취소 요청 | 별도 CancelStatus/ACK Op를 Command처럼 등록 |
| `m.report<Op>(handler)` | Consumer 출력 Report/Ack/CommandStatus/ExecutionStatus | Command Op를 Report로 등록 |
| `send<Op>(payload)` | Consumer가 Command 송신 | 일반 Report Op 전송 |
| `publish<Op>(payload)` | Provider가 보고 데이터 발행 | 앱이 일반 CommandStatus를 임의로 발행하여 상태를 변경 |
| `respond(token, result)` | Provider의 현재 Command 상태·결과 통지 | 다른 Session/이전 버전 Token으로 응답 |

`m.command<Service>` / `m.cancel<Service>` **축약형은 해당 Service에 Command 역할 Operation이 정확히 하나인 경우만** 허용한다. `m.report<Service>` 역시 해당 용도로 등록 가능한 출력 Operation이 정확히 하나인 경우에만 축약을 허용한다. 다수면 `Service::Operation`이 필수다. 잘못된 역할·모호한 축약·중복 충돌 등록은 컴파일 시 또는 등록 시점에서 명확한 오류로 처리한다.

**Traits의 최소 사용자 가시 형태(계약적 선언 스케치):**

```cpp
// 실제 메타데이터 값은 공식 ICD/IDL에서 생성되어야 한다.
enum class ServiceKind { Config, Control, Specs, StatusReport, Other };
enum class FlowKind { CommandResponse, RequestReplyNoQueryData };
enum class OperationRole {
    Command, CommandAck, CommandStatus, ExecutionStatus, Report
};

template<class Service> struct ServiceTraits; // kind, command operation 집합, report operation 집합
template<class Op> struct OperationTraits;     // Service 타입, role, flow, payload 타입, 연관 Command
```

이 Traits는 **SDK가 생성·제공하는 읽기 전용 타입 정보**다. 애플리케이션 사용자가 모든 Service마다 이러한 클래스를 직접 작성하게 하지 않는다. Queryless `query`와 DDS Dispose 취소는 표준의 Operation 개념이지만 별도 `Command` 또는 `Report` Payload Handler를 요구하는 것으로 오해하지 않는다.

> **Provider 완전성:** Operation 등록은 해당 Operation을 제공한다는 뜻이다. `m.command<Op>` 하나만으로 **해당 Service의 모든 필수 Operation 제공을 선언했다고 간주하지 않는다.** 사용자가 Component를 *완전한 Service Provider*로 선언하는 경우, 서비스별 필수 입력 Handler 및 업무 데이터 제공 경로가 빠졌는지 별도 검증해야 한다. ACK·일반 CommandStatus의 프로토콜 처리를 사용자 Handler 여러 개로 요구하지 않는다. `ServiceTraits`는 이것을 판별하는 근거이지 추가 등록 동사가 아니다.

### 1.2 Config/Control Provider에 필요한 두 입력 이벤트

```cpp
// 제안된 C++11 공개 계약 — 실제 generated 헤더/API 구현은 미완료
m.command<ConfigService::SetA>(&MyComponent::onSetA);
m.command<ConfigService::SetB>(&MyComponent::onSetB);
m.cancel<ConfigService::SetA>(&MyComponent::onCancelA);
m.cancel<ConfigService::SetB>(&MyComponent::onCancelB);
```

- **`m.command`**: New / Updated / Recovered를 하나의 Handler와 `CommandInfo` 문맥으로 처리한다.
- **`m.cancel`**: 위에 등록한 **동일 Command Op**의 활성 Session 취소를 다룬다. Cancel은 새 일반 Command가 아니며 별도 `m.cancelCommandStatus` Handler를 요구하지 않는다.
- **Report 쪽은 `m.report`, `publish`**를 사용한다. Config라는 이름만으로 `m.command` Handler가 Config Report를 대신 수신하지 않는다.

## 2. Command Provider: 사용자 의사결정 (EO §5.1.3–5.1.4)

### 2.1 `CommandInfo` — 원본/문맥 보존

**반드시 사용자 접근 가능:** `id()`(원본 Source+Session 구별), `source()`, `destination()`, `timestamp()`, `isUpdate()`, `isRecovered()`, `previousStatus()`(없으면 Unknown/Optional), `token()`(수신된 갱신 버전의 불변 핸들).

`source`, `sessionID`, `destination`, `timeStamp`는 실제 Command IDL에 있고, 갱신은 **동일 Source/Session + 더 최신 Timestamp**로 판정된다. `CommandToken`이라는 이름/갱신 세대 핸들은 ICD 필드가 아닌 **SDK 비동기 결과 혼선 방지 장치**다. ICD에 존재하지 않는 별도 wire-level Command revision ID를 만들어 보여 주지 않는다.



### 2.1.1 `m.command<Op>` Handler 호출 계약 (v0.8.3)

**기본 Signature(의미 기준)**: `CommandDecision onCommand(const Op::Command& payload, const CommandInfo& info)`; 정확한 Payload alias는 생성된 타입에서 결정한다. 등록할 Operation은 *Command 역할*이어야 한다.

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

ACK 생성 여부나 `ISSUED` 상태 전환을 Handler에서 직접 호출하게 하지 않는다. `respond`를 통해 요청한 상태 변경은 ICD에서 허용한 전이 및 Reason만 수용한다. 이미 완료된 Command의 새 Update는 독립적인 새 `send()`가 필요하다.

### 2.2 `CommandDecision` / `respond`

- `CommandDecision::accepted()`: 업무 검증/준비 완료. `COMMANDED`에 진행할 수 있음. **COMPLETED 아님**.
- `CommandDecision::rejected(CommandReason)`: ICD 허용 Reason을 가진 `FAILED`.
- `CommandDecision::deferred()`: 검증/준비가 지연되어 나중에 `respond(token, commanded())` 또는 `respond(token, failed(reason))`를 호출하는 **SDK 비동기 UX**. `DEFERRED`는 UMAA 표준 Status가 아님.
- `respond(token, commanded()/executing()/completed()/failed(reason)/canceled())`: 실제 앱 실행 상태/결과를 전달. ICD의 상태 전이와 Status-Reason 조합을 벗어나면 명시적 오류.
- **`accepted()` 반환 즉시 `executing()`으로 간주하지 않는다.** 하드웨어가 실제 실행하기 전에는 `EXECUTING`을 거짓으로 보고하지 않아야 한다.
- 새 Command 또는 갱신에 대한 ACK/Status 관리 자체는 앱이 개별 Topic 동사로 처리하지 않아도 되게 한다. Command-specific `ExecutionStatus` 값은 앱이 `publish<ExecutionStatusOp>(value)`로 제공한다.

**갱신 버전 안전:** `respond(oldToken, ...)`이 이미 새로 갱신된 Session의 상태를 덮어쓰면 안 된다. 오래된 토큰의 응답은 `StaleToken` 등 오류로 처리한다. 다만 wire-level Status에는 별도 갱신 버전 필드가 없으므로 Consumer에게 수신 Status의 버전 귀속이 항상 확정되었다고 말할 수 없다.

## 3. Command Consumer: 상태 관찰, 갱신, 취소, 복구 (EO §5.1.2–5.1.4)

### 3.1 `CommandCall` 및 결과

필수 사용자 경로:

- `CommandCall.id()`: 표준 Source/Session에 해당하는 명령 식별.
- `CommandCall.update(newPayload)`: 동일 활성 Session의 Command를 갱신. 종결 상태에서는 실패; 새 Command로 묵시적 재해석 금지.
- `CommandCall.get()` 또는 전달된 Callback: `SendOptions.responseStage(Ack/Commanded/Terminal)`에서 정한 관찰 조건까지 상태를 얻음. `ResponseStage`는 사용자 대기 조건이고 `CommandStatus`와 다르다.
- `m.report<StatusOp>()` 및 필요 시 `m.report<AckOp>()`, `m.report<ExecutionOp>()`: 명령의 중간 상태/실행 상태를 계속 구독.
- `CommandResponse`: `id`, `source`, `destination`(발신 Command 문맥에 기록된 경우), `status`, `reason`, `timeStamp`, `outcome`, `association`(현재 갱신 귀속 Confirmed/SessionOnly/Unknown), `logMessage`(진단용) 등의 읽기 전용 정보 보존.
- `outcome`: **원격 Terminal 결과**(Completed/Failed/Canceled)와 **관찰 결과 불명/로컬 오류**(Timeout/ProviderLost/TransportError/ProtocolError)를 분리. 로컬 Timeout을 원격 `FAILED(TIMEOUT)`으로 변환하지 않음.

**UX 결정:** v0.8.1의 `get()` 기본 Terminal 대기는 그대로 유지하되, 종료 시간이 없는 지속형 명령은 `responseStage(Commanded)`를 명시하도록 문서에 강하게 안내한다. `wait_for()`는 선택적 편의 함수이며 원격 상태 전이를 의미하지 않는다.

### 3.2 `cancel(call_or_id)`와 `m.cancel`

- `cancel()`의 의미는 *취소 의사 제출*이다. 호출 성공이 원격 `CANCELED`를 보장하지 않는다.
- `CancelAttempt.submission()`은 제출 성공/실패/불명, `CancelAttempt.final()`은 원래 명령의 Terminal 관찰을 나타낸다(제안 결과 타입; 새로운 동사 아님).
- `m.cancel` Handler는 `CancelDecision::canceled()`(실제 중단 확인), `deferred()`(중단 진행 중), `cannotCancel()`(불가능; 원 실행 계속 진행) 중 업무에 맞게 처리할 수 있어야 한다. **이들은 SDK 반환값이지 별도 UMAA Cancel Status 메시지가 아님**.
- 취소 실패 후 원 명령이 `COMPLETED`가 될 수 있다. Liveliness Loss로 인해 취소 처리해야 하는 경우도 있다. Cancel 원인을 알 수 없으면 `Unknown`을 반환해야 하며, 이를 항상 `ConsumerRequest`로 단정하면 안 된다.



### 3.2.1 `m.cancel<Op>` Handler 호출 계약 (v0.8.3)

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

**Handler 미등록 정책 (중요):** `m.command<Op>`만 등록하고 `m.cancel<Op>`를 생략해도 *장비가 안전하게 중단되었다고 SDK가 가정해서는 안 된다*. 애플리케이션의 취소/정지가 필요한 비동기·지속 명령은 대응 `m.cancel<Op>` 등록을 **필수 사용자 계약**으로 한다. 이미 종료되었거나 해당 Operation의 의미상 별도 업무 취소가 없는 경우는 **명시적으로 검증된 정책**으로 생략할 수 있으나, 미등록이 곧 `CANCELED`라는 해석은 금지한다. 누락/충돌은 Provider 서비스 등록 검증에서 경고가 아니라 **서비스 완전성 오류**로 취급한다. (이 마지막 검증 방식은 ICD가 지정한 C++ 요구사항이 아니라 SDK 설계 안전 규칙이다.)

**명령 연결 상실:** EO §5.1.4.5의 Consumer liveliness 상실에 따른 취소 처리는 실제 업무 중단이 필요하다면 동일 `m.cancel` Handler로 전달한다. 연결 상실 원인을 항상 확정할 수는 없으며, 일반적인 Service 발견/소멸 이벤트까지 `m.cancel`에 섞지 않는다.

### 3.2.2 `cancel()`과 `m.cancel`은 다른 측면의 한 흐름

| 주체 | API | 책임 |
|---|---|---|
| Consumer | `cancel(CommandCall 또는 id)` | 기존 명령에 대한 **취소 의사 제출**; 최종 결과와 구분 |
| Provider | `m.cancel<SameCommandOp>` | 취소 요청을 수신하고 실제 정지 가능성 판단 |
| Provider | `respond(token, canceled()/completed()/failed(...))` | 지연 중단/원래 업무의 **실제 결과** 보고 |
| Consumer | 기존 `CommandCall`/`m.report<StatusOp>` | 원격 `CANCELED` vs `COMPLETED` vs 결과 불명 확인 |

ICD는 별도 Cancel Reply를 요구하지 않는다. `m.cancel` 반환 타입을 새 DDS CancelStatus 메시지처럼 간주하지 않는다.

### 3.3 재기동·복구 의사결정

- Provider: 기존 `m.command` Handler에 `isRecovered`, `previousStatus`를 제공해 **계속 수행 또는 FAILED(SERVICE_FAILED)** 결정을 내릴 수 있게 한다. 무조건 재실행하지 않는다.
- Consumer: ICD §5.1.2.2는 이미 존재하는 **자기 CommandType을 발견해** 복구할지 취소할지 정하도록 한다. `CommandStatus`만으로는 기존 Command Payload를 항상 확인할 수 없으므로 **Status 구독만 있으면 충분하다고 가정하지 않는다.** 기존 `m.report<CommandStatusOp>(statusHandler, ReceiveOptions)`의 선택적 **`onRecoveredCommand(RecoveredCommand<CommandPayload>)` Callback** 또는 동등한 `ReceiveOptions.recoveryPolicy`를 제공한다. Callback에는 원래 Command Payload, Source/Session/대상, 현재 알려진 상태가 전달되며 `RecoveryDecision::Continue/Cancel`을 반환한다. **새 최상위 `m.recover`/`resume()` 동사는 추가하지 않는다.** Callback을 등록하지 않은 경우의 기본 정책(예: Cancel)을 명시해야 하며, 무조건 Continue로 가정하지 않는다.
- 연결 상실: EO §5.1.4.5가 규정하는 **명령 관련** Liveliness 상실에 따른 안전 취소 처리 및 결과 불명 표현을 지원한다. 범용 서비스 사용 가능/불가능 이벤트 `onServiceEvent()`는 여기서 필수로 추가하지 않는다.

## 4. Report: 실제 데이터, Key, Dispose (EO §5.2.1; §6.1)

동일 `m.report<Op>`의 Handler 오버로드를 제공한다.

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

`dispose<Op>(key)`는 **일반 데이터 인스턴스 폐기**이고 Command `cancel()`과 다른 사용자 동사다. 삭제 이벤트가 필요한 소비자는 반드시 `ReportEvent<T>` Handler를 선택한다. Provider의 Report 재시작 시 기존 잔존 데이터에 대한 갱신/폐기 판단을 위한 초기 상태/키 접근도 가능해야 한다.

## 5. LargeSet<T>/LargeList<T> (EO §3.8)

새 `m.collection` 또는 `publishCollection` 동사는 불필요. **Command/Report payload 안에서 동일 컬렉션 모델을 사용**한다.

**필수 공개 의미:**

- Collection `id()`, `size()`, `elements()`, `state()`; `state`는 적어도 `Complete/Incomplete/Invalid`를 구분. 빈 완성 Collection과 미수신 Collection을 다르게 나타냄.
- Set: Element `id`, `add`, `set`, `remove`, `clear`, `replaceAll`.
- List: 위 기능 + `insert`, `move`, `elements`의 논리적 순서 유지. 위치와 Element ID는 서로 다름.
- 구독 Handler 기본값은 **완성된 논리적 Snapshot**을 전달한다. 중간/누락 데이터가 관측되면 정상 완성 데이터로 위장하지 않는다. **ICD §3.8.3은 소비자가 부분 Element를 즉시 처리할지, 원자적 업데이트 완료를 기다릴지 선택할 수 있게 한다.** 따라서 `m.report<Op>(handler, ReceiveOptions{delivery=CompleteOnly|IncludePartial})`처럼 **기존 등록 함수의 옵션**으로 부분 이벤트를 선택할 수 있어야 한다. `IncludePartial`을 선택한 경우 `CollectionState::Incomplete`와 개별 Element/변경 정보를 관찰할 수 있어야 하며, 완성되지 않은 상태를 `Complete`로 위장하지 않는다. (옵션 구문은 C++11 계약 개념 표기이며 실제 선언부 아님)
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

## 8. C++11 사용자 예시 — 설명용 계약 스케치

> 생성된 타입의 정확한 이름과 namespace는 코드 생성 시 확정한다. 아래는 **구현/컴파일 완료 코드가 아니라 공개 UX 계약의 스케치**다.

```cpp
class AutoPilot : public umaa::Component {
public:
    umaa::CommandDecision onVector(
        const mo::GlobalVectorCommand& cmd,
        const umaa::CommandInfo& info) {
        if (!validate(cmd))
            return umaa::CommandDecision::rejected(
                umaa::CommandReason::ValidationFailed);

        umaa::CommandToken token = info.token();
        startHardwareAsync(cmd, token);
        return umaa::CommandDecision::accepted();
    }

    umaa::CancelDecision onCancel(const umaa::CancelInfo& info) {
        return requestSafeStop(info.token())
            ? umaa::CancelDecision::deferred()
            : umaa::CancelDecision::cannotCancel();
    }

    void onPose(const umaa::ReportEvent<sa::GlobalPoseReport>& event) {
        if (event.kind() == umaa::ReportKind::Disposed) {
            erasePose(event.key());
            return;
        }
        applyPose(event.data(), event.source(), event.timestamp());
    }

    void onHardwareStarted(const umaa::CommandToken& token) {
        respond(token, umaa::CommandResult::executing());
    }
    void onHardwareStopped(const umaa::CommandToken& token) {
        respond(token, umaa::CommandResult::canceled());
    }
};

UMAA_COMPONENT(AutoPilot, m) {
    m.command<mo::GlobalVectorControl>(&AutoPilot::onVector);
    m.cancel<mo::GlobalVectorControl>(&AutoPilot::onCancel);
    m.report<sa::GlobalPoseStatus>(&AutoPilot::onPose);
}
```

실제 SDK는 단순 Handler 선택 시 Data만 전달하고, 상태가 중요한 Handler에서는 위와 같이 Key/Dispose까지 접근 가능하게 한다. `m.cancel`의 Return 값은 장비 중단 결과에 따라 적용하며 `deferred`를 선택했다면 나중에 실제 `respond(token, canceled())`를 호출해야 한다.

## 8.1 다중 Command Config 및 일반 Report 동시 처리 예

```cpp
// Service/Operation 이름은 공개 계약 스케치, 실제 generated 이름은 추후 확정
UMAA_COMPONENT(CommsManager, m) {
    m.command<co::CommsChannelConfig::AddMessage>(&CommsManager::onAdd);
    m.cancel <co::CommsChannelConfig::AddMessage>(&CommsManager::onCancelAdd);
    m.command<co::CommsChannelConfig::DeleteMessage>(&CommsManager::onDelete);
    m.cancel <co::CommsChannelConfig::DeleteMessage>(&CommsManager::onCancelDelete);
    m.report <co::CommsChannelConfig::ConfigReport>(&CommsManager::onConfig);
}

// Config Report 송신은 Command Handler가 아니라 별도의 일반 publish 동작
publish<co::CommsChannelConfig::ConfigReport>(configValue);
```

실제 CO-EXP §5.1.1 `CommsChannelConfig`에는 두 개의 설정 Command와 일반 Config Report가 있어 Command마다 등록이 필요하다. 다만 **Config Report의 Provider는 publish, Consumer는 m.report**를 사용한다. 위는 한 Component가 Provider와 Consumer 역할을 섞어 가진 예시로, Component가 양측 역할을 모두 해야 한다는 뜻은 아니다. `m.cancel`에 사용하는 `Op`는 `m.command`의 원래 Operation과 같다.

## 8.2 사용자 API 검증 제약

| 사례 | 기대되는 사용자 관찰 |
|---|---|
| 한 Service에 Command Op가 2개인데 `m.command<Service>` 등록 | 모호성 오류; `Service::Op` 사용 유도 |
| `m.command<SpecsOp>`, `m.cancel<ReportOp>` | 역할 불일치 오류 |
| `m.cancel<Op>`만 등록, 동일 Component의 Provider Command 미등록 | 의도한 외부 Provider 역할인지 확인 필요; 오등록 조합을 검출 |
| `m.command<Op>`에 긴 실행이 있지만 `m.cancel<Op>`가 없고 안전 정책 미지정 | Provider 완전성 오류; SDK가 CANCELED라고 가정 금지 |
| Command A 갱신 후 과거 A Token으로 `respond` | `StaleToken` 등 명확한 실패; B 상태 영향 없음 |
| Cancel `cannotCancel()` 이후 원 명령 완료 | Consumer가 Terminal `COMPLETED`를 받으며 Cancel 성공으로 보고하지 않음 |
| Config Service가 두 Command + 일반 Report 포함 | 두 `m.command`/`m.cancel` 조합과 별도의 `publish`/`m.report`로 표현 |

## 9. 사용자 표현 가능성 판정 / 미완료 사항

| 검증 질문 | 판정 |
|---|---|
| Command 수락·거절·지연/진행·완료·실패·취소를 8개 동사로 표현? | **설계상 가능**; `m.command/m.cancel`의 Operation 타입·문맥·결과 계약을 v0.8.3에서 구체화 |
| Command 갱신·복구 의사결정? | **설계상 가능**; Token/Source/Status 및 `ReceiveOptions.onRecoveredCommand` 계약 필요 |
| Report Data/Dispose·원본 Key/Source/Timestamp? | **설계상 가능**; `ReportEvent<T>` 및 typed key 필요 |
| Large Set/List 데이터 수정·삭제·순서·완전성? | **설계상 가능**; `LargeSet/List` 계약 필요 |
| Generalization/Specialization 타입·연결·미완성? | **설계상 가능**; `Specialized<T>` 계약 필요 |
| 139개 고유 Service의 모든 Provider/Consumer 업무 코드로 실증? | **미완료**; 서비스별 생성/컴파일 및 모든 Provider 업무 구현 완전성 실증 없음 |
| 실제 DDS/IDL 상호운용성? | **이번 검증 범위 밖** |

**최종 권고(v0.8.3):** 기존 `m.command`/`m.cancel`을 유지하되, **동일 Command Operation 토큰, `CommandInfo.kind()`, `CancelInfo`, Handler 미등록 정책 및 생성된 Service/Operation Traits를 필수 공개 계약으로 확정한다.** ICD에 근거한 보완은 **새 동사 없이 데이터 타입·Handler 문맥·결과 타입에서 완료**하는 방향으로 고정한다. 미완료 검증을 구현 적합성으로 오인하지 않는다. 다음 단계에서 서비스별 실제 타입을 넣은 사용자 예시를 전수 생성/컴파일하면, 단순한 설계상 커버 가능성과 사용자 API 실증을 구분할 수 있다.

## 10. 핵심 원문 근거 (대표 EO ICD)

| ICD 조항 | 사용자 관점 필수 정보 |
|---|---|
| §3.8.2–3.8.7 | Large Collection 생성/갱신/삭제/공집합/Set·List/순서 |
| §3.9.1–3.9.3 | Specialization ID/Topic/Timestamp 연결·갱신·폐기 |
| §5.1.2.1–5.1.2.2 | Provider/Consumer의 기존 명령 복구 또는 실패·취소 판단 |
| §5.1.4.1–5.1.4.5 | ACK/Status/ExecutionStatus, 새 갱신, Terminal 및 Cancel |
| §5.2.1.1–5.2.1.4 | Report 제공/구독/재발행/폐기/재시작 |
| §5.2.2 | Query Data를 가진 Request/Reply는 6.0에서 미정의 |
| §6.1 | Topic·Key·Optional 및 Cancel/Query의 DDS 방식 |

타 영역의 규범 Service별 예외는 해당 ICD/IDL의 Operation 정의를 우선한다. EO ICD 공통 설명만으로 모든 서비스별 비즈니스 규칙이 일치한다고 가정하지 않는다.

## 11. v0.8.3 근거 및 판정 경계

- UMAA EO ICD 6.0 **§3.4 / Table 3**: 4종 Service 이름은 convention; 실제 Operation의 유무 및 결합은 Service마다 다르며 Query/Cancel의 일부는 별도 DDS Topic 송신 동작이 아님.
- UMAA EO ICD 6.0 **§5.1.2.1–2**: Provider·Consumer 재기동과 기존 Command 복구 의사결정.
- UMAA EO ICD 6.0 **§5.1.4.1–5**: ACK, 상태 전이, 갱신, 취소 및 실행 결과.
- UMAA CO-EXP ICD 6.0 **§5.1.1**: `CommsChannelConfig`의 다중 Config Command와 Config Report.
- **ICD가 요구하는 것:** 새/갱신/복구 명령의 처리, 실제 취소 판단, 표준 상태와 결과의 보존.
- **SDK가 선택한 C++ UX:** `m.command`, `m.cancel`, `CommandInfo.kind`, `CancelInfo`, 생성 Traits, Handler 미등록시 명확한 오류. 이 이름들이 ICD에서 요구된 것은 아니다.
- **검증 경계:** 정적 사용자 API 설계 검토이며 실제 SDK 헤더 구현, 생성 결과의 C++11 컴파일, DDS 상호운용성은 이번 판정에 포함되지 않는다.
