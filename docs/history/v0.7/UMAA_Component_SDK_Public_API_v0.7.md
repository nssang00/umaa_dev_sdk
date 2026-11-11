# UMAA Component SDK — Public API 설계 v0.7

> **상태:** Public API 계약 검토안 (2026-11-10)  
> **기준:** C++11, UMAA ICD 6.0, AutoPilot 14 Service / 56 Operation Handoff  
> **범위:** Component 사용자에게 보이는 API와 의미. 내부 DDS 구현·빌드·성능·ABI·테스트는 제외.  
> **명명 주의:** 아래 `Service::Operation`, `...Data`, `...Options` 등은 SDK 생성/제안 타입이다. 원본 IDL 이름과 1:1 동일하다고 간주하지 않는다.

## A. 설계 원칙

1. **Full UMAA Capability, Minimal Public API.** Command/Response, Request/Reply, Large Collections, Generalization/Specialization, Control/Config/Specs/Status·Report 기능을 모두 표현하되, 사용자에게 기능마다 새 Component 함수를 강제하지 않는다.
2. `AutoPilot : public umaa::Component`를 직접 상속한다. 사용자 Handler 선언과 `UMAA_COMPONENT` 등록 블록은 **AutoPilot.hpp의 클래스 선언 바로 아래**, Handler 구현은 AutoPilot.cpp에 둔다.
3. 등록은 `m.command<S>()`, `m.cancel<S>()`, `m.report<S>()`를 기본으로 하고, 명시적 Query Data가 정의된 Request/Reply에만 조건부 `m.request<Op>()`를 사용한다. **Large Collection 또는 Specialization이라는 이유로 등록 함수가 바뀌지 않는다.**
4. 호출은 `send`, `cancel`, `publish`, `dispose`, `respond`를 사용한다. 실제 Query Payload를 가진 Operation에는 조건부 `request`를 제공한다.
5. `send`/`cancel`/`request`는 C++11 Future 형태의 대기 또는 호출 시 Lambda/Callback을 지원한다. `std::future`에 없는 `.then()`/`co_await`를 C++11 기본 API로 가정하지 않는다.
6. 사용자에게 CommandSession, DDS Listener, Topic ID/Metadata 상관관계 관리를 요구하지 않는다. 명령 식별, 필요한 수신 문맥과 결과는 **읽기 전용** 정보/핸들로 제공한다.
7. Service/Operation 등록은 타입 안전해야 한다. Service에 해당 종류의 Operation이 하나라면 `m.command<Service>` 형태, 여러 개면 `m.command<Service::SetX>` 형태로 모호성을 제거한다.
8. 기능을 숨기지 않는다. 기본 Handler는 단순 데이터로, 고급 Handler는 `CommandInfo`/`ReportEvent<T>`를 선택해 원본 상태·식별자·오류를 확인할 수 있어야 한다.

## B. 최소 Component Public API

| 영역 | API | 사용자 의미 | 제공 조건 |
|---|---|---|---|
| 이벤트 | `m.command<S>(handler)` | Provider가 새/갱신/복구 Command 처리 | 기본 |
| 이벤트 | `m.cancel<S>(handler)` | Provider가 Command 취소 요청 처리 | 해당 Control/Config |
| 이벤트 | `m.report<S>(handler)` | Consumer가 일반/상태/Ack/실행/컬렉션/특수 데이터 수신 | 기본 |
| 이벤트 | `m.request<Op>(handler)` | Query Payload가 있는 요청 수신 | 해당 Operation에만 |
| 동작 | `send<Op>(command, ...)` | Consumer Command 송신 + 선택적 비동기 응답 | 기본 |
| 동작 | `cancel(commandId, ...)` | 기존 Command의 취소 요청 + 최종 상태 확인 | 기본 |
| 동작 | `publish<Op>(data, ...)` | Provider Report·Execution Status·컬렉션·특수 데이터 발행 | 기본 |
| 동작 | `dispose<Op>(key)` | Report/데이터 인스턴스 폐기 | Keyed Report/데이터 |
| 동작 | `respond(commandId, result)` | Provider의 실행 준비·시작·완료·실패·취소를 SDK에 통지 | Command Provider |
| 동작 | `request<Op>(query, ...)` | Query Payload 포함 요청 송신/응답 | 해당 Operation에만 |
| 수명주기 | `onStart()` / `onStop()` | 필요한 Component만 override | 선택 |

