# UMAA Component SDK — Public API 계약 v0.8.1

> **v0.8.1 감사 보정 (2026-11-10):** `umaa-6.0-idl-with-exp-services.zip` 594개 IDL을 라이브러리의 8개 ICD 6.0 PDF 원문과 교차 검증. 기존 Public API의 최상위 동사 집합은 유지하되, Experimental 및 모든 Collection 필드 범위와 소스 불일치 사항을 보정한다. 이 문서는 **설계 계약**이지 컴파일/실행/상호운용성 검증 결과가 아니다.

## v0.8.1에서 달라진 필수 계약·범위

1. `LargeSet<T>`와 `LargeList<T>`는 **10 Set + 8 List = 18개** 원본 메타데이터/Element Topic에 대응해야 한다. 이 가운데 이전 정규 검토에서는 10개만 확인하였다. 추가 8개도 동일한 객체 의미·수명주기·동기화 계약으로 처리한다.
2. EXP는 독립 `UMAA::EXP` 네임스페이스가 아니다. 각 Operation의 **ICD `Namespace`**를 그대로 사용하는 generated type/operation descriptor가 필요하다. `CO-EXP/ContactFilterConfig`도 `UMAA::SA::ContactFilterConfig`이다.
3. **3개 Report Operation 예외:** `AcousticInterferenceStatus::reportAcousticInterference`, `FinsStatus::reportFins`, `PropulsorsStatus::reportPropulsors`는 ICD에서 `Topic`을 정의하지만 제공 IDL의 해당 타입은 `@nested`, `const string *Topic` 없음. `m.report<Op>`와 `publish<Op>`를 정상 DDS Topic으로 자동 생성할 수 있다고 주장하지 않는다. **표준 소스 정정 또는 공식 Topic/타입 계약 확인 전에는 API 생성·적합성 판정 보류**.
4. 같은 `ContactFilterConfig` 서비스가 CO-EXP와 EXP 두 ICD에 실려 있으나 IDL의 고유 Service/Topic은 한 벌이다. Service 개수와 실제 DDS 자산 개수를 구분한다.
5. IDL에는 18개 **취소 관련 Topic 선언**이 있지만 ICD Flow Control은 취소를 Command 인스턴스 Dispose로 취급하고 CancelCommandStatus 반응을 별도 DDS 메시지로 구현하지 않는다고 적는다. **IDL 선언 존재만으로 별도 `sendCancel` 또는 `m.cancelStatus` API를 만들지 않는다.** 사용자 `cancel()`의 의미는 유지하되, 모순 항목은 소스 확인 대상으로 분리한다.
6. ICD Operation이 아닌 별도 Topic 상수 `FinCommandType`, `PropulsorCommandType`도 존재한다. 해당 값들은 `FinsCommandType`·`PropulsorsCommandType`의 중첩 배열 요소로 사용된다. 원문에 독립 Service Operation이 없는 한 별도 필수 Component API를 추가하지 않는다.
7. 새 IDL 594개 전체에서 `@optional` 필드 453건, `@key` 필드 739건, `union` 선언 19건이 확인된다(소스 코드의 단순 정적 카운트). C++11 Public Types는 optional 미설정·복합 키·IDL union 분기를 데이터 손실 없이 나타내야 한다. 이는 표준 적합성을 증명하는 것이 아니라 생성기 검증 범위를 구체화한다.

**비공개 계약 추가 금지:** 이상 3개 Report의 누락 DDS Topic을 임의 이름으로 합성하거나 IDL `@nested`를 임의로 top-level DDS Topic으로 취급해서는 안 된다.

---


> 2026-11-10 | 기준: UMAA ICD 6.0 / 제공된 v0.7 Handoff / RTI UMAA Starter Kit  
> **상태:** 사용자 API 설계 계약 보완안. **SDK 구현, C++ 컴파일, DDS 동작 및 표준 적합성 검증 결과가 아님.**  
> 범위: UMAA Service Provider/Consumer 개발자가 사용하는 C++11 Public API. 내부 DDS 및 엔진 구현 선택은 제외.  
> 표기의 `Operation`, `Service::...` 및 열거형은 **제안된 생성 API**이며, 실제 구현/생성기 API가 이미 존재함을 뜻하지 않는다.

## 1. 설계 결정 (v0.7 → v0.8.1)

