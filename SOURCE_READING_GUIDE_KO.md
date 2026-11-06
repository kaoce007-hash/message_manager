# v15 코드 학습용 주석판 읽기 순서

실행 코드는 v15와 동일하고 주석을 다시 구성한 배포본(v17)입니다.
소스 안에 흐름과 표준 라이브러리 설명이 있으므로 이 문서를 열어 두지 않아도 읽을 수 있습니다.

| 읽는 순서 | 파일/함수 | 확인할 내용 |
|---|---|---|
| 1 | examples/simple_main.cpp | enum/링크 등록, 원본 메시지 참조, set/get |
| 2 | src/MessageManager.cpp / addLink | 정의 검사, 임시 메시지 목록, 코드 중복 검사, 등록 확정 |
| 3 | include/BinaryMessage.hpp / set | 임시 복사본에 설정 후 성공한 값만 반영 |
| 4 | src/BinaryMessage.cpp / expandFields, structureAlignment | 중첩 구조체 펼치기, 정렬, 비트 저장 단위 |
| 5 | src/BinaryMessage.cpp / serialize, deserialize | 패딩/필드 바이트 생성과 수신 복원 |
| 6 | src/MessageInterface.cpp / convertInto | 원본 검증, 필드 복사·매핑·고정값, 대상 검증 |
| 7 | src/MessageCommon.cpp / JsonKeyChecker | JSON 객체별 키 집합, 중복 키 거부 |
| 8 | include/MessageCommon.hpp / checkedGet | 템플릿, 타입 특성, if constexpr, 숫자 범위 검사 |

## 소스에 설명한 도구

- std::set: 중복 없는 집합. 키/코드/규칙명 중복 검사, 타입 순환 검사에 각각 어떻게 사용하는지 설명합니다.
- std::pair와 insert(...).second: 원소 위치와 새 삽입 여부를 함께 받는 방법.
- std::map: 이름 → 정의/캐시 사전. find/end와 iterator의 first/second.
- std::vector: 연속 동적 배열. reserve와 resize 차이, 범위 삽입.
- std::list: 원소 참조 유지와 splice를 통한 목록 연결 이동.
- iterator/const_iterator: 위치를 나타내는 도구, begin/end의 반열린 범위.
- 참조와 const: 원본 접근과 복사본의 차이, 읽기 전용 접근.
- template, 타입 특성, typename: 타입을 매개변수로 받는 코드와 vector 원소 타입 추출.
- if constexpr: C++17에서 타입에 맞는 코드만 인스턴스화하는 이유.
- using, constexpr, [[noreturn]], static_assert: 별칭, 컴파일 시 상수/검사, 정상 반환하지 않는 함수.
- std::ref/reference_wrapper와 operator(): 상태를 가진 중복 키 검사 객체를 콜백으로 전달하는 방법.
- numeric_limits, uintN_t, static_cast, memcpy: 범위와 비트 패턴 보존.
- ostringstream과 출력 조작자: 실제 전송 바이트를 바꾸지 않고 HEX 문자열을 만드는 방법.

std::set/map/vector는 최근에 생긴 기능이 아니라 오랫동안 사용해 온 C++ 표준 자료구조입니다.
설명은 도입 시기보다 역할과 이 코드에서 선택한 이유를 중심으로 작성했습니다.

단순한 '값을 저장합니다', '값을 출력합니다' 주석은 반복하지 않습니다.
대신 어떤 데이터가 어디로 이동하는지, 왜 임시 객체나 해당 컨테이너가 필요한지를 설명합니다.
