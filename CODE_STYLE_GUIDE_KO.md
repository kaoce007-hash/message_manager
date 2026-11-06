# v17 코드 작성 방식

## 코드는 단계별로, 주석은 필요한 이유만

v15의 중간 변수와 명시적인 if/return 방식은 유지합니다. 주요 함수는 처리 순서를 설명하고, 표준 라이브러리가 낯설 수 있는 사용 지점에는 역할과 이 코드에서의 이유를 함께 씁니다.
변수 대입, 반복, 출력, 반환을 그대로 읽어 주는 주석은 쓰지 않습니다.

```cpp
const Json &typeValue = entry.at("type");
const std::string typeName = typeValue.get<std::string>();
const std::size_t fieldAlignment = valueAlignment(typeName, types);
if (fieldAlignment > result)
{
    result = fieldAlignment;
}
```

## 남기는 주석

- 함수가 제공하는 동작과 호출 조건
- 실패 시 원본을 유지하기 위해 임시 객체를 쓰는 이유
- 버퍼와 length를 따로 설정하는 규칙
- 비트 저장 단위 공유, 정렬과 패딩, 엔디안과 부호 처리
- 숫자 문자열, UTF-8 표시 폭 등 코드만으로 의미를 파악하기 어려운 규칙

## 쓰지 않는 주석

- `result = value` 앞에 “결과를 보관합니다”
- `return result` 앞에 “값을 반환합니다”
- `if` 앞에 “조건을 확인합니다”
- 출력문 앞에 “표시 값이나 구분 문자를 출력합니다”
- 함수 선언과 구현에서 같은 내용을 여러 번 설명하는 긴 주석

## 기존 규칙

JSON 조회·타입 변환·계산을 중간 변수로 나누고 디버거로 확인할 수 있게 합니다.
조건을 나눌 때 && / ||의 단락 평가를 유지합니다.
일반 지역변수는 `value`, 멤버변수는 `_value` 형태로 작성합니다.
외부 nlohmann/json 코드는 수정하지 않습니다.
