#include "MessageInterface.hpp"
#include "MessageManager.hpp"
#include <filesystem>

namespace message_manager
{
    static unsigned checks = 0;

    static void check(bool condition, const std::string &description)
    {
        ++checks;
        if (!condition)
        {
            throw std::runtime_error("FAILED: " + description);
        }
    }

    // 단순한 일반 함수로 잘못된 숫자 표기가 거부되는지 확인합니다.
    static void expectBadInteger(const Json &input)
    {
        bool rejected = false;
        try
        {
            parseInteger(input);
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        const std::string valueText = input.dump();
        check(rejected, "bad integer rejected: " + valueText);
    }

    static void expectBadInterface(const Json &definition, const Json &tables)
    {
        bool rejected = false;
        try
        {
            MessageInterface converter(definition, tables);
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        check(rejected, "invalid interface rejected");
    }

    static void testNumberParser()
    {
        for (const Json &input : Json::array({50, "50", "0x32", "0X32", "+50", "+0x32", "050"}))
        {
            const std::uint64_t unsignedNumber = unsignedValue(input);
            check(unsignedNumber == 50, "equivalent positive integers");
            const std::int64_t signedNumber = signedValue(input);
            check(signedNumber == 50, "positive signed integers");
        }
        const std::int64_t signedNumber2 = signedValue("-0x32");
        check(signedNumber2 == -50, "negative hexadecimal");
        const std::int64_t signedNumber3 = signedValue("-50");
        check(signedNumber3 == -50, "negative decimal");
        const std::uint64_t unsignedNumber2 = unsignedValue("-0x0");
        check(unsignedNumber2 == 0, "negative zero");
        const std::uint64_t unsignedNumber3 = unsignedValue("0xffffffffffffffff");
        check(unsignedNumber3 == UINT64_MAX, "uint64 max hex");
        const std::uint64_t unsignedNumber4 = unsignedValue("18446744073709551615");
        check(unsignedNumber4 == UINT64_MAX, "uint64 max decimal");
        const std::int64_t signedNumber4 = signedValue("-0x8000000000000000");
        check(signedNumber4 == INT64_MIN, "int64 min hex");
        const std::int64_t signedNumber5 = signedValue("-9223372036854775808");
        check(signedNumber5 == INT64_MIN, "int64 min decimal");
        const std::int64_t signedNumber6 = signedValue("0x7fffffffffffffff");
        check(signedNumber6 == INT64_MAX, "int64 max hex");
        for (const Json &input : Json::array({"", "-", "+", "0x", "0xGG", "12x", "1.5", " 50", "50 ",
                "18446744073709551616", "0x10000000000000000", "-9223372036854775809",
                "-0x8000000000000001", true, nullptr, 1.5}))
        {
            expectBadInteger(input);
        }
        bool rejected = false;
        try
        {
            signedValue("0xffffffffffffffff");
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        check(rejected, "signed conversion cannot wrap");
        rejected = false;
        try
        {
            unsignedValue("-1");
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        check(rejected, "unsigned conversion cannot wrap");
    }

    static void testEveryIntegerWidth()
    {
        for (unsigned bits : {8, 16, 32, 64})
        {
            const std::string unsignedType = "uint" + std::to_string(bits) + "_t";
            const std::string signedType = "int" + std::to_string(bits) + "_t";
            const Json array1445 = Json::array({
                    Json{{"name", "u"}, {"type", unsignedType}},
                    Json{{"name", "s"}, {"type", signedType}}
                });
            Message message(Json{{"fields", array1445}});
            message.setLogHandler(nullptr);
            const std::uint64_t maximum = UINT64_MAX >> (64 - bits);
            const std::int64_t minimum = INT64_MIN >> (64 - bits);
            std::ostringstream hexMaximum;
            hexMaximum << "0x" << std::hex << maximum;
            const std::string str1446 = hexMaximum.str();
            message.set("u", str1446);
            const std::string numberText = std::to_string(minimum);
            message.set("s", numberText);
            Message decoded = message.emptyCopy();
            const std::vector<std::uint8_t> serializedBytes = message.serialize();
            decoded.deserialize(serializedBytes);
            const std::uint64_t convertedNumber = decoded.get<std::uint64_t>("u");
            check(convertedNumber == maximum, "unsigned width boundary");
            const std::int64_t convertedNumber2 = decoded.get<std::int64_t>("s");
            check(convertedNumber2 == minimum, "signed width boundary");
            const std::vector<std::uint8_t> before = message.serialize();
            const bool checkResult1 = !message.trySet("u", "-0x1");
            check(checkResult1, "unsigned negative rejected");
            const std::vector<std::uint8_t> serializedBytes2 = message.serialize();
            check(serializedBytes2 == before, "invalid set preserves bytes");
        }
        const Json array1452 = Json::array({
                Json{{"name", "number"}, {"type", "float"}},
                Json{{"name", "text"}, {"type", "string"}, {"length", "0x4"}}
            });
        Message message(Json{{"fields", array1452}});
        message.set("number", "0x32");
        message.set("text", "0x32");
        const float convertedNumber3 = message.get<float>("number");
        check(convertedNumber3 == 50.0f, "integer string into float field");
        const std::string messageText = message.get<std::string>("text");
        check(messageText == "0x32", "string field is not converted");
    }

    static void testFilesAndMappings(const std::string &folder)
    {
        MessageManager catalog;
        catalog.loadEnums(folder + "/message_enums.json");
        catalog.loadLink(folder + "/numeric_source.json");
        catalog.loadLink(folder + "/numeric_target.json");
        Message source = catalog.create("numeric_source", "packet");
        source.setLogHandler(nullptr);
        const Json document = readJsonFile(folder + "/source_values.json");
        const Json & bodyJson = document.at("body");
        source.set("body", bodyJson);
        const std::vector<std::uint8_t> original = source.serialize();
        const int convertedNumber = source.get<int>("body.position.y");
        check(convertedNumber == -50, "signed nested value");
        const Json fieldValue = source.value("body.status");
        const bool matchesCondition = fieldValue.is_number_integer();
        check(matchesCondition, "stored integer is normalized");
        const int convertedNumber2 = source.get<int>("body.status");
        check(convertedNumber2 == 50, "decimal string value");
        const std::vector<int> vector1461 = std::vector<int>({0, 255});
        const std::vector<int> vector1462 = source.get<std::vector<int>>("body.samples");
        check(vector1462 == vector1461, "array integer strings");
        const char firstByte = char(0xAB);
        const char secondByte = char(0xCD);
        const std::vector<char> vector1463 = std::vector<char>({firstByte, secondByte});
        const bool checkResult2 = source.getBuffer("body.buffer") == vector1463;
        check(checkResult2, "buffer integer strings");
        const int convertedNumber3 = source.get<int>("body.val1");
        const int convertedNumber4 = source.get<int>("body.val2");
        check(convertedNumber3 == 31 && convertedNumber4 == 2047, "bitfield integer strings");
        const std::string sourceText = source.get<std::string>("body.text");
        check(sourceText == "50", "numeric text remains text");
        const std::string valueText = source.dump();
        const bool checkResult3 = valueText.find("NumericState::FIFTY") != std::string::npos;
        check(checkResult3, "hexadecimal enum key");
        const bool checkResult4 = catalog.identify("numeric_source", original)._msgcode == 50;
        check(checkResult4, "hexadecimal msgcode");

        // 모든 정수 설정을 숫자 JSON으로 바꾼 정의와 전송 바이트가 같은지도 확인합니다.
        Json decimal = readJsonFile(folder + "/numeric_source.json");
        decimal["alignment"] = 4;
        decimal["messages"]["packet"]["msgcode"] = 50;
        Json &body = decimal["types"]["Body"]["fields"];
        body[2]["count"] = 2;
        body[4]["max_length"] = 4;
        body[5]["bits"] = 5;
        body[6]["bits"] = 11;
        body[7]["length"] = 4;
        EnumDefinitions enums;
        enums.load(folder + "/message_enums.json");
        MessageManager decimalManager(decimal, enums);
        Message decimalSource = decimalManager.create("packet");
        const Json bodySetting = source.value("body");
        decimalSource.set("body", bodySetting);
        const std::vector<std::uint8_t> serializedBytes = decimalSource.serialize();
        check(serializedBytes == original, "all numeric metadata produces same wire bytes");

        MessageInterface converter = MessageInterface::fromFile(folder + "/interface_message.json", catalog);
        Message target = converter.convert(source);
        const int convertedNumber5 = target.get<int>("body.position.y");
        check(convertedNumber5 == 3, "to:3 sets same target path");
        const int convertedNumber6 = target.get<int>("body.status");
        check(convertedNumber6 == 100, "mapping from:0x32 matches numeric 50");
        const std::vector<std::uint8_t> serializedBytes2 = source.serialize();
        check(serializedBytes2 == original, "fixed assignment does not modify source");

        Json definition = readJsonFile(folder + "/interface_message.json");
        const Json tables = readJsonFile(folder + "/field_mapping.json");
        definition["interfaces"][0]["fields"][0]["to"] = "0x3";
        MessageInterface hexFixed(definition, tables, catalog);
        const std::vector<std::uint8_t> serializedBytes3 = target.serialize();
        const message_manager::Message convertedMessage = hexFixed.convert(source);
        const std::vector<std::uint8_t> serializedBytes4 = convertedMessage.serialize();
        check(serializedBytes4 == serializedBytes3, "to hex fixed value");
        definition["interfaces"][0]["fields"][0]["to"] = "3";
        MessageInterface decimalFixed(definition, tables, catalog);
        const std::vector<std::uint8_t> serializedBytes5 = target.serialize();
        const message_manager::Message convertedMessage2 = decimalFixed.convert(source);
        const std::vector<std::uint8_t> serializedBytes6 = convertedMessage2.serialize();
        check(serializedBytes6 == serializedBytes5, "to decimal string fixed value");

        // 경로를 바꾸면서 상수를 넣는 긴 표기입니다.
        definition["interfaces"][0]["fields"][0]["to"] = "body.position.y";
        definition["interfaces"][0]["fields"][0]["value"] = "0x3";
        definition["interfaces"][0]["fields"][0]["from"] = "body.val1";
        MessageInterface explicitFixed(definition, tables, catalog);
        const std::vector<std::uint8_t> serializedBytes7 = target.serialize();
        const message_manager::Message convertedMessage3 = explicitFixed.convert(source);
        const std::vector<std::uint8_t> serializedBytes8 = convertedMessage3.serialize();
        check(serializedBytes8 == serializedBytes7, "different source/target paths with fixed value");
        definition["interfaces"][0]["fields"][0]["value"] = "0x8000";
        MessageInterface overflowFixed(definition, tables, catalog);
        const std::vector<std::uint8_t> before = target.serialize();
        bool rejected = false;
        try
        {
            overflowFixed.convertInto(source, target);
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        const std::vector<std::uint8_t> serializedBytes9 = target.serialize();
        check(rejected && serializedBytes9 == before, "fixed value overflow preserves target");
        definition["interfaces"][0]["fields"][0]["mapping"] = "state_to_hundred";
        expectBadInterface(definition, tables);
        definition["interfaces"][0]["fields"][0].erase("mapping");
        definition["interfaces"][0]["fields"][0]["to"] = 3;
        expectBadInterface(definition, tables);

        Json duplicateTables = tables;
        duplicateTables["mappings"]["state_to_hundred"]["values"].push_back(Json{{"from", 50}, {"to", 200}});
        const Json document2 = readJsonFile(folder + "/interface_message.json");
        expectBadInterface(document2, duplicateTables);

        // constants 항목의 정수 문자열도 대상 필드 타입에 맞춰 해석합니다.
        definition = readJsonFile(folder + "/interface_message.json");
        definition["interfaces"][0]["fields"].erase(0);
        definition["interfaces"][0]["constants"]["body.position.y"] = "-0x3";
        MessageInterface constants(definition, tables, catalog);
        const message_manager::Message convertedMessage4 = constants.convert(source);
        const int convertedNumber7 = convertedMessage4.get<int>("body.position.y");
        check(convertedNumber7 == -3, "constants hexadecimal integer");

        std::filesystem::create_directories("integer_format_output");
        const std::string enumPath = "integer_format_output/duplicate_enum.json";
        writeTextFile(enumPath, "{\"enums\":{\"Alias\":{\"type\":\"uint8_t\",\"values\":{\"50\":\"A\",\"0x32\":\"B\"}}}}");
        rejected = false;
        try
        {
            enums.load(enumPath);
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        check(rejected, "enum decimal/hex aliases are duplicates");
    }
}

int main(int argc, char **argv)
{
    try
    {
        const std::string folder = argc > 1 ? argv[1] : "json/integer_formats";
        message_manager::testNumberParser();
        message_manager::testEveryIntegerWidth();
        message_manager::testFilesAndMappings(folder);
        std::cout << "Passed ";
        std::cout << message_manager::checks;
        std::cout << " integer-format and fixed-value checks\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what();
        std::cerr << '\n';
        return 1;
    }
}
