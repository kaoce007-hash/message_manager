#include "MessageInterface.hpp"
#include "MessageManager.hpp"

#include <cstring>

static std::vector<std::string> logs;
static void collectLog(const std::string &text)
{
    logs.push_back(text);
}

namespace message_manager
{
    std::size_t checks = 0;

    void check(bool condition, const std::string &description)
    {
        ++checks;
        if (!condition)
        {
            throw std::runtime_error("FAILED: " + description);
        }
    }

    void load(message_manager::MessageManager &manager, const std::string &folder)
    {
        manager.loadEnums(folder + "/message_enums.json");
        manager.loadLink(folder + "/link_message1.json");
        manager.loadLink(folder + "/link_message2.json");
        manager.setLogHandler(nullptr);
    }

    void fill(message_manager::Message &source)
    {
        const std::uint32_t uint32_t2 = std::uint32_t(100);
        source.set("header.id", uint32_t2);
        const std::uint16_t uint16_t2 = std::uint16_t(1);
        source.set("header.version", uint16_t2);
        const std::uint8_t uint8_t2 = std::uint8_t(1);
        source.set("header.state", uint8_t2);
        const std::int16_t int16_t1229 = std::int16_t(-10);
        source.set("body.position.x", int16_t1229);
        const std::int16_t int16_t1230 = std::int16_t(20);
        source.set("body.position.y", int16_t1230);
        source.set("body.temperature", 25.5f);
        source.set("body.name", "DEV1");
        // 구조체 또는 배열 정의의 자식 개수를 읽습니다.
        const std::size_t elementCount2 = source.elementCount("body.points");
        const int max = static_cast<int>(elementCount2);
        for (int i = 0; i < max; ++i)
        {
            const std::string numberText = std::to_string(i);
            const std::int16_t int16_t1233 = std::int16_t(i - 1);
            source.set("body.points[" + numberText + "].x", int16_t1233);
            const std::string numberText2 = std::to_string(i);
            const std::int16_t int16_t1235 = std::int16_t(i + 2);
            source.set("body.points[" + numberText2 + "].y", int16_t1235);
        }
        for (int i = 0; i < 3; ++i)
        {
            const std::string numberText3 = std::to_string(i);
            const std::uint16_t uint16_t3 = std::uint16_t((i + 1) * 10);
            source.set("body.samples[" + numberText3 + "]", uint16_t3);
        }
        const unsigned char buffer[] = {0x41, 0x00, 0xFF, 0x42, 0x43};
        source.set("body.data.buffer", buffer, sizeof(buffer));
        const std::uint32_t uint32_t3 = std::uint32_t(sizeof(buffer));
        source.set("body.data.length", uint32_t3);
    }

