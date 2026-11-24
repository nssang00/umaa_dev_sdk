# UMAA Component SDK — 사용자 Public API v0.8.7

> 2026-11-24 · C++11 · 설계 계약 (미구현). [v0.8.6](UMAA_Component_SDK_Public_API_v0.8.6.md)을 대체한다. 원본 ICD/IDL 규범 및 실제 RTI 컴파일/통신 검증을 대체하지 않는다.

## 1. 공개 계층 및 최소 동사

사용자는 Component를 상속하고 생성된 Service별 Provider/Consumer를 멤버로 소유한다. 각 facade는 Service를 한 번 상속하며 내부 네 역할 구현을 합성한다. 역할은 Service suffix가 아닌 Operation inventory로 정한다. GlobalPoseConfig 등 혼합 Service에는 필요한 역할의 API 합집합만 생성한다. Queryless Request/Reply는 구독이며 QueryData RPC가 아니다.

| 내부 역할 | 공개 함수 이름 |
|---|---|
| CommandProvider | onCommand, onCancel, respond |
| CommandConsumer | command, onResponse |
| ReportProvider | publish, remove |
| ReportConsumer | onReport, getLatest |

총 9개 이름 (오버로드, Component lifecycle, value/handle API 제외). `CommandHandle`은 update/cancel/get/wait/release(수동 유지용), `Subscription`은 close. 함수 통합은 의미를 제거하지 않는다. C++11의 typed overload와 명시적인 옵션/이벤트를 사용하며 범용 void*·문자열 Action·virtual template은 사용하지 않는다.

```cpp
// 개념 서명: 실제 Operation별 생성 overload와 타입 제약은 generator가 결정
Registration onCommand(CommandHandler handler);
Registration onCancel(CancelHandler handler);
Result respond(CommandToken token, CommandProgress progress);
Result respond(CommandToken token, const ExecutionIDL& detail);
Result respond(CommandToken token, CommandProgress progress,
               const ExecutionIDL& detail);
CommandHandle<CommandIDL> command(const CommandIDL& data,
                                  const CommandOptions& opts = {});
Registration onResponse(ResponseHandler handler,
                        const ResponseOptions& opts = {});
Result publish(const ReportIDL& data, const PublishOptions& opts = {});
Result remove(const ReportIDL& keyedData);
Subscription onReport(ReportHandler handler,
                      const ReceiveOptions& opts = {});
LatestResult<ReportIDL> getLatest(const TypedKey& key,
                                 const CacheOptions& opts = {}) const;
```

이는 독립 클래스 하나의 헤더가 아니라 역할별 서명 집합이다. Registration/Subscription은 명시적 close 방식의 동일한 등록 핸들 타입으로 통일 가능하다. 반환값을 무시하거나 복사본을 파괴해도 등록/Command가 취소되지 않는다. Component stop이 모든 등록을 종료한다. 명시적 close는 공유 등록을 닫으며 멱등이다. 등록 중복은 (Service, role, Op, event/response type, filter) 단위로 검사한다. 다른 원본 응답 타입 등록은 서로 덮어쓰지 않는다. 명시적 다중 구독이 필요하면 독립 등록으로 관리하고 중복 수신 정책을 명시한다.

Operation 선택: (Service, role, 원본 IDL 타입)이 유일할 때 추론, 아니면 `<Op>` 또는 typed tag를 요구한다. 복합 Key가 같은 경우도 동일하다. 모호한 overload/잘못된 역할은 컴파일 또는 생성 오류. C++11 explicit lambda parameter/typed callable 사용; generic lambda나 std::optional 불가. IDL public field/accessor 매핑을 혼용하지 않는다.

## 2. Command Provider

`onCommand(const Cmd&, const CommandInfo&)`는 New/Updated/Recovered를 delivery 값으로 구분한다. info는 source/destination/session/timestamp, token(갱신 세대), 이전 상태 및 복구 정보를 제공한다. 반환 CommandDecision은 accepted/rejected(reason)/deferred/resume 등 의미 있는 판단을 보존한다. accepted는 실행 완료 아님. recovered를 무조건 실행하지 않는다. 검증/자원 준비 비동기는 deferred 후 respond로 이어진다.

