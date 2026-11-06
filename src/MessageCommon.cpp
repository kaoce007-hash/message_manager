#include "MessageCommon.hpp"

namespace message_manager
{
    [[noreturn]] void fail(const std::string &text)
    {
        throw std::runtime_error(text);
    }

    // JSON 파서가 호출하는 객체입니다. 한 파일을 읽는 동안만 중복 키 목록을 보관합니다.
    // 전역변수를 쓰지 않으므로 여러 스레드가 서로 다른 파일을 읽어도 섞이지 않습니다.
    class JsonKeyChecker
    {
    public:
        // 파서가 객체 시작 / 키 / 객체 끝을 만날 때마다 이 함수를 호출합니다.
        // operator()를 정의한 객체는 checker(...)처럼 함수 모양으로 호출할 수 있습니다. 이를 함수 객체라고 합니다.
        // 이 객체를 Json::parse의 콜백으로 넘기면 파서가 키를 읽을 때마다 같은 객체를 호출합니다.
        bool operator()(int depth, Json::parse_event_t event, Json &parsed)
        {
            (void)depth;
            if (event == Json::parse_event_t::object_start)
            {
                // std::set<T>는 중복 없는 값을 보관하는 집합입니다. 원소를 정렬된 순서로 관리합니다.
                // 여기서는 정렬 결과를 출력하려는 것이 아니라, 같은 JSON 키가 이미 나왔는지 찾기 위해 씁니다.
                // 예: {"id":1,"id":2}에서 두 번째 id는 삽입되지 않아 중복을 알아낼 수 있습니다.
                // 새 객체의 {를 만나면 새 set을 추가합니다. 중첩 객체마다 별도 집합이 필요하기 때문입니다.
                std::set<std::string> emptyKeys;
                _objectKeys.push_back(emptyKeys);
                return true;
            }
            if (event == Json::parse_event_t::key)
            {
                const std::string key = parsed.get<std::string>();
                std::set<std::string> &currentKeys = _objectKeys.back();
                // set.insert(key)의 반환값은 std::pair<iterator, bool>입니다. pair는 두 값을 묶은 자료형입니다.
                // first는 그 원소의 위치, second는 새로 넣었는지 여부입니다. 이미 있으면 second=false입니다.
                // iterator는 컨테이너 안의 원소 위치를 나타내는 도구이며 포인터처럼 원소를 가리킵니다.
                const std::pair<std::set<std::string>::iterator, bool> result = currentKeys.insert(key);
                const bool inserted = result.second;
                if (!inserted)
                {
                    fail("duplicate JSON key: " + key);
                }
                return true;
            }
            if (event == Json::parse_event_t::object_end)
            {
                _objectKeys.pop_back();
            }
            return true;
        }

    protected:

    private:

    private:
        // back()은 현재 읽는 객체, 앞쪽 요소는 그 객체를 감싸는 부모입니다.
        // vector는 길이가 늘어날 수 있는 연속 배열입니다. 여기서는 객체의 중첩 순서를 보관합니다.
        // push_back으로 새 객체의 집합을 뒤에 넣고, back으로 현재 객체를 보고, pop_back으로 끝난 객체를 뺍니다.
        // 예: 바깥 { → [바깥 키 집합], 안쪽 { → [바깥 키 집합, 안쪽 키 집합].
        // 안쪽 }를 만나 마지막 집합을 빼면 다시 바깥 객체의 키를 검사합니다. 이 사용 방식이 스택입니다.
        std::vector<std::set<std::string>> _objectKeys;
    };

    // JSON 파일을 열고 중복 키와 문법을 검사하면서 전체 문서를 읽습니다.
    Json readJsonFile(const std::string &path)
    {
        // 흐름: 파일 열기 → 중복 키 검사 객체 연결 → 문법과 중복을 확인하며 Json으로 읽기.

        // 1. 파일을 엽니다. 실패하면 빈 JSON을 반환하지 않고 이유를 알립니다.
        std::ifstream input(path);
        if (!input)
        {
            fail("cannot open JSON: " + path);
        }
        // 2. 중복 키를 검사하면서 읽습니다.
        // std::ref는 checker의 복사본 대신 같은 검사 객체를 계속 사용하게 합니다.
        // std::ref(checker)는 checker를 복사하지 않고 같은 객체를 가리키는 포장(reference_wrapper)을 만듭니다.
        // JSON 파서가 콜백을 보관해도 검사 중 쌓인 키 목록이 같은 객체에 계속 누적되게 합니다.
        JsonKeyChecker checker;
        const std::reference_wrapper<JsonKeyChecker> ref202 = std::ref(checker);
        const Json document = Json::parse(input, ref202);
        // 3. 마지막까지 읽는 엄격한 파싱이므로 뒤에 붙은 쓰레기 데이터도 거부합니다.
        return document;
    }