1. 최상위 동사는 변경하지 않는다. 등록: `m.command`, `m.cancel`, `m.report`; 실행: `send`, `cancel`, `respond`, `publish`, `dispose`. `onStart/onStop`은 선택 수명주기 훅이다.
2. **Request/Reply with Query Data는 UMAA 6.0 ICD에 정의되어 있지 않다.** 6.0의 `query*`는 Report DDS Topic 구독이다. `request/m.request`는 표준 6.0 기본 API에서 제외하고, 실제 Query Payload가 정의된 외부 확장에만 조건부 허용한다.
3. Service에 한 종류의 `set` Operation만 있으면 `m.command<Service>`/`send<Service>` 단축형을 허용한다. 두 종류 이상이면 생성된 `Service::Set...` Operation 타입을 **반드시** 사용한다. `report`도 여러 Operation이면 명확히 지정한다.
4. Command 응답과 상태, Cancel 요청과 Cancel 결과, 데이터 일부 수신과 완성, 미지원 Specialization과 아직 미수신을 **서로 다른 공개 값**으로 표현한다.
5. User Handler는 업무의 검증/실행/안전 중단/상태 내용만 제공한다. ACK, 프로토콜 상태 전이/정리, Collection 원자 갱신 경계, Specialization Topic 연결은 SDK가 책임지도록 계약한다. 이 책임은 향후 구현에서 입증해야 한다.
6. 정상 UMAA 6.0 서비스의 IDL 필드(`@key`, `@optional`, 중첩 타입, 범위 값, Timestamp, Collection Element ID 등)를 숨겨서 손실시켜서는 안 된다. DDS Topic 메타데이터를 일반 Handler에 일일이 노출하지 않되 필요 정보는 타입과 이벤트에 보존한다.
7. 사용자에게 주어지는 API는 **표준 정보를 잃지 않는 경로를 제공**하면 된다. 단순 Payload Handler를 선택한 사용자가 모든 시스템 이벤트를 구독하도록 강제하지는 않는다. 삭제/실패/불완전 상태가 중요하면 `ReportEvent<T>` Handler를 선택하게 한다.

## 2. 최상위 사용자 API — 잠정 고정

| API | 제공자/소비자 | 사용자 의미 | 필수 계약 |
|---|---|---|---|
| `m.command<Op>(handler)` | Provider | 최초·갱신·복구 Command 수신 | `CommandInfo`에 `CommandToken`, 출처, 갱신/복구 문맥 |
| `m.cancel<Op>(handler)` | Provider | 해당 Command 종료 요청 | 요청 이유, 실제 중단 가능 여부 및 지연 완료 |
| `m.report<Op>(handler[,options])` | Consumer | Report/Ack/Status/Exec/Collection/Specialized 수신 | Payload 또는 정보 보존형 `ReportEvent<T>` |
| `send<Op>(payload[,options])` | Consumer | Command 발행·추적 | `CommandCall`, 대상 선택, 최종 상태와 오류 구분 |
| `cancel(call_or_id)` | Consumer | 기존 Command 취소 의사 전달 | 실제 CANCELED를 보장하지 않음; 원래 세션 결과 추적 |
| `respond(token,result)` | Provider | Command 준비·실행·종결 통지 | *현재 갱신 세대*만 유효, 상태/Reason 적법성 |
| `publish<Op>(data[,options])` | Provider | Report/ExecutionStatus 등 발행 | 원본 시간·Key·Command 연관/데이터 완전성 보존 |
| `dispose<Op>(key)` | Provider | Report/데이터 인스턴스 삭제 | Command 취소에 쓰지 않음; 원본 Key 지원 |
| `onStart/onStop` | Component | 사용자 초기화/종료 | 선택적 구현 |

`sendAsync`, `publishCollection`, `m.collection`, `m.specialized`, `m.execution`, `unpublish`, `m.status`는 추가하지 않는다. `std::future<T>::then()`이나 C++20 coroutine에 의존하지 않는다.

### 타입과 Operation 토큰

