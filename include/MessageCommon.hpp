#pragma once

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace message_manager
{
    // 이 소스의 읽기 안내
    // std::는 C++ 표준 라이브러리의 이름 공간입니다. std::set/map/vector는 오래전부터 쓰인 자료구조입니다.
    // Json은 표준 컨테이너가 아니라 외부 nlohmann::json의 짧은 이름입니다.
    // Json 객체는 {"이름": 값}, 배열은 [값, 값]을 보관하며 .at("이름")은 없는 키면 예외를 냅니다.
    // contains는 존재 여부, is_object/is_array는 값 종류, get<T>()는 C++ 타입으로 읽는 기능입니다.
    // 이 프로젝트는 C++17로 빌드합니다. 실제 C++17 문법인 if constexpr는 아래 checkedGet에서 설명합니다.
    using Json = nlohmann::json;
    // 등록 예: void myLog(const std::string &text) { std::cerr << text; }
    // manager.setLogHandler(myLog); 함수 주소를 보관하며 람다는 필요하지 않습니다.
    // using은 별칭을 붙이는 문법입니다. LogHandler는 객체가 아니라 로그 함수의 주소를 뜻합니다.
    // void (*)(const std::string &) = 문자열을 참조로 받고 반환값이 없는 함수의 포인터입니다.
    // nullptr을 넣으면 호출할 함수가 없다는 뜻이며, 일반 함수를 등록해 오류 기록 방법을 바꿀 수 있습니다.
    using LogHandler = void (*)(const std::string &);

    // 검증과 숫자 처리를 분리하여 메시지 코드를 읽기 쉽게 유지합니다.
    // constexpr(C++11)는 컴파일할 때 확정할 수 있는 상수에 씁니다.
    // std::size_t는 sizeof와 컨테이너 크기에 쓰는 부호 없는 정수입니다. 음수 대신 크기·인덱스를 표현합니다.
    constexpr std::size_t MaxMessageSize = 16 * 1024 * 1024;
    constexpr std::size_t MaxFieldCount = 65536;

    // [[noreturn]](C++11)는 이 함수가 정상적으로 돌아오지 않는다는 표시입니다.
    // 여기서는 항상 예외를 던져 호출 위치 이후 처리를 중단합니다.
    [[noreturn]] void fail(const std::string &text);

    Json readJsonFile(const std::string &path);

    // 허용한 JSON 키 목록과 실제 키를 비교합니다. 오타가 조용히 무시되는 일을 막습니다.
    // std::initializer_list(C++11)는 checkKeys(doc, {"link", "types"})의 중괄호 목록을 받는 타입입니다.
    // 함수 호출 동안 허용할 이름을 읽기만 하며, 그 목록의 메모리를 함수 밖에 따로 보관하지 않습니다.
    void checkKeys(const Json &object, std::initializer_list<const char *> allowed);

    // 필드 이름의 첫 글자로 가능한 영문자/밑줄인지 확인합니다.
    bool identifierStart(char character);

    // 필드 이름 전체가 영문자/숫자/밑줄 규칙에 맞는지 확인합니다.
    bool identifier(const std::string &name);

    // 문자열이 십진수 또는 0x/0X로 시작하는 16진수 형식인지 확인합니다.
    // 실제 범위 검사는 parseInteger에서 합니다. 일반 문자열은 숫자로 바꾸지 않습니다.
    bool isIntegerValue(const Json &value);

    // 50, "50", "0x32"를 같은 정수로 읽습니다. 음수는 "-0x32"도 가능합니다.
    // uint64_t 최댓값과 int64_t 최솟값까지 정확하게 처리합니다.
    Json parseInteger(const Json &value);

    // 십진수/16진수 정수를 uint64_t로 읽습니다. 음수와 실수는 거부합니다.
    std::uint64_t unsignedValue(const Json &value);

    // int64_t 범위 안의 정수만 읽습니다. 큰 unsigned 값을 음수로 바꾸지 않습니다.
    std::int64_t signedValue(const Json &value);

    // template은 타입을 매개변수로 받아 같은 코드를 여러 타입에 적용하는 문법입니다.
    // VectorType<int>::value는 false, VectorType<std::vector<int>>::value는 true입니다.
    // 기본형은 std::false_type, 아래 vector 전용 특수화는 std::true_type을 상속해 그 값을 제공합니다.
    // 이렇게 타입의 성질을 컴파일 시점에 조사하는 도구를 타입 특성(type traits)이라고 합니다.
    template <class T> struct VectorType : std::false_type
    {
    };

    template <class Item, class Allocator>
    struct VectorType<std::vector<Item, Allocator>> : std::true_type
    {
        // Value는 vector 안에 들어 있는 원소 타입의 별칭입니다.
        // 예를 들어 std::vector<uint16_t>라면 Value는 uint16_t가 되어 각 원소를 같은 범위로 검사합니다.
        using Value = Item;
    };

    // JSON의 get<T>()는 작은 정수로 읽을 때 잘라낼 수 있으므로 범위를 명시적으로 검사합니다.
    // get<uint16_t>와 get<std::vector<uint16_t>>가 모두 이 함수로 옵니다.
    // 흐름: T의 종류 선택 → 숫자 범위 또는 원소별 검사 → 안전하게 T로 변환.
    // if constexpr(C++17)는 일반 if와 달리 타입에 맞는 분기만 인스턴스화합니다.
    // T=int일 때 vector 전용 코드를 검사하지 않아, 서로 다른 타입의 처리를 한 템플릿에 둘 수 있습니다.
    // std::is_integral/is_signed/is_floating_point는 각각 정수/부호 있음/실수 타입 여부를 알려줍니다.
    // std::numeric_limits<T>::lowest()/max()는 T의 최솟값/최댓값입니다. 작은 타입으로 잘리는 일을 막습니다.
    // typename VectorType<T>::Value의 typename은 타입 매개변수에 의존하는 이름이 '타입'임을 컴파일러에 알립니다.
    template <class T> T checkedGet(const Json &value)
    {
        // 요청한 C++ 타입에 맞는 코드만 컴파일하도록 분기합니다.
        if constexpr (VectorType<T>::value)
        {
            const bool isArray = value.is_array();
            if (!isArray)
            {
                fail("expected array in get<T>()");
            }
            T result;
            const std::size_t count = value.size();
            result.reserve(count);
            for (const Json &item : value)
            {
                // 요청한 C++ 타입의 범위를 확인하며 값을 변환합니다.
                const typename VectorType<T>::Value converted = checkedGet<typename VectorType<T>::Value>(item);
                result.push_back(converted);
            }
            return result;
        }
        // 앞 타입에 해당하지 않으면 이 C++ 타입 분기를 검사합니다.
        else if constexpr (std::is_integral<T>::value)
        {
            // 요청한 C++ 타입에 맞는 코드만 컴파일하도록 분기합니다.
            if constexpr (std::is_signed<T>::value)
            {
                const std::int64_t number = signedValue(value);
                const T minimum = std::numeric_limits<T>::lowest();
                const std::int64_t minimumValue = static_cast<std::int64_t>(minimum);
                if (number < minimumValue)
                {
                    fail("integer out of range in get<T>()");
                }
                const T maximum = std::numeric_limits<T>::max();
                const std::int64_t maximumValue = static_cast<std::int64_t>(maximum);
                if (number > maximumValue)
                {
                    fail("integer out of range in get<T>()");
                }
                const T result = static_cast<T>(number);
                return result;
            }
            else
            {
                const std::uint64_t number = unsignedValue(value);
                const T maximum = std::numeric_limits<T>::max();
                const std::uint64_t maximumValue = static_cast<std::uint64_t>(maximum);
                if (number > maximumValue)
                {
                    fail("unsigned integer out of range in get<T>()");
                }
                const T result = static_cast<T>(number);
                return result;
            }
        }
        // 앞 타입에 해당하지 않으면 이 C++ 타입 분기를 검사합니다.
        else if constexpr (std::is_floating_point<T>::value)
        {
            const bool isNumber = value.is_number();
            if (!isNumber)
            {
                fail("expected number in get<T>()");
            }
            const long double number = value.get<long double>();
            const bool isFinite = std::isfinite(number);
            if (!isFinite)
            {
                fail("floating point out of range in get<T>()");
            }
            const long double magnitude = std::abs(number);
            const T maximum = std::numeric_limits<T>::max();
            const long double maximumValue = static_cast<long double>(maximum);
            if (magnitude > maximumValue)
            {
                fail("floating point out of range in get<T>()");
            }
            const T result = static_cast<T>(number);
            return result;
        }
        else
        {
            // 일반 Json 읽기로 처리할 타입은 마지막 분기로 옵니다. 숫자는 위에서 별도로 범위를 검사했습니다.
            const T result = value.get<T>();
            return result;
        }
    }

    // 타입 별칭을 표준 이름으로 통일합니다. 예: uint16 -> uint16_t, char* -> bytes.
    std::string canonical(std::string type);

    bool integerType(const std::string &type);

    // 기본 정수/실수/문자열/버퍼 타입인지 확인합니다. 아니면 사용자 구조체로 처리합니다.
    bool builtin(const std::string &type);

    // 정수/실수의 저장 바이트 수를 반환합니다. 예: uint16_t -> 2.
    std::size_t width(const std::string &type);

    // 설정 문자열 big/little을 bool로 변환합니다. 잘못된 표기는 거부합니다.
    bool bigEndian(const std::string &name);

    std::uint64_t integerBits(const std::string &type, const Json &value);

    // 정수 비트를 1바이트씩 나눠 엔디안 순서대로 출력 버퍼 뒤에 붙입니다.
    void appendBits(std::vector<std::uint8_t> &output,
        std::uint64_t bits,
        std::size_t bytes,
        bool big);

    void writeTextFile(const std::string &path, const std::string &text);

    // 사용자 로그 함수가 없을 때 기본 오류 출력을 담당합니다.
    void defaultLog(const std::string &text);

}
