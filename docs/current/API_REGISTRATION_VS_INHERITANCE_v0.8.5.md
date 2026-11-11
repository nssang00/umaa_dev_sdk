# v0.8.5 — 실제 UMAA 6.0 명칭과 Callback 등록 vs 상속/override

**상태:** 사용자 Public API 설계 판단 · 개선안 A (2026-11-12). ICD Operation/IDL 데이터 명칭은 첨부된 UMAA 6.0 IDL ZIP의 Service Markdown 및 IDL 파일에서 확인. C++ SDK 래퍼의 네임스페이스와 타입 태그는 **제안하는 생성 규칙**이지 ICD가 정의한 C++ 타입이 아님.

## 1. 정확한 원본 명칭 (IDL/Service Definition)

| 종류 | ICD 원문 Operation/Service | IDL Payload 타입 |
|---|---|---|
| MO Control Service | `GlobalVectorControl` | 해당 없음(Service) |
| Command request | `setGlobalVector` | `UMAA::MO::GlobalVectorControl::GlobalVectorCommandType` |
| Command ACK | `reportGlobalVectorCommandAck` | `UMAA::MO::GlobalVectorControl::GlobalVectorCommandAckReportType` |
| Command Status | `reportGlobalVectorCommandStatus` | `UMAA::MO::GlobalVectorControl::GlobalVectorCommandStatusType` |
| Execution Status | `reportGlobalVectorExecutionStatus` | `UMAA::MO::GlobalVectorControl::GlobalVectorExecutionStatusReportType` |
| SA Status Service | `GlobalPoseStatus` | 해당 없음(Service) |
| Report | `reportGlobalPose` | `UMAA::SA::GlobalPoseStatus::GlobalPoseReportType` |

**확인된 IDL 세부 내용:** `GlobalVectorCommandType`의 required 필드는 `direction`(`DirectionRequirementVariantType`), `directionMode`, `speed`(`SpeedRequirementVariantType`)다. `depthChangePitch`, `elevation`, `endTime`는 optional. `GlobalPoseReportType`의 위치는 `position`(`GeoPosition2D`)이며, `latitude/longitude/sensorId`라는 최상위 필드는 없다. `GlobalPoseReportType`의 Key는 `source`(`UMAA::Common::IdentifierType`)이다.

원본 파일(첨부된 `umaa-6.0-idl-with-exp-services.zip` 내부):
- `UMAA/MO/GlobalVectorControl/GlobalVectorControl.md`
- `UMAA/MO/GlobalVectorControl/GlobalVectorCommandType.idl`
- `UMAA/MO/GlobalVectorControl/GlobalVectorCommandAckReportType.idl`
- `UMAA/MO/GlobalVectorControl/GlobalVectorCommandStatusType.idl`
- `UMAA/MO/GlobalVectorControl/GlobalVectorExecutionStatusReportType.idl`
- `UMAA/SA/GlobalPoseStatus/GlobalPoseStatus.md`
- `UMAA/SA/GlobalPoseStatus/GlobalPoseReportType.idl`

**C++ 이름 충돌 주의:** IDL의 `module UMAA { module MO { module GlobalVectorControl {...} } }`는 보통 C++ 네임스페이스 `UMAA::MO::GlobalVectorControl`에 대응한다. 동일한 이름을 그 네임스페이스의 *클래스*로 재정의할 수 없다. 설계 예시에서는 SDK가 생성할 **별도 facade**를 `umaa::services::MO::GlobalVectorControl`로 두고, 그 중첩 Operation 태그 이름을 ICD와 동일한 `setGlobalVector`, `reportGlobalVectorExecutionStatus`처럼 둔다.

이전 예제의 `VectorSvc`, `VectorOp`, `PoseSvc`, `PoseOp`, `SetGlobalVector`, `Report` 및 `GlobalPoseReport.latitude`는 정규 ICD 생성 이름이 아니므로 **v0.8.5 기본 AutoPilot 예제에서 제거했다.**

## 2. 방식 A — Service 중심 Callback 등록 (2026-11-12 권장 기본 API)

```cpp
namespace MO = umaa::services::MO;
namespace SA = umaa::services::SA;

class AutoPilot : public umaa::Component {
public:
    umaa::CommandDecision onSetGlobalVector(
        const UMAA::MO::GlobalVectorControl::GlobalVectorCommandType&,
        const umaa::CommandInfo&);
    umaa::CancelDecision onCancelGlobalVector(const umaa::CancelInfo&);
    void onGlobalPoseReport(
        const umaa::ReportEvent<
            UMAA::SA::GlobalPoseStatus::GlobalPoseReportType>&);
};

UMAA_COMPONENT(AutoPilot, m) {
    m.provide<MO::GlobalVectorControl>()
      .onCommand<MO::GlobalVectorControl::setGlobalVector>(
          &AutoPilot::onSetGlobalVector)
      .onCancel<MO::GlobalVectorControl::setGlobalVector>(
          &AutoPilot::onCancelGlobalVector);

    m.consume<SA::GlobalPoseStatus>()
      .onReport<SA::GlobalPoseStatus::reportGlobalPose>(
          &AutoPilot::onGlobalPoseReport);
}
```