- Service의 이름만 보고 Operation Topic을 추론하지 않는다. 생성된 `ServiceDescriptor<Op>`가 ICD의 정확한 Namespace/Topic/IDL Type 및 유형(Provider 입력/출력)을 나타내야 한다.
- 하나의 Service에 두 개 이상 `set`이 있으면 단축 호출을 컴파일 단계에서 금지한다. 예: MM `ConditionalControl`의 Add/Delete, `MissionPlanMissionControl`의 Add/Clear/Delete.
- `m.command<Op>`와 `m.report<Op>`의 잘못된 방향 또는 `publish` 가능한 타입이 아닌 Operation은 컴파일 오류 또는 명시적 기동 오류여야 한다.
- `report...CancelCommandStatus`는 ICD MDE 표기에만 있고 DDS Topic으로 제공되지 않으므로 생성 API로 위장하지 않는다.

## 3. Command — 반드시 손실 없이 표현할 정보

### 3.1 식별자와 갱신 세대

`CommandId`는 Source/Session을 보존하고 대상과 연결될 수 있어야 한다. `CommandToken`은 **하나의 명령 갱신 버전**을 지정하는 불변 식별자이다. 서로 다른 갱신의 Command는 세션을 공유하므로 `respond(id,...)` 대신 **`respond(token,...)`**를 사용한다.

`CommandInfo` 최소 계약:

- `id()` : 세션 식별자; `token()` : 해당 수신 명령의 갱신 세대 핸들
- `source()` / `destination()` : 실제 역할의 GUID
- `timestamp()` : 원본 명령의 Timestamp
- `isUpdate()` / `isRecovered()` : 새로운 명령·업데이트·재시작 후 복구 구분
- `previousStatus()` : 복구/갱신 시 알려진 이전 상태. 없으면 'Unknown'을 명시하는 `Optional`

`CommandToken`이 이전 갱신에 속하는 경우 `respond()`는 새 갱신의 상태를 변경하지 **않으며**, 호출자에게 `StaleCommandToken` 등 검증 가능한 오류를 전달한다. Command가 이미 Terminal이면 업데이트 요청을 새로운 명령으로 임의 해석하지 않는다. `CommandCall::update(payload)`는 이전 DDS Sample을 제자리 수정하지 않고 **같은 Source/Session과 더 새로운 Timestamp의 Command Sample을 발행**하는 의도로 제한한다.

### 3.2 Provider의 의사결정

`CommandDecision`은 `accepted()`, `rejected(CommandReason)`, `deferred()`를 포함한다.

- `accepted()`: 검증·자원 준비 완료. `COMMANDED` 단계로 진행할 수 있다는 뜻이며 실행 완료가 아니다.
- `rejected(reason)`: `FAILED`의 규격상 합법 Reason으로 거절한다. 자유 문자열을 ICD Reason enum에 강제 캐스팅하지 않는다.
- `deferred()`: 검증·자원 준비 중이며 나중에 `respond(token, commanded())` 또는 `respond(token, failed(reason))`로 확정한다. **Deferred는 새 UMAA CommandStatus가 아니다.**

`respond(token, CommandResult::commanded|executing|completed|failed(reason)|canceled())`라는 하나의 함수로 표현한다. ICD의 허용 전이 및 Reason 조합만 성공해야 한다. ACK는 원칙적으로 SDK가 처리하며, Provider가 `accepted()`를 반환한 것만으로 Ack 성공 또는 Execution 시작을 추정해서는 안 된다.

### 3.3 Consumer 응답·실행상태·오류

`CommandCall`의 필수 관찰값은 `id()`, `generation()/token()`(해당 호출 버전), `get()`, `wait_for(duration)`, `update(payload)`이다. `CommandResponse`에는 다음을 보존한다.

- `stage()` : 호출 응답 완료 조건 (`Ack`, `Commanded`, `Terminal` 등). 실제 UMAA `status()`와 구분
- `status()` : `ISSUED/COMMANDED/EXECUTING/COMPLETED/FAILED/CANCELED/Unknown`
- `reason()` : ICD Reason 또는 `Unknown` (임의 합성하지 않음)
- `outcome()` : 표준 Terminal 결과와 **관찰 실패**(local timeout, destination unavailable, transport error, protocol error)를 구분
- `id()`, `source()`, `destination()`, `timestamp()`, `message()` 등 원본 정보. `message()`는 진단용 `logMessage`로, 표준 기계 처리에 사용하지 않는다.
- `association()` : `Confirmed / SessionOnly / Unknown` 등 해당 Status가 특정 갱신 버전에 확정적으로 귀속 가능한지의 정보. **CommandStatus에는 별도의 명령 갱신 버전 필드가 없으므로 단정 금지**