**기본 API에서 제외:** `m.collection`, `m.specialized`, `m.status`, `m.execution`, `sendAsync`, `publishCollection`, `unpublish`, `latest`, `next`, 독립 `subscribe`. 이 기능의 *표현 능력*은 누락시키지 않고 `m.report`, 반환 객체, 데이터 타입, 선택적 구독 정책으로 제공한다.

## C. AutoPilot.hpp — 사용자 경험

```cpp
#pragma once
#include <umaa/umaa.hpp>

class AutoPilot : public umaa::Component {
public:
    umaa::CommandDecision onVector(
        const mo::GlobalVectorCommand& command,
        const umaa::CommandInfo& info);

    umaa::CancelDecision onCancelVector(
        const umaa::CommandInfo& info);

    void onPose(const sa::GlobalPoseReport& report);
    void onConditionals(const mm::ConditionalReportData& report);

protected:
    void onStart() override;  // optional
    void onStop() override;   // optional
};

UMAA_COMPONENT(AutoPilot, m) {
    m.command<mo::GlobalVectorControl>(&AutoPilot::onVector);
    m.cancel<mo::GlobalVectorControl>(&AutoPilot::onCancelVector);
    m.report<sa::GlobalPoseStatus>(&AutoPilot::onPose);
    m.report<mm::ConditionalReport>(&AutoPilot::onConditionals);
}
```

Lambda는 같은 등록 함수의 오버로드로 지원한다. 헤더 밖 등록 블록에서는 `[this]`가 없으므로 `[](AutoPilot& self, const T& value) { ... }`처럼 인스턴스 인자를 제공한다. 정적 등록 Handler가 `private`인 경우 별도 접근 제어 계약이 필요하다.

## D. Command/Response — 중요한 공개 계약

### D.1 Consumer의 CommandCall

```cpp
// 기본: 어느 응답에서 완료할지 공개 계약으로 정의
umaa::CommandCall call = send<mo::GlobalVectorControl>(cmd);
auto id = call.id();
auto result = call.get();  // 기다리는 경우에만 블로킹

// 실행 완료까지 관찰하고 싶을 때
umaa::SendOptions options;
options.destination(providerId);
options.responseStage(umaa::ResponseStage::Terminal);
auto terminal = send<mo::GlobalVectorControl>(cmd, options).get();

// Qt 스타일 Callback (C++11)
send<mo::GlobalVectorControl>(cmd, options,
    [](const umaa::CommandResponse& response) {
        // status / reason / timeout / transport error 구분
    });

// 동일 source + session의 Command 갱신
// 응답 객체는 기존 ID를 공유하지만 새로운 갱신에 대한 결과를 나타냄
auto updatedCall = call.update(updatedCmd);
auto updateResult = updatedCall.get();

// 취소는 DDS Dispose 기반이며, 별도의 Cancel Reply가 아님
auto cancelCall = cancel(id);
auto finalResult = cancelCall.get();
```