`onCancel(const CancelInfo&)`는 원본 payload가 없을 수 있어 분리한다. token, 대상 명령, 원인(Unknown 허용), 이전 상태를 포함한다. CancelDecision은 canceled(정지 확인), deferred, cannotCancel을 표현한다. Cancel hook 누락을 자동 CANCELED로 바꾸지 않는다. 안전 중단이 필요한 Operation은 등록 검증으로 요구한다. Liveliness loss와 명시적 취소를 가능한 범위에서 구분한다.

`respond(token, CommandProgress)`는 commanded/executing/completed/failed(reason)/canceled 등 실제 상태와 사유를 전달한다. ACK/CommandStatus 발행, 상태 전이, 종료/정리는 내부에서 규범에 따라 처리한다. `respond(token, executionIDL)`와 상태+상세 overload는 실제 ExecutionStatus를 정의한 Operation에만 생성. 상태와 상세를 함께 제출해도 cross-topic 원자적 전달을 보장하지 않는다. stale token, 종료/취소 경쟁, 불법 상태 전이는 검증된 로컬 오류로 반환하고 새 세대를 변경하지 않는다. Callback 반환 이전/중 발생하는 동기 장비 이벤트는 직렬화해서 decision이 적용되기 전에 잘못된 응답이 송신되지 않도록 한다.

## 3. Command Consumer

```cpp
consumer.command(cmd); // 새 세션, 반환값 무시 가능
auto h = consumer.command(cmd, options);
h.update(revised); // 같은 Source/Session, 새로운 Timestamp/세대
h.cancel();        // 취소 제출, 실제 정지/최종 CANCELED와 다름
auto snap = h.get();
auto observed = h.wait(umaa::WaitStage::Commanded, timeout);
```

CommandOptions: destination(명시/구성 기반 기본값; 모호한 대상 오류), observation timeout, retention(Automatic/Manual), 기타 해당 Operation 옵션. timeout은 원격 FAILED를 만들지 않는다. Local submission Result와 원격 상태/ACK를 구분한다. 원본 IDL 객체를 변경하지 않고 전송 복사본에 SDK 관리 metadata를 채우며 충돌 정책을 명시한다. Handle은 세션 식별/추적 객체이지 ACK나 RAII 취소 객체가 아니다. get은 상태, reason, 관찰 오류, 식별 및 완전성 정보를 가진 snapshot. wait는 stage/timeout과 관찰 결과를 구분하며 동일 dispatch thread에서 블로킹하지 못하게 한다. Manual retention에서는 terminal 이후 `h.release()`로 소유 인스턴스 정리 요청; 활성 명령 release는 오류, cancel과 구분. Automatic은 ICD 절차에 따라 정리하고 관찰 snapshot 수명은 별도 정책으로 관리. 복구된 command의 Handle도 같은 동작을 지원한다.

```cpp
consumer.onResponse([](const VectorAck& ack) { /* original IDL */ });
consumer.onResponse([](const VectorStatus& status) { /* original IDL */ });
consumer.onResponse([](const VectorExecution& detail) { /* original IDL */ });
```

onResponse는 typed original IDL 또는 원본+CommandInfo callback, 그리고 `ResponseEvent<Op>` callback으로 제공한다. 이벤트는 Ack/Status/Execution/ProviderLost/ObservationError를 구분하며 해당 Op의 typed payload accessor만 제공한다. 상태/이벤트 payload 부재가 허용되는 경우 nullable 또는 검사 가능한 값 객체로 표현한다. 같은 Op에서 raw/event 관찰 중복은 명시적으로 관리한다. SDK 관찰 오류를 원격 Status로 합성하지 않는다.