    // 허용한 JSON 키 목록과 실제 키를 비교합니다. 오타가 조용히 무시되는 일을 막습니다.
    void checkKeys(const Json &object, std::initializer_list<const char *> allowed)
    {
        // 흐름: 객체인지 확인 → 실제 키를 하나씩 꺼내 허용 목록과 비교 → 오타/불필요한 키를 거부.

        if (!object.is_object())
        {
            fail("expected JSON object");
        }
        // allowed의 std::initializer_list는 {"link", "types"}처럼 전달한 목록을 잠시 읽는 가벼운 뷰입니다.
        // 별도 vector를 만들지 않고 함수 호출 동안 허용할 이름들을 전달합니다. 목록 원소는 수정하지 않습니다.
        for (Json::const_iterator item = object.begin(); item != object.end(); ++item)
        {
            bool found = false;
            for (const char *key : allowed)
            {
                if (item.key() == key)
                {
                    found = true;
                }
            }
            if (!found)
            {
                const std::string itemName = item.key();
                fail("unknown schema key: " + itemName);
            }
        }
    }

    // 필드 이름의 첫 글자로 가능한 영문자/밑줄인지 확인합니다.
    bool identifierStart(char character)
    {
        const bool isUppercase = character >= 'A' && character <= 'Z';
        if (isUppercase)
        {
            return true;
        }
        const bool isLowercase = character >= 'a' && character <= 'z';
        if (isLowercase)
        {
            return true;
        }
        if (character == '_')
        {
            return true;
        }
        return false;
    }

    // 필드 이름 전체가 영문자/숫자/밑줄 규칙에 맞는지 확인합니다.
    bool identifier(const std::string &name)
    {
        if (name.empty())
        {
            return false;
        }
        const char firstCharacter = name.front();
        bool matchesCondition = name.empty();
        if (!matchesCondition)
        {
            matchesCondition = !identifierStart(firstCharacter);
        }
        if (matchesCondition)
        {
            return false;
        }
        for (char character : name)
        {
            bool matchesCondition2 = !identifierStart(character);
            if (matchesCondition2)
            {
                matchesCondition2 = !(character >= '0' && character <= '9');
            }
            if (matchesCondition2)
            {
                return false;
            }
        }
        return true;
    }

    // 숫자 한 글자를 0..15로 바꿉니다. 숫자가 아니면 -1입니다.
    static int integerDigit(char character)
    {
        if (character >= '0' && character <= '9')
        {
            return character - '0';
        }
        if (character >= 'a' && character <= 'f')
        {
            return character - 'a' + 10;
        }
        if (character >= 'A' && character <= 'F')
        {
            return character - 'A' + 10;
        }
        return -1;
    }

    // 먼저 형식만 확인합니다. 매핑의 일반 문자열과 숫자 문자열을 구분할 때 사용합니다.
    bool isIntegerValue(const Json &value)
    {
        if (value.is_number_integer())
        {
            return true;
        }
        if (!value.is_string())
        {
            return false;
        }
        const std::string text = value.get<std::string>();
        std::size_t index = 0;
        bool matchesCondition = !text.empty();
        if (matchesCondition)
        {
            bool matchesCondition2 = text[0] == '-';
            if (!matchesCondition2)
            {
                matchesCondition2 = text[0] == '+';
            }
            matchesCondition = matchesCondition2;
        }
        if (matchesCondition)
        {
            index = 1;
        }
        int base = 10;
        bool matchesCondition3 = index + 1 < text.size();
        if (matchesCondition3)
        {
            bool matchesCondition4 = text[index] == '0';
            if (matchesCondition4)
            {
                bool matchesCondition5 = text[index + 1] == 'x';
                if (!matchesCondition5)
                {
                    matchesCondition5 = text[index + 1] == 'X';
                }
                matchesCondition4 = matchesCondition5;
            }
            matchesCondition3 = matchesCondition4;
        }
        if (matchesCondition3)
        {
            base = 16;
            index += 2;
        }
        if (index == text.size())
        {
            return false;
        }
        for (; index < text.size(); ++index)
        {
            const int digit = integerDigit(text[index]);
            if (digit < 0 || digit >= base)
            {
                return false;
            }
        }
        return true;
    }

