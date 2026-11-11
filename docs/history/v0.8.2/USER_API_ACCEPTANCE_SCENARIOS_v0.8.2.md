# UMAA ICD 기반 사용자 API 검증 시나리오 v0.8.2

> 이 문서의 '표현 경로'는 v0.8.2에서 **정의한 사용자 API 계약**의 충족 경로이다. **컴파일 실행한 테스트 PASS가 아니다.** 불필요한 DDS Topic 생성/엔진 구현 설계는 포함하지 않는다.

## 판정 조건

- **필수 P0:** ICD가 요구한 사용자 의사결정/데이터가 기존 8개 상위 동사와 공개 타입·옵션으로 모두 표현되어야 한다.
- **SDK 유도 P1:** ICD가 함수명을 요구하지 않지만 잘못된 상태 보고·정보 손실을 피하기 위한 타입·오류 계약.
- **편의 P2:** ICD 필수 기능으로 취급하지 않고 이번 기본 API에 추가하지 않는다.

| ID | ICD 근거 | 사용자 시나리오(Provider/Consumer) | 최소 공개 API 경로 | 이번 계약 설계 판정 | 실제 코드 검증 |
|---|---|---|---|---|---|
| S01 | EO §5.1.4.1 | Provider가 수락·거절·비동기 검증 후 COMMANDED/EXECUTING/완료를 보고 | `m.command`, `CommandDecision`, `respond(CommandToken, CommandResult)` | 경로 있음 | 미실시 |
| S02 | EO §5.1.3/5.1.4 | Consumer가 ACK·CommandStatus·ExecutionStatus를 구별해 관찰 | `send`, `CommandCall`, `m.report<StatusOp>`, `m.report<ExecutionOp>` | 경로 있음 | 미실시 |
| S03 | EO §5.1.4.2 | 같은 Session의 명령 갱신, 이전 비동기 작업 종료 | `CommandCall.update`, `info.isUpdate`, `info.token`, `respond(token,...)` | 경로 있음, wire 상태 귀속 제한 명시 | 미실시 |
| S04 | EO §5.1.4.5 | 취소 요청이 접수되었으나 실제 중단 불가; COMLETED 수신 | `cancel`, `m.cancel`, `CancelDecision.cannotCancel`, `CommandResponse.status` | 경로 있음 | 미실시 |
| S05 | EO §5.1.4.5 | 장비 정지 지연 후 CANCELED, consumer lost 이벤트 | `m.cancel`, `deferred`, `respond(token,canceled)`; 원인 Unknown 가능 | 경로 있음 | 미실시 |
| S06 | EO §5.1.2.1 | Provider 재기동 후 이전 명령을 복구/실패로 결정 | `m.command`, `CommandInfo.isRecovered/previousStatus` | 경로 있음 | 미실시 |
| S07 | EO §5.1.2.2 | Consumer 재기동 후 자기 Command Payload를 확인하고 기존 Session을 계속 관찰/취소 | `m.report<StatusOp>(handler, ReceiveOptions.onRecoveredCommand)`, `RecoveredCommand<Payload>`, `RecoveryDecision`, `cancel(id)` | **v0.8.2에서 복구 Callback 보강** | 미실시 |
| S08 | EO §5.2.1 | 소스가 여러 인스턴스의 Report를 발행·폐기, Consumer가 Key로 삭제 | `publish`, `dispose`, `m.report<ReportEvent<T>>` | 경로 있음 | 미실시 |
| S09 | EO §6.1 + IDL | @optional 미설정과 값 기본값, @key 복합 키, union 분기 보존 | `Optional<T>`, typed Key/Union, `ReportEvent<T>` | 경로 있음 | 미실시 |
| S10 | EO §3.8.2/5 | 생성된 빈 LargeSet은 유효한 크기 0 Snapshot | `LargeSet<T>.clear/id/size/state`, `publish<Op>` | 경로 있음 | 미실시 |
| S11 | EO §3.8.3 | Consumer가 완성 Snapshot만 처리하거나 부분 Element를 즉시 처리할지 선택 | `m.report<Op>(handler, ReceiveOptions.delivery)`, `collection.state/elements` | **v0.8.2에서 옵션 보강** | 미실시 |
| S12 | EO §3.8.4/7 | 목록 중간 요소 제거, 순서 변경, Element ID 유지 | `LargeList<T>.remove/move/elements`, `publish<Op>` | 경로 있음 | 미실시 |
| S13 | EO §3.9.1/2 | 조건 기반 타입이 아직 도착하지 않거나 지원하지 않음 | `Specialized<T>.type/identity/state/as` | 경로 있음 | 미실시 |
| S14 | EO §3.9.3 | 일반화/특수화 관계 제거, 컬렉션 내 Specialization 삭제 | `dispose<Op>(key)` 또는 `LargeSet<T>.remove(id)` | 경로 있음 | 미실시 |
| S15 | MM §6.1 | 동일 Service의 add/delete 등 여러 Operation을 혼동하지 않고 호출 | `send<Service::AddOp>` / `send<Service::DeleteOp>` | 경로 있음 | 미실시 |
| S16 | EO §5.2.1/2 | QueryData 없이 Status·Specs 데이터를 수신 | `m.report<ReportOp>` | 경로 있음; request 불필요 | 미실시 |
| S17 | MO GlobalWaypointControl §6.1 + IDL | Command Payload 안의 LargeList 경유점 구성 | `LargeList<Waypoint>` + `send<GlobalWaypointOp>` | 경로 있음 | 미실시 |
| S18 | MM ConditionalReport §6.1 + IDL | Report Payload의 LargeSet 내부 Generalization/Specialization 읽기 | `m.report<ConditionalReportOp>` + `LargeSet<Specialized<Conditional>>` | 경로 있음 | 미실시 |

