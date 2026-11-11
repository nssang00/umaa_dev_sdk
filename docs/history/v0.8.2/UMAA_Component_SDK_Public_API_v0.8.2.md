# UMAA Component SDK — 최소 사용자 Public API v0.8.2

**날짜:** 2026-11-10  
**근거:** 라이브러리 UMAA ICD 6.0 (EO/MM/MO/SA/SEM/SO/CO-EXP/EXP), `umaa-6.0-idl-with-exp-services.zip` (594 IDL), v0.8.1 API 및 검증표.  
**목적:** SDK 내부 구현을 설계하는 것이 아니라 **ICD에서 애플리케이션 사용자에게 필요한 모든 판단·행위·데이터를 적은 수의 동사로 표현**한다.  
**상태:** 규격 기반 **API 계약 설계안**. 생성된 C++ 헤더의 컴파일, 구현·상호운용성·규격 적합성 완료 증명 아님.

## 0. v0.8.1 대비 결정

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

## 2. Command Provider: 사용자 의사결정 (EO §5.1.3–5.1.4)

### 2.1 `CommandInfo` — 원본/문맥 보존

**반드시 사용자 접근 가능:** `id()`(원본 Source+Session 구별), `source()`, `destination()`, `timestamp()`, `isUpdate()`, `isRecovered()`, `previousStatus()`(없으면 Unknown/Optional), `token()`(수신된 갱신 버전의 불변 핸들).

`source`, `sessionID`, `destination`, `timeStamp`는 실제 Command IDL에 있고, 갱신은 **동일 Source/Session + 더 최신 Timestamp**로 판정된다. `CommandToken`이라는 이름/갱신 세대 핸들은 ICD 필드가 아닌 **SDK 비동기 결과 혼선 방지 장치**다. ICD에 존재하지 않는 별도 wire-level Command revision ID를 만들어 보여 주지 않는다.

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

    umaa::CancelDecision onCancel(const umaa::CommandInfo& info) {
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

## 9. 사용자 표현 가능성 판정 / 미완료 사항

| 검증 질문 | 판정 |
|---|---|
| Command 수락·거절·지연/진행·완료·실패·취소를 8개 동사로 표현? | **설계상 가능**; 타입/전이 계약 필요 |
| Command 갱신·복구 의사결정? | **설계상 가능**; Token/Source/Status 및 `ReceiveOptions.onRecoveredCommand` 계약 필요 |
| Report Data/Dispose·원본 Key/Source/Timestamp? | **설계상 가능**; `ReportEvent<T>` 및 typed key 필요 |
| Large Set/List 데이터 수정·삭제·순서·완전성? | **설계상 가능**; `LargeSet/List` 계약 필요 |
| Generalization/Specialization 타입·연결·미완성? | **설계상 가능**; `Specialized<T>` 계약 필요 |
| 139개 고유 Service의 모든 Provider/Consumer 업무 코드로 실증? | **미완료**; 아직 생성/컴파일 사용자 시나리오 증명 없음 |
| 실제 DDS/IDL 상호운용성? | **이번 검증 범위 밖** |

**최종 권고:** ICD에 근거한 보완은 **새 동사 없이 데이터 타입·Handler 문맥·결과 타입에서 완료**하는 방향으로 고정한다. 미완료 검증을 구현 적합성으로 오인하지 않는다. 다음 단계에서 서비스별 실제 타입을 넣은 사용자 예시를 전수 생성/컴파일하면, 단순한 설계상 커버 가능성과 사용자 API 실증을 구분할 수 있다.

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
