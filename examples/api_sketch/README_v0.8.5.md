# v0.8.5 C++11 AutoPilot — 4개 역할 클래스 예제

## 파일
- [Public API 선언 스케치](PUBLIC_API_CXX11_DECLARATION_SKETCH_v0.8.5.hpp)
- [AutoPilot / MissionManager / PositionSensor / PoseProbe 통합 스케치](AUTOPILOT_FOUR_ROLES_v0.8.5.cpp)
- [기준 설계 명세](../../docs/current/UMAA_Component_SDK_Public_API_v0.8.5.md)

## 실제 ICD 명칭 vs SDK 생성 Facade

이 예제는 `GlobalVectorControl`, `setGlobalVector`, `GlobalVectorCommandType`, `GlobalPoseStatus`, `reportGlobalPose`, `GlobalPoseReportType`의 실제 UMAA 6.0 명칭을 반영한다. `VectorSvc`/`VectorOp`/`PoseSvc`/`PoseOp`는 사용하지 않는다. 원본 IDL의 `UMAA::MO::GlobalVectorControl`은 **module/namespace**이고 `umaa::services::MO::GlobalVectorControl`은 **제안한 SDK 생성 클래스**다. [등록 방식과 상속 방식 비교](../../docs/current/API_REGISTRATION_VS_INHERITANCE_v0.8.5.md).

## 시나리오

```text
MissionManager       AutoPilot                     PositionSensor
[CommandConsumer] -> [CommandProvider]
    command()         onVectorCommand()
    onStatus()   <-    respond()
    cancel()     ->    onVectorCancel()

                     [ReportConsumer] <---------- [ReportProvider]
                      onPoseReport()                 publish()/dispose()
```

- **AutoPilot**: `CommandProvider<GlobalVectorControl>`로 GlobalVector Command를 받고, `ReportConsumer<GlobalPoseStatus>`로 최신 Pose를 유지한다.
- **MissionManager**: `CommandConsumer<GlobalVectorControl>`로 `command`, `call.update`, `cancel`을 호출하며 onStatus/onAck/onExecutionStatus/onRecoveredCommand를 처리한다.
- **PositionSensor**: `ReportProvider<GlobalPoseStatus>`로 `publish`/ `dispose` 및 현재 Snapshot을 발행한다.
- **PoseProbe**: ReportConsumer의 대안인 런타임 `request<PoseOp>(lambda)`와 `Subscription.close()`를 보여준다. AutoPilot의 정적 `onReport`와 같은 Component에 중복 등록하지 않는다.

## 정확성 제한

이 파일은 타입·사용 시점을 설명하기 위한 **모의 Service/Operation/IDL Payload와 선언 헤더**를 포함한다. 실제 IDL 타입 이름을 사용하지만 그 구조체는 비어 있거나 일부 필드만 있는 Mock이다. 정식 IDL 생성 타입을 대체하지 않는다. DDS 동작, 실제 CommandStatus 상태 머신, 장치 정지, 리스타트 복구 기능은 구현하지 않았다.

```sh
c++ -std=c++11 -Wall -Wextra -pedantic -fsyntax-only AUTOPILOT_FOUR_ROLES_v0.8.5.cpp
```

이는 **문법 검사 명령**일 뿐, 제공 SDK Runtime 구현이나 네트워크 데모를 실행하는 명령이 아니다.