**필수 의미:**
- `ResponseStage`와 `CommandStatus`는 다르다. 전자는 Future/Callback의 **완료 조건**, 후자는 실제 UMAA **상태**이다.
- `ISSUED → COMMANDED → EXECUTING → COMPLETED`는 정상 흐름의 단계이며, `COMMANDED`가 실행 완료를 의미하지 않는다. `FAILED`, `CANCELED`, `UPDATED`에 의한 재진입을 반영한다.
- Terminal 완료는 `COMPLETED/FAILED/CANCELED`를 포함하고, 대기 중 실패·취소·타임아웃은 API가 명시적으로 표현해야 한다.
- `wait_for(duration)`은 호출 스레드의 대기 시간에 관한 것이지 **Command 취소**가 아니다.
- `CommandResponse`에는 최소 `id/session`, `status`, `reason`, `outcome`(성공·실패·취소·timeout·local transport error 구별)이 있어야 한다.
- `CommandCall`은 Session 관리자라기보다 호출 결과와 식별자를 제공하는 값 타입이다. 중간 상태는 `m.report<CommandStatusOperation>()`로도 관찰한다. 단일 Command에 대한 `call.onStatus(...)`는 편의 API 후보이며 표준 기능 표현을 위해 반드시 필요한 새 동사는 아니다.
- `send()` 호출의 기본 완료 시점은 제안 중인 `ResponseStage::Commanded`이지만, **UMAA 표준이 지정한 SDK 기본값은 아니다**. 사용자가 오해하지 않도록 문서와 옵션에 드러나야 한다.

### D.2 Provider Command Handler와 지연 완료

```cpp
umaa::CommandDecision AutoPilot::onVector(
    const mo::GlobalVectorCommand& cmd,
    const umaa::CommandInfo& info)
{
    if (!validate(cmd))
        return umaa::CommandDecision::rejected(
            umaa::CommandReason::ValidationFailed);

    if (requiresAsyncValidation(cmd)) {
        startAsyncValidation(info.id(), cmd);
        return umaa::CommandDecision::deferred();
    }

    scheduleHardware(cmd, info.id());
    return umaa::CommandDecision::accepted();
}

// 아래는 사용자 표현 예시이며 실제 CommandResult 타입/함수명은 확정 대상
respond(id, umaa::CommandResult::commanded()); // Deferred 수락 처리
respond(id, umaa::CommandResult::executing()); // 실제 시작
respond(id, umaa::CommandResult::completed());
respond(id, umaa::CommandResult::failed(reason));
respond(id, umaa::CommandResult::canceled());
```

**주의:** 즉시 Accepted는 *실행 준비가 완료되어 COMMANDED 단계로 갈 수 있다*는 계약으로 좁혀야 한다. 아직 자원 검증 중이면 Deferred를 사용한다. 취소 요청을 수신한 시점과 실제 취소 완료 시점은 다르다. `m.cancel`의 결과와 `respond(id, canceled())`는 이 차이를 표현한다. `respond()`는 허용되지 않는 상태 전이를 만들지 않아야 한다.

### D.3 실행 정보 및 중간 상태

```cpp
// 상태·Ack는 SDK 관리. 필요 시 Consumer가 Report Operation으로 구독
m.report<SomeService::ReportCommandStatus>(&AutoPilot::onStatus);
m.report<SomeService::ReportCommandAck>(&AutoPilot::onAck);

// 사용자만 아는 실행 데이터는 Provider가 Command ID를 연결해 발행
publish<SomeService::ReportExecutionStatus>(id, executionData);
```

하나의 Service에 Status/Ack/Execution 여러 Report가 있으므로 **Operation 타입 명시**가 필요하다. SDK가 생성할 Operation 태그 이름은 실제 ICD 이름을 근거로 생성한다.

### D.4 CommandInfo (읽기 전용)

기본 Payload Handler와 고급 `(const Command&, const CommandInfo&)` 오버로드를 모두 지원한다. 고급 정보의 최소 범위는 `id()`, `source()`, `destination()`, `isUpdate()`, `isRecovered()`이며 시간 정보/원본 상태가 필요한 경우 메타데이터 View를 통해 접근한다. 자동 복구에 사용자 의사결정이 필요한 경우 별도의 메서드 추가 없이 해당 Command Handler로 전달할 수 있지만, **복구를 자동 재실행으로 오해하지 않도록 계약**이 필요하다.

## E. Request/Reply, Report, Dispose

UMAA 6.0 ICD의 Query Data 없는 Request/Reply는 별도 RPC 호출이 아니라 **Report Topic 구독**이다. Provider는 `publish`, Consumer는 `m.report`를 사용한다. 6.0 EO ICD §5.2.2는 Query Data가 있는 Request/Reply를 아직 정의하지 않는다. 따라서 `request/m.request`는 **미래/확장 Operation이 실제 Query Payload를 정의했을 때만 노출**하며, 현재 `query*`를 가짜 RPC로 변환하지 않는다.

