#include "MessageInterface.hpp"
#include "MessageManager.hpp"
#include <filesystem>
#include <chrono>

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
        const std::int16_t int16_t1490 = std::int16_t(-10);
        source.set("body.position.x", int16_t1490);
        const std::int16_t int16_t1491 = std::int16_t(20);
        source.set("body.position.y", int16_t1491);
        source.set("body.temperature", 25.5f);
        source.set("body.name", "DEV1");
        for (int i = 0; i < 3; ++i)
        {
            const std::string numberText = std::to_string(i);
            const std::uint16_t uint16_t3 = std::uint16_t((i + 1) * 10);
            source.set("body.samples[" + numberText + "]", uint16_t3);
        }
        for (int i = 0; i < 2; ++i)
        {
            const std::string numberText2 = std::to_string(i);
            const std::int16_t int16_t1495 = std::int16_t(i - 1);
            source.set("body.points[" + numberText2 + "].x", int16_t1495);
            const std::string numberText3 = std::to_string(i);
            const std::int16_t int16_t1497 = std::int16_t(i + 2);
            source.set("body.points[" + numberText3 + "].y", int16_t1497);
        }
        const unsigned char buffer[] = {0x41, 0, 0xFF, 0x42, 0x43};
        source.set("body.data.buffer", buffer, sizeof(buffer));
        const std::uint32_t uint32_t3 = std::uint32_t(sizeof(buffer));
        source.set("body.data.length", uint32_t3);
    }

}
using namespace message_manager;

static void invalid(const message_manager::Json &bad)
{
    {
        bool caughtException = false;
        try
        {
            message_manager::MessageInterface test(bad);
        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException, "invalid inline configuration");
    };
}

static void expectNamedError(const message_manager::Json &d, const message_manager::Json &t)
{
    {
        bool caughtException = false;
        try
        {
            message_manager::MessageInterface test(d, t);
        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException, "invalid named configuration");
    };
}

