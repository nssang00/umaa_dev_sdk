# v0.8.3 → v0.8.4 변경 사항

## 추가 및 확정

1. **`Service<Derived, ServiceKind>` 공통 기반**: Config, Control, Specs, StatusReport, Other; 실제 생성 Service는 경량 타입/Operation을 노출. 별도 4종 Handler 기반 클래스는 도입하지 않음.
2. **Provider/Consumer 역할 선언**: `m.provider<S>()`, `m.consumer<S>()`; 동일 Service 양 역할 허용, Report-only Provider 허용.
3. **역할 범위 내 Operation Handler**: Provider `.command<Op>`, `.cancel<같은 CommandOp>`; Consumer `.report<ReportOp>`. 잘못된 역할/소속/다중 Command의 모호한 등록은 오류.
4. **동일한 8개 업무 동사**: 역할 선언 두 개가 새 공개 구성 함수로 추가되므로 전체 함수 종류의 수는 늘어남.
5. **`CommandDecision::resume()`**: 재기동 후 기존 COMMANDED/EXECUTING 등 상태의 연속성을 보존.
6. **`ConsumerOptions.onRecovered<Op>(handler)`**: Consumer 복구 판단을 Status Report 등록 옵션에서 Service Consumer 역할 설정으로 이동.
7. **`publish<ReportOp>(snapshot, ReplaceOwnedInstances)` 오버로드**: Report Provider가 현재 유효한 전체 keyed Report 인스턴스를 제출하고, SDK는 과거 소유 인스턴스를 정리하도록 요청.
8. **`publish<ExecutionOp>(CommandToken, value)` 오버로드**: 여러 활성 Command 중 업무 실행 데이터가 속할 Session을 명확히 지정.
9. **C++11 헤더/사용 예시 문법 스케치**: 실제 SDK가 아니라 공개 선언의 표현성 확인용.

## 변경하지 않음

- Command/Cancel/Report/Send/Respond/Publish/Dispose 등의 기존 의미, LargeSet/List와 Specialized 데이터 모델, `request` 비필수 원칙, SDK 내부 ACK/Queryless/CancelStatus 매핑.

## 남은 검증

- Report Provider 재기동 시 `ReplaceOwnedInstances`의 실제 소유 Key 수집·폐기·오류 처리.
- Command 복구 시 불확실한 이전 상태 처리.
- 취소 불가능한 Command의 명시적 정책과 Consumer liveliness 상실 예외.
- 모든 실제 ICD 서비스의 생성 Operation 목록, 클래스 컴파일, 런타임 적합성 검증.
