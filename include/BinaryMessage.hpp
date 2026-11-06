#pragma once

#include "DumpTable.hpp"
#include "EnumDefinitions.hpp"

namespace message_manager
{
    class Message
    {
    public:
        // 헤더의 코드 위치를 정의에서 계산합니다. 사용자가 바이트 오프셋을 중복 입력하지 않습니다.
        struct UnsignedFieldLocation
        {
            // 메시지 시작부터의 바이트 위치입니다.
            std::size_t _offset = 0;
            // 필드 또는 코드 저장 단위의 바이트 수입니다.
            std::size_t _size = 0;
            // 빅엔디안이면 true, 리틀엔디안이면 false입니다.
            bool _big = true;
            // 비트필드의 유효 비트 수입니다. 0이면 일반 필드입니다.
            std::size_t _bitWidth = 0;
            // 같은 저장 단위 안의 비트 시작 위치입니다. 바이트 오프셋이 아닙니다.
            std::size_t _bitOffset = 0;
        };

        // 메시지 정의를 검사하고 필드/배치 목록을 준비합니다. 이 시점에는 사용자 데이터가 없습니다.
        explicit Message(const Json &schema,
            const std::string &linkName = "",
            const std::string &messageName = "",
            const EnumDefinitions &enums = EnumDefinitions());

        static Message fromFile(const std::string &path);

        const std::string &linkName() const;

        const std::string &messageName() const;

        std::string getLinkName() const;

        std::string getMessageName() const;

        // 오류 로그를 받을 일반 함수의 주소를 등록합니다. nullptr이면 로그 출력을 끕니다.
        void setLogHandler(const LogHandler &handler);

        const LogHandler &logHandler() const;

        /*
         * 입력: name=전체 필드 경로, value=저장할 값.
         * 흐름: [기존 값 복사] → collectSet → checkLengths → [_fields의 값 반영]
         * 예시: set("body.position.x", 10). 하나라도 틀리면 기존 값은 유지합니다.
         */
        template <class T> void set(const std::string &name, const T &value)
        {
        // 흐름: 입력을 Json으로 준비 → 임시 Message에 설정 → 길이 검사 → 성공한 필드 값만 교환.

            try
            {
                const Json input = value;
                // 실패 시 원본을 유지하기 위해 메시지를 임시 작업본에 복사합니다.
                // *this는 지금 set을 호출한 Message 객체입니다. 여기에 &가 없으므로 candidate는 독립된 값 복사본입니다.
                Message candidate = *this;
                candidate.collectSet(name, input);
                // char*와 길이는 어느 순서로든 따로 설정할 수 있습니다.
                // 실제 전송 시 serialize()가 최종 길이 일치를 검사합니다.
                candidate.checkLengths(false);
                // 검사를 모두 통과한 작업본을 실제 값으로 반영합니다.
                // vector.swap은 두 vector의 내용물을 교환합니다. 검사가 끝나기 전까지 원본 _fields에는 손대지 않습니다.
                _fields.swap(candidate._fields);
            }
            catch (const std::exception &error)
            {
                // 값 설정 실패 이유를 등록된 로그 함수에 전달합니다.
                logSetFailure(name, error);
                throw;
            }
        }

        /*
         * 배열의 한 원소를 예전 호출 방식으로 설정합니다.
         * 입력: name=배열 필드 경로, index=0부터 시작하는 원소 번호, value=저장할 값.
         * 예시: set("body.point", 2, 20).
         * 흐름: 배열 원소 경로 만들기 → 기존 set으로 타입/범위 검사 후 저장.
         * 배열 범위를 벗어나거나 값이 잘못되면 기존 값은 유지하고 예외를 전달합니다.
         */
        template <class T> void set(const std::string &name, std::size_t index, const T &value)
        {
            const std::string indexText = std::to_string(index);
            const std::string elementName = name + "[" + indexText + "]";
            set(elementName, value);
        }

        // char*의 주소는 저장하지 않고 length바이트를 메시지 내부로 복사합니다.
        // length_from에 연결된 uint32_t length는 이 함수에서 변경하지 않습니다.
        void set(const std::string &name, const void *data, std::size_t length);