**제안 기본값:** `get()`은 `Terminal` 관찰까지 대기한다. 이는 SDK UX 결정이며 UMAA 표준의 기본값이 아니다. 지속형 GlobalVector처럼 스스로 끝나지 않는 명령에서는 `responseStage(Commanded)`를 명시하거나 별도 이벤트를 구독하고, 필요 시 취소한다.

`wait_for()`의 시간 초과는 **호출자의 대기가 끝난 것**이지 원격 Command 취소나 `FAILED(TIMEOUT)`을 뜻하지 않는다. 또한 `CommandStatus`의 Session 정보만으로 어떤 갱신의 결과인지 항상 결정할 수 없다. `ACK`에 포함된 원래 명령 Payload를 이용한 확인과 단일 Session 관찰 결과를 구분하며, 확정 불가 시 `association=SessionOnly/Unknown`을 사용한다. 상태 진행 전체를 원하는 Consumer는 같은 `m.report<CommandStatusOperation>()`/`m.report<ExecutionStatusOperation>()`로 실시간 관찰할 수 있다. 선택적 Callback/Lambda 오버로드는 허용하지만 별도 `onStatus` 동사는 강제하지 않는다.

### 3.4 Cancel

`cancel(call_or_id)`는 기존 Command를 DDS Dispose하도록 요청하는 사용자 동사이다. 요청 전달 성공과 원격 `CANCELED`는 별개다.

`CancelAttempt`(반환값 타입, 별도 동사 아님)의 최소 관찰 항목:

- `submission()` : 취소 Dispose 제출 결과. 실패/불명 표시
- `final()` 또는 `get()` : **원래 Command 세션**의 최종 `CommandResponse`를 대기/관찰
- 최종 상태가 `COMPLETED`일 수 있으며, 이 경우 취소 성공으로 분류하지 않는다. 연결 상실 시 `Unknown`을 유지한다.

`m.cancel<Op>(handler)`의 `CancelDecision`은 `canceled()`, `deferred()`, `cannotCancel()`을 제공한다. `canceled()`는 실제 취소가 수행된 경우에만, `deferred()`는 장비 정지가 진행 중일 때 사용한다. `cannotCancel()`이면 실행을 이어가고 실제 결과를 보고한다. `CancelInfo::cause()` 또는 공통 `CommandInfo::cancelCause()`는 `ConsumerRequest/ConsumerLost/Shutdown/Unknown` 같은 구별을 제공한다. 표준이 요구하는 Consumer Liveliness 상실 후의 처리 규칙을 약화해서는 안 된다.

### 3.5 Recovery, 충돌·우선순위

- Provider가 복구된 Command를 받는 경우 기존 `m.command` Handler에서 `isRecovered()`/`previousStatus()`를 확인하도록 한다. SDK가 사용자 판단 없이 재실행을 보장하지 않는다.
- Consumer는 복구된 명령의 현재 처리 결과, 고아 상태, Provider 소실을 관찰할 수 있어야 한다. `CommandCall` 재연결 또는 같은 Session을 구독하는 `m.report<StatusOperation>` 계약을 정의한다.
- 동일 서비스의 서로 다른 Source, 여러 제어 서비스 간 경합은 특정 ICD 및 Component Specification의 **사용자 정책**이다. SDK가 임의로 승자를 정한다고 약속하지 않는다. Handler에 `source`, 대상, 기존 명령 상태가 제공되어야 정책을 구현할 수 있다.

### 3.6 예시 (C++11 개념적 의사코드)

```cpp
CommandDecision MyAutoPilot::onVector(
    const mo::GlobalVectorCommand& value,
    const umaa::CommandInfo& info)
{
    if (!validate(value))
        return CommandDecision::rejected(CommandReason::ValidationFailed);

    const umaa::CommandToken token = info.token();
    startControlAsync(value, token);
    return CommandDecision::accepted(); // 준비 완료; COMMANDED 가능
}

void MyAutoPilot::onHardwareStarted(const umaa::CommandToken& token) {
    respond(token, CommandResult::executing());
}

void MyAutoPilot::onHardwareFinished(const umaa::CommandToken& token) {
    respond(token, CommandResult::completed());
}

// Consumer: 실행 완료가 아니라 초기 COMMANDED까지만 기다린다
SendOptions opt;
opt.destination(targetId);
opt.responseStage(ResponseStage::Commanded);
CommandCall call = send<mo::GlobalVectorControl>(cmd, opt);
CommandResponse initial = call.get();
```

