# UMAA Component SDK v0.8.5 변경 내역

**날짜:** 2026-11-11 · **상태:** 설계안 (코드 생성 / DDS 통합 이전)

## 공개 API 변경

- **4개 역할별 실행 클래스 정식 채택:** `CommandProvider<S>`, `CommandConsumer<S>`, `ReportProvider<S>`, `ReportConsumer<S>`. ServiceType별 8개/10개 서브클래스는 만들지 않는다. `S`는 Service 타입이며 Op는 메서드 템플릿 인자.
- **Component 직접 전송 함수 제거:** `Component::send/respond/publish/dispose`는 Service 역할 객체의 `command/cancel/respond/publish/dispose/request`로 이동.
- **Hook 재정리:** Provider `onCommand/onCancel`; CommandConsumer `onStatus/onAck/onExecutionStatus/onRecoveredCommand`; ReportConsumer `onReport`. `onResponse/onReply` 통칭은 기본 API에서 제외.
- **새 Command API:** `CommandConsumer::command<Op>(payload)`로 `send<Op>` 교체. `CommandCall<Op>::update/get/wait`로 갱신·로컬 관찰 기능 유지. Consumer `cancel(call)`은 제출일 뿐 실제 중단 확정이 아님.
- **Provider 상태/데이터 구분:** `respond(token, progress)`는 업무 CommandStatus; `publish<ExecutionOp>(token, data)`는 ExecutionStatus 업무 Payload.
- **Report Provider:** `publish<ReportOp>(data)`, `dispose<ReportOp>(key)`, `publish<ReportOp>(snapshot, ReplaceOwnedInstances)`. 별도 `publishSnapshot` 동사는 도입하지 않는다.
- **Report Consumer 구독:** `onReport<Op>(handler)` 구성 등록은 Component Start 시 자동 구독; `request<Op>(handler)`는 런타임 동적 구독 + Subscription 반환. **같은 Component의 S/Op에 두 방법을 동시에 쓰면 기본적으로 AlreadySubscribed 오류**.
- **선택적인 복구 Hook:** `onRecoveredCommand<CommandOp>`는 기존 ConsumerOptions.onRecovered를 대체하는 사용자 판단 Callback.
- **데이터 API 유지:** `LargeSet/List`, `Specialized<Base>`, `Optional<T>`, union 등은 Payload 객체에 존재; 통신 클래스 증가 없음.

## 동작 계약의 핵심

- `accepted()`는 실행 완료가 아니며, SDK의 단일 초기 상태 전이로 처리한다.
- `CancelDecision::deferred()`를 실제 CANCELED로 오해하지 않는다.
- Queryless Request/Reply에는 Provider `onRequest`/단발 `reply()`가 필요하지 않다.
- Restart Report ReplaceOwnedInstances는 **과거 소유 Key를 확인할 수 없으면 안전하게 실패**해야 한다.
- 정확한 세부 상태/복구 정책과 모든 서비스별 Operation 매핑은 실제 구현 검증 과제로 남긴다.

## 파일

- [최신 명세](UMAA_Component_SDK_Public_API_v0.8.5.md)
- [재검토](REVIEW_v0.8.5.md)
- [AutoPilot 4 역할 예시](../../examples/api_sketch/AUTOPILOT_FOUR_ROLES_v0.8.5.cpp)
- [이전 v0.8.4](../history/v0.8.4/UMAA_Component_SDK_Public_API_v0.8.4.md)

## 검증 한계

실제 생성 IDL 헤더 전체, DDS 런타임, 원격 장비, C++ ABI 호환성은 **검증되지 않음**. 예제는 모델을 표현하는 임시 선언과 Mock 타입을 사용한다.