    // 숫자 또는 십진수/16진수 문자열을 64비트 정수로 변환합니다. 곱하기 전에 범위를 검사합니다.
    Json parseInteger(const Json &value)
    {
        // 흐름: 이미 정수인 값은 유지 → 문자열의 부호/진법 확인 → 자릿수 누적 → 64비트 범위 검증.

        if (value.is_number_integer())
        {
            return value;
        }
        if (!isIntegerValue(value))
        {
            fail("expected decimal or hexadecimal integer");
        }
        const std::string text = value.get<std::string>();
        std::size_t index = 0;
        const bool negative = text[0] == '-';
        if (text[0] == '-' || text[0] == '+')
        {
            index = 1;
        }
        std::uint64_t base = 10;
        bool matchesCondition = index + 1 < text.size();
        if (matchesCondition)
        {
            bool matchesCondition2 = text[index] == '0';
            if (matchesCondition2)
            {
                bool matchesCondition3 = text[index + 1] == 'x';
                if (!matchesCondition3)
                {
                    matchesCondition3 = text[index + 1] == 'X';
                }
                matchesCondition2 = matchesCondition3;
            }
            matchesCondition = matchesCondition2;
        }
        if (matchesCondition)
        {
            base = 16;
            index += 2;
        }

        // 음수의 절댓값은 2^63까지, 양수는 2^64-1까지 허용합니다.
        std::uint64_t limit = UINT64_MAX;
        if (negative)
        {
            limit = std::uint64_t(INT64_MAX) + 1;
        }
        std::uint64_t number = 0;
        for (; index < text.size(); ++index)
        {
            const int integerDigit204 = integerDigit(text[index]);
            const std::uint64_t digit = static_cast<std::uint64_t>(integerDigit204);
            // 곱한 다음 검사하면 이미 넘칠 수 있으므로 곱하기 전에 검사합니다.
            if (number > (limit - digit) / base)
            {
                fail("integer literal overflow: " + text);
            }
            number = number * base + digit;
        }
        if (!negative)
        {
            const Json result = number;
            return result;
        }
        if (number == std::uint64_t(INT64_MAX) + 1)
        {
            const std::int64_t smallest = INT64_MIN;
            const Json result = smallest;
            return result;
        }
        const std::int64_t convertedValue = static_cast<std::int64_t>(number);
        const std::int64_t negativeNumber = -convertedValue;
        const Json result = negativeNumber;
        return result;
    }

    // 십진수/16진수 정수를 uint64_t로 읽습니다. 음수와 실수는 거부합니다.
    std::uint64_t unsignedValue(const Json &value)
    {
        const Json numberValue = parseInteger(value);
        if (numberValue.is_number_unsigned())
        {
            const std::uint64_t result = numberValue.get<std::uint64_t>();
            return result;
        }
        if (numberValue.is_number_integer())
        {
            const std::int64_t number = numberValue.get<std::int64_t>();
            if (number >= 0)
            {
                const std::uint64_t result = static_cast<std::uint64_t>(number);
                return result;
            }
        }
        fail("expected nonnegative integer");
    }

    // int64_t 범위 안의 정수만 읽습니다. 큰 unsigned 값을 음수로 바꾸지 않습니다.
    std::int64_t signedValue(const Json &value)
    {
        const Json numberValue = parseInteger(value);
        if (numberValue.is_number_unsigned())
        {
            const std::uint64_t number = numberValue.get<std::uint64_t>();
            if (number > static_cast<std::uint64_t>(INT64_MAX))
            {
                fail("signed integer overflow");
            }
            const std::int64_t result = static_cast<std::int64_t>(number);
            return result;
        }
        if (numberValue.is_number_integer())
        {
            const std::int64_t result = numberValue.get<std::int64_t>();
            return result;
        }
        fail("expected integer");
    }

