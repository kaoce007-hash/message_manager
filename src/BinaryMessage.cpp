#include "BinaryMessage.hpp"

namespace message_manager
{

    // 메시지 정의를 검사하고 필드/배치 목록을 준비합니다. 이 시점에는 사용자 데이터가 없습니다.
    Message::Message(const Json &schema,
        const std::string &linkName,
        const std::string &messageName,
        const EnumDefinitions &enums)
        : _linkName(linkName), _messageName(messageName), _enums(enums)
    {
        // static_assert(C++11)는 실행 중이 아닌 컴파일 중에 조건을 검사합니다.
        // 8비트 바이트, 4바이트 float, 8바이트 double이 아니면 이 프로토콜의 가정이 깨지므로 빌드를 중단합니다.
        static_assert(CHAR_BIT == 8 && sizeof(float) == 4 && sizeof(double) == 8,
            "8-bit bytes and IEEE binary32/binary64 required");
        // is_iec559는 IEEE 754 실수 표현을 사용하는지 알려줍니다.
        // 상대 프로그램과 실수의 비트 배치를 맞추기 위해 저장 크기뿐 아니라 표현 방식도 검사합니다.
        static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559,
            "IEEE floating point required");
        checkKeys(schema, {"message", "endian", "fields", "types", "alignment"});
        const Json jsonNumber = Json(1);
        const Json alignmentSetting = schema.value("alignment", jsonNumber);
        const std::uint64_t requestedAlignment =
        unsignedValue(alignmentSetting);
        if (requestedAlignment != 1 && requestedAlignment != 2 && requestedAlignment != 4 &&
            requestedAlignment != 8)
        {
            fail("alignment must be 1, 2, 4 or 8");
        }
        _alignment = static_cast<std::size_t>(requestedAlignment);
        const std::string string123 = std::string("big");
        const std::string endianSetting = schema.value("endian", string123);
        const bool big = bigEndian(endianSetting);
        // active 집합은 지금 검사 중인 구조체 타입을 기억합니다. 타입이 자기 자신을 다시 포함하는 순환 정의를 찾습니다.
        std::set<std::string> active;
        if (schema.contains("types"))
        {
            if (schema.contains("fields"))
            {
                fail("use types or root fields, not both");
            }
            const Json &types = schema.at("types");
            bool matchesCondition = !types.is_object();
            if (!matchesCondition)
            {
                matchesCondition = types.empty();
            }
            if (matchesCondition)
            {
                fail("types must be a nonempty object");
            }
            for (Json::const_iterator type = types.begin(); type != types.end(); ++type)
            {
                const std::string itemName = type.key();
                bool matchesCondition2 = !identifier(itemName);
                if (!matchesCondition2)
                {
                    const std::string itemName2 = type.key();
                    matchesCondition2 = builtin(itemName2);
                }
                if (matchesCondition2)
                {
                    const std::string itemName3 = type.key();
                    fail("invalid struct type name: " + itemName3);
                }
                const std::string itemName4 = type.key();
                validateType(types, itemName4, active);
            }
            const Json & messageJson = schema.at("message");
            const std::string root = messageJson.get<std::string>();
            validateType(types, root, active);
            const Json & jsonValue = types.at(root);
            const Json & fieldsJson = jsonValue.at("fields");
            expandFields(fieldsJson, types, "", big);
        }
        else
        {
            bool matchesCondition3 = schema.contains("message");
            if (matchesCondition3)
            {
                const Json & messageJson2 = schema.at("message");
                matchesCondition3 = !messageJson2.is_string();
            }
            if (matchesCondition3)
            {
                fail("message must be string");
            }
            const Json &fields = schema.at("fields");
            const Json object130 = Json::object();
            // 구조체 안의 필드 목록과 중복 이름을 검사합니다.
            validateFields(fields, object130, active);
            const Json object131 = Json::object();
            expandFields(fields, object131, "", big);
        }
        calculateSize(false);
    }

    Message Message::fromFile(const std::string &path)
    {
        const Json definition = readJsonFile(path);
        Message result(definition);
        return result;
    }

    const std::string &Message::linkName() const
    {
        return _linkName;
    }

    const std::string &Message::messageName() const
    {
        return _messageName;
    }

    std::string Message::getLinkName() const
    {
        return _linkName;
    }

    std::string Message::getMessageName() const
    {
        return _messageName;
    }

    // 오류 로그를 받을 일반 함수의 주소를 등록합니다. nullptr이면 로그 출력을 끕니다.
    void Message::setLogHandler(const LogHandler &handler)
    {
        _logHandler = handler;
    }

    const LogHandler &Message::logHandler() const
    {
        return _logHandler;
    }

    void Message::set(const std::string &name, const void *data, std::size_t length)
    {
        std::vector<std::uint8_t> copied;
        try
        {
            const Field &definition = field(name);
            if (definition._type != "bytes")
            {
                fail("pointer input requires a char* field");
            }
            if ((data == nullptr && length != 0) || length > definition._maxLength)
            {
                fail("null buffer or length exceeds max_length");
            }
            if (length != 0)
            {
                const std::uint8_t *first = static_cast<const std::uint8_t *>(data);
                copied.assign(first, first + length);
            }
        }
        catch (const std::exception &error)
        {
            // 값 설정 실패 이유를 등록된 로그 함수에 전달합니다.
            logSetFailure(name, error);
            throw;
        }
        set(name, copied);
    }

    // 가변 버퍼 필드를 vector<char>로 복사해 반환합니다. 0x00도 일반 데이터로 보존합니다.

    // 가변 버퍼 필드를 vector<char>로 복사해 반환합니다. 0x00도 일반 데이터로 보존합니다.
    std::vector<char> Message::getBuffer(const std::string &name) const
    {
        if (field(name)._type != "bytes")
        {
            fail("getBuffer requires a char* field");
        }
        const std::vector<std::uint8_t> bytes = get<std::vector<std::uint8_t>>(name);
        const std::size_t bytesCount = bytes.size();
        std::vector<char> copied(bytesCount);
        if (!bytes.empty())
        {
            const std::size_t bytesCount2 = bytes.size();
            char *destinationBytes = copied.data();
            const std::uint8_t *sourceBytes = bytes.data();
            // 데이터 내용을 지정한 바이트 수만큼 복사합니다. 포인터 주소 자체를 전송하지 않습니다.
            std::memcpy(destinationBytes, sourceBytes, bytesCount2);
        }
        return copied;
    }

    // 정의와 msgcode는 유지하고 사용자 값은 비운 새 메시지를 만듭니다.
    Message Message::emptyCopy() const
    {
        Message result = *this;
        for (Field &current : result._fields)
        {
            current._value = nullptr;
        }
        if (!_codeField.empty())
        {
            Field &codeField = result.field(_codeField);
            codeField._value = _expectedCode;
        }
        return result;
    }

    // 필드 값을 JSON 형태로 읽습니다. get<T>와 달리 호출자가 C++ 타입을 지정하지 않습니다.
    Json Message::value(const std::string &name) const
    {
        const Json result = readValue(name);
        return result;
    }

    bool Message::hasField(const std::string &name) const
    {
        for (const Field &current : _fields)
        {
            if (current._name == name)
            {
                return true;
            }
        }
        const bool isContainer = _containers.count(name) != 0;
        return isContainer;
    }

    // JSON에서 선택한 메시지 정렬 단위(1/2/4/8)를 반환합니다.
    std::size_t Message::alignment() const
    {
        return _alignment;
    }

    // 가변 버퍼를 비워 둔 상태의 최소 크기를 반환합니다. 고정 필드와 패딩은 포함됩니다.
    std::size_t Message::fixedSize() const
    {
        const std::size_t result = calculateSize(false);
        return result;
    }

    std::size_t Message::size() const
    {
        const std::size_t result = calculateSize();
        return result;
    }

    // 정의된 배치 순서대로 패딩과 필드를 써서 전송용 바이트 배열을 만듭니다.
    std::vector<std::uint8_t> Message::serialize() const
    {
        // 1. 길이를 검사하고 _fields를 선언 순서대로 처리합니다.
        // 2. 각 필드 앞에서 필요한 만큼 0을 넣고 encode 결과를 이어 붙입니다.
        // 3. 비트 저장 단위를 공유하는 뒤 필드는 새 바이트 대신 기존 저장 단위에 OR로 합칩니다.

        // 버퍼 내용과 길이 필드가 맞는지 검사합니다.
        checkLengths();
        std::vector<std::uint8_t> output;
        const std::size_t expectedSize = size();
        // vector.reserve는 재할당을 줄이기 위해 용량만 확보합니다. 원소 수(size)는 늘리지 않습니다.
        // 반면 resize는 실제 원소 수를 바꿉니다. 여기서는 이후 insert로 바이트를 추가하므로 reserve를 씁니다.
        // [출력 공간 준비] 전송 길이를 미리 계산해 할당 횟수를 줄입니다.
        output.reserve(expectedSize);
        for (const Field &definition : _fields)
        {
            if (!definition._continuation)
            {
                const std::size_t outputCount = output.size();
                const std::size_t padding = paddingSize(outputCount, definition._alignment);
                const std::uint8_t uint8_t2 = std::uint8_t(0);
                std::vector<std::uint8_t>::iterator insertionPoint = output.end();
                output.insert(insertionPoint, padding, uint8_t2);
            }
            if (definition._value.is_null())
            {
                fail("missing value: " + definition._name);
            }
            // [필드 기록] 타입에 맞게 만든 바이트를 일반 필드면 추가하고, 공유 비트필드면 기존 위치에 합칩니다.
            std::vector<std::uint8_t> bytes = encode(definition, definition._value);
            if (definition._bitWidth)
            {
                bytes.clear();
                const Json fieldValue = definition._value;
                const std::uint64_t number = unsignedValue(fieldValue);
                const std::uint64_t shifted = number << definition._bitOffset;
                // 정수 비트를 엔디안 순서대로 바이트 목록에 붙입니다.
                appendBits(bytes, shifted, definition._size, definition._big);
            }
            if (definition._continuation)
            {
                const std::size_t start = output.size() - definition._size;
                for (std::size_t i = 0; i < bytes.size(); ++i)
                {
                    // 해당 비트 위치의 값을 기존 저장 단위에 합칩니다.
                    output[start + i] |= bytes[i];
                }
            }
            else
            {
                std::vector<std::uint8_t>::iterator destination = output.end();
                std::vector<std::uint8_t>::const_iterator firstByte = bytes.begin();
                std::vector<std::uint8_t>::const_iterator lastByte = bytes.end();
                // begin/end는 범위의 시작과 마지막 다음 위치입니다. [begin, end) 범위는 end를 포함하지 않습니다.
                // 여기서는 bytes의 전체 내용을 output 끝에 복사합니다. iterator 자체가 전송되는 것은 아닙니다.
                output.insert(destination, firstByte, lastByte);
            }
        }
        // 마지막 구조체의 끝 정렬도 실제 버퍼 위치에서 계산합니다.
        const std::size_t endPadding = paddingSize(output.size(), _endAlignment);
        output.insert(output.end(), endPadding, std::uint8_t(0));
        return output;
    }

    // 받은 바이트를 임시 값 목록으로 읽습니다. 길이/코드 검사가 모두 성공해야 현재 값에 반영합니다.
    void Message::deserialize(const std::vector<std::uint8_t> &data)
    {
        // 1. 빈 임시 메시지(candidate)를 만들고 현재 바이트 위치(offset)를 0에서 시작합니다.
        // 2. 각 필드의 정렬 크기로 패딩을 건너뛰고 candidate에만 값을 기록합니다.
        // 3. 남는 바이트와 msgcode를 검사한 뒤 성공한 _fields만 원본과 교환합니다.

        if (data.size() > MaxMessageSize)
        {
            fail("received message exceeds 16 MiB");
        }
        // [수신 작업본] 데이터가 잘려 있거나 코드가 틀려도 현재 메시지를 보존하도록 임시 객체에 읽습니다.
        Message candidate = emptyCopy();
        std::size_t offset = 0;
        for (const Field &definition : _fields)
        {
            if (!definition._continuation)
            {
                const std::size_t padding = paddingSize(offset, definition._alignment);
                if (padding > data.size() - offset)
                {
                    fail("binary message truncated in padding before: " + definition._name);
                }
                // 수신 패딩의 값은 무시합니다. 재직렬화할 때는 0으로 채웁니다.
                offset += padding;
            }
            std::size_t count = definition._size;
            if (definition._type == "bytes")
            {
                // 가변 버퍼 크기는 이미 수신해 둔 길이 필드에서 읽습니다. 송신 측이 보낸 길이를 따라야 필드 경계를 알 수 있습니다.
                const Field &lengthField = candidate.field(definition._lengthFrom);
                const std::uint64_t receivedLength = unsignedValue(lengthField._value);
                if (receivedLength > definition._maxLength)
                {
                    fail("received payload exceeds max_length: " + definition._name);
                }
                count = static_cast<std::size_t>(receivedLength);
            }
            std::size_t fieldOffset = offset;
            if (definition._continuation)
            {
                // 앞 필드와 같은 비트 저장 단위를 공유하므로 현재 위치가 아니라 직전 저장 단위로 돌아가 읽습니다.
                fieldOffset = offset - count;
            }
            if (count > data.size() - fieldOffset)
            {
                fail("binary message truncated at: " + definition._name);
            }
            const Json decodedValue = decodeScalar(definition, data, fieldOffset, count);
            Field &destination = candidate.field(definition._name);
            destination._value = decodedValue;
            if (!definition._continuation)
            {
                offset += count;
            }
        }
        const std::size_t endPadding = paddingSize(offset, _endAlignment);
        if (endPadding > data.size() - offset)
        {
            fail("binary message truncated in final padding");
        }
        offset += endPadding;
        // [최종 검증] 정의대로 읽은 길이와 실제 데이터그램 길이가 같아야 정상 메시지입니다.
        if (offset != data.size())
        {
            fail("unexpected trailing bytes");
        }
        // 현재 헤더 코드가 정의된 메시지 코드인지 검사합니다.
        candidate.checkMessageCode();
        // 모든 검사를 통과한 임시 필드 목록을 실제 메시지에 반영합니다.
        _fields.swap(candidate._fields);
    }

    std::string Message::dump(std::size_t maxHexBytes) const
    {
        std::ostringstream output;
        output << "Link: ";
        output << _linkName;
        output << "  Message: ";
        output << _messageName;
        output << '\n';
        std::vector<std::uint8_t> allBytes;
        std::string status = "READY";
        try
        {
            allBytes = serialize();
        }
        catch (const std::exception &error)
        {
            const char * what136 = error.what();
            status = "INCOMPLETE: " + std::string(what136);
        }
        output << "Status: ";
        output << status;
        output << "  Alignment: ";
        output << _alignment;
        output << "  Current size: ";
        output << size();
        output << " bytes\n";
        DumpTable table;
        table.add({"OFFSET", "FIELD", "TYPE", "SIZE", "ENDIAN", "VALUE", "ENUM", "HEX"});
        std::size_t offset = 0;
        bool knownOffset = true;
        // 덤프도 각 필드의 정렬 크기로 위치를 계산합니다. 패딩 목록은 보관하지 않습니다.
        const auto showPadding = [&](std::size_t alignment, const std::string &label)
        {
            if (alignment <= 1)
            {
                return;
            }
            if (!knownOffset)
            {
                table.add({"?", "<padding " + label + ">", "padding", "?", "-", "-", "-", "-"});
                return;
            }
            const std::size_t padding = paddingSize(offset, alignment);
            if (padding != 0)
            {
                const std::vector<std::uint8_t> zeroes(padding, 0);
                table.add({std::to_string(offset), "<padding " + label + ">", "padding",
                    std::to_string(padding), "-", "0", "-", hexText(zeroes, 12)});
                offset += padding;
            }
        };
        for (const Field &definition : _fields)
        {
            if (!definition._continuation)
            {
                showPadding(definition._alignment, "before " + definition._name);
            }
            const bool present = !definition._value.is_null();
            std::vector<std::uint8_t> bytes;
            std::string valueText = "<unset>";
            std::string enumText = "-";
            std::string hex = "-";
            std::size_t fieldSize = definition._size;
            if (present)
            {
                const Json &currentValue = definition._value;
                bytes = encode(definition, currentValue);
                fieldSize = bytes.size();
                hex = hexText(bytes, 12);
                if (definition._type == "bytes")
                {
                    const std::size_t bytesCount = bytes.size();
                    valueText = std::to_string(bytesCount) + " bytes";
                }
                else
                {
                    const std::string valueText2 = currentValue.dump(-1, ' ', false, Json::error_handler_t::replace);
                    valueText = shorten(
                        valueText2);
                }
                const std::string enumLabel = _enums.label(definition._enumName, currentValue);
                // 표 안에서 줄바꿈이 생기지 않도록 표시 문자를 정리합니다.
                const std::string singleLineText = singleLine(enumLabel);
                enumText =
                shorten(singleLineText);
            }
            std::string offsetText = "?";
            if (knownOffset)
            {
                std::size_t displayOffset = offset;
                if (definition._continuation)
                {
                    displayOffset = offset - definition._size;
                }
                offsetText = std::to_string(displayOffset);
            }
            std::string sizeText = std::to_string(fieldSize);
            if (!present && definition._type == "bytes")
            {
                sizeText = "?";
            }
            std::string endianText = "big";
            if (!definition._big)
            {
                endianText = "little";
            }
            if (definition._type == "bytes" || definition._type == "string")
            {
                endianText = "-";
            }
            std::string displayType = definition._type;
            if (displayType == "bytes")
            {
                displayType = "char*";
            }
            if (definition._bitWidth)
            {
                displayType += ":" + std::to_string(definition._bitWidth);
                sizeText = std::to_string(definition._bitWidth) + " bits @" + std::to_string(definition._bitOffset);
                if (status == "READY")
                {
                    std::size_t start = offset;
                    if (definition._continuation)
                    {
                        start = offset - definition._size;
                    }
                    std::vector<std::uint8_t>::const_iterator firstByte = allBytes.begin();
                    firstByte += start;
                    std::vector<std::uint8_t>::const_iterator lastByte = firstByte;
                    lastByte += definition._size;
                    const std::vector<std::uint8_t> stepValue144(firstByte, lastByte);
                    hex = hexText(stepValue144, 12);
                }
                else
                {
                    hex = "-";
                }
            }
            table.add({offsetText,
                    definition._name,
                    displayType,
                    sizeText,
                    endianText,
                    valueText,
                    enumText,
                    hex});
            if (!present && definition._type == "bytes")
            {
                knownOffset = false;
            }
            if (!definition._continuation)
            {
                offset += fieldSize;
            }
        }
        showPadding(_endAlignment, "end message");
        table.write(output);
        if (status == "READY")
        {
            std::size_t count = allBytes.size();
            if (maxHexBytes != 0 && count > maxHexBytes)
            {
                count = maxHexBytes;
            }
            output << "HEX (";
            output << count;
            output << '/';
            output << allBytes.size();
            output << " bytes):\n";
            for (std::size_t index = 0; index < count; ++index)
            {
                if (index % 16 == 0)
                {
                    output << std::hex;
                    output << std::uppercase;
                    output << std::setw(8);
                    output << std::setfill('0');
                    output << index;
                    output << "  ";
                }
                output << std::setw(2);
                output << static_cast<unsigned int>(allBytes[index]);
                output << ' ';
                if (index % 16 == 15 || index + 1 == count)
                {
                    output << '\n';
                }
            }
            if (count < allBytes.size())
            {
                output << "... remaining bytes omitted; use maxHexBytes=0 for full HEX\n";
            }
        }
        const std::string result = output.str();
        return result;
    }

    void Message::dump(std::ostream &output, std::size_t maxHexBytes) const
    {
        output << dump(maxHexBytes);
    }

    void Message::dumpToFile(const std::string &path, std::size_t maxHexBytes) const
    {
        const std::string text = dump(maxHexBytes);
        writeTextFile(path, text);
    }

    // 펼쳐 둔 필드 이름으로 정의를 찾습니다. 예: body.position.x -> 해당 Field.
    Message::Field &Message::field(const std::string &name)
    {
        for (Field &current : _fields)
        {
            if (current._name == name)
            {
                return current;
            }
        }
        fail("unknown field: " + name);
    }

    const Message::Field &Message::field(const std::string &name) const
    {
        // 이름→인덱스라는 별도 사전을 없앴습니다. 정의 목록에서 이름을 직접 비교합니다.
        for (const Field &current : _fields)
        {
            if (current._name == name)
            {
                return current;
            }
        }
        fail("unknown field: " + name);
    }

    // 부모 경로와 자식 이름을 점으로 연결합니다. 빈 부모라면 자식 이름만 반환합니다.
    std::string Message::fieldPath(const std::string &parent, const std::string &child)
    {
        if (parent.empty())
        {
            return child;
        }
        return parent + "." + child;
    }

    // JSON 필드 하나의 타입/길이/비트 수/enum 설정을 검사합니다. 값 자체를 저장하지는 않습니다.
    void Message::validateEntry(const Json &entry) const
    {
        checkKeys(
            entry, {"name", "type", "length", "endian", "count", "enum", "length_from", "max_length", "bits"});
        const Json & nameJson = entry.at("name");
        const std::string nameText = nameJson.get<std::string>();
        if (!identifier(nameText))
        {
            fail("invalid field name");
        }
        const Json & typeJson = entry.at("type");
        const std::string typeName = typeJson.get<std::string>();
        const std::string type = canonical(typeName);
        if (entry.contains("endian"))
        {
            const Json & endianJson = entry.at("endian");
            const std::string endianText = endianJson.get<std::string>();
            bigEndian(endianText);
        }
        if (entry.contains("bits"))
        {
            bool matchesCondition = !integerType(type);
            if (!matchesCondition)
            {
                bool matchesCondition2 = type.front() != 'u';
                if (!matchesCondition2)
                {
                    matchesCondition2 = entry.contains("count");
                }
                matchesCondition = matchesCondition2;
            }
            if (matchesCondition)
            {
                fail("bits requires an unsigned integer scalar");
            }
            const Json & bitsJson = entry.at("bits");
            const std::uint64_t bits = unsignedValue(bitsJson);
            bool matchesCondition3 = bits == 0;
            if (!matchesCondition3)
            {
                matchesCondition3 = bits > width(type) * 8;
            }
            if (matchesCondition3)
            {
                fail("bits must be 1..storage type width");
            }
        }
        if (entry.contains("count"))
        {
            const Json & countJson = entry.at("count");
            const std::uint64_t count = unsignedValue(countJson);
            if (count == 0 || count > MaxFieldCount)
            {
                fail("array count must be 1..65536");
            }
        }
        if (entry.contains("enum"))
        {
            const Json & enumJson = entry.at("enum");
            const std::string enumText = enumJson.get<std::string>();
            _enums.validate(enumText, type);
        }
        if (type == "string")
        {
            const Json & lengthJson = entry.at("length");
            const std::uint64_t length = unsignedValue(lengthJson);
            if (length == 0 || length > MaxMessageSize)
            {
                fail("invalid string length");
            }
        }
        else
        {
            if (entry.contains("length"))
            {
                fail("length is only valid for string");
            }
        }
        if (type == "bytes")
        {
            if (entry.contains("count"))
            {
                fail("bytes payload cannot have count; use uint8_t + count for fixed data");
            }
            const Json & length_fromJson = entry.at("length_from");
            const std::string length_fromText = length_fromJson.get<std::string>();
            if (length_fromText.empty())
            {
                fail("payload length_from must not be empty");
            }
            const Json jsonNumber = Json(MaxMessageSize);
            const Json max_lengthSetting = entry.value("max_length", jsonNumber);
            const std::uint64_t maximum =
            unsignedValue(max_lengthSetting);
            if (maximum > MaxMessageSize)
            {
                fail("payload max_length exceeds 16 MiB");
            }
        }
        else
        {
            bool matchesCondition4 = entry.contains("length_from");
            if (!matchesCondition4)
            {
                matchesCondition4 = entry.contains("max_length");
            }
            if (matchesCondition4)
            {
                fail("length_from/max_length are only valid for bytes");
            }
        }
    }

    // 같은 구조체 안의 모든 필드를 검사합니다. 중복 이름과 과도한 중첩도 거부합니다.
    std::size_t Message::validateFields(const Json &fields, const Json &types, std::set<std::string> &active) const
    {
        bool matchesCondition = !fields.is_array();
        if (!matchesCondition)
        {
            matchesCondition = fields.empty();
        }
        if (matchesCondition)
        {
            fail("fields must be a nonempty array");
        }
        // 이 집합은 같은 구조체 안에서 이미 사용한 필드명을 기억합니다. 중복 필드명이 있으면 경로가 모호해지므로 거부합니다.
        std::set<std::string> names;
        std::size_t depth = 1;
        for (const Json &entry : fields)
        {
            validateEntry(entry);
            const Json & nameJson = entry.at("name");
            const std::string name = nameJson.get<std::string>();
            if (!names.insert(name).second)
            {
                fail("duplicate field: " + name);
            }
            const Json & typeJson = entry.at("type");
            const std::string type = typeJson.get<std::string>();
            if (!builtin(type))
            {
                const std::size_t childDepth = validateType(types, type, active);
                const std::size_t candidateDepth = 1 + childDepth;
                if (candidateDepth > depth)
                {
                    depth = candidateDepth;
                }
            }
        }
        if (depth > 64)
        {
            fail("struct nesting exceeds 64");
        }
        return depth;
    }

    // 사용자 구조체 타입을 따라가며 검사합니다. active는 현재 방문 중인 타입의 집합입니다.
    std::size_t Message::validateType(const Json &types,
        const std::string &type,
        std::set<std::string> &active) const
    {
        // 흐름: 사용자 타입 존재 확인 → 순환 참조/캐시 확인 → 자식 구조체 검사 → 타입 깊이 캐시에 보관.
        // 기본 타입은 호출자인 validateFields에서 구분하며, 이 함수는 사용자 구조체 타입만 받습니다.

        if (!types.contains(type))
        {
            fail("undefined struct type: " + type);
        }
        bool matchesCondition = active.count(type) != 0;
        if (!matchesCondition)
        {
            matchesCondition = active.size() >= 64;
        }
        if (matchesCondition)
        {
            fail("recursive struct or nesting exceeds 64: " + type);
        }
        // map.find(type)은 해당 타입의 캐시 위치를 돌려주고, 없으면 end()를 돌려줍니다.
        // 찾은 iterator의 ->first는 타입 이름, ->second는 그 타입을 펼쳤을 때의 깊이입니다.
        // const_iterator는 조회용이므로 그 위치의 값을 이 iterator를 통해 바꿀 수 없습니다.
        std::map<std::string, std::size_t>::const_iterator cached = _validatedTypeDepths.find(type);
        if (cached != _validatedTypeDepths.end())
        {
            if (active.size() + cached->second > 64)
            {
                fail("struct nesting exceeds 64: " + type);
            }
            return cached->second;
        }
        // 이 타입을 현재 재귀 경로에 표시합니다. 예: A → B → A에서 두 번째 A는 이미 active에 있어 거부됩니다.
        active.insert(type);
        const Json &definition = types.at(type);
        checkKeys(definition, {"fields"});
        const Json & fieldsJson = definition.at("fields");
        // 구조체 안의 필드 목록과 중복 이름을 검사합니다.
        const std::size_t depth = validateFields(fieldsJson, types, active);
        // 이 타입의 자식 검사를 마쳤으므로 현재 경로에서 지웁니다.
        // 다른 필드가 같은 타입을 다시 쓰는 것은 정상입니다. 영구 방문 목록이 아니라 현재 호출 경로만 기록합니다.
        active.erase(type);
        _validatedTypeDepths.emplace(type, depth);
        return depth;
    }

    // 다음 정렬 경계까지 필요한 0바이트 개수를 계산합니다. 예: offset=3, alignment=4 -> 1.
    std::size_t Message::paddingSize(std::size_t offset, std::size_t alignment)
    {
        const std::size_t remainder = offset % alignment;
        if (remainder == 0)
        {
            return 0;
        }
        return alignment - remainder;
    }

    // 현재 바이트 위치를 count만큼 이동합니다. 메시지 최대 크기를 넘으면 오류입니다.
    void Message::advanceOffset(std::size_t &offset, std::size_t count)
    {
        if (count > MaxMessageSize - offset)
        {
            fail("message including alignment padding exceeds 16 MiB");
        }
        offset += count;
    }

    // 연속된 정렬 요구는 가장 큰 경계 하나로 합칩니다.
    // 허용 경계가 1/2/4/8이므로 2 다음 4로 정렬하는 것은 바로 4로 정렬하는 것과 같습니다.
    // 다음 말단 필드를 만나면 이 값을 그 필드에 옮기고 1로 초기화합니다.
    // 마지막에 남은 값은 메시지 끝 정렬에 사용합니다.
    void Message::mergeAlignment(std::size_t alignment)
    {
        if (alignment > _endAlignment)
        {
            _endAlignment = alignment;
        }
    }

    // 정수/실수/구조체가 요구하는 정렬 크기를 계산합니다. 사용자 alignment를 상한으로 사용합니다.
    std::size_t Message::valueAlignment(const std::string &type, const Json &types)
    {
        const std::string normalized = canonical(type);
        if (normalized == "string" || normalized == "bytes")
        {
            return 1;
        }
        if (builtin(normalized))
        {
            const std::size_t typeSize = width(normalized);
            std::size_t result = typeSize;
            if (_alignment < result)
            {
                result = _alignment;
            }
            return result;
        }
        std::map<std::string, std::size_t>::const_iterator cached = _typeAlignments.find(type);
        if (cached != _typeAlignments.end())
        {
            return cached->second;
        }
        const Json & jsonValue = types.at(type);
        const Json & fieldsJson = jsonValue.at("fields");
        const std::size_t result = structureAlignment(fieldsJson, types);
        _typeAlignments.emplace(type, result);
        return result;
    }

    std::size_t Message::structureAlignment(const Json &fields, const Json &types)
    {
        // 각 필드가 요구하는 정렬 크기 중 최댓값을 구합니다. 예: uint8_t=1, uint32_t=4라면 구조체 정렬은 4입니다.
        // 실제 허용 최대 정렬은 valueAlignment에서 메시지 alignment 설정을 반영합니다.

        std::size_t result = 1;
        for (const Json &entry : fields)
        {
            const Json &typeValue = entry.at("type");
            const std::string typeName = typeValue.get<std::string>();
            const std::size_t fieldAlignment = valueAlignment(typeName, types);
            if (fieldAlignment > result)
            {
                result = fieldAlignment;
            }
        }
        return result;
    }

    // 필드와 패딩을 순서대로 더해 전체 바이트 수를 계산합니다. 가변 버퍼는 현재 값의 길이를 사용합니다.
    std::size_t Message::calculateSize(bool includeValues) const
    {
        std::size_t offset = 0;
        for (const Field &definition : _fields)
        {
            if (!definition._continuation)
            {
                const std::size_t padding = paddingSize(offset, definition._alignment);
                advanceOffset(offset, padding);
            }
            std::size_t count = definition._size;
            // 가변 버퍼이며 현재 값을 포함할 때만 실제 버퍼 길이를 읽습니다.
            if (definition._type == "bytes")
            {
                if (includeValues)
                {
                    const bool isSet = !definition._value.is_null();
                    if (isSet)
                    {
                        count = definition._value.size();
                    }
                }
            }
            // 같은 비트 저장 단위를 공유하는 뒤 필드는 바이트 크기를 더하지 않습니다.
            if (!definition._continuation)
            {
                advanceOffset(offset, count);
            }
        }
        const std::size_t endPadding = paddingSize(offset, _endAlignment);
        advanceOffset(offset, endPadding);
        return offset;
    }

    // 필드 하나를 내부 정의로 바꿉니다. 구조체라면 자식으로 내려가 말단 필드까지 펼칩니다.
    void Message::expandValue(const Json &entry,
        const Json &types,
        const std::string &path,
        const std::string &parent,
        bool inheritedBig)
    {
        // 흐름: 필드 경로와 타입 해석 → 배열/구조체면 자식으로 내려감 → 말단이면 _fields에 등록.

        bool big = inheritedBig;
        if (entry.contains("endian"))
        {
            const Json & endianJson = entry.at("endian");
            const std::string endianText = endianJson.get<std::string>();
            big = bigEndian(endianText);
        }
        const Json & typeJson = entry.at("type");
        const std::string typeName = typeJson.get<std::string>();
        const std::string type = canonical(typeName);
        if (!builtin(type))
        {
            const Json & jsonValue = types.at(type);
            const Json & fieldsJson = jsonValue.at("fields");
            expandFields(fieldsJson, types, path, big);
            return;
        }
        if (_fields.size() >= MaxFieldCount)
        {
            fail("expanded field count exceeds 65536");
        }
        Field next;
        next._name = path;
        next._type = type;
        const Json & typeJson2 = entry.at("type");
        const std::string declaredType = typeJson2.get<std::string>();
        next._manualLength = declaredType == "char*" || declaredType == "char *";
        next._big = big;
        const Json jsonNumber = Json(0);
        const Json bitsSetting = entry.value("bits", jsonNumber);
        const std::uint64_t unsignedNumber = unsignedValue(bitsSetting);
        next._bitWidth = static_cast<std::size_t>(unsignedNumber);
        const std::string string171 = std::string();
        next._enumName = entry.value("enum", string171);
        if (type == "bytes")
        {
            const Json & length_fromJson = entry.at("length_from");
            next._lengthFrom = length_fromJson.get<std::string>();
            // 점이나 인덱스가 없으면 같은 구조체의 필드명으로 해석합니다.
            if (next._lengthFrom.find_first_of(".[") == std::string::npos)
            {
                next._lengthFrom = fieldPath(parent, next._lengthFrom);
            }
            const Field &lengthField = field(next._lengthFrom);
            bool matchesCondition = !integerType(lengthField._type);
            if (!matchesCondition)
            {
                matchesCondition = lengthField._type.front() != 'u';
            }
            if (matchesCondition)
            {
                fail("length_from must reference an earlier unsigned integer field");
            }
            for (const Field &existing : _fields)
            {
                if (existing._lengthFrom == next._lengthFrom)
                {
                    fail("payloads must use separate length fields");
                }
            }
            const Json jsonNumber2 = Json(MaxMessageSize);
            const Json max_lengthSetting = entry.value("max_length", jsonNumber2);
            const std::uint64_t unsignedNumber2 = unsignedValue(max_lengthSetting);
            next._maxLength = static_cast<std::size_t>(
                unsignedNumber2);
        }
        else
        {
            if (type == "string")
            {
                const Json & lengthJson = entry.at("length");
                const std::uint64_t unsignedNumber3 = unsignedValue(lengthJson);
                next._size = static_cast<std::size_t>(unsignedNumber3);
            }
            else
            {
                next._size = width(type);
            }
            if (next._size > MaxMessageSize - _fixedSize)
            {
                fail("message fixed size exceeds 16 MiB");
            }
            _fixedSize += next._size;
        }
        const std::size_t fieldAlignment = valueAlignment(type, types);
        mergeAlignment(fieldAlignment);
        // 구조체 시작/끝의 경계까지 반영한 정렬 크기만 필드에 보관합니다.
        // 실제 패딩 바이트 수는 serialize/deserialize에서 현재 위치로 계산합니다.
        next._alignment = _endAlignment;
        _endAlignment = 1;
        _fields.push_back(next);
    }

    // 구조체/배열을 펼쳐 _fields와 _containers를 만듭니다. 비트필드의 같은 저장 단위도 묶습니다.
    void Message::expandFields(const Json &fields, const Json &types, const std::string &prefix, bool big)
    {
        // 흐름: 부모의 자식 목록 준비 → 필드마다 정렬/비트 저장 단위 결정 → 말단 필드와 정렬 크기 저장.

        ++_expansionDepth;
        if (_expansionDepth > 64)
        {
            fail("struct nesting exceeds 64");
        }
        const std::size_t structureBoundary = structureAlignment(fields, types);
        mergeAlignment(structureBoundary);
        Container structure;
        std::size_t groupIndex = 0, usedBits = 0;
        bool groupActive = false;
        for (const Json &entry : fields)
        {
            const Json & nameJson = entry.at("name");
            const std::string name = nameJson.get<std::string>();
            const std::string path = fieldPath(prefix, name);
            structure._children.push_back(path);
            structure._keys.push_back(name);
            if (entry.contains("count"))
            {
                groupActive = false;
                Container array;
                array._array = true;
                const Json & countJson = entry.at("count");
                const std::uint64_t unsignedNumber = unsignedValue(countJson);
                const std::size_t count =
                static_cast<std::size_t>(unsignedNumber);
                for (std::size_t index = 0; index < count; ++index)
                {
                    const std::string element = path + "[" + std::to_string(index) + "]";
                    array._children.push_back(element);
                    expandValue(entry, types, element, prefix, big);
                }
                _containers.emplace(path, array);
            }
            else
            {
                expandValue(entry, types, path, prefix, big);
                if (entry.contains("bits"))
                {
                    Field &current = _fields.back();
                    const Field &group = _fields[groupIndex];
                    if (groupActive && current._type == group._type && current._big == group._big &&
                        usedBits + current._bitWidth <= current._size * 8)
                    {
                        current._bitOffset = usedBits;
                        current._continuation = true;
                        _fixedSize -= current._size;
                        // 앞 필드와 같은 저장 단위를 공유하므로 중간 패딩도 없습니다.
                        current._alignment = 1;
                    }
                    else
                    {
                        groupIndex = _fields.size() - 1;
                        usedBits = 0;
                    }
                    usedBits += current._bitWidth;
                    groupActive = true;
                }
                else
                {
                    groupActive = false;
                }
            }
        }
        mergeAlignment(structureBoundary);
        _containers.emplace(prefix, structure);
        --_expansionDepth;
    }

    // 필드 값 하나를 전송 바이트로 바꿉니다. 정수 범위와 버퍼 길이를 여기서도 검사합니다.
    // std::uint8_t/uint16_t/uint32_t/uint64_t는 각각 8/16/32/64비트 부호 없는 정수입니다.
    // static_cast는 값의 타입을 바꾸는 명시적 변환입니다. 정수로 바꾸기 전에 범위를 검사해야 잘림을 막습니다.
    // std::memcpy는 지정한 바이트를 그대로 복사합니다. float의 비트 패턴을 정수 저장소로 옮길 때 값 변환을 하지 않습니다.
    std::vector<std::uint8_t> Message::encode(const Field &definition, const Json &value)
    {
        // 흐름: 타입에 맞게 값/길이 확인 → 버퍼·문자열·실수·정수별 바이트 생성.
        // 실수는 비트 패턴을 memcpy로 읽은 뒤 엔디안을 적용합니다. 숫자를 정수로 반올림하는 변환이 아닙니다.

        std::vector<std::uint8_t> output;
        if (definition._type == "bytes")
        {
            bool matchesCondition = !value.is_array();
            if (!matchesCondition)
            {
                matchesCondition = value.size() > definition._maxLength;
            }
            if (matchesCondition)
            {
                fail("payload must be a byte array within max_length");
            }
            const std::size_t valueCount = value.size();
            output.reserve(valueCount);
            for (const Json &item : value)
            {
                const std::uint64_t number = unsignedValue(item);
                if (number > 255)
                {
                    fail("payload byte must be 0..255");
                }
                const std::uint8_t convertedValue = static_cast<std::uint8_t>(number);
                output.push_back(convertedValue);
            }
            return output;
        }
        if (definition._type == "string")
        {
            if (!value.is_string())
            {
                fail("expected string");
            }
            const std::string text = value.get<std::string>();
            bool matchesCondition2 = text.find('\0') != std::string::npos;
            if (!matchesCondition2)
            {
                matchesCondition2 = text.size() > definition._size;
            }
            if (matchesCondition2)
            {
                fail("string exceeds fixed byte length or contains NUL");
            }
            std::vector<std::uint8_t>::iterator destination = output.end();
            std::string::const_iterator firstCharacter = text.begin();
            std::string::const_iterator lastCharacter = text.end();
            output.insert(destination, firstCharacter, lastCharacter);
            output.resize(definition._size, 0);
            return output;
        }
        std::uint64_t bits = 0;
        if (definition._type == "float" || definition._type == "double")
        {
            // 실수 필드라도 정수 값 "50" / "0x32"는 숫자 50으로 사용할 수 있습니다.
            Json numeric = value;
            if (numeric.is_string())
            {
                numeric = parseInteger(numeric);
            }
            if (!numeric.is_number())
            {
                fail("expected number");
            }
            const double number = numeric.get<double>();
            if (!std::isfinite(number))
            {
                fail("non-finite number");
            }
            if (definition._type == "float")
            {
                if (std::abs(number) > std::numeric_limits<float>::max())
                {
                    fail("float overflow");
                }
                const float floatNumber = static_cast<float>(number);
                std::uint32_t floatBits = 0;
                // 데이터 내용을 지정한 바이트 수만큼 복사합니다. 포인터 주소 자체를 전송하지 않습니다.
                std::memcpy(&floatBits, &floatNumber, sizeof(floatBits));
                bits = floatBits;
            }
            else
            {
                // 데이터 내용을 지정한 바이트 수만큼 복사합니다. 포인터 주소 자체를 전송하지 않습니다.
                std::memcpy(&bits, &number, sizeof(bits));
            }
        }
        else
        {
            bits = integerBits(definition._type, value);
            if (definition._bitWidth && bits > (UINT64_MAX >> (64 - definition._bitWidth)))
            {
                fail("bit field overflow: " + definition._name);
            }
        }
        // 정수 비트를 엔디안 순서대로 바이트 목록에 붙입니다.
        appendBits(output, bits, definition._size, definition._big);
        return output;
    }

    // 부모 구조체/배열의 값을 자식으로 나눠 candidate에 기록합니다. 실제 메시지 대신 임시 메시지의 필드 값만 바꿉니다.
    void Message::collectSet(const std::string &name, const Json &value)
    {
        // 흐름: 경로가 말단 필드인지 찾기 → 말단이면 타입/범위 검사 후 저장 → 부모이면 자식 경로로 재귀 설정.
        // 여기서 this는 보통 set이 만든 candidate입니다. 중간 실패로 실제 메시지의 값이 반쯤 바뀌지 않게 합니다.

        std::map<std::string, Container>::const_iterator container = _containers.find(name);
        if (container != _containers.end())
        {
            const Container &definition = container->second;
            if (definition._array)
            {
                bool matchesCondition = !value.is_array();
                if (!matchesCondition)
                {
                    matchesCondition = value.size() != definition._children.size();
                }
                if (matchesCondition)
                {
                    fail("fixed array count mismatch: " + name);
                }
                for (std::size_t index = 0; index < definition._children.size(); ++index)
                {
                    const Json & jsonValue = value.at(index);
                    collectSet(definition._children[index], jsonValue);
                }
            }
            else
            {
                bool matchesCondition2 = !value.is_object();
                if (!matchesCondition2)
                {
                    matchesCondition2 = value.size() != definition._children.size();
                }
                if (matchesCondition2)
                {
                    fail("struct fields mismatch: " + name);
                }
                for (std::size_t index = 0; index < definition._children.size(); ++index)
                {
                    const Json & jsonValue2 = value.at(definition._keys[index]);
                    collectSet(definition._children[index], jsonValue2);
                }
            }
            return;
        }
        Field &definition = field(name);
        encode(definition, value);
        Json normalized = value;
        if (integerType(definition._type))
        {
            // 표기 방식 대신 실제 정수 값을 저장합니다."0x32"도 내부에서는 50입니다.
            normalized = parseInteger(value);
        }
        else
        {
            if (definition._type == "bytes")
            {
                // 버퍼의 각 바이트도 "0xFF"처럼 지정할 수 있습니다.
                normalized = Json::array();
                for (const Json &item : value)
                {
                    const std::uint64_t unsignedNumber = unsignedValue(item);
                    normalized.push_back(unsignedNumber);
                }
            }
            else
            {
                if (definition._type == "float" || definition._type == "double")
                {
                    Json numeric = value;
                    if (numeric.is_string())
                    {
                        numeric = parseInteger(numeric);
                    }
                    if (definition._type == "float")
                    {
                        const double convertedNumber = numeric.get<double>();
                        normalized = static_cast<float>(convertedNumber);
                    }
                    else
                    {
                        normalized = numeric.get<double>();
                    }
                }
            }
        }
        definition._value = normalized;
        if (definition._type == "bytes" && !definition._manualLength)
        {
            // 구형 bytes 정의의 동작입니다. char*의 length는 사용자가 따로 설정합니다.
            const Json length = value.size();
            Field &lengthField = field(definition._lengthFrom);
            encode(lengthField, length);
            lengthField._value = length;
        }
    }

    // 설정 실패 이유를 로그에 남깁니다. 로그 함수가 실패해도 원래 오류가 사라지지 않게 합니다.
    void Message::logSetFailure(const std::string &name, const std::exception &error) const
    {
        if (_logHandler)
        {
            const std::string text = "[Message::set] link=" + _linkName + " message=" + _messageName +
            " field=" + name + " error=" + error.what();
            try
            {
                _logHandler(text);
            }
            catch (...)
            {
                // 사용자 로거의 오류가 실제 설정 오류를 가리지 않도록 합니다.
            }
        }
    }

    // 현재 헤더 코드가 이 메시지 정의의 msgcode와 같은지 확인합니다.
    void Message::checkMessageCode() const
    {
        if (_codeField.empty())
        {
            return;
        }
        const Field &codeField = field(_codeField);
        if (codeField._value.is_null())
        {
            fail("message code does not match definition: " + _codeField);
        }
        const std::uint64_t actualCode = unsignedValue(codeField._value);
        if (actualCode != _expectedCode)
        {
            fail("message code does not match definition: " + _codeField);
        }
    }

    // 링크 정의에서 정한 코드 필드와 기대값을 기억하고 초기 코드 값을 설정합니다.
    void Message::configureMessageCode(const std::string &name, std::uint64_t code)
    {
        unsignedFieldLocation(name);
        const Json jsonNumber = Json(code);
        const Field &definition = field(name);
        encode(definition, jsonNumber);
        _codeField = name;
        _expectedCode = code;
        Field &codeField = field(name);
        codeField._value = code;
    }

    // 가변 버퍼의 실제 크기와 연결된 length 필드가 맞는지 검사합니다. 수동 길이는 전송 전에 최종 확인합니다.
    void Message::checkLengths(bool requireManualLengths) const
    {
        // 현재 헤더 코드가 정의된 메시지 코드인지 검사합니다.
        checkMessageCode();
        std::size_t total = _fixedSize;
        for (const Field &definition : _fields)
        {
            if (definition._type != "bytes")
            {
                continue;
            }
            if (definition._value.is_null())
            {
                continue;
            }
            const std::size_t length = definition._value.size();
            if (requireManualLengths || !definition._manualLength)
            {
                const Field &lengthField = field(definition._lengthFrom);
                if (lengthField._value.is_null())
                {
                    fail("buffer length mismatch: " + definition._name);
                }
                const std::uint64_t recordedLength = unsignedValue(lengthField._value);
                if (recordedLength != length)
                {
                    fail("buffer length mismatch: " + definition._name);
                }
            }
            if (length > MaxMessageSize - total)
            {
                fail("message exceeds 16 MiB");
            }
            total += length;
        }
        calculateSize();
    }

    // 말단 필드는 현재 값을 읽고, 구조체/배열이면 자식 값을 모아 JSON 하나로 반환합니다.
    Json Message::readValue(const std::string &name) const
    {
        std::map<std::string, Container>::const_iterator container = _containers.find(name);
        if (container == _containers.end())
        {
            const Field &definition = field(name);
            if (definition._value.is_null())
            {
                fail("missing value: " + name);
            }
            const Json result = definition._value;
            return result;
        }
        const Container &definition = container->second;
        Json result = Json::object();
        if (definition._array)
        {
            result = Json::array();
        }
        for (std::size_t index = 0; index < definition._children.size(); ++index)
        {
            const Json child = readValue(definition._children[index]);
            if (definition._array)
            {
                result.push_back(child);
            }
            else
            {
                result[definition._keys[index]] = child;
            }
        }
        return result;
    }

    // 바이트에서 필드 하나를 읽습니다. 엔디안/부호/실수/비트 위치를 반영해 JSON 값으로 돌려줍니다.
    Json Message::decodeScalar(const Field &definition,
        const std::vector<std::uint8_t> &data,
        std::size_t offset,
        std::size_t count)
    {
        // 흐름: 입력 바이트의 타입 선택 → 엔디안/비트 위치 복원 → 부호·실수·문자열 규칙에 맞게 Json 값 생성.

        if (definition._type == "bytes")
        {
            Json result = Json::array();
            for (std::size_t index = 0; index < count; ++index)
            {
                result.push_back(data[offset + index]);
            }
            return result;
        }
        if (definition._type == "string")
        {
            std::string text;
            bool padding = false;
            for (std::size_t index = 0; index < count; ++index)
            {
                const std::uint8_t byte = data[offset + index];
                if (byte == 0)
                {
                    padding = true;
                }
                else
                {
                    if (padding)
                    {
                        fail("nonzero byte after string padding");
                    }
                    const char static_cast189 = static_cast<char>(byte);
                    text.push_back(static_cast189);
                }
            }
            return text;
        }
        std::uint64_t bits = 0;
        for (std::size_t index = 0; index < count; ++index)
        {
            std::size_t byteIndex = index;
            if (definition._big)
            {
                byteIndex = count - 1 - index;
            }
            // 해당 비트 위치의 값을 기존 저장 단위에 합칩니다.
            bits |= std::uint64_t(data[offset + index]) << (byteIndex * 8);
        }
        if (definition._bitWidth)
        {
            // 유효 비트만 남길 수 있도록 비트 마스크를 계산합니다.
            const std::uint64_t mask = UINT64_MAX >> (64 - definition._bitWidth);
            const std::uint64_t shifted = bits >> definition._bitOffset;
            const Json result = shifted & mask;
            return result;
        }
        if (definition._type == "float")
        {
            const std::uint32_t floatBits = static_cast<std::uint32_t>(bits);
            float number = 0;
            // 데이터 내용을 지정한 바이트 수만큼 복사합니다. 포인터 주소 자체를 전송하지 않습니다.
            std::memcpy(&number, &floatBits, sizeof(number));
            if (!std::isfinite(number))
            {
                fail("non-finite float received");
            }
            return number;
        }
        if (definition._type == "double")
        {
            double number = 0;
            // 데이터 내용을 지정한 바이트 수만큼 복사합니다. 포인터 주소 자체를 전송하지 않습니다.
            std::memcpy(&number, &bits, sizeof(number));
            if (!std::isfinite(number))
            {
                fail("non-finite double received");
            }
            return number;
        }
        if (definition._type.front() == 'u')
        {
            return bits;
        }
        const bool negative = (bits & (std::uint64_t(1) << (count * 8 - 1))) != 0;
        if (!negative)
        {
            const std::int64_t number = static_cast<std::int64_t>(bits);
            const Json result = number;
            return result;
        }
        // 유효 비트만 남길 수 있도록 비트 마스크를 계산합니다.
        const std::uint64_t mask = UINT64_MAX >> (64 - count * 8);
        // 범위 밖 unsigned→signed 변환 없이 음수 값을 복원합니다.
        const std::uint64_t inverted = (~bits) & mask;
        const std::int64_t magnitude = static_cast<std::int64_t>(inverted);
        const Json result = -1 - magnitude;
        return result;
    }

    // 자동 판별용 정수 필드의 바이트 위치와 비트 위치를 계산합니다.
    Message::UnsignedFieldLocation Message::unsignedFieldLocation(const std::string &name) const
    {
        const Field &requested = field(name);
        bool matchesCondition = !integerType(requested._type);
        if (!matchesCondition)
        {
            matchesCondition = requested._type.front() != 'u';
        }
        if (matchesCondition)
        {
            fail("msgcode field must be an unsigned integer: " + name);
        }
        std::size_t offset = 0;
        for (const Field &definition : _fields)
        {
            if (!definition._continuation)
            {
                const std::size_t padding = paddingSize(offset, definition._alignment);
                advanceOffset(offset, padding);
            }
            if (definition._name == name)
            {
                UnsignedFieldLocation location;
                location._offset = offset;
                if (definition._continuation)
                {
                    location._offset = offset - definition._size;
                }
                location._size = definition._size;
                location._big = definition._big;
                location._bitWidth = definition._bitWidth;
                location._bitOffset = definition._bitOffset;
                return location;
            }
            if (definition._type == "bytes")
            {
                fail("msgcode must appear before variable payload: " + name);
            }
            if (!definition._continuation)
            {
                advanceOffset(offset, definition._size);
            }
        }
        fail("cannot locate msgcode field: " + name);
    }
    // 배열이나 구조체 정의의 자식 개수를 읽습니다. 사용자 값을 아직 넣지 않았어도 조회할 수 있습니다.
    std::size_t Message::elementCount(const std::string &name) const
    {
        const std::map<std::string, Container>::const_iterator found = _containers.find(name);
        if (found != _containers.end())
        {
            const std::size_t count = found->second._children.size();
            return count;
        }
        const Json current = readValue(name);
        const std::size_t count = current.size();
        return count;
    }
}