        // 가변 버퍼 필드를 vector<char>로 복사해 반환합니다. 0x00도 일반 데이터로 보존합니다.
        std::vector<char> getBuffer(const std::string &name) const;

        // 변환 대상은 이전 값이 섞이지 않게 비우되 정의된 msgcode는 유지합니다.
        Message emptyCopy() const;

        // set과 같지만 실패를 예외 대신 false로 받습니다. 실패 이유는 로그로 확인합니다.
        template <class T> bool trySet(const std::string &name, const T &value)
        {
            try
            {
                set(name, value);
                return true;
            }
            catch (const std::exception &)
            {
                return false;
            }
        }

        /*
         * 입력: name=전체 필드 경로, T=꺼낼 C++ 타입.
         * 흐름: [경로] → readValue → checkedGet<T> → [범위를 확인한 C++ 값]
         * 예시: get<int>("body.position.x"). 미설정 값/범위 초과는 예외입니다.
         */
        // get<int>(경로)의 <int>는 원하는 반환 타입입니다. 템플릿이라 int/double/vector 등으로 재사용합니다.
        // 끝의 const는 이 함수를 통해 메시지의 논리적 값을 변경하지 않는다는 뜻입니다.
        // const Json&는 Json 복사본 없이 읽고, Json result처럼 &가 없으면 독립된 값을 만듭니다.
        template <class T> T get(const std::string &name) const
        {
            const Json storedValue = readValue(name);
            // 요청한 C++ 타입의 범위를 확인하며 값을 변환합니다.
            const T result = checkedGet<T>(storedValue);
            return result;
        }

        // 필드 값을 JSON 형태로 읽습니다. get<T>와 달리 호출자가 C++ 타입을 지정하지 않습니다.
        Json value(const std::string &name) const;

        bool hasField(const std::string &name) const;

        // 자동 판별용 필드는 가변 데이터 앞의 고정 위치에 있는 부호 없는 정수여야 합니다.
        UnsignedFieldLocation unsignedFieldLocation(const std::string &name) const;

        // 배열 또는 구조체의 자식 개수입니다. 미설정 상태에서도 정의만 보고 계산합니다.
        std::size_t elementCount(const std::string &name) const;

        // JSON에서 선택한 메시지 정렬 단위(1/2/4/8)를 반환합니다.
        std::size_t alignment() const;

        // payload를 모두 길이 0으로 놓았을 때의 최소 배치 크기입니다. 패딩도 포함합니다.
        std::size_t fixedSize() const;

        // 현재 값으로 직렬화했을 때의 바이트 크기를 계산합니다.
        std::size_t size() const;

        // 정의된 배치 순서대로 패딩과 필드를 써서 전송용 바이트 배열을 만듭니다.
        std::vector<std::uint8_t> serialize() const;

        // 받은 바이트를 임시 값 목록으로 읽습니다. 길이/코드 검사가 모두 성공해야 현재 값에 반영합니다.
        void deserialize(const std::vector<std::uint8_t> &data);

        // 값이 없는 메시지도 표로 표시하므로 관리 객체의 전체 덤프에 사용할 수 있습니다.
        // maxHexBytes=0이면 전체 HEX를 출력하고, 기본은 앞의 256바이트까지 출력합니다.
        std::string dump(std::size_t maxHexBytes = 256) const;

        void dump(std::ostream &output, std::size_t maxHexBytes = 256) const;

        void dumpToFile(const std::string &path, std::size_t maxHexBytes = 256) const;

    protected:

