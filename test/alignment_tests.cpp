#include "MessageInterface.hpp"
#include "MessageManager.hpp"

#include <cstddef>

// 이 테스트는 컴파일러의 pack(4) 구조체 배치와 오프셋도 비교합니다.
#pragma pack(push, 4)
struct NativeInner
{
    std::uint8_t first;
    std::uint32_t number;
    std::uint8_t last;
};

struct NativeOuter
{
    std::uint8_t prefix;
    NativeInner items[2];
    std::uint16_t tail;
};
#pragma pack(pop)

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

    void testScalars()
    {
        const message_manager::Json original = message_manager::Json::parse(R"({"endian":"little","fields":[
            {"name":"a","type":"uint8_t"},
            {"name":"b","type":"uint32_t"},
            {"name":"c","type":"uint16_t"}
            ]})");
        for (int alignment : {1, 2, 4, 8})
        {
            message_manager::Json schema = original;
            schema["alignment"] = alignment;
            message_manager::Message message(schema);
            const std::uint8_t uint8_t2 = std::uint8_t(1);
            message.set("a", uint8_t2);
            const std::uint32_t uint32_t2 = std::uint32_t(0x11223344);
            message.set("b", uint32_t2);
            const std::uint16_t uint16_t2 = std::uint16_t(0x5566);
            message.set("c", uint16_t2);
            std::vector<std::uint8_t> expected;
            if (alignment == 1)
            {
                expected = {1, 0x44, 0x33, 0x22, 0x11, 0x66, 0x55};
            }
            else
            {
                if (alignment == 2)
                {
                    expected = {1, 0, 0x44, 0x33, 0x22, 0x11, 0x66, 0x55};
                }
                else
                {
                    expected = {1, 0, 0, 0, 0x44, 0x33, 0x22, 0x11, 0x66, 0x55, 0, 0};
                }
            }
            const std::vector<std::uint8_t> serializedBytes = message.serialize();
            check(serializedBytes == expected, "scalar alignment exact bytes");
            const std::size_t messageCount = message.size();
            const std::size_t expectedCount = expected.size();
            check(messageCount == expectedCount, "padded size");
            const std::size_t expectedCount2 = expected.size();
            const bool checkResult1 = message.fixedSize() == expectedCount2;
            check(checkResult1, "fixed size includes pads");
            message.deserialize(expected);
            const std::uint32_t convertedNumber = message.get<std::uint32_t>("b");
            check(convertedNumber == 0x11223344, "aligned deserialize");
            const std::size_t convertedValue = static_cast<std::size_t>(alignment);
            const bool checkResult2 = message.alignment() == convertedValue;
            check(checkResult2, "alignment getter");
            if (alignment > 1)
            {
                const std::string valueText = message.dump();
                const bool checkResult3 = valueText.find("<padding before b>") != std::string::npos;
                check(checkResult3, "padding row");
                std::vector<std::uint8_t> dirty = expected;
                dirty[1] = 0xFF;
                message.deserialize(dirty);
                const std::vector<std::uint8_t> serializedBytes2 = message.serialize();
                check(serializedBytes2 == expected, "receive skips padding and serialization zero fills");
            }
            std::vector<std::uint8_t> truncated = expected;
            truncated.pop_back();
            {
                bool caughtException = false;
                try
                {
                    message.deserialize(truncated);

                }
                catch (const std::exception &)
                {
                    caughtException = true;
                }
                check(caughtException, "truncated aligned packet");
            };
            const std::vector<std::uint8_t> serializedBytes3 = message.serialize();
            check(serializedBytes3 == expected, "decode rollback with padding");
        }
        message_manager::Message defaultMessage(original);
        defaultMessage.set("a", 1);
        defaultMessage.set("b", 2);
        defaultMessage.set("c", 3);
        const std::size_t defaultMessageCount = defaultMessage.size();
        check(defaultMessageCount == 7, "omitted alignment keeps packed format");
        for (const message_manager::Json &invalid : message_manager::Json::array({0, 3, 16, -1, true, "0x3", 4.0}))
        {
            message_manager::Json schema = original;
            schema["alignment"] = invalid;
            {
                bool caughtException = false;
                try
                {
                    message_manager::Message bad(schema);

                }
                catch (const std::exception &)
                {
                    caughtException = true;
                }
                check(caughtException, "invalid alignment value");
            };
        }
        message_manager::Json doubleSchema = message_manager::Json::parse(R"({"alignment":8,"fields":[
            {"name":"a","type":"uint8_t"}, {"name":"b","type":"double"}
            ]})");
        message_manager::Message doubleMessage(doubleSchema);
        doubleMessage.set("a", 1);
        doubleMessage.set("b", 1.0);
        const std::size_t doubleMessageCount = doubleMessage.size();
        check(doubleMessageCount == 16, "double uses 8-byte alignment");
        const std::vector<std::uint8_t> serializedBytes4 = doubleMessage.serialize();
        doubleMessage.deserialize(serializedBytes4);
        const double convertedNumber2 = doubleMessage.get<double>("b");
        check(convertedNumber2 == 1.0, "double round trip");
    }

    void testStructArrays()
    {
        const message_manager::Json schema = message_manager::Json::parse(R"({
            "message":"Outer", "alignment":4, "endian":"little", "types":{
                "Inner":{"fields":[
                    {"name":"first","type":"uint8_t"},
                    {"name":"number","type":"uint32_t"},
                    {"name":"last","type":"uint8_t"}
                ]},
                "Outer":{"fields":[
                    {"name":"prefix","type":"uint8_t"},
                    {"name":"items","type":"Inner","count":2},
                    {"name":"tail","type":"uint16_t"}
                ]}
            }
            })");
        message_manager::Message message(schema);
        message.set("prefix", 1);
        message.set("items[0].first", 2);
        message.set("items[0].number", 0x11223344);
        message.set("items[0].last", 3);
        message.set("items[1].first", 4);
        message.set("items[1].number", 0x55667788);
        message.set("items[1].last", 5);
        message.set("tail", 0x1234);
        const std::vector<std::uint8_t> expected = {1,    0,    0, 0, 2, 0, 0,    0,    0x44, 0x33, 0x22,
            0x11, 3,    0, 0, 0, 4, 0,    0,    0,    0x88, 0x77,
            0x66, 0x55, 5, 0, 0, 0, 0x34, 0x12, 0,    0};
        const bool checkResult4 = offsetof(NativeOuter, items) == 4;
        check(checkResult4, "compiler nested struct alignment");
        check(sizeof(NativeInner) == 12, "compiler struct array stride");
        const bool checkResult5 = offsetof(NativeOuter, tail) == 28;
        check(checkResult5, "compiler trailing field offset");
        check(sizeof(NativeOuter) == 32, "compiler end padding");
        const std::vector<std::uint8_t> serializedBytes = message.serialize();
        check(serializedBytes == expected, "nested struct array exact bytes");
        const std::size_t messageCount = message.size();
        check(messageCount == sizeof(NativeOuter), "matches compiler pack4 size");
        message.deserialize(expected);
        const std::uint32_t convertedNumber = message.get<std::uint32_t>("items[1].number");
        check(convertedNumber == 0x55667788, "second struct index");
        const std::string valueText = message.dump();
        const bool checkResult6 = valueText.find("<padding before items[1].first>") != std::string::npos;
        check(checkResult6, "struct boundary padding shown before next field");
        const std::string valueText2 = message.dump();
        const bool checkResult7 = valueText2.find("<padding end message>") != std::string::npos;
        check(checkResult7, "message end padding shown");
    }

    void testPayload()
    {
        const message_manager::Json schema = message_manager::Json::parse(R"({"alignment":4,"endian":"little","fields":[
            {"name":"length","type":"uint16_t"},
            {"name":"payload","type":"bytes","length_from":"length","max_length":32},
            {"name":"tail","type":"uint32_t"}
            ]})");
        message_manager::Message message(schema);
        const std::uint32_t uint32_t2 = std::uint32_t(0x11223344);
        message.set("tail", uint32_t2);
        for (std::size_t length = 0; length < 18; ++length)
        {
            const std::vector<std::uint8_t> payload(length, 0xAA);
            message.set("payload", payload);
            const std::size_t tailOffset = ((2 + length + 3) / 4) * 4;
            const std::size_t messageCount = message.size();
            check(messageCount == tailOffset + 4, "dynamic padding follows payload length");
            const std::uint16_t convertedNumber = message.get<std::uint16_t>("length");
            check(convertedNumber == length, "length excludes alignment padding");
            const std::vector<std::uint8_t> bytes = message.serialize();
            check(bytes[tailOffset] == 0x44 && bytes[tailOffset + 3] == 0x11, "dynamic tail offset");
            message.deserialize(bytes);
            const std::vector<std::uint8_t> convertedElements = message.get<std::vector<std::uint8_t>>("payload");
            check(convertedElements == payload, "dynamic aligned round trip");
        }
        const bool checkResult8 = message.fixedSize() == 8;
        check(checkResult8, "minimum size is empty-payload layout");
        const message_manager::Json nested = message_manager::Json::parse(R"({"message":"Root","alignment":4,"types":{
            "Item":{"fields":[
                {"name":"length","type":"uint16_t"},
                {"name":"data","type":"bytes","length_from":"length","max_length":8}
            ]},
            "Root":{"fields":[{"name":"items","type":"Item","count":2}]}
            }})");
        message_manager::Message records(nested);
        const std::vector<std::uint8_t> copiedElements = std::vector<std::uint8_t>({0xAA});
        records.set("items[0].data", copiedElements);
        const std::vector<std::uint8_t> copiedElements2 = std::vector<std::uint8_t>({0xBB, 0xCC});
        records.set("items[1].data", copiedElements2);
        const std::vector<std::uint8_t> expected = {0, 1, 0xAA, 0, 0, 2, 0xBB, 0xCC};
        const std::vector<std::uint8_t> serializedBytes = records.serialize();
        check(serializedBytes == expected, "dynamic struct array end padding");
        records.deserialize(expected);
        const unsigned int convertedNumber2 = records.get<unsigned int>("items[1].length");
        check(convertedNumber2 == 2, "second dynamic struct length");
        message_manager::Json tooLarge = message_manager::Json::parse(R"({"alignment":4,"fields":[
            {"name":"a","type":"uint8_t"},
            {"name":"b","type":"uint32_t"},
            {"name":"text","type":"string","length":1}
            ]})");
        tooLarge["fields"][2]["length"] = message_manager::MaxMessageSize - 5;
        {
            bool caughtException = false;
            try
            {
                message_manager::Message bad(tooLarge);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "padding contributes to maximum size");
        };
    }

    void testLinks(const std::string &folder)
    {
        message_manager::MessageManager manager;
        manager.loadEnums(folder + "/message_enums.json");
        manager.loadLink(folder + "/link_message1.json");
        manager.loadLink(folder + "/link_message2.json");
        message_manager::Message &source = manager.message("link_message1", "message1");
        source.set("header.id", 100);
        source.set("header.version", 1);
        source.set("header.state", 1);
        source.set("body.position.x", -10);
        source.set("body.position.y", 20);
        source.set("body.temperature", 25.5f);
        source.set("body.name", "DEV1");
        const std::vector<int> vector1423 = std::vector<int>({10, 25, 30});
        source.set("body.samples", vector1423);
        const message_manager::Json preparedValue2 = message_manager::Json::parse(R"([{"x":-1,"y":2},{"x":3,"y":4}])");
        source.set("body.points", preparedValue2);
        const std::vector<std::uint8_t> copiedElements = std::vector<std::uint8_t>({0, 16, 255, 32, 48});
        source.set("body.payload", copiedElements);
        const std::size_t sourceCount = source.size();
        const bool checkResult9 = source.alignment() == 4 && sourceCount == 48;
        check(checkResult9, "link1 alignment4 example");
        const bool checkResult10 = source.fixedSize() == 44;
        check(checkResult10, "link1 aligned empty payload size");
        const std::vector<std::uint8_t> original = source.serialize();
        check(original[7] == 0 && original[8] == 0 && original[9] == 5,
            "link1 alignment padding before payload length");
        check(original[10] == 0 && original[11] == 0 && original[47] == 0, "link1 struct padding");
        message_manager::MessageInterface converter =
        message_manager::MessageInterface::fromFile(folder + "/interface_message.json");
        message_manager::Message &target = manager.convert(converter, "link1_to_link2_message1");
        const std::size_t targetCount = target.size();
        const bool checkResult11 = target.alignment() == 1 && targetCount == 77;
        check(checkResult11, "link2 independent packed alignment");
        message_manager::Message restored = converter.convert("link2_to_link1_message1", target, manager);
        const std::vector<std::uint8_t> serializedBytes = restored.serialize();
        check(serializedBytes == original, "cross-alignment conversion round trip");
        manager.receive("link_message1", "message1", original);
        const std::vector<std::uint8_t> serializedBytes2 = source.serialize();
        check(serializedBytes2 == original, "manager receives aligned source");
        const std::string dumpText1 = manager.dumpAll();
        const bool checkResult12 = dumpText1.find("Alignment: 4") != std::string::npos;
        check(checkResult12, "dump shows link alignment");
    }
}

using namespace message_manager;

int main(int argc, char **argv)
{
    try
    {
        std::string folder = "testdata/json";
        if (argc > 1)
        {
            folder = argv[1];
        }
        testScalars();
        testStructArrays();
        testPayload();
        testLinks(folder);
        std::cout << "Passed ";
        std::cout << checks;
        std::cout << " alignment checks\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what();
        std::cerr << '\n';
        return 1;
    }
    return 0;
}
