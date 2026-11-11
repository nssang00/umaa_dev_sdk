# v0.8.4 사용자 관점 API 재검토

## 요약

- **채택:** `Service<Derived, ServiceKind>` 경량 타입; `Component`의 `provider<Service>()`/`consumer<Service>()` 역할 바인딩; 실제 업무 Handler는 역할 아래 Operation 단위로 등록.
- **채택하지 않음:** 4개 사용자 Handler 베이스(`ControlService`, `ConfigService`, `SpecsService`, `StatusReportService`)를 강제 상속하거나, ACK/Query/CancelStatus마다 업무 Callback을 만드는 설계.
- **함수 집계:** 기존 **업무 동사 8개** 유지. 새로 **역할 선언 함수 2개**가 사용자에게 보이므로 전체 표면은 증가함. `.command/.cancel/.report`는 역할 바인딩의 메서드가 됨.
- **ICD 준수 판단:** 설계 계약 기준의 정적 검토다. DDS 상호운용성, 코드 생성, 실제 모든 서비스 구현 통과가 아니다.

## 1. 필수 시나리오 — 역할·서비스 조합

| # | 사용자 시나리오 | v0.8.4 경로 | 판정 |
|---:|---|---|---|
| 1 | Control Provider가 Command를 받는다 | `provider<Control>().command<CommandOp>` | 표현 가능 |
| 2 | Control Provider가 취소를 업무적으로 처리한다 | `provider<Control>().cancel<동일 CommandOp>` | 표현 가능 |
| 3 | Control Consumer가 명령을 보낸다 | `consumer<Control>()`, `send<CommandOp>` | 표현 가능 |
| 4 | Control Consumer가 ACK/Status/Execution을 구독한다 | `consumer<Control>().report<Ack/Status/ExecutionOp>` | 표현 가능 |
| 5 | Control Provider가 실제 ExecutionStatus를 낸다 | `publish<ExecOp>(CommandToken, value)` | 표현 가능 (신규 오버로드) |
| 6 | Config Provider가 여러 Command를 처리한다 | `provider<Config>().command<각 CommandOp>` | 표현 가능 |
| 7 | Config Service의 일반 Report가 함께 존재한다 | `provider<Config>()`+`publish<ReportOp>` / `consumer<Config>().report<ReportOp>` | 표현 가능 |
| 8 | Specs Provider가 명령 Handler 없이 제원을 제공한다 | `provider<Specs>()`+`publish<ReportOp>` | 표현 가능 |
| 9 | StatusReport Provider가 데이터/Key를 관리한다 | `provider<StatusReport>()`+`publish`/`dispose` | 표현 가능 |
| 10 | Specs/Status Consumer가 Query payload 없이 Report를 받는다 | `consumer<S>().report<ReportOp>` | 표현 가능 |
| 11 | 하나의 Component가 같은 Service의 Provider·Consumer 모두이다 | `provider<S>()` 및 `consumer<S>()` | 표현 가능 |
| 12 | ServiceKind가 정형 Operation 구성을 보장하지 않는다 | 실제 `OperationRole/FlowKind`로 등록 검증 | 표현 가능 |
| 13 | 다른 Service의 CommandOp를 잘못 등록한다 | 소속 타입 검사 | C++11 선언 스케치에서 오류 검출 |
| 14 | ReportOp를 `.command`로 등록한다 | 역할 타입 검사 | C++11 선언 스케치에서 오류 검출 |
| 15 | Recovered Command가 EXECUTING에서 이어간다 | `info.kind()==Recovered`, `CommandDecision::resume()` | 설계상 보완; 실제 상태 전이 검증 미실시 |
| 16 | Consumer가 이전 Command payload로 계속/취소 판단 | `ConsumerOptions.onRecovered<CommandOp>(handler)` | 설계상 보완; 실제 복구 검증 미실시 |
| 17 | 동일 Command의 갱신 토큰 혼선을 막는다 | `CommandInfo.token()`, `respond(token,...)` | 기존 계약 유지 |
| 18 | LargeSet/List/Spec 객체가 Command/Report 안에 있다 | 기존 타입 모델 유지 | 기존 계약의 설계상 경로 유지 |
| 19 | Report Provider 재시작 후 과거 keyed 데이터 정리 | `publish<ReportOp>(snapshot, ReplaceOwnedInstances)` | **설계상 보완 — SDK 이전 소유 인스턴스 판별/실행 미검증** |
| 20 | 모든 실제 ICD Service에 필요한 핸들러를 누락 없이 제공한다 | 생성 Service/Operation 메타데이터 기반 검사 | **미검증** |

## 2. 변경 전 설계에서 발견한 문제와 이번 수정

