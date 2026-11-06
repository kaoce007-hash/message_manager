#include "MessageInterface.hpp"
#include "MessageManager.hpp"

#include <filesystem>

static void throwingLog(const std::string &)
{
    throw std::runtime_error("logger failed");
}

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
        // 기존 220개 검증은 1바이트 배치를 그대로 검사합니다.
        // 새 예제 JSON의 alignment=4 검증은 alignment_tests.cpp에서 수행합니다.
        message_manager::Json packed = message_manager::readJsonFile(folder + "/link_message1.json");
        packed["alignment"] = 1;
        const std::string valueText = packed.dump(4);
        message_manager::writeTextFile("test_output/packed_link_message1.json", valueText);
        manager.loadLink("test_output/packed_link_message1.json");
        manager.loadLink(folder + "/link_message2.json");
    }

    void fill(message_manager::Message &source)
    {
        const std::uint32_t uint32_t2 = std::uint32_t(100);
        source.set("header.id", uint32_t2);
        const std::uint16_t uint16_t2 = std::uint16_t(1);
        source.set("header.version", uint16_t2);
        const std::uint8_t uint8_t2 = std::uint8_t(1);
        source.set("header.state", uint8_t2);
        const std::int16_t int16_t1005 = std::int16_t(-10);
        source.set("body.position.x", int16_t1005);
        const std::int16_t int16_t1006 = std::int16_t(20);
        source.set("body.position.y", int16_t1006);
        source.set("body.temperature", 25.5f);
        source.set("body.name", "DEV1");
        const std::vector<std::uint16_t> copiedElements = std::vector<std::uint16_t>({10, 25, 30});
        source.set("body.samples", copiedElements);
        const message_manager::Json points = message_manager::Json::parse(R"([
            {"x": -1, "y": 2},
            {"x": 3, "y": 4}
            ])");
        source.set("body.points", points);
        const std::vector<std::uint8_t> copiedElements2 = std::vector<std::uint8_t>({0x00, 0x10, 0xFF, 0x20, 0x30});
        source.set("body.payload", copiedElements2);
    }

    // 각 표의 모든 데이터 줄에서 구분자의 화면상 위치가 같은지 확인합니다.
    // 한글 enum의 UTF-8 바이트 수와 표시 폭이 다른 경우도 포함합니다.
    void checkTableAlignment(const std::string &dump)
    {
        std::istringstream input(dump);
        std::string line;
        std::vector<std::size_t> expected;
        bool inTable = false;
        while (std::getline(input, line))
        {
            if (line.rfind("OFFSET", 0) == 0)
            {
                expected.clear();
                inTable = true;
            }
            if (!inTable)
            {
                continue;
            }
            if (line.find(" | ") == std::string::npos)
            {
                if (line.find("-+-") == std::string::npos)
                {
                    inTable = false;
                }
                continue;
            }
            std::vector<std::size_t> columns;
            std::size_t position = line.find(" | ");
            while (position != std::string::npos)
            {
                const std::string columnPrefix = line.substr(0, position);
                // 문자열의 화면 표시 폭을 계산합니다.
                const std::size_t preparedValue6 = message_manager::displayWidth(columnPrefix);
                columns.push_back(preparedValue6);
                position = line.find(" | ", position + 3);
            }
            if (expected.empty())
            {
                expected = columns;
            }
            else
            {
                check(columns == expected, "aligned dump columns");
            }
        }
    }

    void testManager(const std::string &folder)
    {
        message_manager::MessageManager manager;
        load(manager, folder);
        const std::string emptyDump = manager.dumpAll();
        const bool checkResult1 = emptyDump.find("MESSAGES: 4") != std::string::npos;
        check(checkResult1, "all four messages registered");
        const bool checkResult2 = emptyDump.find("INCOMPLETE") != std::string::npos;
        check(checkResult2, "unset messages shown");
        const bool checkResult3 = emptyDump.find("<unset>") != std::string::npos;
        check(checkResult3, "unset fields shown");
        checkTableAlignment(emptyDump);

        logs.clear();
        manager.setLogHandler(
            collectLog);
        message_manager::Message &source = manager.message("link_message1", "message1");
        fill(source);
        const bool matchesCondition = logs.empty();
        check(matchesCondition, "successful set does not log");
        const std::size_t sourceCount = source.size();
        const bool checkResult4 = source.fixedSize() == 39 && sourceCount == 44;
        check(checkResult4, "dynamic payload contributes to size");
        const std::uint16_t convertedNumber = source.get<std::uint16_t>("header.payload_length");
        check(convertedNumber == 5, "automatic payload length");
        const std::vector<std::uint16_t> copiedElements = std::vector<std::uint16_t>({10, 25, 30});
        const std::vector<std::uint16_t> convertedElements = source.get<std::vector<std::uint16_t>>("body.samples");
        check(convertedElements ==
            copiedElements,
            "array get");
        const std::int16_t convertedNumber2 = source.get<std::int16_t>("body.points[0].x");
        check(convertedNumber2 == -1, "struct array index");
        const Json fieldValue = source.value("body.points");
        const Json & jsonValue = fieldValue.at(1);
        const Json & yJson = jsonValue.at("y");
        check(yJson == 4, "struct array bulk get");
        const std::uint16_t uint16_t2 = std::uint16_t(31);
        source.set("body.samples[2]", uint16_t2);
        const std::vector<std::uint16_t> convertedElements2 = source.get<std::vector<std::uint16_t>>("body.samples");
        check(convertedElements2[2] == 31, "single array element");
        const std::uint16_t uint16_t3 = std::uint16_t(30);
        source.set("body.samples[2]", uint16_t3);

        const std::vector<std::uint8_t> expected = {
            0x00, 0x00, 0x00, 0x64, 0x00, 0x01, 0x01, 0x00, 0x05, 0xFF, 0xF6, 0x00, 0x14, 0x41, 0xCC,
            0x00, 0x00, 0x44, 0x45, 0x56, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x19, 0x00,
            0x1E, 0xFF, 0xFF, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x10, 0xFF, 0x20, 0x30};
        const std::vector<std::uint8_t> serializedBytes = source.serialize();
        check(serializedBytes == expected, "exact big endian bytes with arrays and payload");
        message_manager::Message received = manager.create("link_message1", "message1");
        received.deserialize(expected);
        const Json fieldValue2 = source.value("body.points");
        const Json fieldValue3 = received.value("body.points");
        check(fieldValue3 == fieldValue2, "struct arrays decoded");
        const std::vector<std::uint8_t> copiedElements2 = std::vector<std::uint8_t>({0, 16, 255, 32, 48});
        const std::vector<std::uint8_t> convertedElements3 = received.get<std::vector<std::uint8_t>>("body.payload");
        check(convertedElements3 ==
            copiedElements2,
            "binary payload preserves 00/FF");

        message_manager::MessageInterface converter =
        message_manager::MessageInterface::fromFile(folder + "/interface_message.json");
        message_manager::Message &target = manager.convert(converter, "link1_to_link2_message1");
        const std::size_t targetCount = target.size();
        check(targetCount == 77, "target variable size");
        const std::uint32_t convertedNumber3 = target.get<std::uint32_t>("packet.payload_length");
        check(convertedNumber3 == 5, "target length automatically updated");
        const std::vector<std::uint32_t> copiedElements3 = std::vector<std::uint32_t>({10, 25, 30});
        const std::vector<std::uint32_t> convertedElements4 = target.get<std::vector<std::uint32_t>>("payload.measurements");
        check(convertedElements4 ==
            copiedElements3,
            "array conversion widens elements");
        const std::int32_t convertedNumber4 = target.get<std::int32_t>("payload.tracks[0].east");
        check(convertedNumber4 == -1, "mapped struct array");
        const std::vector<std::uint8_t> targetBytes = target.serialize();
        check(targetBytes[0] == 0xD1 && targetBytes[1] == 0x07 && targetBytes[8] == 5,
            "little endian target prefix");
        message_manager::Message reverse = converter.convert("link2_to_link1_message1", target, manager);
        const std::vector<std::uint8_t> serializedBytes2 = reverse.serialize();
        check(serializedBytes2 == expected, "round trip exact original bytes");
        message_manager::Message binaryConversion =
        // 수신 바이트를 원본으로 읽은 다음 대상 메시지로 변환합니다.
        converter.convertBytes("link1_to_link2_message1", expected, manager);
        const std::vector<std::uint8_t> serializedBytes3 = binaryConversion.serialize();
        check(serializedBytes3 == targetBytes, "binary conversion");
        manager.receive("link_message1", "message1", expected);
        const std::vector<std::uint8_t> serializedBytes4 = source.serialize();
        check(serializedBytes4 == expected, "manager receive keeps registered object reference");

        message_manager::Message &ack = manager.message("link_message2", "message2");
        const std::uint32_t uint32_t2 = std::uint32_t(5);
        ack.set("sequence", uint32_t2);
        const std::int16_t int16_t1035 = std::int16_t(0);
        ack.set("result", int16_t1035);
        message_manager::Message &command = manager.message("link_message1", "message2");
        const std::uint32_t uint32_t3 = std::uint32_t(5);
        command.set("sequence", uint32_t3);
        const std::uint8_t uint8_t2 = std::uint8_t(1);
        command.set("enabled", uint8_t2);
        const std::string dump = manager.dumpAll();
        const bool checkResult5 = dump.find("DeviceState::RUNNING") != std::string::npos;
        check(checkResult5, "enum label shown");
        const bool checkResult6 = dump.find("ResultCode::정상") != std::string::npos;
        check(checkResult6, "Korean enum label shown");
        const bool checkResult7 = dump.find("body.samples[0]") != std::string::npos;
        check(checkResult7, "array element dump");
        const bool checkResult8 = dump.find("INCOMPLETE") == std::string::npos;
        check(checkResult8, "all current messages ready");
        checkTableAlignment(dump);
        std::ostringstream output;
        output << std::hex;
        manager.dumpAll(output);
        output << 16;
        const std::string str1038 = output.str();
        const std::string str1039 = output.str();
        const std::size_t str1039Count = str1039.size();
        const bool checkResult9 = str1038.substr(str1039Count - 2) == "10";
        check(checkResult9, "caller stream format preserved");
        // 전체 메시지의 표를 파일로 저장합니다.
        manager.dumpAllToFile("test_output/all_dump.txt");
        std::ifstream dumpFile("test_output/all_dump.txt");
        std::stringstream dumped;
        dumped << dumpFile.rdbuf();
        const std::string str1041 = dumped.str();
        check(str1041 == dump, "dump all file matches text");

        const std::uint8_t uint8_t3 = std::uint8_t(255);
        source.set("header.state", uint8_t3);
        const std::string dumpText1 = manager.dumpAll();
        const bool checkResult10 = dumpText1.find("DeviceState::UNKNOWN") != std::string::npos;
        check(checkResult10, "unknown enum retained");
        const std::uint8_t uint8_t4 = std::uint8_t(1);
        source.set("header.state", uint8_t4);
        const std::int16_t int16_t1044 = std::int16_t(-1);
        ack.set("result", int16_t1044);
        const std::string dumpText2 = manager.dumpAll();
        const bool checkResult11 = dumpText2.find("ResultCode::오류") != std::string::npos;
        check(checkResult11, "negative enum");
        const std::string dumpText3 = manager.dumpAll();
        checkTableAlignment(dumpText3);

        {
            bool caughtException = false;
            try
            {
                source.set("unknown", 1);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "unknown set field");
        };
        const std::string & lastLog4 = logs.back();
        const bool checkResult12 = lastLog4.find("field=unknown") != std::string::npos;
        check(checkResult12, "unknown field error logged");
        const std::string & lastLog5 = logs.back();
        const bool checkResult13 = lastLog5.find("link=link_message1 message=message1") != std::string::npos;
        check(checkResult13, "log identity");
        const std::size_t before = logs.size();
        const bool checkResult14 = !source.trySet("body.position.x", 40000);
        check(checkResult14, "trySet false on overflow");
        const std::size_t logsCount = logs.size();
        check(logsCount == before + 1, "exactly one log on trySet failure");
        const std::vector<std::uint8_t> serializedBytes5 = source.serialize();
        check(serializedBytes5 == expected, "failed scalar set preserves message");
        {
            bool caughtException = false;
            try
            {
                const std::vector<int> vector1047 = std::vector<int>({1, 2});
                source.set("body.samples", vector1047);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "wrong array count");
        };
        {
            bool caughtException = false;
            try
            {
                const std::vector<int> vector1048 = std::vector<int>({1, 2, -1});
                source.set("body.samples", vector1048);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "bad last element");
        };
        const std::vector<std::uint8_t> serializedBytes6 = source.serialize();
        check(serializedBytes6 == expected, "array set is atomic");
        {
            bool caughtException = false;
            try
            {
                source.set("body.samples[3]", 1);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "array index out of range");
        };
        message_manager::Json badPoints = source.value("body.points");
        badPoints[1]["y"] = 40000;
        {
            bool caughtException = false;
            try
            {
                source.set("body.points", badPoints);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "struct array set error");
        };
        const std::vector<std::uint8_t> serializedBytes7 = source.serialize();
        check(serializedBytes7 == expected, "struct array set is atomic");
        {
            bool caughtException = false;
            try
            {
                const std::vector<int> vector1051 = std::vector<int>({0, 256});
                source.set("body.payload", vector1051);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "payload invalid byte");
        };
        {
            bool caughtException = false;
            try
            {
                source.set("body.payload", "abc");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "payload requires bytes");
        };
        {
            bool caughtException = false;
            try
            {
                const std::vector<int> vector1052 = std::vector<int>(1025, 0);
                source.set("body.payload", vector1052);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "max payload length");
        };
        {
            bool caughtException = false;
            try
            {
                source.set("header.payload_length", 4);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "manual inconsistent length");
        };
        const std::vector<std::uint8_t> serializedBytes8 = source.serialize();
        check(serializedBytes8 == expected, "payload errors preserve length and bytes");

        std::vector<std::uint8_t> malformed = expected;
        malformed[7] = 0xFF;
        malformed[8] = 0xFF;
        {
            bool caughtException = false;
            try
            {
                received.deserialize(malformed);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "received length exceeds max");
        };
        malformed = expected;
        malformed.pop_back();
        {
            bool caughtException = false;
            try
            {
                received.deserialize(malformed);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "truncated payload");
        };
        malformed = expected;
        malformed.push_back(0);
        {
            bool caughtException = false;
            try
            {
                received.deserialize(malformed);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "trailing payload data");
        };
        const std::vector<std::uint8_t> serializedBytes9 = received.serialize();
        check(serializedBytes9 == expected, "decode errors preserve previous state");

        const std::vector<std::uint8_t> copiedElements4 = std::vector<std::uint8_t>();
        source.set("body.payload", copiedElements4);
        const std::size_t sourceCount2 = source.size();
        const unsigned int convertedNumber5 = source.get<unsigned int>("header.payload_length");
        check(sourceCount2 == 39 && convertedNumber5 == 0, "empty payload");
        const std::vector<std::uint8_t> serializedBytes10 = source.serialize();
        received.deserialize(serializedBytes10);
        const std::vector<std::uint8_t> decodedPayload = received.get<std::vector<std::uint8_t>>("body.payload");
        const bool isPayloadEmpty = decodedPayload.empty();
        check(isPayloadEmpty, "empty payload decoded");
        const std::vector<std::uint8_t> copiedElements5 = std::vector<std::uint8_t>(1024, 0xAA);
        source.set("body.payload", copiedElements5);
        const std::size_t sourceCount3 = source.size();
        check(sourceCount3 == 1063, "max payload accepted");
        const std::string valueText = source.dump();
        const bool checkResult15 = valueText.find("256/1063") != std::string::npos;
        check(checkResult15, "default hex preview limit");
        const std::string valueText2 = source.dump(0);
        const bool checkResult16 = valueText2.find("1063/1063") != std::string::npos;
        check(checkResult16, "full hex option");
        const std::string valueText3 = source.dump();
        checkTableAlignment(valueText3);
        const std::vector<std::uint8_t> copiedElements6 = std::vector<std::uint8_t>({0, 16, 255, 32, 48});
        source.set("body.payload", copiedElements6);

        const std::uint32_t uint32_t4 = std::uint32_t(70000);
        target.set("payload.measurements[2]", uint32_t4);
        {
            bool caughtException = false;
            try
            {
                manager.convert(converter, "link2_to_link1_message1");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "array conversion narrowing overflow");
        };
        const std::vector<std::uint8_t> serializedBytes11 = source.serialize();
        check(serializedBytes11 == expected, "failed manager conversion does not overwrite destination");
        {
            bool caughtException = false;
            try
            {
                manager.loadEnums(folder + "/message_enums.json");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "enums must load before links");
        };
        {
            bool caughtException = false;
            try
            {
                manager.loadLink(folder + "/link_message1.json");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "duplicate link");
        };
        {
            bool caughtException = false;
            try
            {
                manager.message("wrong", "message1");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "unknown manager message");
        };

        // 사용자 로거가 예외를 던져도 set 원래 오류와 기존 값이 보존됩니다.
        source.setLogHandler(
            throwingLog);
        {
            bool caughtException = false;
            try
            {
                source.set("body.position.x", 40000);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "throwing logger");
        };
        const std::vector<std::uint8_t> serializedBytes12 = source.serialize();
        check(serializedBytes12 == expected, "throwing logger leaves state unchanged");
    }

    void testSchema(const std::string &folder)
    {
        const message_manager::Json base = message_manager::Json::parse(R"({"fields":[
            {"name":"length","type":"uint8_t"},
            {"name":"payload","type":"bytes","length_from":"length","max_length":300},
            {"name":"tail","type":"uint16_t"}
            ]})");
        message_manager::Message message(base);
        message.setLogHandler(nullptr);
        const std::uint16_t uint16_t2 = std::uint16_t(0x1234);
        message.set("tail", uint16_t2);
        const std::vector<int> vector1070 = std::vector<int>({0xAA, 0xBB});
        message.set("payload", vector1070);
        const std::vector<std::uint8_t> serializedBytes = message.serialize();
        const std::vector<std::uint8_t> copiedElements = std::vector<std::uint8_t>({2, 0xAA, 0xBB, 0x12, 0x34});
        check(serializedBytes == copiedElements,
            "variable payload before fixed tail");
        const std::vector<std::uint8_t> serializedBytes2 = message.serialize();
        message.deserialize(serializedBytes2);
        const unsigned int convertedNumber = message.get<unsigned int>("tail");
        check(convertedNumber == 0x1234, "tail offset after dynamic bytes");
        {
            bool caughtException = false;
            try
            {
                const std::vector<int> vector1075 = std::vector<int>(256, 0);
                message.set("payload", vector1075);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "length field overflow");
        };
        const unsigned int convertedNumber2 = message.get<unsigned int>("length");
        check(convertedNumber2 == 2, "length overflow rollback");

        message_manager::Json invalid = base;
        invalid["fields"][1]["length_from"] = "tail";
        {
            bool caughtException = false;
            try
            {
                message_manager::Message bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "forward length reference");
        };
        invalid = base;
        invalid["fields"][0]["type"] = "int8_t";
        {
            bool caughtException = false;
            try
            {
                message_manager::Message bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "signed length field rejected");
        };
        invalid = base;
        invalid["fields"][1]["max_length"] = -1;
        {
            bool caughtException = false;
            try
            {
                message_manager::Message bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "negative max length");
        };
        invalid = base;
        invalid["fields"][1]["count"] = 2;
        {
            bool caughtException = false;
            try
            {
                message_manager::Message bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "payload count not supported");
        };
        invalid = base;
        invalid["fields"].push_back({{"name", "second"}, {"type", "bytes"}, {"length_from", "length"}});
        {
            bool caughtException = false;
            try
            {
                message_manager::Message bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "shared payload length rejected");
        };
        invalid = base;
        invalid["fields"][2]["count"] = 0;
        {
            bool caughtException = false;
            try
            {
                message_manager::Message bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "zero array count");
        };
        invalid["fields"][2]["count"] = 65537;
        {
            bool caughtException = false;
            try
            {
                message_manager::Message bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "array count limit");
        };
        invalid = base;
        invalid["fields"][2]["enum"] = "Missing";
        {
            bool caughtException = false;
            try
            {
                message_manager::Message bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "undefined enum");
        };

        const message_manager::Json nested = message_manager::Json::parse(R"({
            "message":"Root", "types":{
                "Item":{"fields":[
                    {"name":"length","type":"uint8_t"},
                    {"name":"data","type":"bytes","length_from":"length","max_length":10}
                ]},
                "Root":{"fields":[{"name":"items","type":"Item","count":2}]}
            }
            })");
        message_manager::Message records(nested);
        const std::vector<int> vector1077 = std::vector<int>({1, 2});
        records.set("items[0].data", vector1077);
        const std::vector<int> vector1078 = std::vector<int>({3});
        records.set("items[1].data", vector1078);
        const std::vector<std::uint8_t> serializedBytes3 = records.serialize();
        const std::vector<std::uint8_t> copiedElements2 = std::vector<std::uint8_t>({2, 1, 2, 1, 3});
        check(serializedBytes3 == copiedElements2,
            "relative lengths in struct array");
        const std::vector<std::uint8_t> serializedBytes4 = records.serialize();
        records.deserialize(serializedBytes4);
        const unsigned int convertedNumber3 = records.get<unsigned int>("items[1].length");
        check(convertedNumber3 == 1, "second dynamic struct decoded");

        message_manager::EnumDefinitions enums;
        enums.load(folder + "/message_enums.json");
        {
            bool caughtException = false;
            try
            {
                enums.load(folder + "/message_enums.json");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "duplicate enum");
        };
        message_manager::Json enumSchema = message_manager::Json::parse(R"({"fields":[
            {"name":"flags","type":"uint8_t","count":2,"enum":"DeviceState"}
            ]})");
        message_manager::Message flags(enumSchema, "test", "enum_array", enums);
        const std::vector<int> vector1083 = std::vector<int>({0, 2});
        flags.set("flags", vector1083);
        const std::string valueText = flags.dump();
        const bool checkResult17 = valueText.find("DeviceState::FAULT") != std::string::npos;
        check(checkResult17, "enum on array elements");
        enumSchema["fields"][0]["type"] = "uint16_t";
        {
            bool caughtException = false;
            try
            {
                message_manager::Message bad(enumSchema, "", "", enums);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "enum underlying type mismatch");
        };
        message_manager::writeTextFile("test_output/bad_enums.json", R"({"enums":{
            "Bad":{"type":"uint8_t","values":{"256":"TOO_LARGE"}}
            }})");
        {
            bool caughtException = false;
            try
            {
                enums.load("test_output/bad_enums.json");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "enum numeric overflow");
        };
        const message_manager::Json enumNumber = 1;
        const std::string jsonNumber = enums.label("DeviceState", enumNumber);
        check(jsonNumber == "DeviceState::RUNNING", "failed enum load atomic");

        const message_manager::Json precision = message_manager::Json::parse(R"({"fields":[{"name":"x","type":"float"}]})");
        message_manager::Message precise(precision);
        precise.set("x", 1.1);
        message_manager::Message decoded(precision);
        const std::vector<std::uint8_t> serializedBytes5 = precise.serialize();
        decoded.deserialize(serializedBytes5);
        const Json xSetting = decoded.value("x");
        const Json xSetting2 = precise.value("x");
        check(xSetting == xSetting2, "stored float equals wire precision");
    }

    void testInterfaces(const std::string &folder)
    {
        message_manager::MessageManager manager;
        load(manager, folder);
        manager.setLogHandler(nullptr);
        message_manager::Message &source = manager.message("link_message1", "message1");
        fill(source);
        const message_manager::Json definition = message_manager::readJsonFile(folder + "/interface_message.json");
        message_manager::Json invalid = definition;
        invalid["interfaces"][0]["fields"][1]["to"] = "packet.sequence";
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageInterface bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "duplicate mapped target");
        };
        invalid = definition;
        invalid["interfaces"][0]["constants"]["packet.sequence"] = 1;
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageInterface bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "constant target collision");
        };
        invalid = definition;
        invalid["interfaces"].push_back(invalid["interfaces"][0]);
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageInterface bad(invalid);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "duplicate rule name");
        };
        invalid = definition;
        invalid["interfaces"][0]["fields"][0]["from"] = "missing.field";
        message_manager::MessageInterface missing(invalid);
        {
            bool caughtException = false;
            try
            {
                missing.convert("link1_to_link2_message1", source, manager);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "missing mapping source");
        };
        invalid = definition;
        invalid["interfaces"][0]["constants"] = message_manager::Json::object();
        message_manager::MessageInterface incomplete(invalid);
        {
            bool caughtException = false;
            try
            {
                incomplete.convert("link1_to_link2_message1", source, manager);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "missing target constant");
        };
        message_manager::MessageInterface converter(definition);
        message_manager::Message wrong = manager.create("link_message2", "message1");
        {
            bool caughtException = false;
            try
            {
                converter.convert("link1_to_link2_message1", wrong, manager);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "wrong source link");
        };
        {
            bool caughtException = false;
            try
            {
                converter.convert("missing_rule", source, manager);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "unknown interface name");
        };
        const std::vector<std::uint8_t> before = source.serialize();
        invalid = definition;
        invalid["interfaces"][0]["target"]["link"] = "missing_link";
        message_manager::MessageInterface missingLink(invalid);
        {
            bool caughtException = false;
            try
            {
                missingLink.convert("link1_to_link2_message1", source, manager);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "missing destination link");
        };
        const std::vector<std::uint8_t> serializedBytes = source.serialize();
        check(serializedBytes == before, "interface errors leave source intact");
    }

    void testDefaultLogging()
    {
        const message_manager::Json schema = message_manager::Json::parse(R"({"fields":[{"name":"x","type":"int8_t"}]})");
        message_manager::Message source(schema, "test_link", "test_message");
        std::ostringstream logged;
        std::streambuf *logBuffer = logged.rdbuf();
        std::streambuf *previous = std::cerr.rdbuf(logBuffer);
        const bool success = source.trySet("x", 128);
        std::cerr.rdbuf(previous);
        check(!success, "default trySet failed");
        const std::string str1090 = logged.str();
        const bool checkResult18 = str1090.find("[Message::set] link=test_link message=test_message field=x") !=
        std::string::npos;
        check(checkResult18, "default stderr log");
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
        std::filesystem::create_directories("test_output");
        testManager(folder);
        testSchema(folder);
        testInterfaces(folder);
        testDefaultLogging();
        std::cout << "Passed ";
        std::cout << checks;
        std::cout << " checks\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what();
        std::cerr << '\n';
        return 1;
    }
    return 0;
}
