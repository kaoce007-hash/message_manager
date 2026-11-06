#include "MessageManager.hpp"
#include "MessageInterface.hpp"

namespace message_manager
{
    // 빈 관리자를 만들거나 JSON 정의의 링크를 등록합니다. 메시지 객체를 직접 소유합니다.
    MessageManager::MessageManager()
    {
    }

    // 빈 관리자를 만들거나 JSON 정의의 링크를 등록합니다. 메시지 객체를 직접 소유합니다.
    MessageManager::MessageManager(const Json &definition, const EnumDefinitions &enums)
        : _enums(enums)
    {
        addLink(definition);
    }

    // 공통 enum 파일을 읽습니다. enum을 사용하는 링크보다 먼저 호출합니다.
    void MessageManager::loadEnums(const std::string &path)
    {
        if (!_links.empty())
        {
            fail("load enum JSON before link JSON files");
        }
        _enums.load(path);
    }

    // JSON 파일을 읽고 addLink를 통해 실제 메시지 객체들을 Manager에 직접 등록합니다.
    std::string MessageManager::loadLink(const std::string &path)
    {
        const Json definition = readJsonFile(path);
        const std::string loadedName = addLink(definition);
        return loadedName;
    }

    // JSON 정의로 메시지들을 임시 생성하고 전체 검사가 성공한 경우에만 Manager에 등록합니다.
    std::string MessageManager::addLink(const Json &input)
    {
        // 1. 링크 이름·dispatch·messages 형식과 중복 링크를 검사합니다.
        // 2. 각 메시지를 임시 prepared에 만들고 코드 중복과 코드 위치를 검사합니다.
        // 3. 모든 메시지가 정상일 때 _links와 _messages에 등록합니다.

        Json definition = input;
        checkKeys(definition, {"link", "endian", "alignment", "dispatch", "types", "messages"});
        LinkInfo link;
        const Json & linkJson = definition.at("link");
        link._name = linkJson.get<std::string>();
        if (link._name.empty())
        {
            fail("link name must not be empty");
        }
        for (const LinkInfo &existing : _links)
        {
            if (existing._name == link._name)
            {
                fail("duplicate link: " + link._name);
            }
        }
        if (definition.contains("dispatch"))
        {
            const Json &dispatch = definition.at("dispatch");
            checkKeys(dispatch, {"field"});
            const Json & fieldJson = dispatch.at("field");
            link._codeField = fieldJson.get<std::string>();
            if (link._codeField.empty())
            {
                fail("dispatch.field must not be empty");
            }
        }
        // [메시지 목록 준비] 링크 메타정보 검사 후 실제 메시지 정의들을 읽습니다.
        const Json &entries = definition.at("messages");
        bool matchesCondition = !entries.is_object();
        if (!matchesCondition)
        {
            matchesCondition = entries.empty();
        }
        if (matchesCondition)
        {
            fail("messages must be a nonempty object");
        }
        // 먼저 임시 목록에 전부 만듭니다. 한 메시지라도 실패하면 현재 Manager는 바뀌지 않습니다.
        // std::list<T>는 원소가 연결된 목록입니다. vector와 달리 연속 메모리일 필요가 없습니다.
        // 마지막 splice에서 원소를 다시 복사하지 않고 목록 연결을 옮기려고 list를 사용합니다.
        std::list<Message> prepared;
        // 이 set의 내용은 이 링크에서 이미 사용한 msgcode들입니다.
        // 예: 1001을 넣은 뒤 다시 1001을 넣으면 insert(...).second가 false여서 중복을 거부합니다.
        std::set<std::uint64_t> codes;
        std::size_t typeNumber = 0;
        bool first = true;
        // JSON을 읽으면서 types가 추가될 수 있으므로 messages만 따로 복사해 순회합니다.
        const Json messageEntries = entries;
        for (Json::const_iterator item = messageEntries.begin(); item != messageEntries.end(); ++item)
        {
            const std::string itemName = item.key();
            if (itemName.empty())
            {
                fail("message name must not be empty");
            }
            const std::string itemName2 = item.key();
            const Json & itemValue = item.value();
            Message next = makeMessage(definition, link, itemName2, itemValue, typeNumber);
            if (!link._codeField.empty())
            {
                const std::uint64_t code = next._expectedCode;
                const bool inserted = codes.insert(code).second;
                if (!inserted)
                {
                    fail("duplicate msgcode in link: " + link._name);
                }
                checkCodeLocation(link, next, first);
            }
            prepared.push_back(next);
            first = false;
        }
        // [등록 확정] 이 지점은 모든 메시지와 코드 검사가 끝난 뒤에만 도달합니다.
        _links.push_back(link);
        std::list<Message>::iterator insertionPoint = _messages.end();
        // 준비한 메시지들을 기존 메시지 주소를 바꾸지 않고 목록에 연결합니다.
        // splice(삽입 위치, 다른 목록)는 다른 list의 노드를 이 목록으로 이동합니다. 이동 후 prepared는 비어 있습니다.
        // 기존 _messages 원소를 재배치하지 않아, 사용자가 이미 받아 둔 Message& 참조가 유지됩니다.
        _messages.splice(insertionPoint, prepared);
        return link._name;
    }