```cpp
// 기본 지속 Report
m.report<sa::GlobalPoseStatus>(&AutoPilot::onPose);

// 수신 메타데이터 또는 Dispose가 필요할 때 (동일 m.report 사용)
void onPoseEvent(const umaa::ReportEvent<sa::GlobalPoseReport>& event);
// event.kind(): Data 또는 Disposed 등
// event.key(), event.source(), event.data() (Data일 때만 유효)

// Provider의 명시적 인스턴스 폐기
publish<so::HealthReport>(health);
dispose<so::HealthReport>(reportKey);
```

**누락되면 안 되는 계약:** Report의 다중 인스턴스 Key, Source 식별/선택, 원본 Timestamp, 초기 데이터/변경 데이터의 구별, Dispose 수신 알림. `m.report`의 Value Handler만 등록한 경우 Dispose는 SDK가 내부적으로 관리할 수 있지만, 사용자가 데이터 무효화를 알아야 할 때 `ReportEvent<T>` Handler를 선택할 수 있어야 한다. `dispose`는 보고용 인스턴스와 관련 데이터의 폐기를 뜻하고, **Command 취소는 항상 `cancel`**로 구별한다. 일부 DDS 객체에는 `unregister`와 `dispose`가 다른 의미를 가지므로 두 동작을 혼동하지 않는다.

필요하다면 `m.report<S>(handler, ReportOptions)`에 Source Filter 및 동기화 전달 정책을 둔다. 정적 Component 구독을 기본으로 삼되 런타임 구독 시작/해제가 사용자 요구에 포함된다면 **동적 구독 제어 계약**이 추가로 필요하다(현재 필수 확정 아님).

## F. Large Collections — Map/List 스타일

Large Collection은 `Report`뿐 아니라 **Command와 일반 중첩 데이터 타입에도 포함**된다. `m.report`/`m.command` 선택은 Operation 종류로 결정하고, Collection 여부에 따라 등록 동사를 바꾸지 않는다.

| 작업 | `LargeSet<T>` (ID 기반 Map) | `LargeList<T>` (순서 + ID 기반 List) |
|---|---|---|
| 추가 | `add(id, value)` | `add(value)` / `add(id, value)` |
| 삽입 | 해당 없음 | `insert(index, value)` / ID 오버로드 |
| 추가 또는 교체 | `set(id, value)` | `set(index, value)` |
| 삭제 | `remove(id)` | `removeAt(index)` / `remove(id)` 선택 |
| 전체 교체 | `replaceAll(elements)` | `replaceAll(elements)` |
| 전체 비우기 | `clear()` | `clear()` |
| 재배치 | 해당 없음 | `move(from, to)` |
| 조회 | `find(id)` / `contains(id)` | `at(index)` |
| 공통 읽기 | `elements()`, `changes()`, `size()`, range-for | 동일 |

```cpp
// Provider: 하나의 논리적 수정 묶음
mm::ConditionalReportData report;
auto& set = report.conditionals();
set.add(idA, depthCondition);
set.set(idB, logicalCondition);
set.remove(idC);
publish<ReportOperation>(report);

// 전체 교체는 기존 데이터 중 목록에 없는 요소도 제거한다는 뜻
set.replaceAll(allElements);
publish<ReportOperation>(report);

// Consumer: 조건 Report 안의 이름 있는 Large Set
void AutoPilot::onConditionals(const mm::ConditionalReportData& report) {
    const auto& set = report.conditionals();
    for (const auto& element : set.elements())
        process(element.id(), element.value());
    for (const auto& change : set.changes())
        processChange(change);
}
```