`CommandToken`은 외부 DDS 개체에 직접 적용하는 Topic 이름이 아니라 SDK의 사용자용 불변 핸들이다. 이 코드는 **시그니처 제안**이고 컴파일 검증 결과가 아니다.

## 4. Report, Status, Key, Dispose

Provider 발행은 `publish<Op>(value, PublishOptions)` 하나로 충분하다. `PublishOptions`는 필요할 때 원본 생성 시각, 타입에 맞는 복합 Key, 관련 `CommandToken`(ExecutionStatus), Source 정책/보고 주기 등을 보존할 수 있어야 한다. 일반적인 Source는 Component Identity에서 자동 채운다.

Consumer에는 둘 중 하나의 Handler 형태를 같은 `m.report`로 지원한다.

```cpp
void onSimplePose(const sa::GlobalPoseReport& value);        // 유효한 Data만 처리
void onPoseEvent(const ReportEvent<sa::GlobalPoseReport>& e); // Data, Disposed, Unavailable 등
```

`ReportEvent<T>` 최소 정보:

- `kind()`: `Data`, `Disposed`, `Unavailable`, `Fault` (마지막 둘은 SDK 관찰 이벤트로, UMAA 표준 Topic 상태와 구분)
- `data()`: 유효한 Data 이벤트에서만 접근 가능한 Payload
- `key()`: 해당 Operation의 **정확한 복합 Key 타입**(단일 GUID로 축소 금지)
- `source()`, `timestamp()`(데이터 origination vs 수신 시각 구분), `isInitial()`/`revision()`(SDK 관찰 정보)
- `completeness()`(중첩 Collection/Specialization이 완성되지 않았을 때 유효 상태)

`dispose<Op>(key)`는 일반 Report/데이터 인스턴스의 폐기이며 `cancel`과 다르다. DDS의 `dispose`와 `unregister`를 같은 것으로 일반화하지 않는다. SDK의 정상 데이터 Handler가 Dispose를 무시하는 단순 오버로드를 제공할 수 있으나, 상태를 유지하는 앱에서는 반드시 Event Handler를 사용해야 한다.

**IDL optional 필드는 C++11에서 `umaa::Optional<T>`(또는 동등한 타입 안전 래퍼)로 표현하고, 값의 미설정과 기본값을 구분한다.** 모든 Key/Enum/Union/Nested Type의 표현은 원본 값·범위·정밀도를 잃지 않아야 한다.

## 5. LargeSet / LargeList (상위 호출 동사 추가 없음)

`LargeSet<T>` = 안정된 Element ID 기반 집합. `LargeList<T>` = Element ID + 순서. 동일 컬렉션 객체를 Report, Command, 중첩 Objective/TaskPlan에도 사용한다.

| 행위 | `LargeSet<T>` | `LargeList<T>` |
|---|---|---|
| Element 생성·수정 | `add(id,value)`, `set(id,value)` | `add([id,]value)`, `insert(index,[id,]value)`, `set(index,value)` |
| 삭제·전체 교체 | `remove(id)`, `clear()`, `replaceAll(items)` | `remove(id)`/`removeAt(index)`, `clear()`, `replaceAll(items)` |
| 순서 변경 | 해당 없음 | `move(from,to)`; Element ID 유지 |
| 읽기 | `elements()`, `find(id)`, `changes()`, `size()` | `elements()`, `at(index)`, `changes()`, `size()` |
| 무결성 | `id()`, `state()`, `version()` | `id()`, `state()`, `version()` |

필수 의미:

