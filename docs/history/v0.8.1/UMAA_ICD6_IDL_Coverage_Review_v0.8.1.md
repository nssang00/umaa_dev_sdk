# UMAA ICD 6.0 + 확장 IDL v0.8.1 — 사용자 API 커버리지 재검증

**검증일:** 2026-11-10 · **범위:** 라이브러리 UMAA ICD 6.0 PDF 8종, 새로 업로드한 IDL ZIP 594개, 기존 v0.8 Public API 설계.

## 핵심 판정

- **정규 54개 서비스, 114개 Operation은 114/114 IDL DDS Topic·Type과 정확 일치**한다. v0.8의 이 정적 판정은 유지된다.
- **전체 8개 ICD의 Service 기술 항목 140개는 고유 Service 139개**다. `ContactFilterConfig`가 CO-EXP와 EXP에 중복 기재되어 있기 때문이다.
- **ICD Operation 기술 항목 317개는 고유 `(Namespace, Topic)` 314개**다. 각 IDL 타입 파일은 317/317 확인했다.
- 그중 **314/317 Operation 행은 DDS Topic 상수·정확한 타입을 확인**했다. 중복 3행을 제거한 **고유 311/314개** Topic이 정확히 일치한다.
- **나머지 3개**는 IDL에 타입은 있지만 `@nested`이며 Topic 상수가 없다. 이 3건에 대해 **완전한 Report Provider/Consumer 지원을 검증했다고 말할 수 없다.**
- **추가 IDL Topic 선언 86개**는 ICD Operation의 직접 Topic이 아니다. Large Collection Element 18, Specialization 48, 취소 관련 선언 18, 그 외 중첩 명령 관련 2개다.
- **최상위 SDK 동사 추가는 필요하지 않다는 설계 판단을 유지한다.** 그러나 3개 소스 불일치의 해소, 생성 API 실제 검증, 상태·라이프사이클 검증 없이는 UMAA ICD 전체 커버리지 완료가 아니다.

## 1. 검증 대상과 계산 방식

| 구분 | 이전 v0.8 | 확장 IDL 재검증 |
|---|---:|---:|
| IDL 파일 수 | 352 | **594** (기존 352개 동일 + 242개 추가) |
| 전체 IDL `#include` 참조 | 995 | **1,705** (미해결 0) |
| 라이브러리 ICD 문서 | 8 | **8** |
| ICD Service 기재 항목 | 140 | **140** |
| 중복 제외 고유 Service | 미구분 | **139** |
| ICD Operation 기재 항목 | 317 | **317** |
| 중복 제외 고유 Operation Topic | 미구분 | **314** |
| 직접 Operation DDS Topic 선언 확인 | 정규 114개 | **314행 / 고유 311개** |
| Topic 선언 없는 IDL `@nested` Operation | Experimental 미검증 | **3개** |
| Large Set/List Element | 10 | **18 (Set 10, List 8)** |
| Specialization Topic | 48 | **48** |
| 별도 취소 관련 IDL Topic 선언 | 범위 밖 | **18 (ICD 프로토콜과 차이)** |
| Operation 아닌 명령형 Topic 선언 | 미검증 | **2** |

Operation 구분은 파일 이름 추측이 아니라 **PDF의 해당 Operation 절에서 `Namespace`, `Topic`, `Data Type`을 각각 추출**한 뒤 같은 IDL Namespace, 타입 파일명, `struct`, `const string *Topic`을 대조했다. `ContactFilterConfig`의 PDF 중복은 두 줄로 두되, 실제 DDS Topic 개수를 계산할 때는 한 번만 센다.

IDL의 139개 Service `.md` 문서와 ICD의 고유 Service 139개 이름도 일치한다. ICD에 기술된 Operation 명칭은 모두 대응 Service `.md` 문서에서 찾았다. 다만 `.md`에는 ICD의 실제 DDS Operation 이외에 Cancel 관련 개념 Operation도 기록되므로 `###` 표제 수를 Topic 수로 사용하지 않았다.

## 2. ICD 문서별 Operation 검증

