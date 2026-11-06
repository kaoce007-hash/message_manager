#pragma once

#include "BinaryMessage.hpp"
#include <list>

namespace message_manager
{
    class MessageInterface;

    // 수신 코드로 알아낸 결과입니다. 처리 기능이 없는 단순 데이터 묶음입니다.
    struct MessageIdentification
    {
        // 이 메시지가 소속된 링크 이름입니다.
        std::string _linkName;
        // 이 메시지의 정의 이름입니다.
        std::string _messageName;
        // 수신 헤더에서 알아낸 메시지 코드입니다.
        std::uint64_t _msgcode = 0;
    };

    /*
     * 전체 구조
     *   사용자 → MessageManager → Message
     *                  ↓
     *           MessageInterface (다른 메시지로 변환할 때만 사용)
     *
     * Manager가 JSON을 읽고 메시지를 직접 보관합니다.
     * 이전의 Catalog와 Link라는 중간 클래스는 없습니다.
     */
    class MessageManager
    {
    public:
        // 빈 관리자 또는 링크 하나를 등록한 관리자를 만듭니다.
        MessageManager();
        // 빈 관리자 또는 링크 하나를 등록한 관리자를 만듭니다.
        explicit MessageManager(const Json &definition,
            const EnumDefinitions &enums = EnumDefinitions());

        // 공통 enum 파일을 먼저 읽습니다. 링크 등록 전에 호출합니다.
        void loadEnums(const std::string &path);
        // JSON 파일을 읽어 실제 메시지들을 직접 등록하고 링크 이름을 반환합니다.
        std::string loadLink(const std::string &path);
        // 이미 읽은 JSON을 등록합니다. 전부 검사한 다음 한 번에 반영합니다.
        std::string addLink(const Json &definition);

        Message &message(const std::string &linkName, const std::string &messageName);
        const Message &message(const std::string &linkName, const std::string &messageName) const;

        // 정의와 msgcode만 가진 독립 메시지를 새로 만듭니다. 현재 값은 복사하지 않습니다.
        Message create(const std::string &linkName, const std::string &messageName) const;
        // 정의와 msgcode만 가진 독립 메시지를 새로 만듭니다. 현재 값은 복사하지 않습니다.
        Message create(const std::string &messageName) const;
        // msgcode로 수신 메시지를 구분할 수 있는 링크가 있는지 확인합니다.
        bool canIdentify() const;

        // 오류를 출력할 일반 함수 주소를 등록합니다. nullptr이면 출력하지 않습니다.
        void setLogHandler(const LogHandler &handler);
        Message &convert(const MessageInterface &converter, const std::string &ruleName);

        // 바이트를 대상 Message로 읽습니다. 이름을 생략하면 msgcode로 자동 판별합니다.
        void receive(const std::string &linkName, const std::string &messageName,
            const std::vector<std::uint8_t> &bytes);
        // 바이트를 대상 Message로 읽습니다. 이름을 생략하면 msgcode로 자동 판별합니다.
        MessageIdentification receive(const std::string &linkName, const std::vector<std::uint8_t> &bytes);
        // 바이트를 대상 Message로 읽습니다. 이름을 생략하면 msgcode로 자동 판별합니다.
        MessageIdentification receive(const std::vector<std::uint8_t> &bytes);
        // 바이트를 대상 Message로 읽습니다. 이름을 생략하면 msgcode로 자동 판별합니다.
        MessageIdentification receive(const std::string &linkName, const void *data, std::size_t length);
        // 본문은 바꾸지 않고 헤더 코드로 링크와 메시지 이름을 알아냅니다.
        MessageIdentification identify(const std::string &linkName, const std::vector<std::uint8_t> &bytes) const;
        // 본문은 바꾸지 않고 헤더 코드로 링크와 메시지 이름을 알아냅니다.
        MessageIdentification identify(const std::vector<std::uint8_t> &bytes) const;

        // 관리 중인 전체 메시지를 표로 출력합니다. 미설정 값도 보입니다.
        std::string dumpAll(std::size_t maxHexBytes = 256) const;
        // 관리 중인 전체 메시지를 표로 출력합니다. 미설정 값도 보입니다.
        void dumpAll(std::ostream &output, std::size_t maxHexBytes = 256) const;
        // 전체 메시지 표를 텍스트 파일로 저장합니다.
        void dumpAllToFile(const std::string &path, std::size_t maxHexBytes = 256) const;

    protected:

    private:
        friend class MessageInterface;

        // 링크는 독립 클래스가 아니라 이 세 값만 가진 정보입니다.
        struct LinkInfo
        {
            std::string _name;
            std::string _codeField;
            Message::UnsignedFieldLocation _codeLocation;
        };

        const LinkInfo &findLink(const std::string &linkName) const;
        // 원본 Manager가 사라져도 사용할 수 있는 정의 복사본을 만듭니다.
        MessageManager copyDefinitions() const;
        // 인라인 구조체를 공통 types 형식으로 바꿔 저장합니다.
        static std::string addInlineType(Json &definition, const Json &fields,
            std::size_t &typeNumber, std::size_t depth);
        // 메시지 내부의 직접 작성한 fields를 루트 타입으로 바꿉니다.
        static std::string inlineMessageType(Json &definition, const Json &entry,
            std::size_t &typeNumber);
        // 링크의 모든 메시지가 같은 위치에서 코드를 읽는지 확인합니다.
        static void checkCodeLocation(LinkInfo &link, const Message &message, bool first);
        // 항목 하나를 해석해 Message 하나를 만듭니다. 목록 등록은 addLink가 합니다.
        Message makeMessage(Json &definition, const LinkInfo &link, const std::string &name,
            const Json &entry, std::size_t &typeNumber) const;

    private:
        EnumDefinitions _enums;
        std::vector<LinkInfo> _links;
        // list의 각 원소가 실제 메시지입니다. 링크를 추가해도 기존 메시지 참조가 유지됩니다.
        // vector는 커질 때 메모리를 옮길 수 있지만 list는 다른 원소를 추가해도 기존 원소 참조가 유지됩니다.
        // manager.message(...)로 받은 Message&를 새 링크 추가 후에도 사용할 수 있게 list를 선택했습니다.
        std::list<Message> _messages;
        LogHandler _logHandler = defaultLog;
    };
}
