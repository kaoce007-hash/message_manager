#include "MessageInterface.hpp"
#include <filesystem>

namespace message_manager
{

    // 변환 설정을 읽어 규칙을 준비합니다. manager가 주어지면 대상 메시지도 스스로 만들 수 있습니다.
    MessageInterface::MessageInterface(const Json &definition)
    {
        Json tables = Json::object();
        tables["mappings"] = Json::object();
        initialize(definition, tables);
        if (definition.contains("mapping_file"))
        {
            throw std::runtime_error("mapping_file requires fromFile() or explicit mapping definitions");
        }
    }

    // 변환 설정을 읽어 규칙을 준비합니다. manager가 주어지면 대상 메시지도 스스로 만들 수 있습니다.
    MessageInterface::MessageInterface(const Json &definition, const Json &mappingDefinitions)
    {
        initialize(definition, mappingDefinitions);
    }

    // 변환 설정을 읽어 규칙을 준비합니다. manager가 주어지면 대상 메시지도 스스로 만들 수 있습니다.
    MessageInterface::MessageInterface(const Json &definition, const MessageManager &manager)
    {
        Json tables = Json::object();
        tables["mappings"] = Json::object();
        initialize(definition, tables);
        if (definition.contains("mapping_file"))
        {
            throw std::runtime_error("mapping_file requires fromFile() or explicit mapping definitions");
        }
        // 현재 값은 비우고 정의만 복사합니다.
        _manager = manager.copyDefinitions();
        _hasManager = true;
    }

    // 변환 설정을 읽어 규칙을 준비합니다. manager가 주어지면 대상 메시지도 스스로 만들 수 있습니다.
    MessageInterface::MessageInterface(const Json &definition, const Json &mappingDefinitions,
        const MessageManager &manager)
    {
        initialize(definition, mappingDefinitions);
        // 현재 값은 비우고 정의만 복사합니다.
        _manager = manager.copyDefinitions();
        _hasManager = true;
    }

    MessageInterface MessageInterface::fromFile(const std::string &path, const MessageManager &manager)
    {
        const Json definition = readJsonFile(path);
        const Json mappingDefinitions = readMappingFile(definition, path);
        MessageInterface result(definition, mappingDefinitions, manager);
        return result;
    }

    MessageInterface MessageInterface::fromFile(const std::string &path)
    {
        const Json definition = readJsonFile(path);
        const Json mappingDefinitions = readMappingFile(definition, path);
        MessageInterface result(definition, mappingDefinitions);
        return result;
    }

    const std::string &MessageInterface::sourceLink(const std::string &ruleName) const
    {
        const Rule &rule = findRule(ruleName);
        return rule._sourceLink;
    }

    const std::string &MessageInterface::sourceMessage(const std::string &ruleName) const
    {
        const Rule &rule = findRule(ruleName);
        return rule._sourceMessage;
    }

    // 설정된 변환 규칙으로 원본 값을 읽어 대상 메시지를 만듭니다. 원본은 수정하지 않습니다.
    Message MessageInterface::convert(const Message &source) const
    {
        if (!_hasManager)
        {
            throw std::runtime_error("convert(source) requires fromFile(path, manager)");
        }
        const Rule *selected = nullptr;
        for (const Rule &rule : _rules)
        {
            bool matchesCondition = source.linkName() == rule._sourceLink;
            if (matchesCondition)
            {
                matchesCondition = source.messageName() == rule._sourceMessage;
            }
            if (matchesCondition)
            {
                if (selected != nullptr)
                {
                    throw std::runtime_error("multiple target rules match; specify the rule name");
                }
                selected = &rule;
            }
        }
        if (selected == nullptr)
        {
            throw std::runtime_error("no conversion rule matches the source message");
        }
        const std::string ruleName = selected->_name;
        Message result = convert(ruleName, source, _manager);
        return result;
    }

    // 설정된 변환 규칙으로 원본 값을 읽어 대상 메시지를 만듭니다. 원본은 수정하지 않습니다.
    Message MessageInterface::convert(const std::string &ruleName, const Message &source) const
    {
        if (!_hasManager)
        {
            throw std::runtime_error("convert(rule, source) requires fromFile(path, manager)");
        }
        Message result = convert(ruleName, source, _manager);
        return result;
    }