**필수 사용자 의미:**
- Element ID는 LargeList에서도 존재하며, `move` 후에도 동일 요소의 ID는 유지한다. `add()`/`insert()`는 ID 생성 또는 명시 ID 입력을 지원해야 한다.
- `replaceAll` vs `set` vs `clear`의 의미는 서로 다르다. 변경 묶음의 발행 경계는 `publish` 시점이며 SDK가 이전 발행 상태와의 차이를 관리한다는 공개 계약을 세워야 한다.
- `elements()`는 현재 완성된 논리적 상태, `changes()`는 **이번 전달에서 확정된 변경분**이다. 초기 생성 시 변경분 표현·Reset 이벤트의 의미를 정의한다.
- 기본 `m.report`는 동기화 완료된 데이터만 제공한다. 진행 중 요소를 노출하는 `Delivery::Progressive`는 옵션이며, 이를 지원하는 경우 `isComplete()` 또는 이벤트 상태를 반드시 제공해야 한다.
- `LargeSet/List`은 원본 IDL Metadata의 존재를 숨기지만 필요하면 `setID/listID`, `elementID`, timestamps를 읽기 전용으로 확인할 경로가 있어야 한다.

### F.1 코드 생성 규칙

- IDL 필드 타입이 `LargeSetMetadata`이고 이름이 `*SetMetadata`이면 접미사를 제거해 접근자를 생성한다.
- IDL 필드 타입이 `LargeListMetadata`이고 이름이 `*ListMetadata`이면 접미사를 제거해 접근자를 생성한다.
- **임의 복수형 변환 금지:** `missionPlanSetMetadata` → `missionPlan()`, `conditionalsSetMetadata` → `conditionals()`.
- Element IDL 타입·Collection 소유 타입을 함께 검사한다. 단순 문자열 치환만 하지 않는다.
- 이름 충돌은 코드 생성 오류 또는 명시적 매핑으로 해결한다.

## G. Generalization/Specialization — 합의한 이름 반영

```cpp
umaa::Specialized<ConditionalType> condition;
condition.base() = commonData;  // Generalization의 공통 속성
condition.set(depthDetail);     // Specialization 설정/교체

// Consumer: 타입 검사와 접근
const auto& general = condition.base();
if (condition.is<DepthConditionalType>()) {
    const DepthConditionalType* depth =
        condition.as<DepthConditionalType>();
    if (depth) processDepth(general, *depth);
}

// 데이터가 유효하게 해석된 Specialization을 포함하는지
bool available = condition.has();
```

| API | 의미 | 반환/동작 |
|---|---|---|
| `base()` | Generalization 공통 데이터 조회/설정 | const/non-const Base 참조 |
| `is<T>()` | 현재 접근 가능한 Specialization이 T인지 검사 | bool |
| `as<T>()` | 현재 접근 가능한 Specialization을 T로 조회 | `T*` 또는 `const T*`, 불일치 시 null |
| `has()` | 접근 가능한 Specialization 데이터가 있는지 | bool |
| `set(value)` | Specialization 설정·교체 | value 타입으로 식별, 발행과는 별개 |

**주의:** UMAA 일반화/특수화는 실제 C++ 상속 관계가 아니다. 기본 정책이 완성된 객체만 전달하는 경우 `has()`는 보통 true여서 사용 빈도가 낮다. 그러나 **데이터 없음 / 수신 중 / 미지원 타입 / 무효 데이터**를 `has()==false` 하나로 합치면 정보가 손실된다. 이 차이는 `ReportEvent`의 `state()`/`info()` 같은 읽기 전용 이벤트 문맥으로 표현해야 한다. 미지원 Specialization ID/Topic 식별자에 접근할 경로도 보존해야 한다.

수정·삭제는 기존 `set`/상위 Collection의 `remove`/독립 인스턴스의 `dispose`로 표현한다. 별도 `publishSpecialization`을 추가하지 않는다.

## H. Service Type 적용

| Type | 주요 사용자 작업 | 기본 Public API |
|---|---|---|
| Control | Command 수신·전송·갱신·취소·진행 보고 | `m.command/m.cancel/m.report`, `send/cancel/respond/publish` |
| Config | Config Command·Ack·Report·실행 상태 | 위와 동일, Operation 타입만 구분 |
| Specs | 사양/Capability 발행·구독 | `publish`, `m.report`, 필요 시 `dispose` |
| Status/Report | 상태/Report 발행·구독·인스턴스 제거 | `publish`, `m.report`, `dispose` |