    void testAccessAndBuffer(const std::string &folder)
    {
        message_manager::MessageManager manager;
        load(manager, folder);
        message_manager::Message &source = manager.message("link_message1", "message1");
        fill(source);
        const message_manager::Message &view = source;
        const std::int16_t convertedNumber = view.get<std::int16_t>("body.points[0].x");
        check(convertedNumber == -1, "const indexed read");
        // 구조체 또는 배열 정의의 자식 개수를 읽습니다.
        const std::size_t elementCount2 = source.elementCount("body.points");
        check(elementCount2 == 2, "array size");
        const std::uint32_t convertedNumber2 = source.get<std::uint32_t>("body.data.length");
        check(convertedNumber2 == 5, "uint32 length set by caller");
        const std::size_t sourceCount = source.size();
        check(sourceCount == 56, "length plus inline buffer aligned layout");
        const std::vector<std::uint8_t> original = source.serialize();
        check(original[44] == 0 && original[47] == 5, "length wire bytes");
        check(original[48] == 0x41 && original[49] == 0 && original[50] == 0xFF,
            "wire contains data instead of pointer address");
        // 버퍼 내용을 독립된 char 벡터로 복사해 읽습니다.
        const std::vector<char> output = view.getBuffer("body.data.buffer");
        const unsigned char expected[] = {0x41, 0, 0xFF, 0x42, 0x43};
        const std::size_t outputCount = output.size();
        const std::size_t expectedSize = sizeof(expected);
        bool checkResult1 = outputCount == expectedSize;
        // 크기가 같은 경우에만 메모리 내용을 비교합니다.
        if (checkResult1)
        {
            const char *actualBytes = output.data();
            const int comparison = std::memcmp(actualBytes, expected, expectedSize);
            checkResult1 = comparison == 0;
        }
        check(checkResult1, "char buffer get preserves NUL and FF");

        logs.clear();
        manager.setLogHandler(
            collectLog);
        {
            bool caughtException = false;
            try
            {
                source.set("body.points[2].x", 1);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "out of range index");
        };
        {
            bool caughtException = false;
            try
            {
                source.set("body.points[-1].x", 1);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "negative index");
        };
        {
            bool caughtException = false;
            try
            {
                source.set("body.temperature[0]", 1);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "indexing non-array");
        };
        {
            bool caughtException = false;
            try
            {
                source.set("body.points", 1);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "array path rejects scalar assignment");
        };
        // const Message에는 set을 호출할 수 없습니다. 대신 읽기 결과를 확인합니다.
        const std::uint32_t convertedNumber3 = view.get<std::uint32_t>("header.id");
        check(convertedNumber3 == 100, "const direct read");
        const std::vector<std::uint8_t> serializedBytes = source.serialize();
        check(serializedBytes == original, "bad access leaves message intact");
        const std::size_t logCount = logs.size();
        {
            bool caughtException = false;
            try
            {
                source.set("body.data.buffer", nullptr, 5);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "null nonempty buffer");
        };
        const std::size_t logsCount = logs.size();
        const std::string & lastLog1 = logs.back();
        const bool checkResult2 = logsCount == logCount + 1 && lastLog1.find("body.data.buffer") != std::string::npos;
        check(checkResult2, "pointer error logged once");
        {
            bool caughtException = false;
            try
            {
                char data = 'x';
                source.set("body.data.buffer", &data, 1025);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "oversized pointer input checked before copy");
        };
        {
            bool caughtException = false;
            try
            {
                char data = 'x';
                source.set("body.position.x", &data, 1);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "pointer to non-buffer field rejected");
        };
        {
            bool caughtException = false;
            try
            {
                source.set("body.points[0].x", 40000);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "indexed range check");
        };
        const std::vector<std::uint8_t> serializedBytes2 = source.serialize();
        check(serializedBytes2 == original, "failed updates preserve values and length");
        char data[] = {'x', '\0', 'y'};
        source.set("body.data.buffer", data, sizeof(data));
        data[0] = 'z';
        const std::vector<char> bufferCopy2 = source.getBuffer("body.data.buffer");
        const bool checkResult3 = bufferCopy2[0] == 'x';
        check(checkResult3, "input buffer is owned copy");
        const std::uint32_t convertedNumber4 = source.get<std::uint32_t>("body.data.length");
        check(convertedNumber4 == 5, "buffer does not update length");
        {
            bool caughtException = false;
            try
            {
                source.serialize();

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "manual length mismatch rejected on serialization");
        };
        const std::uint32_t uint32_t2 = std::uint32_t(3);
        source.set("body.data.length", uint32_t2);
        const std::uint32_t convertedNumber5 = source.get<std::uint32_t>("body.data.length");
        check(convertedNumber5 == 3, "caller updates length");
        source.set("body.data.buffer", nullptr, 0);
        const std::uint32_t uint32_t3 = std::uint32_t(0);
        source.set("body.data.length", uint32_t3);
        const unsigned int convertedNumber6 = source.get<unsigned int>("body.data.length");
        check(convertedNumber6 == 0, "empty buffer length");
        const std::vector<char> bufferCopy3 = source.getBuffer("body.data.buffer");
        const bool matchesCondition = bufferCopy3.empty();
        check(matchesCondition, "empty get buffer");
        const std::size_t sourceCount2 = source.size();
        check(sourceCount2 == 48, "empty buffer padded size");
        fill(source);
        const std::int16_t int16_t1255 = std::int16_t(123);
        source.set("body.points[0].x", int16_t1255);
        const std::string dump = source.dump();
        const bool checkResult4 = dump.find("123") != std::string::npos;
        check(checkResult4, "dump reflects current value");
        const bool checkResult5 = dump.find("DeviceState::RUNNING") != std::string::npos;
        check(checkResult5, "dump enum string");
        const bool checkResult6 = dump.find("Link1MessageCode::STATUS") != std::string::npos;
        check(checkResult6, "dump code and enum label");
        const bool checkResult7 = dump.find("char*") != std::string::npos && dump.find("41 00 FF 42 43") != std::string::npos;
        check(checkResult7, "dump char pointer contents");
        const bool checkResult8 = dump.find("INCOMPLETE") == std::string::npos;
        check(checkResult8, "complete current values dumped");
        const std::uint8_t uint8_t2 = std::uint8_t(255);
        source.set("header.state", uint8_t2);
        const std::string valueText = source.dump();
        const bool checkResult9 = valueText.find("DeviceState::UNKNOWN") != std::string::npos;
        check(checkResult9, "unknown enum numeric preserved");
        const std::uint8_t uint8_t3 = std::uint8_t(1);
        source.set("header.state", uint8_t3);

        message_manager::MessageManager receiver;
        load(receiver, folder);
        const std::vector<std::uint8_t> serializedBytes3 = source.serialize();
        const message_manager::MessageIdentification found = receiver.receive(serializedBytes3);
        check(found._msgcode == 1001 && found._messageName == "message1", "inline header routing");
        const std::vector<std::uint8_t> serializedBytes4 = source.serialize();
        const message_manager::Message & message1261 = receiver.message(found._linkName, found._messageName);
        const std::vector<std::uint8_t> serializedBytes5 = message1261.serialize();
        check(serializedBytes5 == serializedBytes4,
            "inline message buffer receive round trip");
    }