    // 타입 별칭을 표준 이름으로 통일합니다. 예: uint16 -> uint16_t, char* -> bytes.
    std::string canonical(std::string type)
    {
        if (type == "char*" || type == "char *")
        {
            return "bytes";
        }
        for (const char *base : {"uint8", "int8", "uint16", "int16", "uint32", "int32", "uint64", "int64"})
        {
            bool matchesCondition = type == base;
            if (!matchesCondition)
            {
                matchesCondition = type == std::string(base) + "_t";
            }
            if (matchesCondition)
            {
                const std::string baseName = base;
                const std::string result = baseName + "_t";
                return result;
            }
        }
        return type;
    }

    bool integerType(const std::string &type)
    {
        const char *supportedTypes[] = {"uint8_t", "int8_t", "uint16_t", "int16_t",
            "uint32_t", "int32_t", "uint64_t", "int64_t"};
        for (const char *supported : supportedTypes)
        {
            if (type == supported)
            {
                return true;
            }
        }
        return false;
    }

    // 기본 정수/실수/문자열/버퍼 타입인지 확인합니다. 아니면 사용자 구조체로 처리합니다.
    bool builtin(const std::string &type)
    {
        const std::string normalized = canonical(type);
        const bool isInteger = integerType(normalized);
        if (isInteger)
        {
            return true;
        }
        if (normalized == "float")
        {
            return true;
        }
        if (normalized == "double")
        {
            return true;
        }
        if (normalized == "string")
        {
            return true;
        }
        if (normalized == "bytes")
        {
            return true;
        }
        return false;
    }

    // 정수/실수의 저장 바이트 수를 반환합니다. 예: uint16_t -> 2.
    std::size_t width(const std::string &type)
    {
        if (type == "uint8_t" || type == "int8_t")
        {
            return 1;
        }
        if (type == "uint16_t" || type == "int16_t")
        {
            return 2;
        }
        if (type == "uint32_t" || type == "int32_t" || type == "float")
        {
            return 4;
        }
        if (type == "uint64_t" || type == "int64_t" || type == "double")
        {
            return 8;
        }
        fail("unsupported scalar type: " + type);
    }

    // 설정 문자열 big/little을 bool로 변환합니다. 잘못된 표기는 거부합니다.
    bool bigEndian(const std::string &name)
    {
        if (name == "big")
        {
            return true;
        }
        if (name == "little")
        {
            return false;
        }
        fail("endian must be big or little");
    }

    // 정수의 범위를 검사한 뒤 저장할 비트 패턴을 uint64_t로 반환합니다.
    std::uint64_t integerBits(const std::string &type, const Json &value)
    {
        const std::size_t bytes = width(type);
        if (type.front() == 'u')
        {
            const std::uint64_t number = unsignedValue(value);
            bool matchesCondition = bytes < 8;
            if (matchesCondition)
            {
                matchesCondition = number >= (std::uint64_t(1) << (bytes * 8));
            }
            if (matchesCondition)
            {
                fail("unsigned integer overflow");
            }
            return number;
        }
        const std::int64_t number = signedValue(value);
        if (bytes < 8)
        {
            const std::int64_t limit = std::int64_t(1) << (bytes * 8 - 1);
            if (number < -limit || number >= limit)
            {
                fail("signed integer overflow");
            }
        }
        const std::uint64_t result = static_cast<std::uint64_t>(number);
        return result;
    }

    // 정수 비트를 1바이트씩 나눠 엔디안 순서대로 출력 버퍼 뒤에 붙입니다.
    void appendBits(std::vector<std::uint8_t> &output,
        std::uint64_t bits,
        std::size_t bytes,
        bool big)
    {
        // 흐름: 저장할 바이트 수만큼 반복 → 엔디안에 맞는 비트 위치 선택 → 한 바이트씩 출력.

        for (std::size_t index = 0; index < bytes; ++index)
        {
            std::size_t byteIndex = index;
            if (big)
            {
                byteIndex = bytes - 1 - index;
            }
            const std::uint8_t convertedValue = static_cast<std::uint8_t>(bits >> (byteIndex * 8));
            output.push_back(convertedValue);
        }
    }

    void writeTextFile(const std::string &path, const std::string &text)
    {
        std::ofstream output(path);
        if (!output)
        {
            fail("cannot open output: " + path);
        }
        output << text;
        output.close();
        if (!output)
        {
            fail("cannot write output: " + path);
        }
    }

    // 사용자 로그 함수가 없을 때 기본 오류 출력을 담당합니다.
    void defaultLog(const std::string &text)
    {
        std::cerr << text;
        std::cerr << '\n';
    }

}
