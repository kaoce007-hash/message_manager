#include "MessageInterface.hpp"
#include "MessageManager.hpp"
#include <filesystem>

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
        source.set("header.id", 100);
        source.set("header.version", 1);
        source.set("header.state", 1);
        source.set("body.position.x", -10);
        source.set("body.position.y", 20);
        source.set("body.temperature", 25.5f);
        source.set("body.name", "DEV1");
        const std::vector<int> vector1131 = std::vector<int>({10, 25, 30});
        source.set("body.samples", vector1131);
        const message_manager::Json preparedValue2 = message_manager::Json::parse(R"([{"x":-1,"y":2},{"x":3,"y":4}])");
        source.set("body.points", preparedValue2);
        const std::vector<std::uint8_t> copiedElements = std::vector<std::uint8_t>({0, 16, 255, 32, 48});
        source.set("body.payload", copiedElements);
    }

    void testAutomaticReceive(const std::string &folder)
    {
        message_manager::MessageManager sender;
        message_manager::MessageManager receiver;
        load(sender, folder);
        load(receiver, folder);
        message_manager::Message &source = sender.message("link_message1", "message1");
        const unsigned int convertedNumber = source.get<unsigned int>("header.msgcode");
        check(convertedNumber == 1001, "code initialized by JSON");
        fill(source);
        const std::vector<std::uint8_t> bytes = source.serialize();
        const std::size_t bytesCount = bytes.size();
        check(bytesCount == 52, "aligned status packet size");
        check(bytes[0] == 0x03 && bytes[1] == 0xE9, "big endian msgcode1001 bytes");
        const std::string beforeIdentify = receiver.dumpAll();
        // 헤더 코드로 링크와 메시지 이름을 알아냅니다.
        const message_manager::MessageIdentification identified = receiver.identify("link_message1", bytes);
        check(identified._msgcode == 1001 && identified._messageName == "message1", "identify header");
        const bool checkResult1 = beforeIdentify == receiver.dumpAll();
        check(checkResult1, "identify does not change message data");

        const message_manager::MessageIdentification received = receiver.receive("link_message1", bytes);
        check(received._linkName == "link_message1", "returned link");
        check(received._messageName == "message1" && received._msgcode == 1001, "returned message identity");
        message_manager::Message &stored = receiver.message(received._linkName, received._messageName);
        const std::vector<std::uint8_t> serializedBytes = stored.serialize();
        check(serializedBytes == bytes, "matched message updated");
        const std::int16_t convertedNumber2 = stored.get<std::int16_t>("body.position.x");
        check(convertedNumber2 == -10, "nested data decoded");
        const std::vector<std::uint8_t> decodedPayload = stored.get<std::vector<std::uint8_t>>("body.payload");
        const std::size_t decodedLength = decodedPayload.size();
        check(decodedLength == 5, "dynamic payload decoded");
        const std::string dumpText1 = receiver.dumpAll();
        const bool checkResult2 = dumpText1.find("Link1MessageCode::STATUS") != std::string::npos;
        check(checkResult2, "code enum in dump");

        const message_manager::MessageIdentification automatic = receiver.receive(bytes);
        check(automatic._linkName == "link_message1" && automatic._messageName == "message1",
            "automatic link and message detection");
        const bool checkResult3 = receiver.identify(bytes)._msgcode == 1001;
        check(checkResult3, "automatic identify without mutation");

        message_manager::Message &command = sender.message("link_message1", "message2");
        const unsigned int convertedNumber3 = command.get<unsigned int>("header.msgcode");
        check(convertedNumber3 == 1002, "second code initialized automatically");
        command.set("sequence", 42);
        command.set("enabled", 1);
        const std::vector<std::uint8_t> commandBytes = command.serialize();
        const std::size_t commandBytesCount = commandBytes.size();
        check(commandBytesCount == 12, "command aligned size");
        const std::size_t commandBytesCount2 = commandBytes.size();
        const std::uint8_t *commandBuffer = commandBytes.data();
        const message_manager::MessageIdentification commandId = receiver.receive("link_message1", commandBuffer, commandBytesCount2);
        check(commandId._messageName == "message2" && commandId._msgcode == 1002,
            "buffer overload routes command");
        const message_manager::Message & message1141 = receiver.message("link_message1", "message2");
        const unsigned int convertedNumber4 = message1141.get<unsigned int>("sequence");
        check(convertedNumber4 == 42,
            "second message updated");
        const std::vector<std::uint8_t> serializedBytes2 = stored.serialize();
        check(serializedBytes2 == bytes, "other message unaffected");

        message_manager::MessageInterface converter =
        message_manager::MessageInterface::fromFile(folder + "/interface_message.json");
        message_manager::Message &target = sender.convert(converter, "link1_to_link2_message1");
        const unsigned int convertedNumber5 = target.get<unsigned int>("header.msgcode");
        check(convertedNumber5 == 2001, "conversion keeps target msgcode");
        const std::vector<std::uint8_t> littleBytes = target.serialize();
        const std::size_t littleBytesCount = littleBytes.size();
        check(littleBytesCount == 77 && littleBytes[0] == 0xD1 && littleBytes[1] == 0x07,
            "little endian routing code");
        const message_manager::MessageIdentification targetId = receiver.receive(littleBytes);
        check(targetId._msgcode == 2001 && targetId._messageName == "message1", "second link automatic route");
        message_manager::Message reverse = converter.convert("link2_to_link1_message1", target, sender);
        const std::vector<std::uint8_t> serializedBytes3 = reverse.serialize();
        check(serializedBytes3 == bytes, "round trip with different link codes");
        message_manager::Message &ack = sender.message("link_message2", "message2");
        ack.set("sequence", 42);
        ack.set("result", 0);
        const std::vector<std::uint8_t> serializedBytes4 = ack.serialize();
        const bool checkResult4 = receiver.receive("link_message2", serializedBytes4)._msgcode == 2002;
        check(checkResult4, "ack automatic receive");

        std::vector<std::uint8_t> invalid = bytes;
        invalid[0] = 0xFF;
        invalid[1] = 0xFF;
        {
            bool caughtException = false;
            try
            {
                receiver.receive("link_message1", invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "unknown code");
        };
        {
            bool caughtException = false;
            try
            {
                const std::vector<std::uint8_t> copiedElements = std::vector<std::uint8_t>({3});
                receiver.receive("link_message1", copiedElements);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "truncated code header");
        };
        {
            bool caughtException = false;
            try
            {
                receiver.receive("link_message1", nullptr, 5);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "null buffer");
        };
        {
            bool caughtException = false;
            try
            {
                receiver.receive("link_message1", nullptr, 0);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "empty buffer");
        };
        {
            bool caughtException = false;
            try
            {
                receiver.receive("missing_link", bytes);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "unknown link");
        };
        invalid = bytes;
        invalid.pop_back();
        const bool checkResult5 = receiver.identify("link_message1", invalid)._msgcode == 1001;
        check(checkResult5, "identify only validates header");
        {
            bool caughtException = false;
            try
            {
                receiver.receive("link_message1", invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "truncated body");
        };
        invalid = bytes;
        invalid.push_back(0);
        {
            bool caughtException = false;
            try
            {
                receiver.receive("link_message1", invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "trailing bytes");
        };
        invalid = bytes;
        invalid[12] = 0xFF;
        invalid[13] = 0xFF;
        {
            bool caughtException = false;
            try
            {
                receiver.receive("link_message1", invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "invalid payload length");
        };
        const std::vector<std::uint8_t> serializedBytes5 = stored.serialize();
        check(serializedBytes5 == bytes, "bad receives preserve previous message");
        {
            bool caughtException = false;
            try
            {
                source.set("header.msgcode", 1002);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "cannot change bound msgcode");
        };
        const std::vector<std::uint8_t> serializedBytes6 = source.serialize();
        check(serializedBytes6 == bytes, "bad msgcode set is atomic");
        // 여러 링크가 같은 헤더/코드를 사용하면 명시적 링크가 필요합니다.
        message_manager::Json duplicateLink = message_manager::readJsonFile(folder + "/link_message1.json");
        duplicateLink["link"] = "same_code_link";
        const std::string valueText = duplicateLink.dump(4);
        message_manager::writeTextFile("test_output/ambiguous_link.json", valueText);
        receiver.loadLink("test_output/ambiguous_link.json");
        const std::string beforeAmbiguous = receiver.dumpAll();
        {
            bool caughtException = false;
            try
            {
                receiver.receive(bytes);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "ambiguous link rejected");
        };
        const bool checkResult6 = receiver.dumpAll() == beforeAmbiguous;
        check(checkResult6, "ambiguous receive changes no messages");
        const bool checkResult7 = receiver.receive("link_message1", bytes)._messageName == "message1";
        check(checkResult7, "explicit link resolves ambiguity");

        receiver.receive("link_message1", "message1", bytes);
        const std::vector<std::uint8_t> serializedBytes7 = stored.serialize();
        check(serializedBytes7 == bytes, "explicit receive remains supported");
    }

    message_manager::Json probeDefinition()
    {
        const message_manager::Json result = message_manager::Json::parse(R"({
            "link":"probe", "alignment":4, "endian":"big",
            "dispatch":{"field":"header.msgcode"},
            "types":{
                "Header":{"fields":[
                    {"name":"prefix","type":"uint8_t"},
                    {"name":"msgcode","type":"uint32_t","endian":"little"}
                ]},
                "Packet":{"fields":[
                    {"name":"header","type":"Header"},
                    {"name":"value","type":"uint16_t"}
                ]}
            },
            "messages":{
                "one":{"type":"Packet","msgcode":287454020},
                "two":{"type":"Packet","msgcode":1432778632}
            }
            })");
        return result;
    }

    void testDefinitionValidation()
    {
        const message_manager::Json definition = probeDefinition();
        message_manager::MessageManager link(definition);
        message_manager::Message one = link.create("one");
        one.setLogHandler(nullptr);
        one.set("header.prefix", 9);
        one.set("value", 7);
        const std::vector<std::uint8_t> bytes = one.serialize();
        const std::size_t bytesCount = bytes.size();
        check(bytesCount == 12 && bytes[4] == 0x44 && bytes[7] == 0x11, "aligned nonzero code offset");
        const bool checkResult8 = link.identify(bytes)._messageName == "one";
        check(checkResult8, "schema-derived offset and field endian");
        message_manager::Message two = link.create("two");
        two.set("header.prefix", 9);
        two.set("value", 8);
        const std::vector<std::uint8_t> before = two.serialize();
        {
            bool caughtException = false;
            try
            {
                two.deserialize(bytes);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "same-size wrong identity rejected");
        };
        const std::vector<std::uint8_t> serializedBytes = two.serialize();
        check(serializedBytes == before, "wrong identity receive rollback");
        message_manager::Json invalid = definition;
        invalid["messages"]["two"]["msgcode"] = invalid["messages"]["one"]["msgcode"];
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageManager bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "duplicate code");
        };
        invalid = definition;
        invalid["messages"]["one"]["msgcode"] = -1;
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageManager bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "negative code");
        };
        invalid = definition;
        invalid["messages"]["one"]["msgcode"] = 4294967296ULL;
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageManager bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "code width overflow");
        };
        invalid = definition;
        invalid["messages"]["one"].erase("msgcode");
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageManager bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "missing code");
        };
        invalid = definition;
        invalid["messages"]["one"] = "Packet";
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageManager bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "old entry with dispatch rejected");
        };
        invalid = definition;
        invalid["dispatch"]["field"] = "missing.code";
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageManager bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "missing code field");
        };
        invalid = definition;
        invalid["types"]["Header"]["fields"][1]["type"] = "int32_t";
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageManager bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "signed code field");
        };
        invalid = definition;
        invalid["types"]["OtherHeader"] = invalid["types"]["Header"];
        invalid["types"]["OtherHeader"]["fields"][1]["endian"] = "big";
        invalid["types"]["OtherPacket"] = invalid["types"]["Packet"];
        invalid["types"]["OtherPacket"]["fields"][0]["type"] = "OtherHeader";
        invalid["messages"]["two"]["type"] = "OtherPacket";
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageManager bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "inconsistent code endian");
        };
        invalid["types"]["OtherHeader"]["fields"][1]["endian"] = "little";
        invalid["types"]["OtherHeader"]["fields"][0]["type"] = "uint64_t";
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageManager bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "inconsistent code offset");
        };
        invalid = definition;
        invalid.erase("dispatch");
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageManager bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "code without field definition");
        };

        message_manager::Json dynamic = message_manager::Json::parse(R"({
            "link":"dynamic", "dispatch":{"field":"msgcode"},
            "types":{"Packet":{"fields":[
                {"name":"length","type":"uint8_t"},
                {"name":"data","type":"bytes","length_from":"length"},
                {"name":"msgcode","type":"uint16_t"}
            ]}}, "messages":{"one":{"type":"Packet","msgcode":1}}
            })");
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageManager bad(dynamic);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "code after dynamic bytes rejected");
        };
        const message_manager::Json legacy = message_manager::Json::parse(R"({
            "link":"legacy", "types":{"Packet":{"fields":[{"name":"x","type":"uint8_t"}]}},
            "messages":{"one":"Packet"}
            })");
        message_manager::MessageManager legacyLink(legacy);
        const bool checkResult9 = !legacyLink.canIdentify();
        check(checkResult9, "legacy definitions supported without automatic routing");
        {
            bool caughtException = false;
            try
            {
                const std::vector<std::uint8_t> copiedElements = std::vector<std::uint8_t>({1});
                // 헤더 코드로 링크와 메시지 이름을 알아냅니다.
                legacyLink.identify(copiedElements);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "legacy link requires explicit receive");
        };
        message_manager::Json wide = definition;
        wide["types"]["Header"]["fields"][1]["type"] = "uint64_t";
        wide["messages"]["one"]["msgcode"] = UINT64_MAX;
        wide["messages"]["two"]["msgcode"] = 0;
        message_manager::MessageManager wideLink(wide);
        message_manager::Message maximum = wideLink.create("one");
        maximum.set("header.prefix", 1);
        maximum.set("value", 2);
        const std::vector<std::uint8_t> serializedBytes2 = maximum.serialize();
        const bool checkResult10 = wideLink.identify(serializedBytes2)._msgcode == UINT64_MAX;
        check(checkResult10, "full uint64 code range");
        message_manager::Message zero = wideLink.create("two");
        zero.set("header.prefix", 1);
        zero.set("value", 2);
        const std::vector<std::uint8_t> serializedBytes3 = zero.serialize();
        const bool checkResult11 = wideLink.identify(serializedBytes3)._msgcode == 0;
        check(checkResult11, "zero code supported");
    }
}

using namespace message_manager;

int main(int argc, char **argv)
{
    try
    {
        std::string folder = "testdata/v5/json";
        if (argc > 1)
        {
            folder = argv[1];
        }
        std::filesystem::create_directories("test_output");
        testAutomaticReceive(folder);
        testDefinitionValidation();
        std::cout << "Passed ";
        std::cout << checks;
        std::cout << " msgcode checks\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what();
        std::cerr << '\n';
        return 1;
    }
    return 0;
}