    void testExplicitConversion(const std::string &folder)
    {
        message_manager::MessageManager manager;
        load(manager, folder);
        message_manager::Message &source = manager.message("link_message1", "message1");
        message_manager::Message &target = manager.message("link_message2", "message1");
        fill(source);
        const std::vector<std::uint8_t> original = source.serialize();
        message_manager::MessageInterface converter =
        message_manager::MessageInterface::fromFile(folder + "/interface_message.json");
        converter.convertInto(source, target);
        const std::vector<std::uint8_t> serializedBytes = source.serialize();
        check(serializedBytes == original, "explicit conversion preserves source");
        const unsigned int convertedNumber = target.get<unsigned int>("header.msgcode");
        check(convertedNumber == 2001, "target keeps its defined code");
        const std::int32_t convertedNumber2 = target.get<std::int32_t>("body.points[0].x");
        check(convertedNumber2 == -1, "whole struct array conversion");
        const std::uint32_t convertedNumber3 = target.get<std::uint32_t>("body.data.length");
        check(convertedNumber3 == 5, "buffer length conversion");
        const bool checkResult10 = target.getBuffer("body.data.buffer") == source.getBuffer("body.data.buffer");
        check(checkResult10, "char buffer conversion");
        const std::size_t targetCount = target.size();
        check(targetCount == 77, "explicit target wire format");
        message_manager::Message restored = manager.create("link_message1", "message1");
        converter.convertInto(target, restored);
        const std::vector<std::uint8_t> serializedBytes2 = restored.serialize();
        check(serializedBytes2 == original, "explicit reverse conversion");
        const std::vector<std::uint8_t> targetBefore = target.serialize();
        message_manager::Json badRules = message_manager::readJsonFile(folder + "/interface_message.json");
        badRules["interfaces"][0]["fields"].erase(9);
        message_manager::MessageInterface incomplete(badRules);
        {
            bool caughtException = false;
            try
            {
                incomplete.convertInto(source, target);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "missing buffer mapping does not reuse stale target data");
        };
        const std::vector<std::uint8_t> serializedBytes3 = target.serialize();
        check(serializedBytes3 == targetBefore, "target unchanged on conversion failure");
        const std::int32_t int32_t1270 = std::int32_t(40000);
        target.set("body.points[1].x", int32_t1270);
        const std::vector<std::uint8_t> wideBefore = target.serialize();
        {
            bool caughtException = false;
            try
            {
                converter.convertInto(target, source);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "narrowing conversion rejected");
        };
        const std::vector<std::uint8_t> serializedBytes4 = source.serialize();
        const std::vector<std::uint8_t> serializedBytes5 = target.serialize();
        check(serializedBytes4 == original && serializedBytes5 == wideBefore,
            "both messages preserved on failed conversion");
        message_manager::Message &wrongTarget = manager.message("link_message2", "message2");
        {
            bool caughtException = false;
            try
            {
                converter.convertInto(source, wrongTarget);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "wrong destination identity rejected");
        };
        message_manager::Json ambiguous = message_manager::readJsonFile(folder + "/interface_message.json");
        message_manager::Json duplicate = ambiguous["interfaces"][0];
        duplicate["name"] = "another_rule";
        ambiguous["interfaces"].push_back(duplicate);
        message_manager::MessageInterface multiple(ambiguous);
        {
            bool caughtException = false;
            try
            {
                multiple.convertInto(source, target);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "ambiguous rule");
        };
        multiple.convertInto("link1_to_link2_message1", source, target);
        const std::vector<std::uint8_t> serializedBytes6 = target.serialize();
        check(serializedBytes6 == targetBefore, "named explicit rule resolves ambiguity");
    }