    // Manager가 직접 보관하는 목록에서 링크 이름과 메시지 이름이 같은 객체를 찾아 참조를 반환합니다.
    Message &MessageManager::message(const std::string &linkName, const std::string &messageName)
    {
        // 흐름: 관리 중인 메시지를 순회 → 링크 이름과 메시지 이름이 모두 같은 원본의 참조 반환.

        // 범위 기반 for(C++11)는 목록을 처음부터 끝까지 순회합니다.
        // Message &current의 &는 복사본이 아닌 원소 자체를 뜻합니다. 반환한 참조로 set하면 관리자 안의 값이 바뀝니다.
        for (Message &current : _messages)
        {
            bool matchesCondition = current.linkName() == linkName;
            if (matchesCondition)
            {
                matchesCondition = current.messageName() == messageName;
            }
            if (matchesCondition)
            {
                return current;
            }
        }
        fail("unknown message: " + linkName + "." + messageName);
    }

    // Manager가 직접 보관하는 목록에서 링크 이름과 메시지 이름이 같은 객체를 찾아 참조를 반환합니다.
    const Message &MessageManager::message(const std::string &linkName, const std::string &messageName) const
    {
        for (const Message &current : _messages)
        {
            bool matchesCondition = current.linkName() == linkName;
            if (matchesCondition)
            {
                matchesCondition = current.messageName() == messageName;
            }
            if (matchesCondition)
            {
                return current;
            }
        }
        fail("unknown message: " + linkName + "." + messageName);
    }

    // 등록된 메시지를 찾고 emptyCopy로 사용자 값을 비운 독립 객체를 반환합니다.
    Message MessageManager::create(const std::string &linkName, const std::string &messageName) const
    {
        // 흐름: 등록된 원본 찾기 → 정의와 msgcode만 가진 emptyCopy 생성 → 독립 객체 반환.

        const Message &definition = message(linkName, messageName);
        Message result = definition.emptyCopy();
        return result;
    }

    // 등록된 메시지를 찾고 emptyCopy로 사용자 값을 비운 독립 객체를 반환합니다.
    Message MessageManager::create(const std::string &messageName) const
    {
        if (_links.size() != 1)
        {
            fail("specify link name when zero or multiple links are loaded");
        }
        const LinkInfo &onlyLink = _links.front();
        const std::string linkName = onlyLink._name;
        Message result = create(linkName, messageName);
        return result;
    }

    // msgcode 판별 설정을 가진 링크가 하나라도 있는지 확인합니다.
    bool MessageManager::canIdentify() const
    {
        for (const LinkInfo &link : _links)
        {
            if (!link._codeField.empty())
            {
                return true;
            }
        }
        return false;
    }

    // 오류 로그를 받을 일반 함수의 주소를 등록합니다. nullptr이면 로그 출력을 끕니다.
    void MessageManager::setLogHandler(const LogHandler &handler)
    {
        _logHandler = handler;
        for (Message &current : _messages)
        {
            current.setLogHandler(handler);
        }
    }

    // 설정된 변환 규칙으로 원본 값을 읽어 대상 메시지를 만듭니다. 원본은 수정하지 않습니다.
    Message &MessageManager::convert(const MessageInterface &converter, const std::string &ruleName)
    {
        const std::string sourceLink = converter.sourceLink(ruleName);
        const std::string sourceName = converter.sourceMessage(ruleName);
        const Message &source = message(sourceLink, sourceName);
        Message converted = converter.convert(ruleName, source, *this);
        converted.setLogHandler(_logHandler);
        const std::string targetLink = converted.linkName();
        const std::string targetName = converted.messageName();
        Message &target = message(targetLink, targetName);
        target = converted;
        return target;
    }

