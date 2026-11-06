# v13 검증 결과

검증일: 2026-11-04
환경: Ubuntu 24.04.3 LTS x86_64, GCC 13.3.0, C++17
빌드: Make, -O2 -Wall -Wextra -Wpedantic -fPIC

## 변경 내용

- 직접 작성한 C++ 네임스페이스를 message_manager 하나로 통일.
- 멤버변수 _abc 표기, public 함수 / protected 함수 / private 함수 / private 변수 순서.
- 라이브러리와 예제, 회귀시험의 람다 제거.
- 라이브러리 구현의 auto, optional, std::move 사용 제거.
- 로그 콜백을 일반 함수 포인터로 변경. 호출부 변경 사항은 유지보수 안내서에 명시.
- JSON 중복 키 검사는 파일별 상태를 갖는 작은 검사 객체로 유지.
- 매핑 읽기를 loadMappingTables, readFieldMapping, loadRules로 나눔.
- 한국어 주석과 간단한 시작 예제, 유지보수 안내서 추가.
- 모든 정수 입력에 숫자/십진수 문자열/16진수 문자열 지원.
- enum 숫자 키 정규화와 동일 숫자의 중복 별칭 검사.
- 정수 문자열은 정수 필드와 버퍼에서 실제 숫자로 보관. 문자열 필드는 그대로 유지.
- interface fields의 to:3 단축 상수와 to:대상경로,value:3 명시형 상수 지원.

## 최종 회귀시험

`make test` 전체 통과. 빌드 경고 없음.

| 시험 | 결과 |
|---|---|
| legacy_tests | All tests passed |
| tests | 220 checks |
| alignment_tests | 137 checks |
| msgcode_tests | 58 checks |
| interface_v6_tests | 56 checks |
| interface_v7_tests | 47 checks |
| field_mapping_tests | 49 checks |
| audit_integration_tests | 2,759 checks; 190 UDP cases / 380 datagrams |
| bitfield_tests | 14,052 checks |
| integer_format_tests | 80 checks |
| Python 독립 struct oracle | 190 cases / 380 frames 바이트 단위 일치 |
| simple_demo | x=-10 및 미설정 메시지 덤프 확인 |
| demo | 생성/변환/수신 성공, msgcode=1001, buffer length=5 |

새 시험은 정수 타입별 경계, uint64 최대/int64 최소, 잘못된 형식과 범위 초과,
정수 문자열과 기존 숫자 정의의 바이트 일치, enum/매핑 별칭, 비트필드/배열/버퍼,
고정값과 다른 대상 경로, 실패 시 대상 보존을 확인합니다.

기존 시험에서 변경한 기대값은 새 요구사항 두 가지에 해당합니다.
- alignment="4"는 이제 정상 입력. 잘못된 정렬 검사에는 "0x3"을 사용.
- mapping의 from="100"은 이제 정수 100과 일치. bool/float 구분은 유지.

최종 빌드 및 실행 로그는 같은 폴더의 v13_*.log 파일에 있습니다.
소스 파일 해시는 source_sha256_v13.json에 기록했습니다.
CMake는 이 환경에 설치되어 있지 않아 실행하지 않았습니다. Make 빌드를 검증했습니다.

## 적용 시 유의할 변경

기존 wire 네임스페이스나 public record의 이전 멤버 이름을 사용하는 프로그램은
유지보수 안내서대로 수정하고, 새 헤더 및 새 .a로 다시 빌드해야 합니다.
캡처 람다 로그 콜백은 일반 함수로 변경합니다.
16진수 값은 JSON 표준에 맞게 문자열("0x32")로 작성합니다.
숫자처럼 생긴 문자열 매핑 입력은 숫자와 동일하게 비교됩니다.

기본 예제 JSON과 기존 전송 규칙은 유지했으며, 새 기능 예제는 json/integer_formats에 추가했습니다.