    void testInlineDefinitions(const std::string &folder)
    {
        const message_manager::Json definition = message_manager::readJsonFile(folder + "/link_message1.json");
        const bool matchesCondition = definition.contains("types");
        check(!matchesCondition, "message JSON has no separate type registry");
        const Json & messagesJson = definition.at("messages");
        const Json & message1Json = messagesJson.at("message1");
        const bool matchesCondition2 = message1Json.contains("header");
        check(matchesCondition2, "header defined inside message");
        const Json & messagesJson2 = definition.at("messages");
        const Json & message1Json2 = messagesJson2.at("message1");
        const bool matchesCondition3 = message1Json2.contains("fields");
        check(matchesCondition3, "fields defined inside message");
        message_manager::EnumDefinitions enums;
        enums.load(folder + "/message_enums.json");
        message_manager::Json invalid = definition;
        invalid["messages"]["message1"]["fields"][0]["fields"][4]["count"] = 0;
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageManager bad(invalid, enums);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "inline bad array count");
        };
        invalid = definition;
        invalid["messages"]["message1"]["header"]["fields"][0]["type"] = "int16_t";
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageManager bad(invalid, enums);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "inline signed code rejected");
        };
        invalid = definition;
        invalid["messages"]["message1"]["fields"][0]["fields"][0].erase("fields");
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageManager bad(invalid, enums);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "struct fields required");
        };
        invalid = definition;
        invalid["messages"]["message1"]["unexpected"] = true;
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageManager bad(invalid, enums);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "inline unknown key");
        };
        const message_manager::Json emptyHeader = message_manager::Json::parse(R"({"link":"minimal","messages":{
            "one":{"fields":[{"name":"x","type":"uint32_t"}]}
            }})");
        message_manager::MessageManager simple(emptyHeader);
        message_manager::Message message = simple.create("one");
        message.set("x", 7);
        const std::vector<std::uint8_t> serializedBytes = message.serialize();
        const std::vector<std::uint8_t> copiedElements = std::vector<std::uint8_t>({0, 0, 0, 7});
        check(serializedBytes == copiedElements,
            "inline message without routing");
    }
}

using namespace message_manager;

int main(int argc, char **argv)
{
    try
    {
        std::string folder = "testdata/v6/json";
        if (argc > 1)
        {
            folder = argv[1];
        }
        testAccessAndBuffer(folder);
        testExplicitConversion(folder);
        testInlineDefinitions(folder);
        std::cout << "Passed ";
        std::cout << checks;
        std::cout << " v6 checks\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what();
        std::cerr << '\n';
        return 1;
    }
    return 0;
}