    // 수신 데이터를 메시지에 반영합니다. 코드 자동 판별 버전은 identify를 먼저 거칩니다.
    void MessageManager::receive(const std::string &linkName, const std::string &messageName,
        const std::vector<std::uint8_t> &bytes)
    {
        Message &target = message(linkName, messageName);
        target.deserialize(bytes);
    }

    // 수신 데이터를 메시지에 반영합니다. 코드 자동 판별 버전은 identify를 먼저 거칩니다.
    MessageIdentification MessageManager::receive(const std::string &linkName,
        const std::vector<std::uint8_t> &bytes)
    {
        // 흐름: 헤더로 메시지 식별 → 해당 Message에 전체 바이트 해석 → 식별 정보 반환.

        const MessageIdentification detected = identify(linkName, bytes);
        Message &target = message(linkName, detected._messageName);
        target.deserialize(bytes);
        return detected;
    }

    // 수신 데이터를 메시지에 반영합니다. 코드 자동 판별 버전은 identify를 먼저 거칩니다.
    MessageIdentification MessageManager::receive(const std::vector<std::uint8_t> &bytes)
    {
        const MessageIdentification detected = identify(bytes);
        Message &target = message(detected._linkName, detected._messageName);
        target.deserialize(bytes);
        return detected;
    }

    // 수신 데이터를 메시지에 반영합니다. 코드 자동 판별 버전은 identify를 먼저 거칩니다.
    MessageIdentification MessageManager::receive(const std::string &linkName, const void *data, std::size_t length)
    {
        if (length > MaxMessageSize || (data == nullptr && length != 0))
        {
            fail("invalid receive buffer or size");
        }
        std::vector<std::uint8_t> bytes;
        if (length != 0)
        {
            const std::uint8_t *first = static_cast<const std::uint8_t *>(data);
            bytes.assign(first, first + length);
        }
        MessageIdentification result = receive(linkName, bytes);
        return result;
    }

    // 지정된 헤더 위치의 msgcode를 읽어 링크/메시지 이름을 찾습니다. 본문 전체 해석은 하지 않습니다.
    MessageIdentification MessageManager::identify(const std::string &linkName,
        const std::vector<std::uint8_t> &bytes) const
    {
        // 흐름: 링크의 코드 위치 확보 → 수신 길이 확인 → 엔디안/비트 위치로 코드 복원 → 해당 메시지 검색.

        const LinkInfo &link = findLink(linkName);
        if (link._codeField.empty())
        {
            fail("link has no dispatch definition: " + linkName);
        }
        const Message::UnsignedFieldLocation &location = link._codeLocation;
        bool matchesCondition = bytes.size() > MaxMessageSize;
        if (!matchesCondition)
        {
            matchesCondition = bytes.size() < location._offset + location._size;
        }
        if (matchesCondition)
        {
            fail("invalid binary size or truncated msgcode header: " + linkName);
        }
        std::uint64_t code = 0;
        for (std::size_t index = 0; index < location._size; ++index)
        {
            std::size_t byteIndex = index;
            if (location._big)
            {
                byteIndex = location._size - 1 - index;
            }
            const std::uint8_t byte = bytes[location._offset + index];
            // 해당 비트 위치의 값을 기존 저장 단위에 합칩니다.
            code |= std::uint64_t(byte) << (byteIndex * 8);
        }
        if (location._bitWidth != 0)
        {
            // 유효 비트만 남길 수 있도록 비트 마스크를 계산합니다.
            const std::uint64_t mask = UINT64_MAX >> (64 - location._bitWidth);
            // 유효 비트만 남길 수 있도록 비트 마스크를 계산합니다.
            code = (code >> location._bitOffset) & mask;
        }
        // 별도의 코드→이름 사전 대신 실제 메시지 목록에서 직접 찾습니다.
        for (const Message &current : _messages)
        {
            bool matchesCondition2 = current.linkName() == linkName;
            if (matchesCondition2)
            {
                matchesCondition2 = current._expectedCode == code;
            }
            if (matchesCondition2)
            {
                MessageIdentification result;
                result._linkName = linkName;
                result._messageName = current.messageName();
                result._msgcode = code;
                return result;
            }
        }
        const std::string numberText = std::to_string(code);
        fail("unknown msgcode " + numberText + " in link: " + linkName);
    }

