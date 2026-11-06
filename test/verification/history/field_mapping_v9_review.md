# v9 JSON 필드 값 매핑 검증 — 2026-10-30

기반: 기존 binary_message_cpp.zip v8. 메시지 구조체 정의, 수동 buffer length,
header.msgcode 판별 및 기존 API는 유지했습니다.

## 변경

- MessageInterface에 선택적 값 대응표 JSON 생성자/fromFile 오버로드 추가.
- 규칙 이름 + from/to 연결별 값 치환. 미등록 값은 기본 그대로 대입.
- 미등록 값의 copy/default/error 처리와 JSON 설정 검증 추가.
- 변환 결과는 기존 set/serialize 타입 및 길이 검사를 거침.
- 실패 시 대상 객체에 반영하지 않는 기존 변환 방식 유지.
- json/field_mapping.json, demo 사용 예, 문서, 전용 회귀 시험 추가.

## 환경과 실행

Ubuntu 24.04.3 LTS x86_64 / GCC 13.3.0 / C++17.
현재 환경에 CMake가 없어 CTest 대신 아래의 동일 C++ 시험 실행 파일들을 직접 빌드/실행했습니다.
전체 .cpp 파일을 다음 옵션으로 빌드했고 경고 없이 통과했습니다.

```sh
g++ -std=c++17 -O1 -Wall -Wextra -Wpedantic -Werror -I third_party <source.cpp> -o <executable>
```

| 실행 | 결과 |
| --- | --- |
| legacy_tests | All tests passed |
| tests testdata/json | 220 checks |
| alignment_tests testdata/json | 137 checks |
| msgcode_tests testdata/v5/json | 58 checks |
| interface_v6_tests testdata/v6/json | 56 checks |
| interface_v7_tests json | 47 checks |
| field_mapping_tests json | 32 checks |
| audit_integration_tests json | 2759 checks |
| audit_wire_oracle.py | 190 cases / 380 frames byte-for-byte |
| main json | 정상 종료 (exit 0)、샘플 덤프 갱신 |

새 시험은 기본 대입, 정수/문자열/실수 대응표, 역방향 명시 설정, 미등록 값의 세 정책,
중복/잘못된 JSON 설정, 정수 부호 비교, 숫자·문자열·불리언 구분, unsigned 범위 오류,
실패 시 대상 보존, 원본 보존, manager/convertInto/convertBytes/파일 오버로드,
바이너리 직렬화·수신 및 버퍼 수동 길이 보존을 확인했습니다.
기존 UDP 검증도 380개 데이터그램과 1024개 변형 입력을 통과했습니다.

출력 원문: field_mapping_v9.log. v8 로그 및 source_sha256.json은 이전 버전 기록입니다.
v9 소스 체크섬은 source_sha256_v9.json에 기록합니다.

추가로 새 매핑 시험을 `-fsanitize=address,undefined -fno-omit-frame-pointer -g`로
빌드해 32개 검사를 통과했습니다. 실행 환경 제약을 고려해
`ASAN_OPTIONS=detect_leaks=0`으로 실행했으며 누수 검사는 포함하지 않습니다.