1. 기본 Event/Command Handler의 Collection은 **동기화가 완료된 논리적 Snapshot**을 제공한다. `elements()`는 진행 중인 원소와 혼합한 값을 반환하지 않는다.
2. `changes()`는 이번 안정 버전의 Added/Updated/Removed/Moved를 표시하고, 최초 Snapshot의 변경 기준과 재동기화 후 Reset 의미를 정의한다.
3. `state()`는 `Complete/Pending/Gap/Invalid/Disposed` 등을 구분한다. 기본 Snapshot은 Complete를 전달하지만, 누락·유실·진행 중 상태를 알고자 하는 사용자는 Event/Policy로 구분할 수 있다.
4. Element ID와 Collection ID는 복제·이동·변경 중 무분별하게 재생성하지 않는다. 이들의 수명과 다시 사용하는 정책은 ICD와 일치해야 한다.
5. 빈 집합은 **유효한 Complete Empty** 상태이며 '아직 데이터가 없다'와 구분한다. `clear()`는 빈 컬렉션을 생성하는 사용자 의미이고, 제공자 측 Element Topic Dispose·메타데이터 규칙은 SDK가 준수해야 한다.
6. `publish<Op>(containingData)`는 논리적 갱신 경계이다. 여러 Element의 Update/Delete와 메타데이터의 완료 마커를 SDK가 단일 안정 변경 사이클로 다룬다는 계약을 정의한다. 별도 `commit`, `publishCollection` 동사는 필요하지 않다.
7. Large Collection은 6.0 표준에서 **18개 메타데이터 필드**(Large Set 10개 / Large List 8개)와 별도 Element Topic이 확인되었다. 모든 중첩 위치를 지원해야 한다.

## 6. Generalization / Specialization

기본 값 타입 `Specialized<Base>`를 유지한다. 지원 타입은 컴파일된 6.0 IDL/생성 타입의 정확한 매핑을 사용한다.

| API | 의미 |
|---|---|
| `base()` | Generalization 공통 필드 참조 |
| `set(specializedValue)` | 해당 subtype의 내용과 연결 관계 생성/갱신 |
| `is<T>()` | 실제 타입이 T인지 판정. 데이터 미완성일 경우 접근 가능성과 구분해야 함 |
| `as<T>()` | `Ready`일 때 타입 안전하게 접근; 아니면 null |
| `state()` | `Ready/Pending/Missing/Unsupported/Invalid/Disposed` 구분 |
| `type()` | 미지원이어도 원래 Topic/타입 식별 가능 |
| `identity()` | specializationID, 원본 Timestamp 등 보존된 읽기 전용 연결 정보 |
| `clear()` 또는 컨테이너 Element 삭제 | 관계 제거/폐기. 삭제 시 양쪽 Topic의 폐기 의미 준수 |

`has()`는 `Ready`의 편의 검사로 둘 수 있으나 필수 함수가 아니다. `is<T>()`가 참이어도 미완성이라면 `as<T>()`가 null이 될 수 있으므로 **타입과 준비 상태를 따로 표현**한다.

- ICD §3.9는 Base/Specific 사이의 Topic/ID/Timestamp 연결이 맞아야 하고, 변경은 양쪽 데이터의 갱신이 필요하며, 삭제는 양쪽 Topic이 폐기되어야 한다. 중첩 Large Collection을 사용한다면 해당 메타데이터 동기화도 함께 필요하다.
- **알 수 없는 외부 확장 타입**에 대해 최소 API가 전송 내용을 완전히 해석한다고 주장하지 않는다. 미지원의 타입/ID/Timestamp는 손실 없이 식별할 수 있어야 하고, 외부 타입 Payload의 임의 포워딩은 별도 기능 범위이다.
- 확장 6.0 IDL에도 Specialization Topic 48개가 있다. API 생성은 구체 타입 목록을 모두 정적으로 지원해야 한다.

## 7. Configuration / Component 서비스 적합성