    // 지정된 헤더 위치의 msgcode를 읽어 링크/메시지 이름을 찾습니다. 본문 전체 해석은 하지 않습니다.
    MessageIdentification MessageManager::identify(const std::vector<std::uint8_t> &bytes) const
    {
        // 흐름: 판별 가능한 모든 링크를 시험 → 후보 없음/여러 개면 실패 → 유일한 후보만 반환.

        if (bytes.size() > MaxMessageSize)
        {
            fail("received message exceeds 16 MiB");
        }
        std::vector<MessageIdentification> matches;
        for (const LinkInfo &link : _links)
        {
            if (link._codeField.empty())
            {
                continue;
            }
            try
            {
                const MessageIdentification found = identify(link._name, bytes);
                matches.push_back(found);
            }
            catch (const std::runtime_error &)
            {
                // 이 링크의 코드가 아니면 다음 링크를 확인합니다.
            }
        }
        if (matches.empty())
        {
            fail("no link/message matches the received msgcode");
        }
        if (matches.size() != 1)
        {
            fail("ambiguous msgcode across links; specify the receiving link");
        }
        const MessageIdentification result = matches.front();
        return result;
    }

    // 관리 중인 모든 메시지의 현재 값을 표로 모읍니다. 미설정 필드도 표시합니다.
    std::string MessageManager::dumpAll(std::size_t maxHexBytes) const
    {
        std::ostringstream output;
        output << "MESSAGES: ";
        output << _messages.size();
        output << "\n\n";
        for (const Message &current : _messages)
        {
            const std::string text = current.dump(maxHexBytes);
            output << text;
            output << '\n';
        }
        const std::string result = output.str();
        return result;
    }

    // 관리 중인 모든 메시지의 현재 값을 표로 모읍니다. 미설정 필드도 표시합니다.
    void MessageManager::dumpAll(std::ostream &output, std::size_t maxHexBytes) const
    {
        const std::string text = dumpAll(maxHexBytes);
        output << text;
    }

    void MessageManager::dumpAllToFile(const std::string &path, std::size_t maxHexBytes) const
    {
        const std::string text = dumpAll(maxHexBytes);
        writeTextFile(path, text);
    }

    const MessageManager::LinkInfo &MessageManager::findLink(const std::string &linkName) const
    {
        for (const LinkInfo &link : _links)
        {
            if (link._name == linkName)
            {
                return link;
            }
        }
        fail("unknown link: " + linkName);
    }

    // Manager 정의를 복사하고 사용자 값을 비웁니다. 변환기가 원래 Manager의 수명에 의존하지 않게 합니다.
    MessageManager MessageManager::copyDefinitions() const
    {
        MessageManager result = *this;
        for (Message &current : result._messages)
        {
            Message empty = current.emptyCopy();
            current = empty;
        }
        return result;
    }

    // 같은 링크에 속한 메시지들의 msgcode 위치와 타입/엔디안/비트 위치가 같은지 검사합니다.
    void MessageManager::checkCodeLocation(LinkInfo &link, const Message &message, bool first)
    {
        const Message::UnsignedFieldLocation location = message.unsignedFieldLocation(link._codeField);
        if (first)
        {
            link._codeLocation = location;
        }
        else
        {
            if (location._offset != link._codeLocation._offset || location._size != link._codeLocation._size ||
                location._big != link._codeLocation._big || location._bitWidth != link._codeLocation._bitWidth ||
                location._bitOffset != link._codeLocation._bitOffset)
            {
                fail("all messages in a link must share msgcode offset, size and endian: " + link._name);
            }
        }
    }