| ICD 문서 | 구분 | Service 항목 | Operation 행 | Topic/Type 일치 | 불일치 |
|---|---|---:|---:|---:|---:|
| MM | 정규 | 14 | 50 | 50 | 0 |
| MO | 정규 | 7 | 22 | 22 | 0 |
| EO | 정규 | 14 | 21 | 21 | 0 |
| SA | 정규 | 14 | 14 | 14 | 0 |
| SEM | 정규 | 3 | 5 | 5 | 0 |
| SO | 정규 | 2 | 2 | 2 | 0 |
| CO-EXP | 실험 | 11 | 42 | 42 | 0 |
| EXP | 실험 | 75 | 161 | 158 | **3** |
| **합계** | | **140** | **317** | **314** | **3** |

**해석:** 이 값은 *파일 및 타입/Topic 선언 정합성* 검사다. C++11 코드 생성 성공, DDS QoS/Topic 실제 매칭, Flow Control 적합성 또는 실행 검증을 뜻하지 않는다.

## 3. 실제로 확인된 소스 불일치 3건

| ICD Namespace와 Topic | ICD Operation | IDL 파일 | 분류 |
|---|---|---|---|
| `UMAA::SEM::AcousticInterferenceStatus::AcousticSelfNoiseInterferenceStatusType` | `reportAcousticInterference` | `UMAA/SEM/AcousticInterferenceStatus/AcousticSelfNoiseInterferenceStatusType.idl` | `NESTED_NO_TOPIC_DECLARATION` |
| `UMAA::EO::FinsStatus::FinsStatusType` | `reportFins` | `UMAA/EO/FinsStatus/FinsStatusType.idl` | `NESTED_NO_TOPIC_DECLARATION` |
| `UMAA::EO::PropulsorsStatus::PropulsorsStatusType` | `reportPropulsors` | `UMAA/EO/PropulsorsStatus/PropulsorsStatusType.idl` | `NESTED_NO_TOPIC_DECLARATION` |

공통 현상: ICD는 이를 Report Operation의 `Topic`으로 기술하지만, 제공 IDL 파일은 해당 타입을 `@nested struct`로 선언하며 `const string ...Topic` 선언이 없다. 따라서 현재 소스로는 Topic 자동 생성을 입증할 수 없다.

**권고:** SDK 생성기는 이 셋을 다른 정상 `ReportOp`와 구분하여 **명시적인 SourceMismatch 진단**을 내야 한다. 임의 DDS Topic/Type을 만들어 적합성이 확보된 것처럼 처리하면 안 된다. 공식 수정 IDL 또는 Topic Type 정의 확인이 선행되어야 한다.

## 4. Data Modeling 범위와 최소 사용자 API

- **Large Collection:** 메타데이터 필드 18개(10 Set + 8 List), 대응 Element Topic 18개. 정규 10개뿐 아니라 CO-EXP·EXP 서비스의 8개도 `LargeSet<T>/LargeList<T>`로 표현해야 한다. 완성 스냅샷, 변경/삭제, Key/Element ID, list 순서, Gap/Empty를 지원한다. Command 내부/Report 내부 양쪽 모두 적용한다.
- **Specialization:** 48개 특수화 Topic. `Specialized<Base>`에 `base/is/as/set/state/type/identity`를 제공하며 미지원·미수신·미완성을 구분한다.
- **IDL 데이터 표현:** 확장 아카이브에는 `@optional` 453개, `@key` 739개, `union` 선언 19개가 있다. 이는 **정적 문자열 카운트**이며 각 필드 의미와 값 보존의 구현 검증이 필요하다.
- **Command / Response:** 기존 `m.command`, `send`, `respond`, `cancel`, `m.cancel` 유지. 갱신 세대 `CommandToken`, Reason/Terminal, 취소 요청과 취소 성공 구분, Liveliness Loss 및 복구 문맥은 계속 P0로 관리한다.
- **Report:** `m.report`, `publish`, `dispose`, `ReportEvent<T>` 유지. 원본 Source/Key/시간/Disposed와 완전성 상태 보존.

### 추가 Topic 86개의 분류

| 추가 Topic 종류 | 수 | 권고 |
|---|---:|---|
| Large Collection Element | 18 | 별도 상위 동사 없이 포함 객체와 연계 |
| Specialization | 48 | `Specialized<T>` 타입 계층으로 표현 |
| Cancel 관련 선언 | 18 | ICD의 Dispose/NOOP 서술과 차이. 자동 노출 금지, 소스 계약 검토 |
| 그 외 (`FinCommandType`, `PropulsorCommandType`) | 2 | 상위 Fins/Propulsors Command에 포함된 데이터 타입. 독립 Operation 여부 재검토 |
| **합계** | **86** | |

