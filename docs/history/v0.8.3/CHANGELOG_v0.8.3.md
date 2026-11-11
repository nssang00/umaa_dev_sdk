# v0.8.2 → v0.8.3 변경 요약

## 유지
- 최상위 8개 동사 이름과 역할 유지. `m.command`/`m.cancel`을 `m.config`/`m.control`로 분리하지 않음.
- `CommandToken`, `CommandCall`, `respond`, `ReportEvent`, `LargeSet/List`, `Specialized<T>`와 Queryless Report의 기존 v0.8.2 경계 유지.

## 필수 보완
1. 서비스의 4종 분류는 **명명 규칙**이며 Service마다 Operation 집합이 다름을 명문화. Config에는 Command/Response와 일반 Report가 함께 존재할 수 있음 (EO §3.4).
2. Service→Operation→FlowControl 구조를 **생성된 읽기 전용 Traits 계약**으로 명문화. 신규 사용자 동사는 없음.
3. `m.command<Op>`의 허용 타입, 명령 Payload/`CommandInfo` Handler 서명, New/Updated/Recovered 호출 의미를 명문화 (EO §5.1.2, §5.1.4).
4. `m.cancel<Op>`의 `Op`는 동일 **원래 Command Op**이며 별도 Cancel Op가 아님을 명문화 (EO §5.1.4.5).
5. 취소 Handler 문맥을 기존 예시의 `CommandInfo`에서 **`CancelInfo`**로 분리. Source/Session, Token, Trigger, 마지막 상태에 접근 가능; Unknown 허용.
6. `CancelDecision::canceled()`는 실제 장비 중단 확인을 뜻하고 `deferred()`/`cannotCancel()`과 구별; 이후 Terminal은 원 Command 흐름으로 확인 (EO §5.1.4.5).
7. 실제 중단 로직이 필요한 활성 명령에 대응 `m.cancel<Op>` 미등록을 암묵적 성공으로 처리하지 않고, Provider 완전성 검증에서 오류로 표시.
8. `m.command<Service>` 및 `m.report<Service>` 축약은 해당 역할의 Op가 정확히 하나일 때만 허용; 복수면 `Service::Op`를 지정.
9. Config 다중 Command + Report 예시 추가 (CO-EXP §5.1.1).

## 별도 구분
- **ICD 직접 근거:** Command 갱신·복구·취소, 프로토콜 단계, 다중 Operation, Report와 Config의 공존.
- **SDK 공개 API 설계:** `CommandInfo.kind()`, `CancelInfo`, `ServiceTraits/OperationTraits`, Handler 누락 오류 정책 등 C++ 형태.
- **미실시:** 실제 코드 생성/컴파일, DDS/RTI 연동, Service 139개 전수 사용자 코드 검증.