## 3가지 핵심 경계 사례

### A. 연속 실행 Command의 응답 완료

GlobalVector와 같이 종료되지 않을 수 있는 Command는 `send()` 후 무조건 Terminal `get()`만 기다리면 영원히 대기할 수 있다. **`SendOptions.responseStage(Commanded)`**로 검증/시작 승인 시점만 기다리고 상태 Report를 계속 관찰하게 해야 한다. 원격 완료 시점과 로컬 기다림을 구분한다.

### B. 부분 Collection을 즉시 사용할지 결정

ICD EO §3.8.3은 *부분 데이터를 즉시 처리*하거나 *전체 원자적 업데이트 완료까지 대기*하는 선택을 소비자에게 둔다. `m.report<Op>(handler, ReceiveOptions{CompleteOnly/IncludePartial})`는 **기존 이벤트 등록 동사의 옵션**이므로 새 `m.collection`을 만들지 않아도 사용자 결정을 표현한다.

### C. 복구와 상태 미확정

재기동 뒤 Consumer는 **기존 CommandType 자체에 해당하는 데이터와 Source/Session**을 확인하고 계속 관찰할지 취소할지 결정해야 한다. `m.report<CommandStatusOp>`의 `ReceiveOptions.onRecoveredCommand`를 이용해 `RecoveredCommand<Payload>`를 전달받고 `RecoveryDecision::Continue/Cancel`을 반환한다. `CommandStatus`만으로 원본 Command Payload를 복원했다고 주장하지 않는다. 다만 CommandStatus에 특정 갱신 버전을 구분하는 전용 필드는 없어서 오래된 버전의 Status인지 판별 불가능한 경우 `association=SessionOnly/Unknown`을 노출한다.

## API 완전성 요약

- **상위 API에 필요한 신규 동사 수:** 0
- **기존 등록 함수의 옵션 보완:** `m.report<Op>`의 Collection 전달 정책(부분/완료) 및 Consumer 기존 Command 복구 판단 Callback
- **기존 함수 반환/문맥 보완:** `CommandDecision`, `CommandInfo`, `CommandToken`, `CommandResponse`, `CancelDecision`, `ReportEvent<T>`
- **데이터 타입 보완:** C++11 `Optional<T>`, 원본 Key/Union, `LargeSet/List`, `Specialized<Base>`
- **문서상 커버리지:** 위 18개 핵심 사용자 시나리오 각각에 표현 경로 존재
- **검증 한계:** 139개 고유 Service의 모든 Provider/Consumer 코드를 실제 생성·컴파일한 증거는 없음. 이 문서는 '실행 PASS'가 아닌 'API 계약 표현 경로 존재'를 보여 준다.