**Cancel 주의:** IDL에 `...CancelConfigTypeTopic`과 `...CancelConfigCommandStatusTypeTopic` 등이 존재하지만, ICD Flow Control은 별도의 CancelCommandStatus 메시지 발행을 구현하지 않고 Command Dispose로 취소를 처리한다고 설명한다. 따라서 **IDL 상수의 존재 ≠ 실제 규격상 별도 Cancel API 필수**다. SDK `cancel(call)`은 원래 Command의 Terminal 결과를 관찰하도록 두고, 이 18개 상수의 역할은 별도 문서 확인 대상으로 유지한다.

## 5. v0.8 → v0.8.1 수정 결론

| 항목 | 이전 판정 | 수정 판정 |
|---|---|---|
| Experimental 타입 검증 | 해당 IDL 없음 | 새 ZIP에서 **200/203 Operation 행 Topic·Type 일치** (중복 3행 포함) |
| Experimental 전체 커버 | 불명 | **3개 Report Topic 불일치가 명확히 남음**, 전체 커버 아직 불합격 |
| SDK 새 최상위 동사 | 필요 없음 | **유지**. 상태/타입/메타데이터 계약 보완 |
| Large Collection | 정규 10개 | **확장 전체 18개**로 범위 확대 |
| 서비스/Operation 개수 | 140 / 317 | **기재 140 / 317, 고유 139 / 314** |
| 정상 Topic 매핑 | 정규 114/114 | **정규 114/114, 전체 고유 311/314** |

## 6. 검증을 '완료'로 바꾸기 위한 남은 게이트

1. **소스 정합성 3건:** `@nested`로만 제공된 세 Report의 공식 토픽/타입 계약 확인 및 수정.
2. **타입/생성기 게이트:** 모든 139개 고유 Service의 Operation API를 C++11에서 컴파일 가능한 서명으로 생성하고 실제로 요구 필드를 보존하는지 점검.
3. **Data Modeling 게이트:** 18개 컬렉션의 원자적 Snapshot/Element ID/Gap/순서/삭제, 48개 Specialization Topic의 ID·Timestamp 동기화, 모든 Optional/Union/Key 분기 테스트.
4. **Flow Control 게이트:** 정규 24개 + Experimental 41개 Command의 ACK/Status/Reason, 갱신, 취소/Dispose, 연결 상실, 종료 상태 및 복구 동작을 사용자 시나리오로 검증.
5. **Provider/Consumer 게이트:** Service마다 모든 규범 인터페이스를 누락 없이 제공하고, 사용자 `m.command/m.report/publish/respond`만으로 필수 판단과 데이터를 전달하는 예제를 컴파일·실행 검증.
6. **실통신 게이트:** 해당 환경의 DDS/RTI Starter Kit와 일치하는 실제 Endpoint/QoS·상호운용성 시험. 본 정적 검증으로 갈음할 수 없다.

### 최종 판정

**정규 UMAA ICD의 정적 Operation↔IDL 정합성은 통과한다. 확장 Experimental 전체는 3개 Topic/IDL 불일치와 미구현·미검증 게이트 때문에 '완전 커버 가능성 입증'에는 도달하지 못했다.** 다만 발견된 3건을 제외한 311개 고유 DDS Topic은 제안한 적은 수의 사용자 동사와 데이터 타입으로 매핑할 **구조적 경로**가 있다.

## 참고 자료

- 원본: 라이브러리 UMAA ICD 6.0 8종 (MM/MO/EO/SA/SEM/SO/CO-EXP/EXP) 중 각 `Services and Interfaces` Operation 절, EO §3.8·§3.9·§5.
- 업로드: `umaa-6.0-idl-with-exp-services.zip` (SHA-256 `15e71e24df4663e8b4891c461f0e37823bce5087f0b5d5a52667f3c25cb56d08`)
- 이전 Handoff: `UMAA_Component_SDK_Handoff_v0.7_2026-11-10(1).zip` 내부 기존 IDL 352개 및 v0.8 사용자 API 초안.
- 부속 CSV: `ICD_Operation_vs_IDL_v0.8.1.csv`, `ICD_Service_Summary_v0.8.1.csv`, `IDL_Auxiliary_Topics_v0.8.1.csv`, `IDL_Large_Collections_v0.8.1.csv`.
- 기계 판정: `audit_summary_v0.8.1.json` 및 `audit_idl_exp.py`.
