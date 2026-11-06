# v11 검증 결과

환경: Ubuntu 24.04.3 LTS x86_64, GCC 13.3.0, C++17.

- `make clean` 이후 `make -j4 all test`로 라이브러리/예제/테스트 빌드.
- `make test` 전체 통과: 기존 binary tests, 220 general, 137 alignment, 58 msgcode, 56 v6, 47 v7, 49 inline/named mapping, 2759 audit integration checks.
- UDP 190 cases / 380 datagrams; Python struct oracle 380 frames byte-for-byte 일치.
- 변경한 공유 JSON을 사용하는 실제 demo 실행 성공.
- 공유 규칙의 여러 필드/규칙/인터페이스 파일 재사용, 상대 경로, 재로드, 이름 오류, 누락 파일, 기본값/오류 정책, 실패 시 대상 보존 검증.
- 이전 inline 설정 회귀 검사 유지.
- Make가 기존 CMake archive에 다른 이름의 오브젝트를 추가해 중복 심볼을 만들던 문제 수정: ar 실행 전 archive 제거.
- 정적 라이브러리에는 생산 코드 오브젝트 7개만 포함. 테스트는 test/에 유지.
- 이 환경에 CMake가 설치되어 있지 않아 v11 CMake 실행 검증은 수행하지 않음. CMake 구성의 변경은 프로젝트 버전 번호뿐.

공유 규칙은 로드 시 snapshot으로 복사합니다. 파일 변경 후 converter를 재생성해야 합니다.
이전 v10 이하 로그와 체크섬은 해당 이전 버전의 기록입니다.