1. **역할 누락:** `m.command<Op>` 단독 등록만으로 `Component`의 Service별 Provider/Consumer 의도가 명확하지 않았다. `m.provider<S>` / `m.consumer<S>`로 역할 선언을 도입했다.
2. **Service 분류 과적용 위험:** ServiceKind가 Config라고 모든 표준 Operation이 존재하는 것으로 가정하면 안 된다. 실제 ICD Operation을 타입으로 생성해 Flow를 Operation마다 연결한다.
3. **Report-only Provider:** 기존 등록 동사 3개만으로는 보고만 하는 Service의 Provider 역할을 표현하기 어색했다. Handler 없는 `m.provider<Specs/StatusReport>()`도 허용한다.
4. **Report 재시작 키 정리:** 앱이 현재 데이터 집합은 알지만 이전 DDS Report의 모든 Key를 모를 수 있다. `publish<ReportOp>(snapshot, ReplaceOwnedInstances)` 오버로드로 현재 유효 집합을 지정하도록 보완했다.
5. **동시 명령의 ExecutionStatus 연관:** `publish<ExecOp>(value)`만으로는 Command ID를 앱이 직접 채워야 하거나 여러 활성 명령을 혼동할 수 있다. `publish<ExecOp>(CommandToken, value)` 오버로드를 추가했다.
6. **복구 시 잘못된 상태 되감기:** 이전 `EXECUTING`인 복구 Command에 단순 `accepted()`를 적용하면 표준 상태 전이와 어긋날 수 있다. `CommandDecision::resume()`을 별도 결과로 추가하고 이전 상태를 이어받도록 계약을 강화했다.
7. **Consumer 복구 UX:** 이전 v0.8.3에서는 Status Report 수신 옵션에 기존 Command의 복구를 결합했다. v0.8.4에서는 `ConsumerOptions`의 역할 설정으로 분리해, 원래 Command Payload를 전달하는 콜백을 명시했다.

## 3. 의도적으로 숨기는 요소

- 각 Service의 Config/Control/Specs/StatusReport 구분을 사용자가 `switch`할 의무 없음.
- ACK/CommandStatus 자동 생성, `query...`의 subscribe 매핑, `cancel...`의 DDS dispose 프로토콜, Participant/Reader/Writer 및 복잡한 Topic 이름은 SDK 내부.
- `Service` 상속 타입은 **타입 정의**이며 사용자가 직접 Provider/Consumer 클래스 하위 타입을 구현해야 하는 뜻이 아님.
- LargeCollection, Specialization은 사용자 데이터의 한 형태. 별도 `m.collection`, `m.specialized` 필수 등록 없음.

## 4. 미해결/오류 가능성

| 등급 | 남은 문제 | 사용자에게 미치는 영향 | 권고 |
|---|---|---|---|
| P0 | Report-only Provider 재시작 시 SDK가 **이전 owned 인스턴스의 Key를 신뢰성 있게 확보하는지** | 오래된 Report가 유효한 현재 상태로 보일 수 있음 | v0.8.4의 `ReplaceOwnedInstances` 실행 경로, 실패 반환, 타 Provider 데이터 불침범을 구현 검증 |
| P0 | Consumer/Provider Command 복구의 이전 상태·Payload 획득 실패, 불명확한 버전 연결 | 중복 실행 또는 부정확한 복구 판단 | 정보 부재 상태를 명시적으로 표시하고 안전한 기본 동작 및 실패 처리 확정 |
| P1 | Cancel Handler 미등록 및 비중단 명령에 대한 정책 | 실장비 안전 정지를 SDK가 판단할 수 없음 | Service별 취소 지원/미지원, Liveliness 취소 필수 여부의 선언·검증 규칙 확정 |
| P1 | Report/Operation 생성 타입의 전체 139개 고유 Service 매핑 검증 | 일부 예외 Service에서 역할·Operation 오배치 가능 | 표준 54 + 확장 85개 고유 서비스 전수 Operation 바인딩 검사; 3개 EXP IDL Topic 불일치는 별도 품질 이슈 |
| P1 | C++11 실제 SDK 헤더 및 코드 생성기와의 연동 | 명세 스케치와 실제 사용 코드가 다를 수 있음 | 실제 생성 타입 사용 컴파일 테스트 |

**중요:** `m.provider<S>()`만으로 Service를 운영할 수 있다고 주장하면 안 된다. Provider의 서비스 제공 선언은 정적 계약이며, 실제 Report 데이터 공급·갱신과 명령의 업무 상태·안전 정지는 앱 책임이다.

## 5. 출처: 명시된 ICD 사실 vs SDK 설계 선택

| 사실/설계 | 분류 | 근거 |
|---|---|---|
| Config/Control/Specs/Status 또는 Report 분류와 Operation 예외 | ICD 직접 | EO ICD §3.4, Table 3 |
| `GlobalVectorControl` 명령/ACK/Status/ExecutionStatus와 취소 | ICD 직접 | MO ICD §6.1.5, Table 22–26 |
| Query 없는 Report는 구독/발행으로 Request/Reply 구현 | ICD 직접 | EO ICD §5.2.1 |
| Provider/Consumer Command 재시작 처리 | ICD 직접 | EO ICD §5.1.2.1–2 |
| 명령 갱신/취소와 Status 결과 | ICD 직접 | EO ICD §5.1.4.2–5 |
| `Service<Derived,Kind>` CRTP 타입 | SDK 설계 선택 | ICD가 C++ API 정의하지 않음 |
| `.provider/.consumer` 역할 선언과 Builder형 메서드 | SDK 설계 선택 | 사용자 역할의 명시적 모델화 |
| `CommandDecision::resume()`와 `publish<ExecOp>(token,value)` | SDK 설계 선택 | 표준 상태 전이·연결 요구에서 도출 |

## 6. 검증 수행 범위

- 대표 `GlobalVectorControl`의 **4개 데이터 Topic 타입**이 제공 IDL 압축 파일에 존재함을 확인.
- `GlobalVectorControl`, `GlobalPoseStatus`, 양 역할 등록, 복구 설정, ExecutionStatus 오버로드의 **공개 API C++11 문법 스케치를 `-fsyntax-only`로 검사**.
- 이것은 **실제 배포 헤더의 컴파일, DDS 동작 또는 ICD 완전성 테스트가 아니다.**