    // 메시지 항목 하나의 루트 타입과 코드를 읽고 Message 객체 하나를 생성합니다.
    Message MessageManager::makeMessage(Json &definition, const LinkInfo &link, const std::string &name,
        const Json &entry, std::size_t &typeNumber) const
    {
        std::string rootType;
        std::uint64_t code = 0;
        if (entry.is_string())
        {
            rootType = entry.get<std::string>();
            if (!link._codeField.empty())
            {
                fail("dispatch requires type/msgcode objects for every message");
            }
        }
        else
        {
            if (entry.contains("type"))
            {
                checkKeys(entry, {"type", "msgcode"});
                const Json & typeJson = entry.at("type");
                rootType = typeJson.get<std::string>();
            }
            else
            {
                rootType = inlineMessageType(definition, entry, typeNumber);
            }
            if (!link._codeField.empty())
            {
                const Json & msgcodeJson = entry.at("msgcode");
                code = unsignedValue(msgcodeJson);
            }
            else
            {
                if (entry.contains("msgcode"))
                {
                    fail("msgcode mapping requires dispatch.field");
                }
            }
        }
        if (rootType.empty())
        {
            fail("message root type must not be empty");
        }
        Json schema = Json::object();
        schema["message"] = rootType;
        const std::string string90 = std::string("big");
        schema["endian"] = definition.value("endian", string90);
        const Json jsonNumber = Json(1);
        schema["alignment"] = definition.value("alignment", jsonNumber);
        schema["types"] = definition.at("types");
        Message result(schema, link._name, name, _enums);
        if (!link._codeField.empty())
        {
            result.configureMessageCode(link._codeField, code);
        }
        result.setLogHandler(_logHandler);
        return result;
    }
    // 중첩 인라인 구조체에 중복되지 않는 이름을 붙여 JSON types 목록에 추가합니다.
    std::string MessageManager::addInlineType(Json &definition, const Json &fields, std::size_t &typeNumber, std::size_t depth)
    {
        // 흐름: 구조체의 fields 검사 → 중첩 inline struct를 먼저 이름 있는 타입으로 정리 → types에 새 타입 등록.

        bool matchesCondition = depth > 64;
        if (!matchesCondition)
        {
            bool matchesCondition2 = !fields.is_array();
            if (!matchesCondition2)
            {
                matchesCondition2 = fields.empty();
            }
            matchesCondition = matchesCondition2;
        }
        if (matchesCondition)
        {
            fail("inline fields must be nonempty and nesting must not exceed 64");
        }
        if (!definition.contains("types"))
        {
            definition["types"] = Json::object();
        }
        const Json & typesJson = definition.at("types");
        if (!typesJson.is_object())
        {
            fail("types must be an object");
        }
        Json normalized = Json::array();
        for (const Json &field : fields)
        {
            Json next = field;
            const Json & typeJson = next.at("type");
            const std::string type = typeJson.get<std::string>();
            if (type == "struct")
            {
                const Json & fieldsJson = next.at("fields");
                // 인라인 구조체에 이름을 붙여 공통 타입 목록에 추가합니다.
                const std::string child = addInlineType(definition, fieldsJson, typeNumber, depth + 1);
                next.erase("fields");
                next["type"] = child;
            }
            else
            {
                if (next.contains("fields"))
                {
                    fail("nested fields require type=struct");
                }
            }
            normalized.push_back(next);
        }
        std::string typeName;
        do
        {
            typeName = "InlineStruct_" + std::to_string(typeNumber++);
        }
        while (definition["types"].contains(typeName));
        definition["types"][typeName] = Json::object();
        definition["types"][typeName]["fields"] = normalized;
        return typeName;
    }

    // 메시지 안에 직접 적은 fields를 내부 구조체 타입 하나로 정규화합니다.
    std::string MessageManager::inlineMessageType(Json &definition, const Json &entry, std::size_t &typeNumber)
    {
        checkKeys(entry, {"header", "fields", "msgcode"});
        Json fields = Json::array();
        if (entry.contains("header"))
        {
            const Json &header = entry.at("header");
            checkKeys(header, {"fields", "endian"});
            Json headerField = Json::object();
            headerField["name"] = "header";
            headerField["type"] = "struct";
            headerField["fields"] = header.at("fields");
            if (header.contains("endian"))
            {
                headerField["endian"] = header.at("endian");
            }
            fields.push_back(headerField);
        }
        const Json & fieldsJson = entry.at("fields");
        if (!fieldsJson.is_array())
        {
            fail("message fields must be an array");
        }
        for (const Json &field : entry.at("fields"))
        {
            fields.push_back(field);
        }
        // 인라인 구조체에 이름을 붙여 공통 타입 목록에 추가합니다.
        const std::string typeName = addInlineType(definition, fields, typeNumber, 1);
        return typeName;
    }
}