    private:
        friend class MessageManager;
        // 중첩 구조체와 고정 배열을 최종 말단 필드 목록으로 펼쳐 보관합니다.
        // 예: header.id, samples[0], points[1].x.
        struct Field
        {
            // 이 필드의 현재 값입니다. null은 아직 값을 설정하지 않았다는 뜻입니다.
            Json _value;
            std::string _name;
            // 정수·실수·문자열·버퍼 등의 필드 타입입니다.
            std::string _type;
            // 이 필드를 쓰기/읽기 전에 맞출 바이트 경계입니다. 패딩 수는 현재 위치에서 계산합니다.
            std::size_t _alignment = 1;
            // 필드 또는 코드 저장 단위의 바이트 수입니다.
            std::size_t _size = 0;
            // 빅엔디안이면 true, 리틀엔디안이면 false입니다.
            bool _big = true;
            // 비트필드의 유효 비트 수입니다. 0이면 일반 필드입니다.
            std::size_t _bitWidth = 0;
            // 같은 저장 단위 안의 비트 시작 위치입니다. 바이트 오프셋이 아닙니다.
            std::size_t _bitOffset = 0;
            // 앞 비트필드와 저장 바이트를 공유하면 true입니다. 이 필드 때문에 바이트 수를 늘리지 않습니다.
            bool _continuation = false;
            // 값에 붙일 표시 이름을 찾는 enum 정의 이름입니다.
            std::string _enumName;
            // 버퍼 길이를 기록하는 다른 필드의 전체 경로입니다.
            std::string _lengthFrom;
            // 가변 버퍼가 허용하는 최대 바이트 수입니다.
            std::size_t _maxLength = 0;
            // char* 버퍼처럼 사용자가 길이 필드도 직접 설정하는 경우 true입니다.
            bool _manualLength = false;
        };

        // 전체 배열/구조체를 set/get하기 위한 부모-자식 연결입니다.
        struct Container
        {
            // 부모가 배열이면 true, 구조체이면 false입니다.
            bool _array = false;
            // 자식 필드의 전체 경로를 선언 순서대로 보관합니다.
            std::vector<std::string> _children;
            // 구조체 자식의 짧은 이름입니다. 예: x, y. 부모 경로와는 분리합니다.
            std::vector<std::string> _keys;
        };

        Field &field(const std::string &name);
        const Field &field(const std::string &name) const;

        // 부모 경로와 자식 이름을 점으로 연결합니다. 빈 부모라면 자식 이름만 반환합니다.
        static std::string fieldPath(const std::string &parent, const std::string &child);

        // JSON 필드 하나의 타입/길이/비트 수/enum 설정을 검사합니다. 값 자체를 저장하지는 않습니다.
        void validateEntry(const Json &entry) const;

        // 같은 구조체 안의 모든 필드를 검사합니다. 중복 이름과 과도한 중첩도 거부합니다.
        std::size_t validateFields(const Json &fields, const Json &types, std::set<std::string> &active) const;

        // 사용자 구조체 타입을 따라가며 검사합니다. active는 현재 방문 중인 타입의 집합입니다.
        std::size_t validateType(const Json &types,
            const std::string &type,
            std::set<std::string> &active) const;

        // 다음 정렬 경계까지 필요한 0바이트 개수를 계산합니다. 예: offset=3, alignment=4 -> 1.
        static std::size_t paddingSize(std::size_t offset, std::size_t alignment);

        // 현재 바이트 위치를 count만큼 이동합니다. 메시지 최대 크기를 넘으면 오류입니다.
        static void advanceOffset(std::size_t &offset, std::size_t count);

        // 구조체 경계의 정렬 요구를 다음 필드에 합칩니다. 실제 패딩은 여기서 만들지 않습니다.
        void mergeAlignment(std::size_t alignment);

        // alignment은 최대 필드 정렬입니다. uint16_t는 최대 2, double은 최대 8을 사용합니다.
        std::size_t valueAlignment(const std::string &type, const Json &types);

        std::size_t structureAlignment(const Json &fields, const Json &types);

        // payload가 미설정이면 길이 0으로 계산합니다. 덤프는 INCOMPLETE 상태를 별도 표시합니다.
        std::size_t calculateSize(bool includeValues = true) const;

        // 필드 하나를 내부 정의로 바꿉니다. 구조체라면 자식으로 내려가 말단 필드까지 펼칩니다.
        void expandValue(const Json &entry,
            const Json &types,
            const std::string &path,
            const std::string &parent,
            bool inheritedBig);

        // 구조체/배열을 펼쳐 _fields와 _containers를 만듭니다. 비트필드의 같은 저장 단위도 묶습니다.
        void expandFields(const Json &fields, const Json &types, const std::string &prefix, bool big);

        // 필드 값 하나를 전송 바이트로 바꿉니다. 정수 범위와 버퍼 길이를 여기서도 검사합니다.
        static std::vector<std::uint8_t> encode(const Field &definition, const Json &value);

        // 배열과 구조체는 자식으로 내려가며 설정합니다. 임시 메시지에서 작업하고 set() 마지막에 값을 반영합니다.
        void collectSet(const std::string &name, const Json &value);