- Domain/Partition/Component GUID/Provider Destination/QoS Profile은 소스 재컴파일 없이 바꿀 수 있도록 **설정(파일/런타임 옵션)**을 제공한다. 이는 `send()`의 동사를 늘릴 문제가 아니다.
- `send`의 대상 GUID가 지정되지 않은 경우 선택 정책(단일 Provider, 명시 선택 필요, 구성된 기본 Provider 등)을 문서화한다. 여러 Provider가 있을 때 임의 선택을 묵시적으로 하지 않는다.
- Component가 Service를 제공한다고 표시할 때 SDK는 해당 Service의 **모든 규격 인터페이스**를 공급해야 한다. Ack·CommandStatus는 자동 관리할 수 있지만, 사용자 값이 필요한 ExecutionStatus·일반 Report는 `publish` 경로가 있어야 하며 누락 가능성을 `ServiceContract` 검사로 드러내야 한다.
- `onStart/onStop` 및 `ReportEvent`/`CommandInfo`를 이용해 Ready/Liveliness/Shutdown 중 사용자에게 필요한 오류·안전 조치를 전달한다. 사용자 로직이 요구되는 안전 정지/명령 충돌 정책을 SDK가 임의 고정하지 않는다.
- 신규 `m.discover`, `m.status`, `m.execution`은 **정규 ICD 표현을 위한 필수 최상위 동사로는 제안하지 않는다**. 외부 Provider를 런타임에서 열거하는 별도 앱 요구가 있으면 작은 편의 API로 추가 검토한다.

## 8. 최소 계약을 실제로 사용하기 위한 통과 조건

| Gate | 검증할 사실 | 현재 상태 |
|---|---|---|
| G1 | 모든 정규 ICD의 Service·실제 Topic Operation 이름과 IDL Topic/Namespace 정확 매칭 | **정적 검사 통과: 54 Service, 114/114 Operation** |
| G2 | 컬렉션 Element Topic 전부 지원, 갱신·삭제·순서·빈 상태·재동기화 동작 | **18/18 Element Topic 발견; API 계약 제안, 구현 미검증** |
| G3 | Specialization Topic 전부 지원, ID/Timestamp 동기화·삭제·미지원 상태 | **48/48 Topic 발견; API 계약 제안, 구현 미검증** |
| G4 | 24개의 정규 set Command 경로에 Ack/Status/Terminal/Update/Cancel/Recovery 입출력 | **정적 Operation 분류 및 계약 정의, 프로토콜 검증 전** |
| G5 | Command/Report의 복합 Key, 원본 Timestamp, `@optional` 필드 및 모든 nested type | **594 IDL / 1,705 include 연결 검사(미해결 0건), 타입 생성 및 값 검증 전** |
| G6 | 54개 정규 서비스의 Provider·Consumer 양방향 사용 코드, 다중 Operation 및 누락 거부 | **정적 매핑 작성, 컴파일·실행 검증 전** |
| G7 | Experimental 86개 Service, 203개 Topic Operation의 생성 타입 | **203 Operation 행 중 200행은 IDL Topic·Type 일치. 3행은 @nested Type이며 DDS Topic 선언 없음. ContactFilterConfig 3행 중복 주의** |
| G8 | RTI Connext 및 타 DDS의 실제 상호운용성/적합성 | **수행하지 않음** |

**판정:** v0.8.1 설계는 114개 정규 Operation과 18개 Large Collection Element, 48개 Specialization Topic에 대한 사용자 API 경로를 **정의**한다. Experimental에서는 3개 Topic/Type 예외가 남는다. 이것을 *실제 SDK가 UMAA ICD 전체를 만족한다는 검증 완료*로 바꿔 말해서는 안 된다.

## 9. 근거 자료

- UMAA ICD 6.0: EO §3.8/§3.9/§5, MO·MM·SA·SEM·SO 각 Flow Control 및 Service Interfaces 장; CO-EXP/EXP의 실험적 표기.
- 인계 ZIP: `UMAA_Component_SDK_Handoff_v0.7_2026-11-10(1).zip`, `01_CURRENT_API/*`, `references/umaa-6.0-idl.zip`. 최신 확장 자료: `umaa-6.0-idl-with-exp-services.zip` (기존 352개 IDL + 242개 신규 IDL; 기존 파일 352개는 바이너리 동일).
- RTI UMAA Starter Kit README: <https://github.com/rticommunity/rticonnextdds-usecases-umaa> (C++ 예제의 범위는 DDS Middleware 레벨 중심; App-level Flow Control/Collection/Specialization 제외라고 명시).
- AUVSI 공식 배포/적합성 설명: <https://www.auvsi.org/advocacy/advocacy-initiatives/unmanned-maritime-autonomy-architecture/>.
- 본 검토의 자동 추출 매핑표: `ICD_Service_Summary_v0.8.1.csv`, `ICD_Operation_vs_IDL_v0.8.1.csv`, `IDL_Auxiliary_Topics_v0.8.1.csv`, `IDL_Large_Collections_v0.8.1.csv`.
