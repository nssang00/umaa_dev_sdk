# UMAA Component SDK v0.8.4 — Service Kind / Provider–Consumer Public API

**목표:** UMAA 6.0의 다양한 Service를 최소 사용자 API로 다루되, 역할과 Operation을 명확히 표현하고 ICD 내부 제어 절차는 SDK에 숨긴다.

## 권고 모델

- `class GeneratedService : umaa::Service<GeneratedService, ServiceKind::Control>`
- `UMAA_COMPONENT(MyComponent, m) { m.provider<Service>().command<Op>(handler)...; m.consumer<Service>().report<Op>(handler)...; }`
- Config/Control/Specs/StatusReport마다 별도 사용자 Handler 기반 클래스는 생성하지 않는다.
- 기존 8개 **업무** 동사 유지 + `provider/consumer` 2개 **구성** 함수 추가.

## 파일

- `UMAA_Component_SDK_Public_API_v0.8.4.md` — v0.8.3 주요 계약을 통합한 사용자 API 전체 명세
- `REVIEW_v0.8.4.md` — 시나리오별 재검토와 미해결 항목
- `CHANGELOG_v0.8.4.md` — 변경 사항
- `PUBLIC_API_CXX11_DECLARATION_SKETCH_v0.8.4.hpp` — 설명용 공개 타입/함수 스케치
- `EXAMPLE_COMPONENT_CXX11_SKETCH_v0.8.4.cpp` — C++11 문법 검사 예시
- `VALIDATION_SUMMARY_v0.8.4.json` — 수행한 검사와 한계

**상태:** API 설계, C++11 문법 검사. 실제 UMAA SDK/IDL codegen/DDS 런타임 검증은 수행하지 않았다.