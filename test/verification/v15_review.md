> 이 문서는 v15 당시 검토 기록입니다. 최신 정렬 구조는 DIRECT_ALIGNMENT_KO.md를 참고하세요.

# v15 전체 처리 단계 분리 및 검증

환경: Ubuntu 24.04.3 LTS x86_64, GCC 13.3.0, C++17
검증일: 2026-11-04

## 적용 범위

직접 작성한 라이브러리 구현 6개, 헤더 6개, 예제 2개, C++ 시험 11개: 총 25개 파일.
외부 nlohmann JSON 라이브러리의 코드와 라이선스는 원본 유지.

- JSON 항목 읽기, C++ 타입 변환, 계산, 비교, 결과 반영을 별도 문장으로 작성.
- structureAlignment를 요청한 단계별 변수/if/주석 예시 그대로 작성.
- 최대/최소, 삼항 선택, return 안의 함수 호출/변환을 명시적 지역변수와 if로 분리.
- &&/||를 풀 때 뒤쪽 조건을 필요한 경우에만 평가하도록 단락 평가 유지.
- 스트림 형식 지정/값 출력과 수신 버퍼 준비/송수신을 분리.
- 각 처리 문장 바로 위에 한국어 설명 추가. 함수 계약과 흐름 설명도 유지.
- 라이브러리뿐 아니라 C++ 시험 코드의 auto, 사용하지 않는 std::function helper도 제거.
- Message의 별도 _values 사전 삭제. Field가 정의와 현재 _value를 함께 보관.
- set/deserialize는 임시 Message에서 처리하고 성공할 때 _fields를 교환해 원본에 반영.
- v14의 공개 함수 사용법과 JSON 파일 형식은 유지. 모든 예제 JSON은 바이트 비교로 변경 없음 확인.

## 스타일 검사

호출 괄호와 리터럴을 구분하는 정적 검사로 25개 C++ 파일 점검.
실행 처리의 중첩 호출 0건, return 내 함수 호출 0건.
생성자 선언의 기본 인수 EnumDefinitions() 두 곳은 처리 코드가 아니므로 그대로 유지.
상세 결과는 style_review_v15.json.

최종 주석·들여쓰기 추가 전후의 C++ 토큰 해시가 25개 파일 모두 일치함.
compiled_tokens_v15.json은 빌드/시험을 완료한 구현의 공백·주석 제외 토큰 해시.
source_sha256_v15.json은 최종 배포 소스의 원본 바이트 해시.

## 실행 검사

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
| simple_structure_tests | 10 checks |
| Python struct oracle | 190 cases / 380 frames 바이트 일치 |
| simple_demo | x=-10 및 미설정 필드 덤프 정상 |
| demo | 생성·변환·수신 정상, msgcode=1001, buffer length=5 |

Mutation 결과: 677 accepted / 347 rejected. 이전 버전과 동일.
최종 빌드/시험에서 경고 없이 모든 확인을 완료.
시험 기대값/검사 수는 유지했으며, 수신과 비교를 중간 변수로 분리.
임시 객체의 항목을 참조할 때는 객체를 지역변수로 먼저 보관해 참조 수명을 유지.
CMake는 설정을 제공하지만 환경에 설치되어 있지 않아 실행하지 않음. Make로 검증함.

## 사용 시 참고

주요 클래스는 Manager/Message/Interface 3개이며, enum/표 출력 등의 보조 클래스는 유지.
Message의 핵심 자료는 _fields(정의+현재 값), _containers(부모/자식), _layout(바이트 배치) 3개.
타입 검사/정렬을 반복하지 않기 위한 보조 캐시 두 개는 유지.
조회는 순차 비교, 값 설정은 원본 보존을 위해 임시 Message 복사 방식.
대규모 반복 호출 성능은 실제 사용 데이터로 별도 측정할 수 있음.
새 헤더와 새 .a를 함께 사용하고 호출 프로그램을 다시 컴파일해야 함.
