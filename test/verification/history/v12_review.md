# v12 비트 필드 검증 결과

환경: Ubuntu 24.04.3 LTS x86_64, GCC 13.3.0, C++17.

- 최종 v11 소스에 bits 속성을 추가하고 정적 라이브러리를 다시 빌드했습니다.
- make -j4 all 및 make -j4 test 전체 통과. 기존 테스트 결과는 v12_make_test.log에 있습니다.
- 추가 검사 14,052개 통과 (최종 추가 검사는 v12_bitfield_tests.log).
- 8/16/32/64비트 저장 타입, 모든 두 필드 폭 분할, big/little endian을 독립 정수 바이트 계산과 비교했습니다.
- 전체 64비트 폭, 3개 필드 공유, 폭 초과 시 다음 저장 단위, alignment, 타입/endian 변경, 중첩 구조체 배열을 확인했습니다.
- 요청한 uint16_t 5+11 예제는 GCC의 실제 네이티브 구조체 바이트와 비교했습니다.
- 두 필드의 enum 표시, 공유 이름 mapping, mapping 생략 대입, mapping 결과 범위 초과와 대상 값 보존을 검사했습니다.
- 비트 필드 msgcode 자동 판별과 비트 필드 length_from 연결도 검사했습니다.
- AddressSanitizer + UndefinedBehaviorSanitizer 검사 통과 (ASAN_OPTIONS=detect_leaks=0).
- LeakSanitizer는 실행 환경의 ptrace 제약으로 실행하지 못했습니다. 해당 로그를 별도 보관했습니다.
- 기존 UDP 190 cases / 380 datagrams 및 Python struct oracle 380 frames 바이트 일치.
- 생산 코드 7개 오브젝트만 포함한 lib/libbinary_message.a를 동봉했습니다.
- CMake는 이 환경에 설치되어 있지 않아 실행하지 못했습니다. 테스트 타깃 등록 및 프로젝트 버전을 수정했습니다.

bits는 unsigned 정수 스칼라 전용이며 비트 순서는 LSB부터입니다. endian은 바이트 순서에만 적용합니다.
네이티브 C++ 비트 필드의 일반적인 ABI 호환성을 보장하지 않으며, 배치 규칙은 README에 기술했습니다.
이전 버전 로그/체크섬은 과거 버전 기록으로 유지했습니다.
