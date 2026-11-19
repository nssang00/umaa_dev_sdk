# v0.8.6 API 변경 이력 — 2026-11-20

**기준:** [UMAA Component SDK Public API v0.8.6](UMAA_Component_SDK_Public_API_v0.8.6.md) · [사용 예제](../../examples/api_sketch/SERVICE_ROLE_USAGE_v0.8.6.md).

## 사용자 결정 반영

1. **Service별 생성 클래스 기본 UX:** `GlobalVectorControlConsumer`, `GlobalPoseStatusProvider` 등 공통 네 역할 기반 클래스를 상속한 구체 클래스 자동 생성.
2. **Action 이름 통일:** `consumer.command(cmd)`, `provider.publish(report)`, `provider.dispose(reportWithKey)`; Operation/Topic 자동 매핑(모호한 경우 명시적 `<Op>`).
3. **원본 IDL Data Type 직접 전달:** `GlobalPoseReportType`, `GlobalVectorCommandType` 등을 사용자가 채우고 Hook에서 직접 읽음. 별도 `Op::Data`는 기본 API에서 제거.
4. **Report Hook:** `onReport(const T&)`가 기본. 필요한 경우에만 `onReport(const T&, ReportInfo&)`; Dispose는 별도 `onDispose(key)`. 기본 Hook에 `ReportEvent<T>::data()` 강제 안 함.
5. **상속형 Hook:** Service별 생성된 virtual overload를 사용자 클래스가 `override`. 기본 역할 클래스에 virtual function template을 두지 않음(C++11 불가능).
6. **Component 등록:** `m.add(&Component::member)`를 새 제안 등록 구문으로 사용. 기존 `m.provide/consume` 기본 경로에서 제외.
7. **복합 데이터:** 실제 IDL Report 타입은 수정하지 않고 `publish(report, LargeList/Set)` 오버로드로 Element 및 Specialization을 함께 전송하도록 제안.
8. **중복 Operation/Key 및 혼합 Config:** message→Operation 유일성 검사, 타입화된 fallback/tag, 복수 기반 클래스의 함수 집합 병합 규칙을 명시.

## v0.8.5 → v0.8.6 예제

| v0.8.5 | v0.8.6 |
|---|---|
| `m.provide<S>().onCommand<Op>(&T::handler)` | `m.add(&T::providerMember)` + `provider.onCommand(const IDL&, info) override` |
| `service<S>().commandConsumer().command<Op>(cmd)` | `consumer.command(cmd)` |
| `service<S>().reportProvider().publish<Op>(report)` | `provider.publish(report)` |
| `service<S>().commandProvider().respond(token, status)` | `provider.respond(token, status)` (상속 함수) |
| `onReport(ReportEvent<IDL>)` | `onReport(const IDL&)` (+ 선택적 info / dispose) |
| 생성 SDK 데이터 `Op::Data` 제안 | 원본 UMAA IDL 데이터 타입 그대로 사용 |

## 호환성·검증

- **공개 API의 파괴적 설계 변경**이며, 이전 등록형 코드와 바이너리/소스 호환을 약속하지 않는다.
- v0.8.5 문서 및 C++11 선언 스케치는 변경하지 않고 이전 버전으로 유지한다. 그 예제를 v0.8.6의 실제 구현 사례로 해석하지 말 것.
- v0.8.6 생성기, 실제 DDS 구현, C++11 + RTI 생성 IDL 컴파일, Command 상태/취소/복구, Large Collection/Specialization end-to-end 검증은 **미실시**.
