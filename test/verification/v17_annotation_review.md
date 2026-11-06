# v17: v15 코드의 이해용 주석 검증 — 2026-11-04

기준: 원본 binary_message_cpp v15 ZIP의 직접 작성 C++ 소스/헤더 25개 파일.
원본 ZIP과 기준 파일들이 바이트 단위로 동일한지 확인했습니다.

변경: 주요 함수의 흐름, 구간별 데이터 이동, std::set/map/vector/list와 iterator,
참조/const, 템플릿/타입 특성/if constexpr, std::ref와 함수 객체 등의 설명을 소스에 추가했습니다.
단순 대입/출력/반환을 반복 설명하는 주석은 정리한 상태를 유지했습니다.

검증:
- g++ -E -P -fpreprocessed로 주석을 제거하고 비교한 C++ 코드 토큰이 25개 파일 모두 v15와 동일합니다.
- JSON 정의, 정적 라이브러리, 외부 nlohmann/json 헤더는 바이트가 동일합니다.
- 따라서 실행 동작이나 공개 API 변경은 없습니다. 실행 검사는 재수행하지 않았습니다.
- 기존 실행 결과는 v15_review.md, 이번 파일별 검증과 해시는 v17_code_identity.json에 있습니다.

파일명은 binary_message_cpp.zip을 유지하며 주석 개정본의 배포 번호는 v17입니다.