- **사용자가 학습할 등록 함수는 `provide<S>()`와 `consume<S>()` 두 가지**다. SDK가 실제 `Op::OperationRole`/`Op::FlowKind`로 네 런타임 역할에 분배한다.
- `Registrar::provide<S>()`는 ProviderBinding, `consume<S>()`는 ConsumerBinding을 반환한다. 등록 객체는 `CommandProvider<S>`/ `ReportConsumer<S>` 런타임 실행 핸들이 **아니다**.
- 하나의 Component에 여러 Service와 Role을 조합할 수 있고, `Config`처럼 Command/Report가 섞인 Service도 `m.provide<S>()` 또는 `m.consume<S>()`로 각 역할을 한 번씩 등록한다.
- `onCommand`는 SDK 등록 메서드, `AutoPilot::onSetGlobalVector`는 사용자 Callback이다. 메서드의 `<setGlobalVector>`가 실제 ICD Operation을 명시한다.
- 실제 업무 전송은 여전히 `service<MO::GlobalVectorControl>().commandProvider().respond(...)` 또는 `service<SA::GlobalPoseStatus>().reportConsumer().request<SA::GlobalPoseStatus::reportGlobalPose>(handler)` 등 네 런타임 핸들에서 수행한다.
- SDK는 Service 소속·Flow·Role·중복·Handler 서명을 검증해야 한다. Hook 없는 `m.provide<SA::GlobalPoseStatus>()`도 일반 Report Provider 선언으로 유효하다.
- `UMAA_COMPONENT` 매크로는 설계 등록 표현이며, 구현에서 `configure(Registrar&)` 멤버로 대체하는 문제는 별도 결정이다.

**대안으로 검토했으나 비채택:** `m.commandProvider<S>()` 등 네 등록 함수와 Service별 `.onSetGlobalVector()` 전용 등록 함수 자동 생성은 기본 API에 넣지 않는다.

## 3. 방식 B — 각 Service Provider/Consumer를 사용자가 상속하여 가상 함수 override (대안, 미채택)

아래 코드는 **비교용 대안 형태**다. 현재 SDK에는 이 `virtual onCommand` 인터페이스가 정의되어 있지 않으며, 컴파일 가능한 기존 API라고 해석하면 안 된다.

```cpp
class MyGlobalVectorProvider
    : public umaa::CommandProvider<umaa::services::MO::GlobalVectorControl>
{
public:
    // 가상 함수 시그니처 자체를 SDK에서 별도로 정의해야만 가능
    // CommandDecision onCommand(const GlobalVectorCommandType&, const CommandInfo&) override;
};

class MyGlobalPoseConsumer
    : public umaa::ReportConsumer<umaa::services::SA::GlobalPoseStatus>
{
public:
    // void onReport(const ReportEvent<GlobalPoseReportType>&) override;
};

// 실제 AutoPilot에서는 두 역할 객체를 합성/소유하거나 다중 상속 필요
```

단점: 한 Component가 여러 Service와 Role을 동시에 사용하면 클래스가 증가하거나 다중 상속이 필요하다. 또 Service에 Command Operation이 여러 개면 `onCommand`의 가상 함수 이름/오버로드 규칙이 복잡해지고, 소비만 하는 역할에도 User Provider 계층 상속을 강요할 수 있다. 장점: 한 Service에 국한된 단순 Provider에서는 `override` 자동완성, 컴파일 단계 필수 구현 검증이 직관적일 수 있다.

## 4. 사용자 관점 비교

| 기준 | 등록 방식 (A) | 상속·override (B) |
|---|---|---|
| 단일 Service/단일 Role | 등록 구문이 한 단계 추가됨 | 단순 가상 Hook override는 매우 직관적 |
| 다중 Service/혼합 역할 AutoPilot | 한 Component에서 선언적으로 결합, 자연스러움 | 클래스/다중 상속·소유 구조가 증가 |
| 실제 Operation 이름과 연결 | `<setGlobalVector>`가 등록에 직접 드러남 | 여러 Operation의 가상 함수를 이름/오버로드로 관리해야 함 |
| 타입 안전성 | Service/Operation/Handler Signature static check 필요 | `override` 검증이 쉽지만 생성된 모든 Hook 설계 필요 |
| 사용자 학습 부담 | Component, 등록, 업무 Callback | 여러 Service별 구현 클래스·상속과 객체 소유 학습 |
| 확장·테스트 | Callback 분리·조합이 비교적 용이 | 작은 전용 어댑터 단위에서는 유리 |

**결론:** 범용 UMAA Component SDK 기본 표면은 **등록 방식(A)**을 유지하는 것이 실용적이다. Service마다 별도의 `ControlServiceProvider`, `GlobalPoseStatusConsumer` 상속을 요구하지 않는다. 단일 Service 예제에서만 별도 `override` 어댑터를 선택적으로 지원할 여지는 있으나 Core API는 늘리지 않는다.

## 5. 구현 권고/후속 확인

1. 자동 생성 Service Facade의 네임스페이스 `umaa::services::MO::GlobalVectorControl` 및 중첩 Operation `setGlobalVector` 명명 규칙을 **문서화하고 생성기 구현시 확정**할 것. 원본 IDL에 facade 클래스가 존재한다고 잘못 주장하지 말 것.
2. Callback 등록 시 `Op::ServiceType == S`, 역할, Flow, Handler 인자/반환 타입을 강제 검증. 잘못 등록하면 컴파일 오류(또는 명시적 등록 오류).
3. `ReportConsumer::onReport` 자동 구독과 `request(handler)` 동적 구독의 동일 `S,Op` 충돌 정책 유지.
4. Command에 속한 ExecutionStatus Report와 일반 Queryless Report를 섞지 않을 것.
5. `GlobalVectorCommandType`는 Optional/Variant/원본 metadata를 유지하는 타입을 생성하고, 사용자 고급 API에서는 Source/Session/Timestamp 설정을 필요할 때만 드러낼 것.
6. **검증 한계:** 이번 수정은 실제 ICD 명칭 대조 및 예제 정리다. 실제 DDS 생성 C++ 헤더와 결합한 컴파일/런타임 시험을 의미하지 않는다.