    // 설정된 변환 규칙으로 원본 값을 읽어 대상 메시지를 만듭니다. 원본은 수정하지 않습니다.
    Message MessageInterface::convert(const std::string &ruleName,
        const Message &source,
        const MessageManager &manager) const
    {
        const Rule &rule = findRule(ruleName);
        Message target = manager.create(rule._targetLink, rule._targetMessage);
        const LogHandler handler = source.logHandler();
        target.setLogHandler(handler);
        convertInto(ruleName, source, target);
        return target;
    }

    // 대상의 임시 복사본에 변환합니다. 전부 성공한 뒤에만 실제 대상에 반영합니다.
    void MessageInterface::convertInto(const std::string &ruleName, const Message &source, Message &target) const
    {
        // 1. 원본·대상의 링크/메시지가 규칙과 맞는지 확인하고 원본을 serialize로 검증합니다.
        // 2. 빈 대상 converted에 필드 복사·값 매핑·고정값과 constants를 적용합니다.
        // 3. 대상도 serialize로 검증한 뒤 target에 반영합니다. 중간 실패 시 target은 그대로입니다.

        const Rule &rule = findRule(ruleName);
        bool matchesCondition = source.linkName() != rule._sourceLink;
        if (!matchesCondition)
        {
            matchesCondition = source.messageName() != rule._sourceMessage;
        }
        if (matchesCondition)
        {
            throw std::runtime_error("source link/message does not match rule: " + ruleName);
        }
        bool matchesCondition2 = target.linkName() != rule._targetLink;
        if (!matchesCondition2)
        {
            matchesCondition2 = target.messageName() != rule._targetMessage;
        }
        if (matchesCondition2)
        {
            throw std::runtime_error("target link/message does not match rule: " + ruleName);
        }
        source.serialize();
        // [변환 작업본] 대상의 이전 값이 섞이지 않게 비우고, 정의에 들어 있는 msgcode는 유지합니다.
        Message converted = target.emptyCopy();
        for (const FieldMapping &mapping : rule._mappings)
        {
            try
            {
                if (mapping._fixed)
                {
                    if (!source.hasField(mapping._sourceField))
                    {
                        throw std::runtime_error("unknown source field: " + mapping._sourceField);
                    }
                    converted.set(mapping._targetField, mapping._fixedValue);
                }
                else
                {
                    const Json originalValue = source.value(mapping._sourceField);
                    const Json mappedValue = mapValue(mapping, originalValue);
                    converted.set(mapping._targetField, mappedValue);
                }
            }
            catch (const std::exception &error)
            {
                const char * what49 = error.what();
                throw std::runtime_error(ruleName + ": " + mapping._sourceField + " -> " +
                    mapping._targetField + ": " + what49);
            }
        }
        for (Json::const_iterator constant = rule._constants.begin(); constant != rule._constants.end();
            ++constant)
        {
            const std::string itemName = constant.key();
            const Json & itemValue = constant.value();
            converted.set(itemName, itemValue);
        }
        // [결과 확정] 필수 값과 버퍼 길이까지 정상인지 확인한 뒤에만 실제 target을 바꿉니다.
        converted.serialize();
        target = converted;
    }

    // 대상의 임시 복사본에 변환합니다. 전부 성공한 뒤에만 실제 대상에 반영합니다.
    void MessageInterface::convertInto(const Message &source, Message &target) const
    {
        const Rule *selected = nullptr;
        for (const Rule &rule : _rules)
        {
            bool matchesCondition = source.linkName() == rule._sourceLink;
            if (matchesCondition)
            {
                bool matchesCondition2 = source.messageName() == rule._sourceMessage;
                if (matchesCondition2)
                {
                    bool matchesCondition3 = target.linkName() == rule._targetLink;
                    if (matchesCondition3)
                    {
                        matchesCondition3 = target.messageName() == rule._targetMessage;
                    }
                    matchesCondition2 = matchesCondition3;
                }
                matchesCondition = matchesCondition2;
            }
            if (matchesCondition)
            {
                if (selected != nullptr)
                {
                    throw std::runtime_error("multiple conversion rules match; specify the rule name");
                }
                selected = &rule;
            }
        }
        if (selected == nullptr)
        {
            throw std::runtime_error("no conversion rule matches the source and target messages");
        }
        convertInto(selected->_name, source, target);
    }

