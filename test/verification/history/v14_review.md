# v14 구조 변경 및 검증

검증일: 2026-11-04
환경: Ubuntu 24.04.3 LTS x86_64, GCC 13.3.0, C++17
빌드: Make, -O2 -Wall -Wextra -Wpedantic -fPIC
최종 빌드 경고 없음. make test 및 두 예제 정상 실행.
CMake는 환경에 설치되어 있지 않아 실행하지 않았음.

## 실제 구조 변경

- 메시지 관련 주요 클래스 5개(Manager/Catalog/Link/Message/Interface)를 3개로 축소.
- MessageCatalog, MessageLink, FieldAccess 클래스를 삭제. 호환 별칭으로 남기지 않음.
- Manager가 LinkInfo 목록과 실제 Message 목록을 직접 소유.
- 메시지 타입/코드별 이름 등 별도 조회 사전과 Message의 필드 이름→인덱스 사전 삭제.
- 원본 메시지 조회와 값이 빈 독립 메시지 생성을 분명하게 구분.
- list로 실제 메시지를 보관해 새 링크 등록 시 기존 참조 유지.
- 임시 메시지 목록의 전체 검사가 성공해야 링크와 메시지 등록.
- set/get의 전체 경로 방식으로 통일. 배열/구조체 전체 설정도 유지.
- 생성/수신/변환 등의 중간 결과를 지역변수에 저장해 여러 문장으로 설명.
- 구현 함수마다 역할/입력/반환/처리 흐름 주석. 주요 알고리즘은 구체적인 흐름 화살표로 설명.
- 기존 JSON 파일을 v13과 바이트 단위 비교해 변경 없음 확인.

## 최종 검사 결과

| 시험 | 결과 |
|---|---|
| legacy_tests | All tests passed |
| tests | 220 checks |
| alignment_tests | 137 checks |
| msgcode_tests | 58 checks |
| interface_v6_tests | 56 checks |
| interface_v7_tests | 47 checks |
| field_mapping_tests | 49 checks |
| audit_integration_tests | 2,759 checks, 190 UDP cases / 380 datagrams |
| bitfield_tests | 14,052 checks |
| integer_format_tests | 80 checks |
| simple_structure_tests | 10 checks |
| Python struct oracle | 190 cases / 380 frames 바이트 일치 |
| simple_demo | x=-10, 미설정 필드 덤프 정상 |
| demo | 변환/수신 정상, msgcode=1001, buffer length=5 |

새 구조 시험은 참조 안정성, 기존 값 보존, 새 객체의 값 초기화, 코드 유지,
등록 실패 시 부분 반영 없음, 실패한 이름의 재사용, 조회 실패 시 무단 삽입 없음을 검증함.

프록시의 const 쓰기를 런타임에 막는 시험은 제거된 인터페이스에 종속된 시험이므로,
const Message의 직접 경로 읽기 시험으로 변경. const Message는 원래 C++ 타입 규칙상 set 호출 불가.
다른 [] 접근 시험은 동일 필드의 직접 set/get 경로로 옮겼으며 배열/인덱스 오류 시험은 유지.

## 변경 시 고려할 점

예전 .catalog()와 연속 [] 접근은 사용하지 않음. README의 변경표대로 호출부 수정 필요.
새 헤더와 새 .a로 전체 호출 프로그램을 다시 빌드해야 함.
이름 조회는 이해하기 쉬운 순차 비교를 사용하므로 큰 필드/메시지 목록은 예전 사전 조회보다 느릴 수 있음.
비트필드/엔디안 처리의 비트 계산, JSON 숫자 변환 안전장치와 타입별 get 검사는 유지.
단순화를 위해 검증 기능이나 전송 규칙을 제거하지 않음.

최종 빌드/실행 로그와 소스 해시를 이 폴더에 보관함. 이전 검증 기록은 history에 있음.
