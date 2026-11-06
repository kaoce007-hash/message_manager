#include "EnumDefinitions.hpp"

namespace message_manager
{
    // 공통 enum 파일을 읽고 타입/값/중복을 검사합니다.
    void EnumDefinitions::load(const std::string &path)
    {
        // 흐름: enums 문서 읽기 → 각 enum의 타입과 숫자/이름 검사 → 모두 정상일 때 기존 정의에 반영.

        const Json document = readJsonFile(path);
        checkKeys(document, {"enums"});
        const Json &enums = document.at("enums");
        const bool isObject = enums.is_object();
        if (!isObject)
        {
            fail("enums must be an object");
        }
        // 실제 정의를 먼저 바꾸지 않고 작업본으로 검사를 시작합니다.
        // candidate는 값 복사본입니다. 참조가 아니므로 뒤쪽 enum 검사가 실패해도 기존 _definitions는 그대로입니다.
        Json candidate = _definitions;
        for (Json::const_iterator entry = enums.begin(); entry != enums.end(); ++entry)
        {
            const std::string enumName = entry.key();
            const Json &definition = entry.value();
            const bool validName = identifier(enumName);
            if (!validName)
            {
                fail("invalid or duplicate enum: " + enumName);
            }
            const bool alreadyDefined = candidate.contains(enumName);
            if (alreadyDefined)
            {
                fail("invalid or duplicate enum: " + enumName);
            }
            checkKeys(definition, {"type", "values"});
            const Json &typeJson = definition.at("type");
            const std::string declaredType = typeJson.get<std::string>();
            const std::string type = canonical(declaredType);
            const bool isInteger = integerType(type);
            if (!isInteger)
            {
                fail("enum type must be an integer: " + enumName);
            }
            const Json &values = definition.at("values");
            const bool isValueObject = values.is_object();
            if (!isValueObject)
            {
                fail("enum values must be a nonempty object");
            }
            const bool isEmpty = values.empty();
            if (isEmpty)
            {
                fail("enum values must be a nonempty object");
            }
            Json labels = Json::object();
            for (Json::const_iterator value = values.begin(); value != values.end(); ++value)
            {
                // 숫자 키는 문자열입니다. "50"과 "0x32"를 같은 정수로 해석합니다.
                const std::string inputKey = value.key();
                const Json keyJson = inputKey;
                const Json number = parseInteger(keyJson);
                integerBits(type, number);
                const std::string normalizedKey = number.dump();
                const bool duplicateValue = labels.contains(normalizedKey);
                if (duplicateValue)
                {
                    fail("duplicate enum numeric value: " + inputKey);
                }
                const Json &labelJson = value.value();
                const bool isLabelString = labelJson.is_string();
                if (!isLabelString)
                {
                    fail("invalid enum value/label: " + inputKey);
                }
                const std::string label = labelJson.get<std::string>();
                const bool emptyLabel = label.empty();
                if (emptyLabel)
                {
                    fail("invalid enum value/label: " + inputKey);
                }
                labels[normalizedKey] = label;
            }
            Json normalizedDefinition = definition;
            normalizedDefinition["values"] = labels;
            candidate[enumName] = normalizedDefinition;
        }
        // 모든 enum이 올바를 때 한 번에 저장합니다.
        _definitions = candidate;
    }

    // 필드가 참조한 enum의 존재와 타입을 확인합니다.
    void EnumDefinitions::validate(const std::string &enumName, const std::string &fieldType) const
    {
        const bool exists = _definitions.contains(enumName);
        if (!exists)
        {
            fail("undefined enum: " + enumName);
        }
        const Json &definition = _definitions.at(enumName);
        const Json &typeJson = definition.at("type");
        const std::string declaredType = typeJson.get<std::string>();
        const std::string type = canonical(declaredType);
        if (type != fieldType)
        {
            fail("enum type does not match field: " + enumName);
        }
    }

    // 현재 숫자에 해당하는 표시 이름을 찾습니다.
    std::string EnumDefinitions::label(const std::string &enumName, const Json &value) const
    {
        const bool hasNoEnum = enumName.empty();
        if (hasNoEnum)
        {
            return "-";
        }
        const Json &definition = _definitions.at(enumName);
        const Json &labels = definition.at("values");
        const Json number = parseInteger(value);
        const std::string key = number.dump();
        const bool exists = labels.contains(key);
        if (!exists)
        {
            const std::string unknown = enumName + "::UNKNOWN";
            return unknown;
        }
        const Json &labelJson = labels.at(key);
        const std::string labelText = labelJson.get<std::string>();
        const std::string result = enumName + "::" + labelText;
        return result;
    }
}
