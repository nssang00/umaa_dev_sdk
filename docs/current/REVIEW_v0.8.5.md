# v0.8.5 Public API 사용자·ICD 설계 재검토

**판정:** 4개 실행 클래스 구조는 지금까지 다룬 일반 Control/Config/Specs/Status/Report 시나리오를 **설계상** 표현한다. 실제 ICD 전체·런타임 적합성은 검증되지 않았다.

## 1. 시나리오 검사표

| # | 사용자 시나리오 | v0.8.5 경로 | 판정 |
|---:|---|---|---|
| 1 | AutoPilot이 Command 수신·실행 | CommandProvider.onCommand + respond | 표현 가능 |
| 2 | 진행 중 Command 취소 의사와 실제 중단 구분 | onCancel -> deferred / respond(Canceled) | 표현 가능 |
| 3 | Command 발송·최종 관찰 | CommandConsumer.command -> CommandCall / onStatus | 표현 가능 |
| 4 | Command ACK만 추가 관찰 | onAck | 표현 가능, 선택적 |
| 5 | Command 실제 실행 상세 정보 | CommandProvider.publish<ExecOp> / Consumer.onExecutionStatus<ExecOp> | 표현 가능 |
| 6 | 기존 활성 Session 명령 갱신 | CommandCall<Op>.update(payload) | 표현 가능; 런타임 timestamp 미검증 |
| 7 | Consumer 기존 Command 복구 의사결정 | onRecoveredCommand<Op> | 표현 가능; 불명 상태 기본 정책 미검증 |
| 8 | Provider 기존 명령 실행 이어가기 | onCommand + info.delivery()==Recovered + resume | 표현 가능; 런타임 미검증 |
| 9 | Specs/StatusReport 보고 전용 Provider | ReportProvider.publish/dispose | 표현 가능 |
| 10 | Consumer 지속 Report 수신 | ReportConsumer.onReport | 정적 자동 구독 계약 |
| 11 | Consumer 필요할 때만 Report 수신 | ReportConsumer.request(handler) | 동적 구독 계약 |
| 12 | 동일 S/Op에 자동/동적 중복 등록 | AlreadySubscribed | 의도적 등록 오류 |
| 13 | Report Data/Disposed 구분 | ReportEvent.kind | 표현 가능 |
| 14 | Report Provider 재시작 stale key 정리 | publish(snapshot, ReplaceOwnedInstances) | **설계상; 소유 Key 조회 미검증** |
| 15 | Config에 Command/Report 양쪽 Operation 존재 | 같은 Service에 두 역할 인터페이스 | 표현 가능 |
| 16 | ServiceKind가 역할 범위를 결정하지 않는 예외 | Generated Operation/Capability Traits | 설계상 가능; 139 Service 컴파일 미검증 |
| 17 | LargeSet/List 명령·보고 Payload | 타입 데이터 API, 통신 동사 증가 없음 | 기존 모델 유지 |
| 18 | Specialization/Generalization 데이터 | Specialized<T> 타입 | 기존 모델 유지 |
| 19 | 다른 Service 소속 Operation 잘못 등록 | 소속/Flow/Role static check | 선언 스케치에 검증 규칙 존재 |
| 20 | 실행/소유권/복구 안전성 완전 확인 | DDS 및 실제 생성 코드 통합 | **아직 미검증** |

## 2. 문서 출처에서 채택한 내용과 추가 설계 선택