서비스 타입마다 Component 함수군을 복제하지 않는다. 같은 서비스에 서로 다른 Command/Report가 있으면 `Service::Operation` 타입을 지정해 모호성을 해소한다.

## I. 사용자에게 반드시 보이는 정보 — 최소 계약

1. **Command 결과:** ID, 실제 UMAA 상태·사유, 명령 갱신/취소/완료, 응답 오류/타임아웃, 필요 시 Destination/Source, 최신 업데이트 여부, 복구 여부.
2. **Report 이벤트:** 실제 값, Data/Disposed 구분, Source, Key, Timestamp, 최초/갱신 구분, 필터 정책.
3. **Large Collection:** Collection ID, Element ID/값, 순서(List), 전체와 변경분, 빈 Collection, 초기화·완성 여부.
4. **Specialization:** 공통값과 구체값, 실제 타입 확인, 값 존재 여부, 미지원/미완성 데이터 상태, 원본 연결 식별자 접근 경로.
5. **Service 선택:** Provider/Consumer 역할, Service 이름, Operation 이름, 다중 Operation 선택/지원 불가 진단.
6. **비동기:** Future/Callback 결과와 호출 수명, 정상/실패/Timeout 구분. Callback을 등록하지 않은 호출의 오류도 SDK 차원에서 관찰 가능해야 한다.

## J. 확정도와 남은 Public API 계약

**유지 방향(합의):** `Component` 상속, Header `UMAA_COMPONENT`, `m.command/m.cancel/m.report`, `send/cancel/publish/dispose/respond`, Future/Lambda, `LargeSet/List`의 Map/List 인터페이스, `elements/changes`, 접미사 제거 코드 생성, `Specialized<T>::base/is/as/has/set`.

**공개 계약의 보완이 필요한 부분(내부 구현 문제가 아님):**

- `CommandDecision`의 거절 사유/Deferred, `respond`의 Commanded/Executing/Terminal 전달 규칙.
- `CommandCall::update` 및 ResponseStage·Cancel 결과·Timeout의 구체 반환 계약.
- `CommandInfo`, `ReportEvent<T>`, `SendOptions`/`ReportOptions`를 사용자에게 어느 시그니처로 노출할지.
- `dispose`의 대상 Key 및 Dispose 이벤트의 수신 의미.
- `LargeList` Element ID 보존, `changes()`의 초기/리셋/삭제 의미, 특수화 미지원 상태 전달.
- Query Data 있는 Operation이 미래에 정의되면 `request/m.request`를 어떤 타입 시그니처로 열지 (현재 6.0 기본 기능에는 실제 Query Payload 없음).

**필수로 추가하지 않는 API:** 별도 `m.collection`, `m.specialized`, `m.status`, `m.execution`, `sendAsync`, `publishCollection`, `unpublish`, `latest`, `next`, `getIf`, `setSpecialization`, `specialize(base,detail)` (생성은 `Specialized<T>::set`으로 가능).

## K. 검토 근거

- Handoff: `metadata/autopilot_service_manifest.json`, `metadata/autopilot_operations.csv` (14 Services, 56 Operations)
- Handoff: `references/umaa-6.0-idl.zip` (IDL 352개; Large Set/List Metadata 필드 10개)
- UMAA 6.0 EO ICD, §3.8 Large Collections, §3.9 Generalization/Specialization, §5.1 Command/Response, §5.2 Request/Reply: https://github.com/rticommunity/rticonnextdds-usecases-umaa/blob/main/docs/UMAA-SPEC-EOICD.md
- RTI UMAA Starter Kit: https://github.com/rticommunity/rticonnextdds-usecases-umaa

> **검토 한계:** 표준 구조와 AutoPilot 14개 Service의 사용자 행위에 대한 정적 설계 평가다. 전체 UMAA 6.0 Service/Operation 각각의 적합성·QoS·실제 DDS 동작이 검증된 것은 아니다.