복구는 ResponseOptions의 per-Operation RecoveryOptions로 구성: Continue/Cancel 정책 또는 `RecoveryDecision(const RecoveredCommand<Op>&)` callback. 복구 정책은 시작 전 등록/검증하며 관찰 handler를 등록하지 않아도 정책 설정이 가능하도록 typed overload(예: `onResponse<Op>(ResponseOptions)`)를 제공한다. 원본 command payload/세션/이전 상태를 확보한 경우에만 전달하고, 확보 실패는 별도 관찰 오류/안전 정책으로 처리한다. 관찰 callback의 반환값을 복구 판단으로 혼동하지 않는다.

## 4. Report Provider

```cpp
provider.publish(report);
provider.publish(reports, umaa::PublishOptions(umaa::PublishMode::Upsert));
provider.publish(reports, umaa::PublishOptions(umaa::PublishMode::ReplaceOwnedInstances));
provider.remove(reportWithKey);
provider.remove<ReportOp>(typedKey);
```

publish는 생성/갱신, batch 및 snapshot, collection attachment를 지원한다. ReplaceOwnedInstances는 해당 Operation의 SDK 소유 Key만 정리하며 빈 typed vector로 전체 소유 데이터 제거 가능. 타 Provider 인스턴스 삭제 금지. batch 실패는 per-item 결과/부분 성공을 구분한다. remove는 Key만 필요하며 유효한 non-Key payload를 요구하지 않는다. DDS Dispose/연결된 collection 및 specialization의 라이프사이클은 내부 책임. 주기/이벤트/변경 발행은 명세에 맞춘 Service 기본 설정 및 PublishOptions/ReportPolicy로 구성; event/log처럼 같은 값이 반복되는 경우 기본 Upsert가 이벤트를 중복 제거하지 않도록 Operation별 정책을 적용한다.

## 5. Report Consumer

```cpp
pose.onReport([](const Pose& report) { use(report); });
pose.onReport([](const umaa::ReportEvent<Pose>& event) {
    if (event.kind() == umaa::ReportKind::Received && event.data())
        use(*event.data());
    else if (event.kind() == umaa::ReportKind::Removed)
        invalidate(event.key());
}, options);
```

정상 raw callback에는 유효한 원본 IDL만 전달한다. event callback은 Received/Removed/Incomplete/Stale/SourceLost 등과 typed key/source/time/assembly metadata를 표현한다. Removed에는 payload가 없을 수 있어 data()는 nullable이며 원본 객체를 조작해 만들지 않는다. SourceLost != Removed != Stale. 선택적으로 원본+ReportInfo/assembled collection callback 지원. 동일 Op raw/event 등록의 중복 정책 명시. 기본 CompleteOnly; AllowPartial을 요청한 경우 completeness와 gap 상태를 반환한다. ReceiveOptions는 delivery(All/ChangedOnly), assembly(CompleteOnly/AllowPartial), source filter, event mask, freshness 및 수동/자동 구독 설정. event형 callback 기본 mask는 정상 수신+제거+오류/상실 등 정의된 이벤트, raw형은 정상 수신만. 잘못된 이벤트 옵션은 등록 오류.

`m.add(&Component::pose)` 기본 자동 구독: 시작 전 onReport 등록, 시작 시 활성화. `m.add(..., SubscriptionMode::Manual)` 수동 구독: 실행 중 onReport가 등록/활성화하고 Subscription 반환. 동일 Op 자동/수동 동시 활성화는 오류. 두 모드 모두 반환값 파괴로 종료되지 않고 명시적 close 또는 Component stop으로 종료된다. close 이후 해당 등록의 callback drain 계약을 제공하되 callback 내부 close는 자기 자신을 기다리지 않는다. 수신 callback 등록/필터 변경은 닫고 재등록하거나 옵션으로 정의한다. RPC request는 추가하지 않는다.

getLatest는 optional typed cache result (C++11 hasValue/value 또는 안전한 result)로 freshness/source/assembly 및 오류를 함께 조회한다. 단일 Key 또는 typed selector를 사용해 여러 값의 snapshot을 같은 이름 overload로 얻는다. 캐시 비활성/미수신/removed/stale/불완전은 서로 구분한다. Source/복합 key 충돌을 피한다. getLatest는 네트워크 요청이 아니다.