        // 설정 실패 이유를 로그에 남깁니다. 로그 함수가 실패해도 원래 오류가 사라지지 않게 합니다.
        void logSetFailure(const std::string &name, const std::exception &error) const;

        // 현재 헤더 코드가 이 메시지 정의의 msgcode와 같은지 확인합니다.
        void checkMessageCode() const;

        // 링크 정의에서 정한 코드 필드와 기대값을 기억하고 초기 코드 값을 설정합니다.
        void configureMessageCode(const std::string &name, std::uint64_t code);

        // 가변 버퍼의 실제 크기와 연결된 length 필드가 맞는지 검사합니다. 수동 길이는 전송 전에 최종 확인합니다.
        void checkLengths(bool requireManualLengths = true) const;

        // 말단 필드는 현재 값을 읽고, 구조체/배열이면 자식 값을 모아 JSON 하나로 반환합니다.
        Json readValue(const std::string &name) const;

        // 바이트에서 필드 하나를 읽습니다. 엔디안/부호/실수/비트 위치를 반영해 JSON 값으로 돌려줍니다.
        static Json decodeScalar(const Field &definition,
            const std::vector<std::uint8_t> &data,
            std::size_t offset,
            std::size_t count);

    private:
        // 코드 필드의 경로입니다. 예: header.msgcode.
        std::string _codeField;
        // 이 메시지가 가져야 하는 msgcode입니다.
        std::uint64_t _expectedCode = 0;

        // 이 메시지가 속한 링크 이름입니다.
        std::string _linkName;
        // 이 메시지 정의의 이름입니다.
        std::string _messageName;
        // 필드의 enum 타입을 검사하고 덤프에 이름을 표시하는 공통 정의입니다.
        EnumDefinitions _enums;
        // 정의와 현재 값을 함께 가진 말단 필드 목록입니다. 예: header.id, body.points[0].x.
        std::vector<Field> _fields;
        // 정의를 펼치는 동안에는 다음 필드의 정렬, 완료 후에는 메시지 끝 정렬입니다.
        // 1/2/4/8 경계는 큰 경계 하나로 합칠 수 있어 별도 배치 목록이 필요 없습니다.
        std::size_t _endAlignment = 1;
        // JSON에서 선택한 최대 정렬 단위입니다. 기본 1은 패딩 없는 배치입니다.
        std::size_t _alignment = 1;
        // 이미 계산한 구조체 타입의 정렬 크기입니다. 같은 계산을 반복하지 않습니다.
        std::map<std::string, std::size_t> _typeAlignments;
        // 부모 경로 -> 자식 경로 목록입니다. 구조체/배열 전체 set/get에 사용합니다.
        // std::map<Key, Value>는 키로 값을 찾는 사전입니다. set이 '키만' 보관한다면 map은 '키와 값'을 보관합니다.
        // 예: "body.position" → 그 구조체의 자식 경로 ["body.position.x", "body.position.y"].
        // find로 찾고 end와 같으면 없음입니다. operator[]는 없는 키의 값을 새로 만들 수 있어 읽기에는 주의합니다.
        std::map<std::string, Container> _containers;
        // 가변 버퍼가 0바이트일 때의 최소 메시지 크기입니다.
        std::size_t _fixedSize = 0;
        // 현재 구조체를 펼치는 깊이입니다. 잘못된 과도한 중첩을 막습니다.
        std::size_t _expansionDepth = 0;
        // 이미 검사한 타입의 중첩 깊이입니다. 검사를 반복하지 않는 용도입니다.
        // mutable은 const 함수 안에서도 이 멤버를 갱신할 수 있게 합니다.
        // 여기서는 메시지 값이 아니라 '이미 검사한 타입의 깊이'를 기억하는 캐시만 갱신합니다.
        // const가 자동으로 스레드 안전을 보장하는 것은 아닙니다. 같은 객체를 동시에 바꾸는 사용에는 동기화가 필요합니다.
        mutable std::map<std::string, std::size_t> _validatedTypeDepths;
        // 오류가 발생할 때 호출할 일반 함수 주소입니다. nullptr은 출력 안 함입니다.
        LogHandler _logHandler = defaultLog;
    };
}