int main(int argc, char **argv)
{
    try
    {
        const std::string folder = argc > 1 ? argv[1] : "json";
        message_manager::MessageManager manager;
        load(manager, folder);
        message_manager::Message &source = manager.message("link_message1", "message1");
        fill(source);
        const std::vector<std::uint8_t> original = source.serialize();
        message_manager::Json valid = message_manager::readJsonFile(folder + "/interface_message.json");
        const message_manager::Json shared = message_manager::readJsonFile(folder + "/field_mapping.json");
        valid.erase("mapping_file");
        for (message_manager::Json &rule : valid["interfaces"])
        {
            for (message_manager::Json &field : rule["fields"])
            {
                bool matchesCondition = field.contains("mapping");
                if (matchesCondition)
                {
                    matchesCondition = field["mapping"].is_string();
                }
                if (matchesCondition)
                {
                    const Json & mappingJson = field.at("mapping");
                    const Json & mappingsJson = shared.at("mappings");
                    const std::string mappingText = mappingJson.get<std::string>();
                    field["mapping"] = mappingsJson.at(mappingText);
                }
            }
        }
        message_manager::Json definition = valid;
        // Field 0 links header.id to header.sequence and owns its mapping.
        message_manager::MessageInterface converter = message_manager::MessageInterface::fromFile(folder + "/interface_message.json", manager);
        message_manager::Message target = converter.convert(source);
        const Json fieldValue = target.value("header.sequence");
        check(fieldValue == 1000, "inline numeric mapping");
        const Json fieldValue2 = target.value("body.data");
        const Json fieldValue3 = source.value("body.data");
        check(fieldValue2 == fieldValue3, "unmapped struct/buffer copy");
        const Json fieldValue4 = target.value("header.msgcode");
        check(fieldValue4 == 2001, "target dispatch code");
        const message_manager::Message convertedMessage = converter.convert(target);
        const std::vector<std::uint8_t> serializedBytes = convertedMessage.serialize();
        check(serializedBytes == original, "explicit reverse mapping");
        const std::vector<std::uint8_t> serializedBytes2 = source.serialize();
        check(serializedBytes2 == original, "source preserved");
        const std::vector<std::uint8_t> serializedBytes3 = target.serialize();
        // 수신 바이트를 원본으로 읽은 다음 대상 메시지로 변환합니다.
        const message_manager::Message convertBytes1510 = converter.convertBytes("link1_to_link2_message1", original, manager);
        const std::vector<std::uint8_t> serializedBytes4 = convertBytes1510.serialize();
        check(serializedBytes4 == serializedBytes3,
            "convertBytes uses inline mapping");
        const std::vector<std::uint8_t> serializedBytes5 = target.serialize();
        const message_manager::Message convertedMessage2 = manager.convert(converter, "link1_to_link2_message1");
        const std::vector<std::uint8_t> serializedBytes6 = convertedMessage2.serialize();
        check(serializedBytes6 == serializedBytes5, "manager mapping");
        message_manager::Message received = manager.create("link_message2", "message1");
        const std::vector<std::uint8_t> serializedBytes7 = target.serialize();
        received.deserialize(serializedBytes7);
        const Json fieldValue5 = received.value("header.sequence");
        check(fieldValue5 == 1000, "wire roundtrip");
        for (message_manager::Json &rule : definition["interfaces"])
        {
            for (message_manager::Json &field : rule["fields"])
            {
                field.erase("mapping");
            }
        }
        message_manager::MessageInterface direct = message_manager::MessageInterface(definition, manager);
        const message_manager::Message convertedMessage3 = direct.convert(source);
        const Json fieldValue6 = convertedMessage3.value("header.sequence");
        check(fieldValue6 == 100, "omitted mapping copies");
        source.set("header.id", 99);
        const message_manager::Message convertedMessage4 = converter.convert(source);
        const Json fieldValue7 = convertedMessage4.value("header.sequence");
        check(fieldValue7 == 99, "unmatched default copy");
        definition = valid;
        definition["interfaces"][0]["fields"][0]["mapping"]["unmatched"] = "default";
        definition["interfaces"][0]["fields"][0]["mapping"]["default"] = 77;
        const message_manager::MessageInterface convertedMessage5Converter(definition, manager);
        const message_manager::Message convertedMessage5 = convertedMessage5Converter.convert(source);
        const Json fieldValue8 = convertedMessage5.value("header.sequence");
        check(fieldValue8 == 77,
            "explicit fallback");
        definition["interfaces"][0]["fields"][0]["mapping"].erase("default");
        definition["interfaces"][0]["fields"][0]["mapping"]["unmatched"] = "error";
        message_manager::MessageInterface strict = message_manager::MessageInterface(definition, manager);
        const std::vector<std::uint8_t> before = target.serialize();
        {
            bool caughtException = false;
            try
            {
                strict.convertInto("link1_to_link2_message1", source, target);
            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "unmatched error");
        };
        const std::vector<std::uint8_t> serializedBytes8 = target.serialize();
        check(serializedBytes8 == before, "failed conversion atomic");

        definition = valid;
        message_manager::Json duplicate = definition["interfaces"][0]["fields"][0]["mapping"]["values"][0];
        definition["interfaces"][0]["fields"][0]["mapping"]["values"].push_back(duplicate);
        invalid(definition);
        definition = valid;
        definition["interfaces"][0]["fields"][0]["mapping"]["unmatched"] = "typo";
        invalid(definition);
        definition = valid;
        definition["interfaces"][0]["fields"][0]["mapping"]["unmatched"] = "default";
        invalid(definition);
        definition = valid;
        definition["interfaces"][0]["fields"][0]["mapping"]["default"] = 0;
        invalid(definition);
        definition = valid;
        definition["interfaces"][0]["fields"][0]["mapping"]["values"][0]["from"] = message_manager::Json::array();
        invalid(definition);
        definition = valid;
        definition["interfaces"][0]["fields"][0]["mapping"]["extra"] = 0;
        invalid(definition);
        definition = valid;
        definition["interfaces"][0]["fields"][0]["mapping"] = nullptr;
        invalid(definition);
        definition = valid;
        definition["interfaces"][0]["fields"][0]["mapping"] = message_manager::Json::array();
        invalid(definition);
        definition = valid;
        definition["interfaces"][0]["fields"][0]["mapping"]["values"] = message_manager::Json::object();
        invalid(definition);
        definition = valid;
        definition["interfaces"][0]["fields"][0]["mapping"]["values"][0].erase("to");
        invalid(definition);
        source.set("header.id", 100);
        definition = valid;
        definition["interfaces"][0]["fields"][0]["mapping"]["values"][2]["to"] = -1;
        message_manager::MessageInterface badOutput = message_manager::MessageInterface(definition, manager);
        {
            bool caughtException = false;
            try
            {
                badOutput.convertInto("link1_to_link2_message1", source, target);
            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "output range");
        };
        const std::vector<std::uint8_t> serializedBytes9 = target.serialize();
        check(serializedBytes9 == before, "invalid output atomic");
        definition = valid;
        definition["interfaces"][0]["fields"][0].erase("mapping");
        definition["interfaces"][0]["fields"][6]["mapping"] = message_manager::Json{
            {
                "values", message_manager::Json::array({message_manager::Json{{"from", "DEV1"}, {"to", "MAPPED"}}})
            }
        };
        const message_manager::MessageInterface convertedMessage6Converter(definition, manager);
        const message_manager::Message convertedMessage6 = convertedMessage6Converter.convert(source);
        const Json fieldValue9 = convertedMessage6.value("body.label");
        check(fieldValue9 == "MAPPED", "string mapping");
        const message_manager::MessageInterface convertedMessage7Converter(definition, manager);
        const message_manager::Message convertedMessage7 = convertedMessage7Converter.convert(source);
        const Json fieldValue10 = convertedMessage7.value("header.sequence");
        check(fieldValue10 == 100,
            "mapping isolated to owning field");
        definition = valid;
        definition["interfaces"][0]["fields"][0]["mapping"]["values"] = message_manager::Json::array();
        const message_manager::MessageInterface convertedMessage8Converter(definition, manager);
        const message_manager::Message convertedMessage8 = convertedMessage8Converter.convert(source);
        const Json fieldValue11 = convertedMessage8.value("header.sequence");
        check(fieldValue11 == 100, "empty table");
        const std::int64_t int64_t1531 = std::int64_t(100);
        const std::uint64_t uint64_t2 = std::uint64_t(100);
        definition["interfaces"][0]["fields"][0]["mapping"]["values"] = message_manager::Json::array({
                message_manager::Json{{"from", int64_t1531}, {"to", 101}}, message_manager::Json{{"from", uint64_t2}, {"to", 102}}});
        invalid(definition);
        const std::uint64_t maximumInput = std::numeric_limits<std::uint64_t>::max();
        definition["interfaces"][0]["fields"][0]["mapping"]["values"] = message_manager::Json::array({
                message_manager::Json{{"from", -1}, {"to", 1}}, message_manager::Json{{"from", maximumInput}, {"to", 2}},
                message_manager::Json{{"from", true}, {"to", 3}}, message_manager::Json{{"from", "100"}, {"to", 4}}, message_manager::Json{{"from", 100.0}, {"to", 5}}});
        // 숫자 문자열 "100"은 정수 100과 같아졌습니다. bool/float는 계속 별도 타입입니다.
        const message_manager::MessageInterface convertedMessage9Converter(definition, manager);
        const message_manager::Message convertedMessage9 = convertedMessage9Converter.convert(source);
        const Json fieldValue12 = convertedMessage9.value("header.sequence");
        check(fieldValue12 == 4,
            "integer strings match integers while bool and float stay distinct");
        definition = valid;
        definition["interfaces"][0]["fields"][5]["mapping"] = message_manager::Json{
            {
                "values", message_manager::Json::array({message_manager::Json{{"from", 25.5}, {"to", 50.5}}})
            }
        };
        const message_manager::MessageInterface convertedMessage10Converter(definition, manager);
        const message_manager::Message convertedMessage10 = convertedMessage10Converter.convert(source);
        const Json fieldValue13 = convertedMessage10.value("body.celsius");
        check(fieldValue13 == 50.5, "floating mapping");
        message_manager::MessageInterface unbound = message_manager::MessageInterface::fromFile(folder + "/interface_message.json");
        const message_manager::Message convertedMessage11 = unbound.convert("link1_to_link2_message1", source, manager);
        const Json fieldValue14 = convertedMessage11.value("header.sequence");
        check(fieldValue14 == 1000,
            "unbound converter");
        source.set("header.id", 1);
        const message_manager::Message convertedMessage12 = converter.convert(source);
        const Json fieldValue15 = convertedMessage12.value("header.sequence");
        check(fieldValue15 == 10, "first configured value");
        source.set("header.id", 2);
        const message_manager::Message convertedMessage13 = converter.convert(source);
        const Json fieldValue16 = convertedMessage13.value("header.sequence");
        check(fieldValue16 == 20, "second configured value");
        message_manager::Json namedDefinition = message_manager::readJsonFile(folder + "/interface_message.json");
        message_manager::Json namedTables = shared;
        source.set("header.id", 1);
        source.set("header.version", 1);
        namedDefinition["interfaces"][0]["fields"][1]["mapping"] = "id_to_sequence";
        message_manager::Json secondRule = namedDefinition["interfaces"][0];
        secondRule["name"] = "shared_rule";
        namedDefinition["interfaces"].push_back(secondRule);
        message_manager::MessageInterface namedConverter = message_manager::MessageInterface(namedDefinition, namedTables, manager);
        message_manager::Message mapped = namedConverter.convert("shared_rule", source);
        const Json fieldValue17 = mapped.value("header.sequence");
        const Json fieldValue18 = mapped.value("header.revision");
        check(fieldValue17 == 10 && fieldValue18 == 10,
            "one named mapping shared by fields and rules");
        const std::vector<std::uint8_t> serializedBytes10 = mapped.serialize();
        const message_manager::Message convertedMessage14 = namedConverter.convert("link1_to_link2_message1", source);
        const std::vector<std::uint8_t> serializedBytes11 = convertedMessage14.serialize();
        check(serializedBytes11 == serializedBytes10,
            "shared mapping consistent across rules");
        namedTables["mappings"]["id_to_sequence"]["values"][0]["to"] = 30;
        message_manager::MessageInterface updated = message_manager::MessageInterface(namedDefinition, namedTables, manager);
        message_manager::Message changed = updated.convert("shared_rule", source);
        const Json fieldValue19 = changed.value("header.sequence");
        const Json fieldValue20 = changed.value("header.revision");
        check(fieldValue19 == 30 && fieldValue20 == 30,
            "single table update affects every reference on reload");
        const message_manager::Message convertedMessage15 = namedConverter.convert("shared_rule", source);
        const Json fieldValue21 = convertedMessage15.value("header.sequence");
        check(fieldValue21 == 10,
            "converter owns configuration snapshot");

        message_manager::Json badNamed = namedDefinition;
        badNamed["interfaces"][0]["fields"][0]["mapping"] = "missing";
        expectNamedError(badNamed, shared);
        badNamed["interfaces"][0]["fields"][0]["mapping"] = "";
        expectNamedError(badNamed, shared);
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageInterface test(namedDefinition);
            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "missing external definitions");
        };
        message_manager::Json badTables = shared;
        badTables["mappings"]["unused"] = {{"values", message_manager::Json::object()}};
        expectNamedError(namedDefinition, badTables);
        badTables = shared;
        badTables["mappings"] = message_manager::Json::array();
        expectNamedError(namedDefinition, badTables);
        namedTables = shared;
        namedTables["mappings"]["id_to_sequence"]["unmatched"] = "default";
        namedTables["mappings"]["id_to_sequence"]["default"] = 77;
        source.set("header.id", 99);
        message_manager::MessageInterface fallback = message_manager::MessageInterface(namedDefinition, namedTables, manager);
        const message_manager::Message convertedMessage16 = fallback.convert("shared_rule", source);
        const Json fieldValue22 = convertedMessage16.value("header.sequence");
        check(fieldValue22 == 77, "named fallback");
        namedTables["mappings"]["id_to_sequence"].erase("default");
        namedTables["mappings"]["id_to_sequence"]["unmatched"] = "error";
        message_manager::MessageInterface namedStrict = message_manager::MessageInterface(namedDefinition, namedTables, manager);
        const std::vector<std::uint8_t> namedBefore = target.serialize();
        {
            bool caughtException = false;
            try
            {
                namedStrict.convertInto("shared_rule", source, target);
            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "named unmatched error");
        };
        const std::vector<std::uint8_t> serializedBytes12 = target.serialize();
        check(serializedBytes12 == namedBefore, "named conversion remains atomic");

        // Resolve the external path from the interface file, independently of CWD.
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        const std::chrono::steady_clock::duration elapsed = now.time_since_epoch();
        const std::chrono::steady_clock::duration::rep ticks = elapsed.count();
        const std::string tickText = std::to_string(ticks);
        const std::string folderName = "wire_named_mapping_" + tickText;
        const std::filesystem::path rootPath = std::filesystem::temp_directory_path();
        const std::filesystem::path directory = rootPath / folderName;
        std::filesystem::create_directories(directory / "interfaces");
        std::filesystem::create_directories(directory / "tables");
        namedDefinition = message_manager::readJsonFile(folder + "/interface_message.json");
        namedDefinition["mapping_file"] = "../tables/shared.json";
        std::ofstream(directory / "tables/shared.json") << shared.dump();
        std::ofstream(directory / "interfaces/a.json") << namedDefinition.dump();
        std::ofstream(directory / "interfaces/b.json") << namedDefinition.dump();
        source.set("header.id", 100);
        const std::filesystem::path firstInterface32874Path = directory / "interfaces/a.json";
        const std::string firstInterface32874Text = firstInterface32874Path.string();
        message_manager::MessageInterface a = message_manager::MessageInterface::fromFile(firstInterface32874Text, manager);
        const std::filesystem::path secondInterface33201Path = directory / "interfaces/b.json";
        const std::string secondInterface33201Text = secondInterface33201Path.string();
        message_manager::MessageInterface b = message_manager::MessageInterface::fromFile(secondInterface33201Text);
        const message_manager::Message convertedMessage17 = a.convert(source);
        const Json fieldValue23 = convertedMessage17.value("header.sequence");
        check(fieldValue23 == 1000, "relative external mapping path");
        const message_manager::Message convertedMessage18 = a.convert(source);
        const std::vector<std::uint8_t> serializedBytes13 = convertedMessage18.serialize();
        const message_manager::Message convertedMessage19 = b.convert("link1_to_link2_message1", source, manager);
        const std::vector<std::uint8_t> serializedBytes14 = convertedMessage19.serialize();
        check(serializedBytes14 == serializedBytes13,
            "two interface files share one JSON");
        std::filesystem::remove(directory / "tables/shared.json");
        {
            bool caughtException = false;
            try
            {
                const std::filesystem::path firstInterface34664Path = directory / "interfaces/a.json";
                const std::string firstInterface34664Text = firstInterface34664Path.string();
                message_manager::MessageInterface::fromFile(firstInterface34664Text);
            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "missing mapping file fails at load");
        };
        std::filesystem::remove_all(directory);
        std::cout << "Passed ";
        std::cout << checks;
        std::cout << " inline and named mapping checks\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what();
        std::cerr << '\n';
        return 1;
    }
}