    // 수신 바이트를 원본 메시지로 해석한 다음 변환 규칙을 적용합니다.
    Message MessageInterface::convertBytes(const std::string &ruleName,
        const std::vector<std::uint8_t> &bytes,
        const MessageManager &manager) const
    {
        const Rule &rule = findRule(ruleName);
        Message source = manager.create(rule._sourceLink, rule._sourceMessage);
        source.deserialize(bytes);
        Message result = convert(ruleName, source, manager);
        return result;
    }
    // 허용한 JSON 키 목록과 실제 키를 비교합니다. 오타가 조용히 무시되는 일을 막습니다.
    void MessageInterface::checkKeys(const Json &object, std::initializer_list<const char *> allowed)
    {
        if (!object.is_object())
        {
            throw std::runtime_error("interface: expected JSON object");
        }
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
                throw std::runtime_error("interface: unknown key: " + itemName);
            }
        }
    }

    // JSON에서 비어 있지 않은 문자열 이름을 꺼냅니다.
    std::string MessageInterface::readName(const Json &object, const std::string &key)
    {
        const Json & jsonValue = object.at(key);
        const std::string name = jsonValue.get<std::string>();
        if (name.empty())
        {
            throw std::runtime_error("interface: empty name: " + key);
        }
        return name;
    }

    const MessageInterface::Rule &MessageInterface::findRule(const std::string &ruleName) const
    {
        for (const Rule &rule : _rules)
        {
            if (rule._name == ruleName)
            {
                return rule;
            }
        }
        throw std::runtime_error("unknown conversion rule: " + ruleName);
    }

    // 정수는 부호가 달라도 같은 수인지 비교합니다. 음수를 unsigned로 잘못 바꾸지 않게 주의합니다.
    bool MessageInterface::sameValue(const Json &a, const Json &b)
    {
        // 숫자 문자열도 정수로 비교합니다. 50, "50", "0x32"는 같은 입력입니다.
        bool matchesCondition = isIntegerValue(a);
        if (matchesCondition)
        {
            matchesCondition = isIntegerValue(b);
        }
        if (matchesCondition)
        {
            const Json left = parseInteger(a);
            const Json right = parseInteger(b);
            bool matchesCondition2 = left.is_number_unsigned();
            if (matchesCondition2)
            {
                matchesCondition2 = !right.is_number_unsigned();
            }
            if (matchesCondition2)
            {
                const std::int64_t signedNumber = right.get<std::int64_t>();
                if (signedNumber < 0)
                {
                    return false;
                }
                const std::uint64_t leftNumber = left.get<std::uint64_t>();
                const std::uint64_t rightNumber = static_cast<std::uint64_t>(signedNumber);
                const bool equal = leftNumber == rightNumber;
                return equal;
            }
            bool matchesCondition3 = !left.is_number_unsigned();
            if (matchesCondition3)
            {
                matchesCondition3 = right.is_number_unsigned();
            }
            if (matchesCondition3)
            {
                const std::int64_t signedNumber = left.get<std::int64_t>();
                if (signedNumber < 0)
                {
                    return false;
                }
                const std::uint64_t leftNumber = static_cast<std::uint64_t>(signedNumber);
                const std::uint64_t rightNumber = right.get<std::uint64_t>();
                const bool equal = leftNumber == rightNumber;
                return equal;
            }
            return left == right;
        }
        if (a.type() != b.type())
        {
            return false;
        }
        const bool equal = a == b;
        return equal;
    }

    // 매핑표에 넣을 수 있는 단일 값인지 확인합니다. 배열/객체/무한대는 거부합니다.
    void MessageInterface::checkScalar(const Json &value)
    {
        if (isIntegerValue(value))
        {
            // 숫자 문자열도 64비트 범위를 넘지 않는지 설정을 읽을 때 검사합니다.
            parseInteger(value);
        }
        bool matchesCondition = !(value.is_string() || value.is_boolean() || value.is_number());
        if (!matchesCondition)
        {
            bool matchesCondition2 = value.is_number_float();
            if (matchesCondition2)
            {
                const double convertedNumber = value.get<double>();
                matchesCondition2 = !std::isfinite(convertedNumber);
            }
            matchesCondition = matchesCondition2;
        }
        if (matchesCondition)
        {
            throw std::runtime_error("field mapping: expected finite number, string or boolean");
        }
    }

    // 매핑 하나의 from/to 목록과 미일치 처리(copy/default/error)를 읽습니다.
    void MessageInterface::configureMapping(FieldMapping &mapping, const Json &entry)
    {
        checkKeys(entry, {"values", "unmatched", "default"});
        const Json &values = entry.at("values");
        if (!values.is_array())
        {
            throw std::runtime_error("mapping values must be an array");
        }
        for (const Json &pair : values)
        {
            checkKeys(pair, {"from", "to"});
            const Json & fromJson = pair.at("from");
            checkScalar(fromJson);
            const Json & toJson = pair.at("to");
            checkScalar(toJson);
            for (const Json &previous : mapping._values)
            {
                const Json & fromJson2 = pair.at("from");
                const Json & fromJson3 = previous.at("from");
                if (sameValue(fromJson3, fromJson2))
                {
                    const Json & fromJson4 = pair.at("from");
                    const std::string valueText = fromJson4.dump();
                    throw std::runtime_error("duplicate mapping input: " + valueText);
                }
            }
            mapping._values.push_back(pair);
        }
        const std::string string58 = std::string("copy");
        mapping._unmatched = entry.value("unmatched", string58);
        if (mapping._unmatched != "copy" && mapping._unmatched != "default" && mapping._unmatched != "error")
        {
            throw std::runtime_error("unmatched must be copy, default or error");
        }
        if (mapping._unmatched == "default")
        {
            const Json & defaultJson = entry.at("default");
            checkScalar(defaultJson);
            mapping._defaultValue = entry.at("default");
        }
        else
        {
            if (entry.contains("default"))
            {
                throw std::runtime_error("default requires unmatched=default");
            }
        }
    }

    // 입력과 일치하는 from을 찾아 to를 반환합니다. 일치하지 않으면 unmatched 설정을 따릅니다.
    Json MessageInterface::mapValue(const FieldMapping &mapping, const Json &source)
    {
        // 흐름: values에서 from과 같은 값을 찾으면 to 반환 → 없으면 unmatched(copy/default/error) 정책 적용.

        for (const Json &pair : mapping._values)
        {
            const Json & fromJson = pair.at("from");
            if (sameValue(source, fromJson))
            {
                const Json result = pair.at("to");
                return result;
            }
        }
        if (mapping._unmatched == "default")
        {
            return mapping._defaultValue;
        }
        if (mapping._unmatched == "error")
        {
            const std::string valueText = source.dump();
            throw std::runtime_error("no mapped value for " + valueText);
        }
        return source;
    }

    // interface JSON과 같은 폴더를 기준으로 mapping_file 경로를 해석해 읽습니다.
    Json MessageInterface::readMappingFile(const Json &definition, const std::string &path)
    {
        if (!definition.contains("mapping_file"))
        {
            Json result = Json::object();
            result["mappings"] = Json::object();
            return result;
        }
        const std::string name = readName(definition, "mapping_file");
        const std::filesystem::path interfacePath(path);
        const std::filesystem::path folderPath = interfacePath.parent_path();
        const std::filesystem::path mappingName(name);
        const std::filesystem::path resolved = folderPath / mappingName;
        const std::string resolvedPath = resolved.string();
        const Json result = readJsonFile(resolvedPath);
        return result;
    }

    // 공통 매핑 정의를 이름 -> FieldMapping 사전으로 만듭니다.
    std::map<std::string, MessageInterface::FieldMapping>
    MessageInterface::loadMappingTables(const Json &mappingDefinitions)
    {
        // 흐름: 공용 mappings 검사 → 이름마다 FieldMapping 생성 → 이름을 키로 하는 map에 보관.

        checkKeys(mappingDefinitions, {"mappings"});
        const Json &named = mappingDefinitions.at("mappings");
        if (!named.is_object())
        {
            throw std::runtime_error("mappings must be an object");
        }
        std::map<std::string, FieldMapping> tables;
        for (Json::const_iterator item = named.begin(); item != named.end(); ++item)
        {
            const std::string itemName = item.key();
            if (itemName.empty())
            {
                throw std::runtime_error("empty mapping name");
            }
            FieldMapping table;
            try
            {
                const Json & itemValue = item.value();
                // 값 매핑표의 중복과 미일치 처리 설정을 검사합니다.
                configureMapping(table, itemValue);
            }
            catch (const std::exception &error)
            {
                const std::string itemName2 = item.key();
                const char * what63 = error.what();
                throw std::runtime_error("mapping " + itemName2 + ": " + what63);
            }
            const std::string itemName3 = item.key();
            tables.emplace(itemName3, table);
        }
        return tables;
    }

    // 필드 변환 설정 하나를 읽습니다. 경로 복사 / 매핑 / 고정값을 여기서 구분합니다.
    MessageInterface::FieldMapping MessageInterface::readFieldMapping(
        const Json &mappingEntry, const std::map<std::string, FieldMapping> &tables)
    {
        checkKeys(mappingEntry, {"from", "to", "mapping", "value"});
        FieldMapping mapping;
        mapping._sourceField = readName(mappingEntry, "from");
        const Json &destination = mappingEntry.at("to");
        bool matchesCondition = destination.is_number();
        if (!matchesCondition)
        {
            bool matchesCondition2 = destination.is_boolean();
            if (!matchesCondition2)
            {
                bool matchesCondition3 = destination.is_string();
                if (matchesCondition3)
                {
                    matchesCondition3 = isIntegerValue(destination);
                }
                matchesCondition2 = matchesCondition3;
            }
            matchesCondition = matchesCondition2;
        }
        if (matchesCondition)
        {
            // 간단 표기: {"from":"body.position.y","to":3}
            // 대상의 같은 경로에 3을 넣습니다. 원본 메시지의 값은 바꾸지 않습니다.
            mapping._targetField = mapping._sourceField;
            mapping._fixed = true;
            mapping._fixedValue = destination;
            if (mappingEntry.contains("value"))
            {
                throw std::runtime_error("literal to cannot also have value");
            }
        }
        else
        {
            mapping._targetField = readName(mappingEntry, "to");
            if (mappingEntry.contains("value"))
            {
                // 경로가 다르면 to에 대상 경로, value에 지정할 상수를 씁니다.
                mapping._fixed = true;
                mapping._fixedValue = mappingEntry.at("value");
            }
        }
        bool matchesCondition4 = mapping._fixed;
        if (matchesCondition4)
        {
            matchesCondition4 = mappingEntry.contains("mapping");
        }
        if (matchesCondition4)
        {
            throw std::runtime_error("fixed value cannot also have mapping");
        }
        if (mappingEntry.contains("mapping"))
        {
            const Json &reference = mappingEntry.at("mapping");
            if (reference.is_string())
            {
                const std::string name = readName(mappingEntry, "mapping");
                // tables는 매핑 이름 → 변환표 사전입니다. find는 이름으로 찾되 없는 항목을 새로 만들지 않습니다.
                // found == tables.end()이면 없는 이름이고, found->second가 실제 FieldMapping입니다.
                const std::map<std::string, FieldMapping>::const_iterator found = tables.find(name);
                if (found == tables.end())
                {
                    throw std::runtime_error("unknown mapping name: " + name);
                }
                mapping._values = found->second._values;
                mapping._unmatched = found->second._unmatched;
                mapping._defaultValue = found->second._defaultValue;
            }
            else
            {
                // 값 매핑표의 중복과 미일치 처리 설정을 검사합니다.
                configureMapping(mapping, reference);
            }
            // 이전 inline 설정 호환
        }
        return mapping;
    }

    // 원본/대상과 필드별 변환 규칙을 검사해 _rules에 보관합니다.
    void MessageInterface::loadRules(const Json &definition,
        const std::map<std::string, FieldMapping> &tables)
    {
        // 흐름: 규칙 이름 중복 검사 → 원본/대상 기록 → 필드 설정과 constants 검사 → 완성된 규칙 보관.

        const Json &interfaces = definition.at("interfaces");
        bool matchesCondition = !interfaces.is_array();
        if (!matchesCondition)
        {
            matchesCondition = interfaces.empty();
        }
        if (matchesCondition)
        {
            throw std::runtime_error("interfaces must be a nonempty array");
        }

        // 이 set은 이미 등록한 변환 규칙 이름을 기억합니다. 같은 이름이면 어떤 규칙을 쓸지 모호해지므로 거부합니다.
        std::set<std::string> ruleNames;
        for (const Json &entry : interfaces)
        {
            checkKeys(entry, {"name", "source", "target", "fields", "constants"});
            const Json & sourceJson = entry.at("source");
            checkKeys(sourceJson, {"link", "message"});
            const Json & targetJson = entry.at("target");
            checkKeys(targetJson, {"link", "message"});

            Rule rule;
            rule._name = readName(entry, "name");
            const Json & sourceJson2 = entry.at("source");
            rule._sourceLink = readName(sourceJson2, "link");
            const Json & sourceJson3 = entry.at("source");
            rule._sourceMessage = readName(sourceJson3, "message");
            const Json & targetJson2 = entry.at("target");
            rule._targetLink = readName(targetJson2, "link");
            const Json & targetJson3 = entry.at("target");
            rule._targetMessage = readName(targetJson3, "message");
            const Json object71 = Json::object();
            rule._constants = entry.value("constants", object71);

            if (!ruleNames.insert(rule._name).second)
            {
                throw std::runtime_error("duplicate conversion rule: " + rule._name);
            }
            if (!rule._constants.is_object())
            {
                throw std::runtime_error("constants must be an object");
            }

            const Json &mappings = entry.at("fields");
            if (!mappings.is_array())
            {
                throw std::runtime_error("interface fields must be an array");
            }

            // 같은 대상 필드를 두 번 쓰면 실수를 숨길 수 있으므로 거부합니다.
            // 이 set은 한 규칙에서 이미 쓰기로 한 대상 필드 경로를 기억합니다.
            // 같은 대상 필드에 두 번 쓰면 뒤의 값이 앞의 값을 덮어 오타를 숨길 수 있어서 중복을 거부합니다.
            std::set<std::string> targetFields;
            for (const Json &mappingEntry : mappings)
            {
                const FieldMapping mapping = readFieldMapping(mappingEntry, tables);
                // 같은 대상 필드에 두 번 쓰는 실수를 설정 단계에서 찾습니다.
                if (!targetFields.insert(mapping._targetField).second)
                {
                    throw std::runtime_error("duplicate target field: " + mapping._targetField);
                }
                rule._mappings.push_back(mapping);
            }
            for (Json::const_iterator constant = rule._constants.begin(); constant != rule._constants.end();
                ++constant)
            {
                const std::string itemName = constant.key();
                bool matchesCondition2 = itemName.empty();
                if (!matchesCondition2)
                {
                    const std::string itemName2 = constant.key();
                    matchesCondition2 = !targetFields.insert(itemName2).second;
                }
                if (matchesCondition2)
                {
                    const std::string itemName3 = constant.key();
                    throw std::runtime_error("duplicate or empty constant target: " + itemName3);
                }
            }
            _rules.push_back(rule);
        }
    }
    // 모든 생성자가 공유하는 변환 정의 검사와 저장입니다.
    void MessageInterface::initialize(const Json &definition, const Json &mappingDefinitions)
    {

        checkKeys(definition, {"interfaces", "mapping_file"});
        if (definition.contains("mapping_file"))
        {
            readName(definition, "mapping_file");
        }
        // 2. 공통 매핑표를 먼저 읽습니다. 이름으로 여러 규칙에서 재사용합니다.
        const std::map<std::string, FieldMapping> tables = loadMappingTables(mappingDefinitions);
        loadRules(definition, tables);
    }
}
