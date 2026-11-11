# v0.8.1 → v0.8.2 사용자 API 변경 내역

## 이번 작업의 엄격한 범위

- **내부 구현/Topic 선언의 정합성 이슈가 아니라** UML/IDL에 정의된 애플리케이션 수준 사용자 행위와 정보만 확인.
- 필수 보완은 ICD 절에서 직접 요구한 기능 또는 이를 손실 없이 드러내기 위한 가장 작은 타입 보완으로 한정.
- 새 최상위 동사를 도입하지 않고 **8개 유지**.

## 확정 보완

1. **Command 실행 상태 및 Reason 계약 정리:** `ISSUED/COMMANDED/EXECUTING/COMPLETED/FAILED/CANCELED`, 적법한 Reason만 사용. **EO §5.1.3~5.1.4**.
2. **Command 업데이트 및 지연 응답 분리:** `info.token()`과 `respond(token, result)`가 지난 버전을 덮어쓰지 않게 함. Deferred는 SDK 응답 방식이며 UMAA Status가 아님. **EO §5.1.4.1~2**.
3. **Cancel 제출과 최종 상태 분리:** `CancelDecision` 및 `CancelAttempt`로 실제 `CANCELED/COMPLETED/UNKNOWN` 구분. **EO §5.1.4.5**.
4. **Consumer/Provider 재기동 후 의사결정 정보:** Provider는 기존 `m.command` 문맥, Consumer는 `m.report<StatusOp>`의 `ReceiveOptions.onRecoveredCommand`와 `RecoveredCommand<Payload>`로 **Command 데이터 확인 후** 복구/취소를 결정. `CommandStatus`만으로 Command Payload를 알 수 있다고 가정하지 않음. **EO §5.1.2.1~2**.
5. **Report Data/Disposed 및 정확한 Key:** `ReportEvent`에 원본 Source, Key, Timestamp, Data/Disposed 구분. **EO §5.2.1, §6.1**.
6. **Large Collection 부분 vs 완료 선택 보강:** `m.report`의 `ReceiveOptions.delivery(CompleteOnly/IncludePartial)`를 **명시적으로 필수**로 승격. `Collection.state`에 부분/완성 구분. **EO §3.8.3**.
7. **LargeSet/List 요소/순서/공집합/삭제:** 원본 Collection/Element ID 보존과 재정렬 지원. **EO §3.8.2~7**.
8. **Specialization 타입/연결/동기화/제거:** `Specialized<Base>.type/state/identity`, 일반화·특수화 삭제는 기존 `dispose`/컬렉션 요소 삭제. **EO §3.9.1~3**.
9. **IDL 필드 무손실:** `@key`, `@optional`, Union branch를 typed Public Data로 보존. **각 ICD의 Service Interfaces, 특히 EO §6.1**.
10. **복수 Operation 정확한 등록/송신:** `Service::Operation` 사용을 명시하고 모호한 Service 단축형 금지. **MM 등 각 Service Interfaces**.

## 필수 목록에서 제외·하향

- `onServiceEvent`/`m.discover`/`latest`/`next`/`onStatus` 등은 **이번 ICD 사용자 기능 필수 요건이 아님**. 필요하면 별도 UX 확장.
- `ReportEvent::Unavailable/Fault`, `wait_for`, `LargeSet::changes`는 기본 Required Contract가 아닌 **선택 편의**. 미지원이어도 원본 상태/자료 표현 경로가 있으면 ICD 사용자 API 미달로 판정하지 않는다.
- `request/m.request`는 6.0 Queryless Report 대체 용도가 아니므로 기본 API에서 제외.
- 3개 Experimental Topic `@nested`는 **IDL/바인딩** 문제로 유지, **새 사용자 함수가 필요한 근거로 사용하지 않음**.

## 판정

**최소 8개 동사 + 기존 타입·옵션 보완으로 ICD에 명시된 공통 사용자 행위의 표현 경로를 갖는다.** 다만 작성한 API 명세가 실제 코드로 생성되거나 139개 고유 Service의 사용자 예제가 컴파일되었다는 뜻은 아니다.
