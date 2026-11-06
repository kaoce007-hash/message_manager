# Ubuntu 24.04 / GCC 연동 검증 결과

검증일: 2026-10-29

## 환경과 결과

| 항목 | 확인 내용 |
|---|---|
| OS | Ubuntu 24.04.3 LTS, x86_64 |
| C++ 컴파일러 | g++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0 |
| glibc | 2.39 |
| 언어 | C++17 |
| Release | CMake Release, -Wall -Wextra -Wpedantic -Werror, 전체 빌드 성공 |
| 기존/연동/독립 대조 시험 | CTest 8종 모두 통과 |
| STL 디버그 | -D_GLIBCXX_DEBUG -D_GLIBCXX_ASSERTIONS 적용, 연동 검사 통과 |
| 메모리/미정의 동작 | AddressSanitizer + UndefinedBehaviorSanitizer 적용, 연동 검사 통과 |
| 누수 검사 | LeakSanitizer가 실행 환경의 ptrace 제약으로 종료되어 확인하지 못함 |

GCC의 C++ 드라이버인 **g++**로 컴파일합니다. nlohmann/json 헤더를 동봉하여
소스 빌드에 인터넷이나 별도 JSON 패키지 설치는 필요하지 않습니다.
CMake가 없으면 기존의 g++ 직접 빌드 명령을 사용할 수 있습니다.
Python은 독립 바이트 대조 시험에만 사용하며 라이브러리/예제의 실행 의존성이 아닙니다.

## 실제로 재현하고 수정한 3개 문제

1. **작은 정수로 get할 때 값이 잘림**
   - 이전: uint16_t 값 300을 get<uint8_t>()로 읽으면 44가 됨.
   - 수정: 스칼라와 배열의 숫자 변환 범위를 검사하여 예외를 발생시킴.
   - 음수→unsigned, uint64_t→int64_t, double→float 초과, 실수→정수 절삭도 거부.
   - 필드에 선언한 올바른 타입으로 읽는 코드는 그대로 동작함.

2. **파일 JSON의 중복 키/뒤쪽 추가 문서가 조용히 통과**
   - 이전: 같은 link 키를 두 번 적으면 마지막 값으로 덮어써짐.
   - 수정: 파일을 읽는 공통 파서에서 모든 객체의 중복 키와 뒤쪽 추가 데이터를 거부.
   - Message, MessageLink, enum, 변환기 파일 로드가 같은 파서를 사용함.
   - 서로 다른 객체에서 같은 키 이름을 쓰는 것은 정상 허용함.

3. **캐시된 공통 타입의 중첩 깊이 검사 우회**
   - 이전: 사용하지 않는 70단계 타입을 순차 선언하면 일부 경로에서 허용됨.
   - 수정: 타입별 실제 중첩 깊이를 보관하고 재사용 시 남은 깊이도 계산함.
   - 사용하는 타입과 미사용 타입 모두 64단계 경계를 검사함.

버퍼/length의 사용자 수동 설정, 공통 msgheader 참조, 정렬, 반환형 변환 API와 이름 조회 방식은 유지했습니다.

## 수행한 연동 시험

- 제공된 실제 link_message1/2 JSON, enum, interface 정의를 함께 로드.
- 버퍼 길이 19종(0..1024바이트 경계값)을 10회씩, 총 190개 사례 생성.
- 원본 데이터를 실제 127.0.0.1 UDP 소켓으로 전송/수신.
- 헤더 msgcode로 링크와 메시지를 자동 판별하고 현재 값을 반영.
- converter.convert(source)로 big endian/alignment=4 원본을 little endian/alignment=1 대상으로 변환.
- 대상을 다시 UDP로 전송/수신하여 총 380개 데이터그램 검사.
- 역변환 결과가 원본의 모든 바이트와 동일한지 검사.
- 원본 값 보존, 수동 length 보존, NUL/FF, 숫자/구조체 배열, 현재 데이터 덤프 검사.
- Python 표준 struct.pack으로 별도로 만든 기준 바이너리와 원본/대상 총 380개 프레임을 바이트 단위로 대조.
- 고정 seed로 데이터 1,024건 변형. 형식상 유효한 677건은 수용하고, 코드/길이/프레임이 잘못된 347건은 거부.
- 수신 실패 시 관리 중인 모든 메시지 상태가 바뀌지 않는지 검사.
- 정상 수용된 변형 프레임은 패딩을 정규화한 뒤 다시 직렬화/해석해도 같은 바이트인지 검사.

변형 입력이 모두 거부되어야 하는 것은 아닙니다. 유효한 데이터 값이나 무시되는 패딩을 바꾸면
여전히 유효한 메시지입니다. 프로토콜의 체크섬/인증 검사는 이 라이브러리에 포함하지 않았습니다.

## 시험 집계

| 시험 | 검증 수/결과 |
|---|---:|
| 기본/과거 바이너리 경계값 | 통과 |
| 기존 메시지 기능 | 220 |
| 정렬 | 137 |
| msgcode 판별 | 58 |
| 구조체/버퍼 인터페이스 | 56 |
| 수동 길이/공통 헤더/반환형 변환 | 47 |
| 추가 GCC 연동/오류 경계 | 2759 |
| 독립 Python 바이트 대조 | 190사례 / 380프레임 일치 |

집계가 있는 C++ 검사 합계는 3,277개이며, 별도 기본 테스트와 Python 대조도 통과했습니다.
변형 시험과 UDP 연동은 Release, ASan/UBSan, STL 디버그 실행에서 같은 결과였습니다.

## 재현 명령

프로젝트 루트에서:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror'
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/demo json
```

메모리/미정의 동작 검사:

```sh
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I third_party audit_integration_tests.cpp -o audit_san
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./audit_san json
```

이 환경은 LeakSanitizer가 ptrace 제약으로 실행되지 않아 detect_leaks=0으로 검사했습니다.
사용자의 일반 Ubuntu PC에서는 detect_leaks=1로 별도 누수 검사를 실행할 수 있습니다.

STL 디버그:

```sh
g++ -std=c++17 -O0 -g -D_GLIBCXX_DEBUG -D_GLIBCXX_ASSERTIONS \
  -Wall -Wextra -Wpedantic -Werror -I third_party \
  audit_integration_tests.cpp -o audit_stl
./audit_stl json
```

## 확인 범위

제공된 정의와 상기 오류/경계/연동 입력에서 통과한 결과입니다.
동일 객체의 동시 수정, TCP 스트림 누적/프레임 분리, 사용자가 준 포인터의 실제 메모리 유효성,
현장 장비 프로토콜과의 호환은 별도의 호출 측/장비 연동 조건입니다.
Windows/MSVC, 다른 CPU 및 컴파일러 버전은 이번 시험 대상이 아닙니다.
검사 결과를 모든 가능한 입력에 버그가 없다는 보장으로 확대하지 않았습니다.
