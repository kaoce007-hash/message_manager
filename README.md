# message_manager

JSON으로 바이너리 메시지를 정의하고 생성·수신·변환하는 C++17 라이브러리입니다.
프로젝트와 정적 라이브러리 이름은 `message_manager`이며 빌드 결과는 `libmessage_manager.a`입니다.
기존 include 파일명과 CMake의 `binary_message` 연결 이름도 계속 사용할 수 있습니다.

배열은 예전처럼 **필드명, 인덱스, 값** 순서로 설정합니다.

```cpp
msg.set("body.samples", 0, 10);
msg.set("body.samples", 1, 20);
msg.set("body.samples", 2, 30);
```

인덱스는 0부터 시작합니다. 범위 초과나 잘못된 값은 예외가 발생하며 이전 값은 유지됩니다.
구조체 배열의 원소 전체는 `Json` 객체를 값으로 전달할 수 있습니다.
기존 배열 경로와 `set(버퍼명, 포인터, 바이트수)`도 지원합니다.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

# 정렬 처리 단순화 업데이트

별도 배치 목록을 제거하고 필드를 직접 순회하도록 변경했습니다.
변경 구조와 읽는 순서는 [DIRECT_ALIGNMENT_KO.md](DIRECT_ALIGNMENT_KO.md)를 참고하세요.

## 소스 구조와 설명

실행 코드는 v15 그대로입니다. 주요 함수의 처리 흐름과 std::set/map/list/vector, iterator, 참조, 템플릿 등 코드 이해에 필요한 설명을 소스 안에 추가했습니다. `MessageCatalog`, `MessageLink`, `FieldAccess`를 제거했습니다.
주요 기능은 **MessageManager(보관), Message(값과 바이트), MessageInterface(변환)** 세 클래스에 있습니다.

| 먼저 볼 파일 | 내용 |
|---|---|
| `SOURCE_READING_GUIDE_KO.md` | 소스 읽는 순서와 설명 위치 |
| `examples/simple_main.cpp` | 최소 사용 예제 |
| `include/MessageManager.hpp` | 관리자가 제공하는 함수와 보관 데이터 |
| `src/MessageManager.cpp` | JSON 등록, 메시지 찾기, 생성, 수신 |
| `include/BinaryMessage.hpp` | 메시지 값 설정/읽기 |
| `src/BinaryMessage.cpp` | 필드 검사와 바이트 변환 |
| `src/MessageInterface.cpp` | 복사/매핑/고정값 변환 |
| `MAINTENANCE_GUIDE_KO.md` | 구조 그림과 함수별 따라 읽기 |

주요 함수 안에 흐름 요약과 실제 처리 구간별 설명을 넣었습니다. 단순 대입·출력·반환 설명은 반복하지 않고, 컨테이너의 동작과 선택 이유, 데이터 이동, 실패 시 상태 보존 원리를 설명합니다.
관리자 내부에서 링크/메시지를 찾고 만드는 과정을 중간 변수로 나눠 썼습니다.

[코드 작성 방식과 단계별 예제](CODE_STYLE_GUIDE_KO.md)를 함께 보시면 됩니다.

## 사용법

```cpp
#include "MessageManager.hpp"

message_manager::MessageManager manager;
manager.loadEnums("json/message_enums.json");
manager.loadLink("json/link_message1.json");

message_manager::Message &msg = manager.message("link_message1", "message1");
msg.set("body.position.x", 10);
int x = msg.get<int>("body.position.x");
```

관리 중인 값을 수정할 때는 `message()`의 참조를 사용합니다.
새로 값을 채울 독립 메시지는 `create(linkName, messageName)`으로 만듭니다.
아직 필드를 모두 채우지 않았다면 serialize() 대신 dump()로 미설정 필드를 확인합니다.
전체 예제는 `examples/main.cpp`에 있습니다.

## 배열과 가변 버퍼

```cpp
msg.set("body.samples", 0, 10);
msg.set("body.samples", 1, 20);
msg.set("body.samples", 2, 30);
msg.set("body.points[0].x", -10);
std::size_t count = msg.elementCount("body.points");

char data[] = {'A', '\0', 'B'};
msg.set("body.data.buffer", data, sizeof(data));
msg.set("body.data.length", sizeof(data));
```

`char*` 버퍼는 내용만 복사합니다. length 필드는 사용자가 별도로 설정합니다.

## 메시지 변환

```cpp
#include "MessageInterface.hpp"

manager.loadLink("json/link_message2.json");
message_manager::MessageInterface converter =
    message_manager::MessageInterface::fromFile("json/interface_message.json", manager);

message_manager::Message result = converter.convert(msg);
```

원본 msg의 필드를 모두 채운 후 변환합니다. 변환 결과는 독립된 메시지입니다.

## 숫자와 고정값

`50`, `"50"`, `"0x32"`는 정수 값으로 사용하는 곳에서 모두 50입니다.
16진수는 JSON 문법에 맞게 문자열로 적습니다. 음수 `"-0x32"`도 지원합니다.
문자열 타입 필드 자체의 숫자 모양 문자열은 바꾸지 않습니다.

```json
{"from":"body.position.y","to":3}
```

대상 메시지의 같은 경로를 3으로 설정합니다. 경로가 다르면 아래처럼 씁니다.

```json
{"from":"body.position.y","to":"body.coordinates.north","value":"0x3"}
```

enum, 비트필드, 공통 mapping, 구조체/배열, 정렬, msgcode 자동 판별 기능은 유지합니다.
완전한 숫자/고정값 예제는 `json/integer_formats`에 있습니다.

## 빌드와 시험

```bash
make -j4
./build/simple_demo
./build/demo json
make test
```

생성되는 정적 라이브러리: `lib/libmessage_manager.a`.
CMake 설정도 제공하지만 이번 환경의 실행 검증은 Make로 진행했습니다.

## 이전 버전에서 바꿀 호출

| 이전 | v15 |
|---|---|
| `manager.catalog().create(linkName, messageName)` | `manager.create(linkName, messageName)` |
| `fromFile(path, manager.catalog())` | `fromFile(path, manager)` |
| `msg["body"]["position"]["x"].set(10)` | `msg.set("body.position.x", 10)` |
| `msg["body"]["points"].size()` | `msg.elementCount("body.points")` |
| `MessageCatalog` / `MessageLink` | `MessageManager` |

새 헤더와 새 .a를 함께 사용하고 호출 프로그램을 다시 컴파일합니다.
자세한 이유와 변경 예시는 유지보수 안내서에 있습니다.
