#pragma once

#include "MessageManager.hpp"

namespace message_manager
{
    // interface_message.json에 기록한 규칙으로 링크 사이의 메시지를 변환합니다.
    class MessageInterface
    {
    public:
        // 변환 설정을 읽어 규칙을 준비합니다. catalog가 주어지면 대상 메시지도 스스로 만들 수 있습니다.
        explicit MessageInterface(const Json &definition);

        MessageInterface(const Json &definition, const MessageManager &manager);

        // 별도 JSON {"mappings": {"name": {"values": [...]}}}을 전달합니다.
        MessageInterface(const Json &definition, const Json &mappingDefinitions);

        MessageInterface(const Json &definition, const Json &mappingDefinitions,
            const MessageManager &manager);

        static MessageInterface fromFile(const std::string &path, const MessageManager &manager);

        // 원본의 링크/메시지에 연결된 유일한 규칙으로 새 대상 메시지를 반환합니다.
        Message convert(const Message &source) const;

        // 설정된 변환 규칙으로 원본 값을 읽어 대상 메시지를 만듭니다. 원본은 수정하지 않습니다.
        Message convert(const std::string &ruleName, const Message &source) const;

        static MessageInterface fromFile(const std::string &path);

        const std::string &sourceLink(const std::string &ruleName) const;

        const std::string &sourceMessage(const std::string &ruleName) const;

        // 원본과 대상을 호출부에서 명확히 지정합니다. 성공한 경우에만 대상에 반영합니다.
        void convertInto(const std::string &ruleName, const Message &source, Message &target) const;

        // 대상의 임시 복사본에 변환합니다. 전부 성공한 뒤에만 실제 대상에 반영합니다.
        void convertInto(const Message &source, Message &target) const;

        // 설정된 변환 규칙으로 원본 값을 읽어 대상 메시지를 만듭니다. 원본은 수정하지 않습니다.
        Message convert(const std::string &ruleName,
            const Message &source,
            const MessageManager &manager) const;

        // 수신 바이너리를 원본 정의로 해석한 뒤 변환하고 싶을 때 사용합니다.
        Message convertBytes(const std::string &ruleName,
            const std::vector<std::uint8_t> &bytes,
            const MessageManager &manager) const;

    protected:

    private:
        struct FieldMapping
        {
            // 읽을 원본 필드의 전체 경로입니다.
            std::string _sourceField;
            // 기록할 대상 필드의 전체 경로입니다.
            std::string _targetField;
            // true이면 원본 값을 복사하지 않고 _fixedValue를 대상에 설정합니다.
            bool _fixed = false;
            // 대상 필드에 설정할 상수입니다.
            Json _fixedValue;
            // from/to 값 쌍을 보관하는 필드 매핑표입니다.
            Json _values = Json::array();
            // 매핑표에 입력이 없을 때 동작입니다: copy, default, error.
            std::string _unmatched = "copy";
            // unmatched=default일 때 사용할 값입니다.
            Json _defaultValue;

        };

        struct Rule
        {
            std::string _name;
            // 원본 링크 이름입니다.
            std::string _sourceLink;
            // 원본 메시지 이름입니다.
            std::string _sourceMessage;
            // 대상 링크 이름입니다.
            std::string _targetLink;
            // 대상 메시지 이름입니다.
            std::string _targetMessage;
            // 이 변환 규칙에 들어 있는 필드별 매핑 목록입니다.
            std::vector<FieldMapping> _mappings;
            // 변환 중 대상 필드에 지정할 상수 값 목록입니다.
            Json _constants;
        };

        void initialize(const Json &definition, const Json &mappingDefinitions);

        // 필드 항목 하나에서 원본/대상/상수 여부를 읽습니다.
        static FieldMapping readFieldMapping(const Json &mappingEntry,
            const std::map<std::string, FieldMapping> &tables);

        // 이름 -> 매핑표 목록을 만들어 반환합니다. 메시지 변환은 아직 하지 않습니다.
        static std::map<std::string, FieldMapping> loadMappingTables(const Json &mappingDefinitions);

        // 변환 규칙을 검사하고 _rules에 저장합니다.
        void loadRules(const Json &definition, const std::map<std::string, FieldMapping> &tables);

        // 허용한 JSON 키 목록과 실제 키를 비교합니다. 오타가 조용히 무시되는 일을 막습니다.
        static void checkKeys(const Json &object, std::initializer_list<const char *> allowed);

        // JSON에서 비어 있지 않은 문자열 이름을 꺼냅니다.
        static std::string readName(const Json &object, const std::string &key);

        const Rule &findRule(const std::string &ruleName) const;

        // 정수는 부호/폭에 관계없이 비교하되 정수와 실수는 구분합니다.
        static bool sameValue(const Json &a, const Json &b);

        // 매핑표에 넣을 수 있는 단일 값인지 확인합니다. 배열/객체/무한대는 거부합니다.
        static void checkScalar(const Json &value);

        // 매핑 하나의 from/to 목록과 미일치 처리(copy/default/error)를 읽습니다.
        static void configureMapping(FieldMapping &mapping, const Json &entry);

        // interface JSON과 같은 폴더를 기준으로 mapping_file 경로를 해석해 읽습니다.
        static Json readMappingFile(const Json &definition, const std::string &path);

        // 입력과 일치하는 from을 찾아 to를 반환합니다. 일치하지 않으면 unmatched 설정을 따릅니다.
        static Json mapValue(const FieldMapping &mapping, const Json &source);

    private:
        // 변환 규칙 목록입니다. 각 규칙은 원본/대상과 필드 매핑을 가집니다.
        std::vector<Rule> _rules;
        // 메시지 정의의 복사본을 소유하여 원래 manager가 사라져도 안전하게 대상 메시지를 생성합니다.
        // 새 메시지를 만들 때 사용할 링크/메시지 정의 목록입니다.
        MessageManager _manager;
        // _manager를 사용자가 제공했는지 표시합니다. true일 때 대상 메시지 자동 생성이 가능합니다.
        bool _hasManager = false;
    };
}