## 6. 공통 데이터 모델: Command와 Report 모두

원본 IDL은 metadata만 가진 경우 실제 collection 필드를 새로 만들지 않는다. 생성된 `Attachments<Op>` 또는 유일한 typed collection overload로 원본 payload와 분리 topic 데이터를 전달한다. 동일 요소 타입의 복수 필드는 field tag 필요. command(cmd, attachments, options), handle.update(cmd, attachments), respond(token, executionIDL, attachments), publish(report, attachments, options), onCommand/onResponse/onReport의 typed assembled callback 모두 같은 조립 계약을 사용한다. 취소/복구도 해당 세션의 attachment 일관성을 보존한다.

LargeSet: set(element), remove(id), assign(elements), elements(). LargeList: 위 기능 + insert(element, position), move(id, position). ListPosition은 front/back/before/after와 대상 Element ID를 표현한다. set은 안정적인 ID의 upsert이며 List의 기존 위치를 유지한다(새 요소 위치 기본값 명시). assign은 ID를 유지하는 전체 교체, 빈 목록은 빈 collection이며 인스턴스 자체 삭제와 다르다. 안정적인 element/collection ID, 순서, metadata, timestamp, gap/empty 및 complete/state를 보존한다. 생성·수정·삭제, nested collections, 다중 field, 부분 업데이트와 재동기화 필요. `Specialized<Base>`는 base/set/is/as/type/identity/state로 실제 subtype, unknown/unsupported/partial을 구분한다. 원본 IDL optional, union 및 composite key를 손실 없이 보존한다. CompleteOnly는 조립 일관성을 의미할 뿐 DDS 다중 Topic 원자성을 보장하지 않는다.

## 7. 런타임·오류 계약

Host가 Component를 배치/기동/정지하고 facade가 protocol을 구현한다. Service 내부 DDS 등록/상태 머신/ACK/Dispose/QoS/Session은 미노출. Source/destination/QoS/domain/partition 구성은 재컴파일 없이 조정 가능하도록 한다. 반환 Result는 제출·검증 오류를 보고하고 remote failure와 구분한다. Callback 인자 const 참조 수명은 호출 중; 비동기 보관은 복사/token. Callback은 정해진 executor에서 직렬화하고 mutex를 잡은 채 사용자 코드를 부르지 않는다. Callback 예외·순서·재진입 및 stop 시 drain, DLL/SO unload 전에 모든 실행 종료를 계약으로 검증한다.

## 8. 범위와 검증 게이트

[이전 정적 ICD/IDL 분석](../history/v0.8.1/UMAA_ICD6_IDL_Coverage_Review_v0.8.1.md): 정규 54 Service/114 Operation 전부 정적 Topic 일치; 확장 139 고유 Service/314 고유 Topic 중 311 일치; 18 Large Collection Element 및 48 Specialization Topic. AcousticInterferenceStatus, FinsStatus, PropulsorsStatus 3건은 @nested/Topic 불일치로 공식 소스 해결 전 전체 지원 주장 금지. Cancel 관련 추가 IDL Topic은 ICD Dispose 계약과 비교 후 결정; 단순 이름으로 공개 API 추가 금지.

[요구사항 매트릭스](../history/v0.8.2/ICD_REQUIREMENTS_v0.8.2.csv), [취소 계약](../history/v0.8.3/M_COMMAND_CANCEL_CONTRACT_v0.8.3.md), [소스 인벤토리](../SOURCE_INVENTORY.md). 원본 8 ICD PDF/594 IDL 미포함. v0.8.7은 신규 정적 전수 검사/실제 코드 생성/RTI 컴파일/DDS conformance를 수행하지 않았다. [Acceptance matrix](UMAA_Component_SDK_API_Review_v0.8.7.md) 참조.
