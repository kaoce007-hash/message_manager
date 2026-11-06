# v10 검증 결과 — 2026-10-30

Ubuntu 24.04.3 LTS x86_64 / GCC 13.3.0 / C++17.

## 변경

- interface_message.json의 fields 각 항목에 mapping을 포함하도록 변경.
- 별도 field_mapping.json과 관련 두 파일 API 제거.
- 테스트 C++/Python, 회귀용 JSON, 검증 자료를 모두 test/ 아래에 배치.
- 일반 함수 구현은 src/*.cpp로 분리, 공개 선언/템플릿은 include/에 배치.
- CMake STATIC 및 Makefile로 하나의 libbinary_message.a 생성.
- 예제/시험은 라이브러리를 실제 링크. 라이브러리에 시험 코드는 포함하지 않음.

## 실행 결과

- Make: 전체 소스 경고를 오류로 처리한 빌드 및 make test 성공.
- CMake Release: -Wall -Wextra -Wpedantic -Werror 빌드 성공.
- CTest: 9/9 통과 (독립 Python 바이너리 비교 포함).
- 새 필드별 매핑 시험: 34 checks 통과.
- 기존 시험: legacy, 220 일반, 137 정렬, 58 msgcode, 56 v6, 47 v7 검사 통과.
- UDP: 190 cases / 380 datagrams, 1024개 변형 입력, 2759 checks 통과.
- Python struct 기준: 380프레임 바이트 단위 일치.
- cmake --install로 설치 후, 설치된 include/와 .a만 지정해 예제 재빌드 및 실행 성공.
- nm/ar로 serialize, deserialize, convert, receive의 실제 구현 심볼 및 생산 코드 오브젝트 확인.
- 동봉 lib/libbinary_message.a는 검증한 CMake Release 산출물.

새 매핑 시험은 mapping 생략, 값 치환, 빈 대응표, 기본값/오류 정책, 역방향,
문자열/실수/정수 비교, 중복·잘못된 설정 거부, 타입 범위 오류, 원본 및 대상 상태 보존,
바이너리 수신과 manager/convertBytes 경로를 확인했습니다.
기존 직접 대입 회귀 시험은 test/data/current/json을 사용하고,
새 매핑 시험은 실제 json/interface_message.json을 사용합니다.

## 빌드 재현

```sh
make -j3 CXXFLAGS='-O1 -std=c++17 -fPIC -Wall -Wextra -Wpedantic -Werror'
make test
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror'
cmake --build build-cmake -j3
ctest --test-dir build-cmake --output-on-failure
cmake --install build-cmake --prefix ./install
```

CMake는 검증 도구로 4.4.4를 사용했으며 프로젝트 최소 요구는 3.16입니다.
v8/v9의 시험 기록은 당시 버전의 이력이며 현재 v10의 검증 결과를 대신하지 않습니다.
이 버전에서는 ASan/누수 검사를 별도로 재실행하지 않았습니다.
