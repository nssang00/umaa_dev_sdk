# `m.command` · `m.cancel` 공개 계약 v0.8.3

## 최종 결정

- **함수명 변경 없음:** `m.command<Op>(handler)`, `m.cancel<Op>(handler)` 유지.
- `Config`/`Control`은 Service의 용도 분류이고 `Command/Response`는 Operation의 Flow Control이다. 한 Config Service에 여러 Command와 Report가 함께 있을 수 있다.
- Provider의 **Command 입력 이벤트는 두 등록 함수로 충분**하다: New/Updated/Recovered는 `m.command`, 취소 요청은 `m.cancel`.
- `Specs`, `Status/Report`, Config에 속하는 일반 Report는 `publish` / `m.report`가 담당한다. Cancel을 또 다른 Command 또는 CancelStatus Report 등록으로 모델링하지 않는다.

## 공개 계약 (정식 이름은 제안, 의미는 필수)

| 공개 요소 | 사용자 의미 | ICD 근거 |
|---|---|---|
| `m.command<Op>` | Config/Control의 **해당 Command Operation** Handler 등록 | EO §3.4, §5.1.4 |
| `CommandInfo.kind()` | `New`, `Updated`, `Recovered` 구분 | EO §5.1.2, §5.1.4.2; enum 형태는 SDK 설계 |
| `CommandInfo.token()` | 이번 갱신 버전에서 지연 `respond()` 대상으로 사용 | EO §5.1.4.2에서 도출; Token은 SDK 설계 |
| `CommandDecision` | `accepted`/`rejected(reason)`/`deferred` 판단 | EO §5.1.4.1; Deferred는 SDK 설계 |
| `m.cancel<Op>` | **같은 Command Op**에 대한 취소 요청 수신 | EO §5.1.4.5 |
| `CancelInfo` | 대상 명령 ID, 활성 token, 원인(확실하지 않으면 Unknown), 직전 상태 | EO §5.1.4.5에서 도출; 객체 형태는 SDK 설계 |
| `CancelDecision` | `canceled`(실제 정지 확인)/`deferred`/`cannotCancel` | EO §5.1.4.5에서 도출; enum은 SDK 설계 |
| `respond(token, result)` | 실제 `commanded`/`executing`/terminal 통지 | EO §5.1.4.1–5 |
| `ServiceTraits<S>` / `OperationTraits<Op>` | 서비스 분류와 Operation 역할·연결관계 정의 | EO §3.4 / 각 ICD Operation 정의; Traits는 SDK 설계 |

## 사용자 Handler 시그니처 스케치 (C++11)

```cpp
// 구체적인 namespace/생성 클래스명은 향후 codegen 계약에서 확정
CommandDecision onSetA(const ConfigCommandA& cmd, const CommandInfo& info);
CommandDecision onSetB(const ConfigCommandB& cmd, const CommandInfo& info);
CancelDecision onCancelA(const CancelInfo& info);
CancelDecision onCancelB(const CancelInfo& info);

UMAA_COMPONENT(MyComponent, m) {
    m.command<SomeConfig::SetA>(&MyComponent::onSetA);
    m.cancel<SomeConfig::SetA>(&MyComponent::onCancelA);
    m.command<SomeConfig::SetB>(&MyComponent::onSetB);
    m.cancel<SomeConfig::SetB>(&MyComponent::onCancelB);
}
```

## 세 가지 호출 흐름

**새 Command:** `m.command<Op>` → `info.kind()==New` → `accepted` (준비 완료) 또는 `rejected(reason)` 또는 `deferred` → 실제 실행·종결은 `respond(token,...)`.

**갱신:** `m.command<같은 Op>` → `info.kind()==Updated`, Payload는 전체 새 Command → 이전 Token으로 `respond()` 시 오류 → 새로운 Token으로 현재 작업 결과 보고.

**취소:** `m.cancel<같은 Op>` → `CancelInfo` → 실제 취소되었으면 `canceled`, 진행 중이면 `deferred` 후 `respond(token,canceled())`, 불가능하면 `cannotCancel` 후 본래 Command의 Terminal을 계속 보고. Consumer의 `cancel()` 제출은 취소 성공을 뜻하지 않는다.

## 강제할 등록·호출 규칙

1. `<Op>`에는 **Command 역할**만 허용한다. `m.cancel<Op>`의 `<Op>`는 `m.command`의 **같은 Command Op**이다.
2. 한 Service가 두 개 이상의 Command를 갖는다면 `m.command<Service>` 축약을 금지하고 `Service::Op`를 요구한다.
3. Command 역할이 아닌 Op, 모호한 Service 축약, 중복 충돌 등록은 컴파일/등록 오류로 드러낸다.
4. 실행 중 장치 중단을 사용자가 수행해야 하는 경우 `m.cancel<Op>`를 생략하지 못하게 한다. 누락을 암묵적인 `CANCELED`로 바꾸지 않는다.
5. `m.cancel`은 애플리케이션의 비즈니스 취소 처리이며 ACK/CommandStatus/CancelStatus Topic을 직접 조작하라는 의미가 아니다.
6. `Config`라고 일반 Config Report를 자동으로 `m.command`에 배달하지 않는다. 사용자 보고는 `publish`, `m.report`.
7. `m.command`의 하나의 `Op`에 New/Updated/Recovered 모두 전달하되, 복구 상태를 새 Command처럼 무조건 재실행하지 않는다.
8. `onCancel`의 원인을 항상 명시적 Consumer 요청으로 취급하지 않는다. `Unknown`을 허용한다.

## 비필수·보류

- 별도 `m.config`, `m.control`, `m.recoverCommand`, `m.updateCommand`, `m.cancelCommandStatus`는 추가하지 않는다.
- 범용 `onServiceEvent`, 서비스 Discovery, 임의 Query Payload RPC는 이번 Command/Cancel 계약에 포함하지 않는다.
- `ServiceTraits/OperationTraits`의 정확한 C++ 선언 형태와 완전 Service Provider 선언 방식은 codegen/Component 공개 타입 계약에서 확정해야 한다. **완전성을 검증할 수 있어야 한다는 요건**은 확정한다.

## 검증 한계

이 문서는 API 설계이며 실제 SDK 헤더 구현·컴파일 PASS나 139개 모든 Service의 Provider/Consumer 코드 생성 검증이 아니다.
