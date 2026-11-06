#include "MessageInterface.hpp"
#include "MessageManager.hpp"

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
        const std::int16_t int16_t1161 = std::int16_t(-10);
        source.set("body.position.x", int16_t1161);
        const std::int16_t int16_t1162 = std::int16_t(20);
        source.set("body.position.y", int16_t1162);
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
            const std::int16_t int16_t1166 = std::int16_t(i - 1);
            source.set("body.points[" + numberText2 + "].x", int16_t1166);
            const std::string numberText3 = std::to_string(i);
            const std::int16_t int16_t1168 = std::int16_t(i + 2);
            source.set("body.points[" + numberText3 + "].y", int16_t1168);
        }
        const unsigned char buffer[] = {0x41, 0, 0xFF, 0x42, 0x43};
        source.set("body.data.buffer", buffer, sizeof(buffer));
        const std::uint32_t uint32_t3 = std::uint32_t(sizeof(buffer));
        source.set("body.data.length", uint32_t3);
    }

    void testManualLength(const std::string &folder)
    {
        message_manager::MessageManager manager;
        load(manager, folder);
        message_manager::Message &source = manager.message("link_message1", "message1");
        fill(source);
        const std::vector<std::uint8_t> original = source.serialize();
        const char smaller[] = {'x', 'y', 'z'};
        source.set("body.data.buffer", smaller, sizeof(smaller));
        const std::uint32_t convertedNumber = source.get<std::uint32_t>("body.data.length");
        check(convertedNumber == 5, "buffer does not change length");
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
            check(caughtException, "mismatched length is rejected at send");
        };
        const std::string valueText = source.dump();
        const bool checkResult1 = valueText.find("buffer length mismatch") != std::string::npos;
        check(checkResult1, "dump exposes manual mismatch");
        const std::uint32_t uint32_t2 = std::uint32_t(3);
        source.set("body.data.length", uint32_t2);
        const std::vector<char> bufferCopy1 = source.getBuffer("body.data.buffer");
        const std::size_t elementCount = bufferCopy1.size();
        check(elementCount == 3, "manual length leaves buffer intact");
        source.serialize();
        const std::uint32_t uint32_t3 = std::uint32_t(8);
        source.set("body.data.length", uint32_t3);
        const std::vector<char> bufferCopy2 = source.getBuffer("body.data.buffer");
        const std::size_t elementCount2 = bufferCopy2.size();
        check(elementCount2 == 3, "length does not resize buffer");
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
            check(caughtException, "length-first temporary state allowed but cannot send");
        };
        const char larger[] = {'1', '2', '3', '4', '5', '6', '7', '8'};
        source.set("body.data.buffer", larger, sizeof(larger));
        const std::uint32_t convertedNumber2 = source.get<std::uint32_t>("body.data.length");
        check(convertedNumber2 == 8, "length-first setting preserved");
        source.serialize();
        source.set("body.data.buffer", nullptr, 0);
        const std::uint32_t convertedNumber3 = source.get<std::uint32_t>("body.data.length");
        check(convertedNumber3 == 8,
            "empty buffer does not set zero length");
        const std::uint32_t uint32_t4 = std::uint32_t(0);
        source.set("body.data.length", uint32_t4);
        const std::vector<std::uint8_t> serializedBytes = source.serialize();
        const std::size_t serializedBytesCount = serializedBytes.size();
        check(serializedBytesCount == 48, "caller-configured empty buffer");
        const std::uint32_t uint32_t5 = std::uint32_t(7);
        source.set("body.data.length", uint32_t5);
        {
            bool caughtException = false;
            try
            {
                source.set("body.data.buffer", nullptr, 1);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "invalid pointer rejected");
        };
        const std::uint32_t convertedNumber4 = source.get<std::uint32_t>("body.data.length");
        check(convertedNumber4 == 7,
            "failed pointer update preserves length");
        const std::vector<char> bufferCopy3 = source.getBuffer("body.data.buffer");
        const bool matchesCondition = bufferCopy3.empty();
        check(matchesCondition, "failed pointer update preserves buffer");
        fill(source);
        const std::vector<std::uint8_t> serializedBytes2 = source.serialize();
        check(serializedBytes2 == original, "restore exact original bytes");

        message_manager::Message fresh = manager.create("link_message1", "message1");
        fresh.set("body.data.buffer", smaller, sizeof(smaller));
        {
            bool caughtException = false;
            try
            {
                fresh.get<std::uint32_t>("body.data.length");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "missing length not invented");
        };
    }

    message_manager::MessageInterface detachedConverter(const std::string &folder)
    {
        message_manager::MessageManager local;
        load(local, folder);
        const message_manager::MessageInterface result = message_manager::MessageInterface::fromFile(folder + "/interface_message.json", local);
        return result;
    }

    void testReturningConverter(const std::string &folder)
    {
        message_manager::MessageManager manager;
        load(manager, folder);
        message_manager::Message &source = manager.message("link_message1", "message1");
        fill(source);
        const std::vector<std::uint8_t> original = source.serialize();
        message_manager::MessageInterface converter =
        message_manager::MessageInterface::fromFile(folder + "/interface_message.json", manager);
        message_manager::Message target = converter.convert(source);
        const std::vector<std::uint8_t> serializedBytes = source.serialize();
        check(serializedBytes == original, "convert returns new object and preserves source");
        const std::string getLinkName1186 = target.getLinkName();
        check(getLinkName1186 == "link_message2", "target link name string");
        const std::string getMessageName1187 = target.getMessageName();
        check(getMessageName1187 == "message1", "target message name string");
        const std::string getLinkName1188 = source.getLinkName();
        check(getLinkName1188 == "link_message1", "source link metadata");
        const std::string getMessageName1189 = source.getMessageName();
        check(getMessageName1189 == "message1", "source message metadata");
        std::string name = source.getLinkName();
        name = "changed copy";
        const std::string getLinkName1190 = source.getLinkName();
        check(getLinkName1190 == "link_message1", "returned name is a value copy");
        const unsigned int convertedNumber = target.get<unsigned int>("header.msgcode");
        check(convertedNumber == 2001, "target code definition preserved");
        const std::uint32_t convertedNumber2 = target.get<std::uint32_t>("body.data.length");
        check(convertedNumber2 == 5, "manual length copied by mapping");
        const bool checkResult2 = target.getBuffer("body.data.buffer") == source.getBuffer("body.data.buffer");
        check(checkResult2, "binary buffer copied by mapping");
        const std::size_t targetCount = target.size();
        const std::size_t sourceCount = source.size();
        check(targetCount == 77 && sourceCount == 56, "link-specific wire layouts");
        message_manager::Message restored = converter.convert(target);
        const std::vector<std::uint8_t> serializedBytes2 = restored.serialize();
        check(serializedBytes2 == original, "returned reverse conversion");
        const std::int32_t int32_t1196 = std::int32_t(123);
        target.set("body.points[0].x", int32_t1196);
        const std::int16_t convertedNumber3 = source.get<std::int16_t>("body.points[0].x");
        check(convertedNumber3 == -1, "independent target values");
        message_manager::MessageInterface detached = detachedConverter(folder);
        message_manager::Message standalone = detached.convert(source);
        const std::string getLinkName1198 = standalone.getLinkName();
        check(getLinkName1198 == "link_message2",
            "owned catalog survives original manager lifetime");
        message_manager::MessageInterface unbound = message_manager::MessageInterface::fromFile(folder + "/interface_message.json");
        {
            bool caughtException = false;
            try
            {
                unbound.convert(source);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "unbound catalog detected");
        };
        message_manager::Message &command = manager.message("link_message1", "message2");
        {
            bool caughtException = false;
            try
            {
                converter.convert(command);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "missing source rule rejected");
        };
        message_manager::Json rules = message_manager::readJsonFile(folder + "/interface_message.json");
        message_manager::Json other = rules["interfaces"][0];
        other["name"] = "other_forward";
        rules["interfaces"].push_back(other);
        message_manager::MessageInterface ambiguous(rules, manager);
        {
            bool caughtException = false;
            try
            {
                ambiguous.convert(source);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "ambiguous target requires rule name");
        };
        message_manager::Message named = ambiguous.convert("link1_to_link2_message1", source);
        const std::string getLinkName1199 = named.getLinkName();
        check(getLinkName1199 == "link_message2", "explicit rule overload");
        const std::uint32_t uint32_t2 = std::uint32_t(4);
        source.set("body.data.length", uint32_t2);
        {
            bool caughtException = false;
            try
            {
                converter.convert(source);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "source manual length checked before conversion");
        };
        const std::uint32_t convertedNumber4 = source.get<std::uint32_t>("body.data.length");
        check(convertedNumber4 == 4,
            "conversion does not repair caller length");
        const std::uint32_t uint32_t3 = std::uint32_t(5);
        source.set("body.data.length", uint32_t3);
        const std::vector<std::uint8_t> serializedBytes3 = source.serialize();
        check(serializedBytes3 == original, "caller repairs source length");
    }

    void testSharedHeader(const std::string &folder)
    {
        const message_manager::Json definition = message_manager::readJsonFile(folder + "/link_message1.json");
        const Json & typesJson = definition.at("types");
        const bool matchesCondition = typesJson.contains("msgheader");
        check(matchesCondition, "common msgheader type exists");
        const Json & dispatchJson = definition.at("dispatch");
        const Json & fieldJson = dispatchJson.at("field");
        check(fieldJson == "header.msgcode", "dispatch uses header.msgcode");
        for (const char *messageName : {"message1", "message2"})
        {
            const Json & messagesJson = definition.at("messages");
            const message_manager::Json &message = messagesJson.at(messageName);
            const bool matchesCondition2 = message.contains("header");
            check(!matchesCondition2, "no separate header section in message");
            const Json & fieldsJson = message.at("fields");
            const Json & jsonValue = fieldsJson.at(0);
            const Json & nameJson = jsonValue.at("name");
            check(nameJson == "header", "header is an ordinary field");
            const Json & fieldsJson2 = message.at("fields");
            const Json & jsonValue2 = fieldsJson2.at(0);
            const Json & typeJson = jsonValue2.at("type");
            check(typeJson == "msgheader", "same type reused across messages");
        }
        message_manager::MessageManager manager;
        message_manager::MessageManager receiver;
        load(manager, folder);
        load(receiver, folder);
        message_manager::Message &source = manager.message("link_message1", "message1");
        fill(source);
        const std::vector<std::uint8_t> serializedBytes = source.serialize();
        const bool checkResult3 = receiver.receive(serializedBytes)._msgcode == 1001;
        check(checkResult3, "shared header routes status");
        message_manager::Message &command = manager.message("link_message1", "message2");
        const std::uint32_t uint32_t2 = std::uint32_t(7);
        command.set("header.id", uint32_t2);
        const std::uint16_t uint16_t2 = std::uint16_t(1);
        command.set("header.version", uint16_t2);
        const std::uint8_t uint8_t2 = std::uint8_t(1);
        command.set("header.state", uint8_t2);
        const std::uint32_t uint32_t3 = std::uint32_t(9);
        command.set("sequence", uint32_t3);
        const std::uint8_t uint8_t3 = std::uint8_t(1);
        command.set("enabled", uint8_t3);
        const std::size_t commandCount = command.size();
        check(commandCount == 20, "common header participates in command layout");
        const std::vector<std::uint8_t> serializedBytes2 = command.serialize();
        const message_manager::MessageIdentification detected = receiver.receive(serializedBytes2);
        check(detected._messageName == "message2" && detected._msgcode == 1002, "shared header routes command");
        const message_manager::Message &received = receiver.message(detected._linkName, detected._messageName);
        const unsigned int convertedNumber = received.get<unsigned int>("header.id");
        check(convertedNumber ==
            7,
            "shared header fields received");
        const std::string valueText = source.dump();
        const bool checkResult4 = valueText.find("DeviceState::RUNNING") != std::string::npos;
        check(checkResult4, "current enum dump retained");
    }
}

using namespace message_manager;

int main(int argc, char **argv)
{
    try
    {
        std::string folder = "json";
        if (argc > 1)
        {
            folder = argv[1];
        }
        testManualLength(folder);
        testReturningConverter(folder);
        testSharedHeader(folder);
        std::cout << "Passed ";
        std::cout << checks;
        std::cout << " v7 checks\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what();
        std::cerr << '\n';
        return 1;
    }
    return 0;
}