| 구분 | 근거·내용 | 적용 |
|---|---|---|
| 사용자 제공 `UMAA_Public_API_Summary_KO.md` | Command 갱신, Status/Ack/ExecutionStatus Hook 분리, Report Data/Disposed, Snapshot, Large Collection/Specialization의 Payload 표현 | 채택 |
| 사용자 제공 RTI Starter Kit Hook 요약 | on_executing, on_updated, on_status, on_ack, on_exec_status, on_report 등 업무 이벤트 구분 | 업무 수준 Hook에 통합/선택화 |
| ICD EO §5.1 | Command Provider/Consumer 명령·갱신·취소·복구 Flow | 프로토콜 내부화, 앱 업무 판단 유지 |
| ICD EO §5.2.1 | Queryless Request/Reply 구독 기반 Report | onRequest 필수 제외; onReport 구독 |
| ICD EO §3.8/§3.9 | Collection/Generalization/Specialization의 ID/상태/연결 | 타입 API로 유지 |
| **SDK UX 선택** | `command()`, `request(handler)`, onReport 자동 구독, 네 역할 클래스, AlreadySubscribed 정책 | ICD가 강제한 C++ API **아님** |

## 3. 설계상 위험 / 미완료 우선순위

**P0:**
- 이전 Provider 소유 인스턴스 Key·DDS ownership을 복구하지 못할 때 `ReplaceOwnedInstances`의 안전한 실패 정의와 실측 검증.
- 이전 Command 원본 Payload/상태/타임스탬프 확인 불가 시 복구/취소 기본 정책: 중복 실행과 거짓 완료 금지.

**P1:**
- `onCancel` 미등록, consumer liveliness loss, 실제 정지 불가 장비의 안전 정책.
- 서비스별 Operation 139개 고유 Service 정적 적합성/명세 생성기와 연동.
- 자동 onReport / 동적 request 중복 오류의 타입, Subscription RAII/close, 재진입·Callback 순서/예외 처리.
- CommandCall.wait() 타임아웃과 원격 FAILED 상태 분리, Result/Status/Ack 종결성 확인.
- C++11 배포 헤더 컴파일 + 잘못된 역할 등록 음성 테스트 + 실제 DDS 통합.
- 중첩 LargeList<Specialized<...>>, ID 유지, Partial/Unsupported 상태 분리.

## 4. 변경 전과의 중요 차이

v0.8.4의 `m.consumer<S>().report<Op>()`는 일반 Report와 CommandStatus를 모두 받았으나, v0.8.5는 **CommandStatus/Ack/ExecutionStatus는 CommandConsumer Hook에**, 일반 Queryless Report는 **ReportConsumer.onReport에** 각각 전용으로 위치시킨다. Payload를 단순 Topic 단위로 유출하지 않고 업무 의도에 맞는 그룹으로 정리한다.

**실제 검증 주의:** 선언 스케치 정적 문법 검사 결과는 `VALIDATION_SUMMARY_v0.8.5.json`이 기록한 상태만 인정한다.

## 5. 2026-11-12 개선안 A 사용자 API 검토

**등록 인터페이스를 두 개로 축소**: `Registrar::provide<S>()`와 `Registrar::consume<S>()`. ProviderBinding은 `onCommand<CommandOp>()`, `onCancel<CommandOp>()`; ConsumerBinding은 `onStatus<CommandOp>()`, `onAck<CommandOp>()`, `onExecutionStatus<ExecOp>()`, `onRecoveredCommand<CommandOp>()`, `onReport<ReportOp>()`를 제공한다. 모든 Operation은 정확한 Service 소속·FlowKind·Role을 확인한다. ReportProvider는 독립 수신 Hook이 없으므로 `provide<S>()`만으로 등록한다.

**정적 등록과 런타임 역할의 분리:** `provide/consume`이 네 Runtime 클래스를 대체하는 것은 아니다. 실제 `command/cancel/respond/publish/dispose/request`는 `service<S>()`의 네 런타임 역할 인터페이스에 남는다. Config 혼합 Service에서는 동일 `S`의 Command/Report Capability가 함께 존재할 수 있다. 신규 AutoPilot 예제는 정확한 ICD 이름을 유지하면서 이 분리를 보여 준다.

**미검증:** Callback 서명 자동 검사, 전체 생성 Service capability, 중복 역할 바인딩, C++11 실제 생성 헤더 컴파일, GitHub Actions CI 성공 및 DDS 런타임. 선언 스케치만으로 이 항목을 완료했다고 주장하지 않는다.
