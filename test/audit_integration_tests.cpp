#include "MessageInterface.hpp"
#include "MessageManager.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <filesystem>
#include <random>
#include <sys/socket.h>
#include <unistd.h>

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

    void testCheckedReads()
    {
        const message_manager::Json schema = message_manager::Json::parse(R"({"fields":[
            {"name":"u","type":"uint16_t"},
            {"name":"s","type":"int16_t"},
            {"name":"a","type":"uint16_t","count":2},
            {"name":"d","type":"double"},
            {"name":"wide","type":"uint64_t"}
            ]})");
        message_manager::Message message(schema);
        message.set("u", 300);
        message.set("s", -129);
        const std::vector<int> vector1093 = std::vector<int>({1, 300});
        message.set("a", vector1093);
        const double maximumDouble = std::numeric_limits<double>::max();
        message.set("d", maximumDouble);
        message.set("wide", UINT64_MAX);
        {
            bool caughtException = false;
            try
            {
                message.get<std::uint8_t>("u");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "scalar narrowing does not wrap");
        };
        {
            bool caughtException = false;
            try
            {
                message.get<std::uint8_t>("u");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "proxy narrowing does not wrap");
        };
        {
            bool caughtException = false;
            try
            {
                message.get<std::int8_t>("s");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "negative narrowing does not wrap");
        };
        {
            bool caughtException = false;
            try
            {
                message.get<std::uint16_t>("s");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "negative to unsigned rejected");
        };
        {
            bool caughtException = false;
            try
            {
                message.get<std::vector<std::uint8_t>>("a");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "array narrowing checked");
        };
        {
            bool caughtException = false;
            try
            {
                message.get<float>("d");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "floating overflow rejected");
        };
        {
            bool caughtException = false;
            try
            {
                message.get<std::int64_t>("wide");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "uint64 to int64 overflow rejected");
        };
        const std::uint16_t convertedNumber = message.get<std::uint16_t>("u");
        check(convertedNumber == 300, "correct integer type unchanged");
        const std::uint64_t convertedNumber2 = message.get<std::uint64_t>("wide");
        check(convertedNumber2 == UINT64_MAX, "full uint64 range unchanged");
        const std::vector<std::uint16_t> copiedElements = std::vector<std::uint16_t>({1, 300});
        const std::vector<std::uint16_t> convertedElements = message.get<std::vector<std::uint16_t>>("a");
        check(convertedElements == copiedElements,
            "correct array read unchanged");
        const double convertedNumber3 = message.get<double>("d");
        const bool checkResult1 = convertedNumber3 == std::numeric_limits<double>::max();
        check(checkResult1, "double maximum unchanged");
        message.set("u", 1);
        const bool convertedNumber4 = message.get<bool>("u");
        check(convertedNumber4, "boolean read of one");
        message.set("u", 2);
        {
            bool caughtException = false;
            try
            {
                message.get<bool>("u");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "boolean range checked");
        };
        message.set("d", 1.5);
        {
            bool caughtException = false;
            try
            {
                message.get<int>("d");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "fractional number does not truncate");
        };
    }

    message_manager::Json deepDefinition(int count)
    {
        message_manager::Json definition = message_manager::Json::object();
        definition["message"] = "Root";
        definition["types"] = message_manager::Json::object();
        definition["types"]["Root"] = message_manager::Json::parse(R"({"fields":[{"name":"x","type":"uint8_t"}]})");
        for (int index = 0; index < count; ++index)
        {
            const std::string name = "A" + std::to_string(1000 + index);
            std::string previous = "uint8_t";
            if (index != 0)
            {
                previous = "A" + std::to_string(1000 + index - 1);
            }
            message_manager::Json entry = message_manager::Json::parse(R"({"fields":[{"name":"v","type":"uint8_t"}]})");
            entry["fields"][0]["type"] = previous;
            definition["types"][name] = entry;
        }
        return definition;
    }

    void testSchemaBoundaries()
    {
        const message_manager::Json good = deepDefinition(64);
        message_manager::Message unusedBoundary(good);
        unusedBoundary.set("x", 7);
        const std::vector<std::uint8_t> serializedBytes = unusedBoundary.serialize();
        const std::vector<std::uint8_t> copiedElements = std::vector<std::uint8_t>({7});
        check(serializedBytes == copiedElements, "unused 64-level type allowed");
        message_manager::Json selected = good;
        selected["message"] = "A1063";
        message_manager::Message selectedBoundary(selected);
        const bool checkResult2 = selectedBoundary.fixedSize() == 1;
        check(checkResult2, "selected 64-level type allowed");
        const message_manager::Json bad = deepDefinition(65);
        {
            bool caughtException = false;
            try
            {
                message_manager::Message rejected(bad);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "cached unused 65-level type rejected");
        };
        selected = bad;
        selected["message"] = "A1064";
        {
            bool caughtException = false;
            try
            {
                message_manager::Message rejected(selected);

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "selected 65-level type rejected");
        };

        message_manager::writeTextFile("audit_output/duplicate.json", R"({"link":"a","link":"b"})");
        {
            bool caughtException = false;
            try
            {
                message_manager::readJsonFile("audit_output/duplicate.json");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "duplicate root key");
        };
        message_manager::writeTextFile("audit_output/duplicate.json",
            R"({"items":[{"msgcode":1,"msgcode":2}]})");
        {
            bool caughtException = false;
            try
            {
                message_manager::readJsonFile("audit_output/duplicate.json");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "duplicate nested key");
        };
        message_manager::writeTextFile("audit_output/valid.json",
            R"({"items":[{"x":1},{"x":2}],"other":{"x":3}})");
        const Json parsedDocument = readJsonFile("audit_output/valid.json");
        const Json &itemsJson = parsedDocument.at("items");
        const std::size_t elementCount = itemsJson.size();
        check(elementCount == 2,
            "same key in separate objects is valid");
        message_manager::writeTextFile("audit_output/trailing.json", R"({"x":1} {"x":2})");
        {
            bool caughtException = false;
            try
            {
                message_manager::readJsonFile("audit_output/trailing.json");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "trailing document rejected");
        };
        message_manager::writeTextFile("audit_output/duplicate.json", R"({"interfaces":[],"interfaces":[]})");
        {
            bool caughtException = false;
            try
            {
                message_manager::MessageInterface::fromFile("audit_output/duplicate.json");

            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException, "unbound interface loader also rejects duplicate keys");
        };
    }

    // 로컬 UDP 시험 전용 객체입니다. 소켓 생성 → 송수신 → 닫기 순서로만 동작합니다.
    class DatagramSocket
    {
    public:
        // 소켓 생성, 시간 제한 설정, 로컬 포트 배정을 순서대로 수행합니다.
        DatagramSocket()
        {
            _descriptor = ::socket(AF_INET, SOCK_DGRAM, 0);
            if (_descriptor < 0)
            {
                throw std::runtime_error("cannot create UDP socket");
            }
            _address.sin_family = AF_INET;
            const std::uint32_t loopbackAddress = htonl(INADDR_LOOPBACK);
            _address.sin_addr.s_addr = loopbackAddress;
            _address.sin_port = 0;
            timeval timeout{};
            timeout.tv_sec = 2;
            const socklen_t timeoutSize = sizeof(timeout);
            const int timeoutStatus = ::setsockopt(_descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, timeoutSize);
            if (timeoutStatus != 0)
            {
                closeSocket();
                throw std::runtime_error("cannot configure loopback UDP socket");
            }
            const sockaddr *bindAddress = reinterpret_cast<const sockaddr *>(&_address);
            socklen_t addressSize = sizeof(_address);
            const int bindStatus = ::bind(_descriptor, bindAddress, addressSize);
            if (bindStatus != 0)
            {
                closeSocket();
                throw std::runtime_error("cannot configure loopback UDP socket");
            }
            sockaddr *assignedAddress = reinterpret_cast<sockaddr *>(&_address);
            const int addressStatus = ::getsockname(_descriptor, assignedAddress, &addressSize);
            if (addressStatus != 0)
            {
                closeSocket();
                throw std::runtime_error("cannot inspect loopback UDP socket");
            }
        }

        // 객체가 사라질 때 소켓을 닫습니다.
        ~DatagramSocket()
        {
            closeSocket();
        }

        // 같은 소켓 번호를 가진 복사본이 중복으로 닫는 일을 막습니다.
        DatagramSocket(const DatagramSocket &) = delete;
        DatagramSocket &operator=(const DatagramSocket &) = delete;

        // 버퍼 주소와 길이, 수신 주소를 먼저 구한 다음 sendto 한 번으로 보냅니다.
        void send(const std::vector<std::uint8_t> &bytes, const DatagramSocket &peer) const
        {
            const std::uint8_t *buffer = bytes.data();
            const std::size_t length = bytes.size();
            const sockaddr *peerAddress = reinterpret_cast<const sockaddr *>(&peer._address);
            const socklen_t addressSize = sizeof(peer._address);
            const ssize_t count = ::sendto(_descriptor, buffer, length, 0, peerAddress, addressSize);
            if (count < 0)
            {
                throw std::runtime_error("UDP send failed");
            }
            const std::size_t sentLength = static_cast<std::size_t>(count);
            if (sentLength != length)
            {
                throw std::runtime_error("UDP send failed");
            }
        }

        // 수신 공간 준비 → recvfrom → 실제 수신 길이만 남기기 순서입니다.
        std::vector<std::uint8_t> receive() const
        {
            std::vector<std::uint8_t> bytes(4096);
            std::uint8_t *buffer = bytes.data();
            const std::size_t capacity = bytes.size();
            const ssize_t count = ::recvfrom(_descriptor, buffer, capacity, 0, nullptr, nullptr);
            if (count < 0)
            {
                throw std::runtime_error("UDP receive failed or timed out");
            }
            const std::size_t receivedLength = static_cast<std::size_t>(count);
            bytes.resize(receivedLength);
            return bytes;
        }

    protected:

    private:
        // 열린 소켓을 닫고 번호를 무효화합니다.
        void closeSocket()
        {
            if (_descriptor >= 0)
            {
                    ::close(_descriptor);
                _descriptor = -1;
            }
        }

    private:
        // 운영체제가 반환한 소켓 번호입니다. -1이면 닫힌 상태입니다.
        int _descriptor = -1;
        // 시험에 사용하는 로컬 IP와 포트입니다.
        sockaddr_in _address{};
    };

    void load(message_manager::MessageManager &manager, const std::string &folder)
    {
        manager.loadEnums(folder + "/message_enums.json");
        manager.loadLink(folder + "/link_message1.json");
        manager.loadLink(folder + "/link_message2.json");
        manager.setLogHandler(nullptr);
    }

    void fill(message_manager::Message &message, std::size_t iteration, std::size_t length)
    {
        const int signedIteration = static_cast<int>(iteration);
        const std::uint32_t uint32_t2 = std::uint32_t(0x12340000U + iteration);
        message.set("header.id", uint32_t2);
        const std::uint16_t uint16_t2 = std::uint16_t(1);
        message.set("header.version", uint16_t2);
        const std::uint8_t uint8_t2 = std::uint8_t(iteration % 3);
        message.set("header.state", uint8_t2);
        const std::int16_t int16_t1110 = std::int16_t(-1000 + signedIteration);
        message.set("body.position.x", int16_t1110);
        const std::int16_t int16_t1111 = std::int16_t(1000 - signedIteration);
        message.set("body.position.y", int16_t1111);
        const float static_cast1112 = static_cast<float>(iteration);
        message.set("body.temperature", static_cast1112 / 8.0f);
        message.set("body.name", "DEV1");
        for (int index = 0; index < 3; ++index)
        {
            const std::string numberText = std::to_string(index);
            const std::uint16_t uint16_t3 = std::uint16_t(iteration + index * 17);
            message.set("body.samples[" + numberText + "]", uint16_t3);
        }
        for (int index = 0; index < 2; ++index)
        {
            const std::string numberText2 = std::to_string(index);
            const std::int16_t int16_t1116 = std::int16_t(-300 + signedIteration + index);
            message.set("body.points[" + numberText2 + "].x", int16_t1116);
            const std::string numberText3 = std::to_string(index);
            const std::int16_t int16_t1118 = std::int16_t(300 - signedIteration - index);
            message.set("body.points[" + numberText3 + "].y", int16_t1118);
        }
        std::vector<std::uint8_t> buffer(length);
        for (std::size_t index = 0; index < length; ++index)
        {
            buffer[index] = static_cast<std::uint8_t>((iteration * 37 + index * 13) % 256);
        }
        if (!buffer.empty())
        {
            buffer.front() = 0;
            buffer.back() = 0xFF;
        }
        const std::size_t bufferCount = buffer.size();
        const std::uint8_t *bufferAddress = buffer.data();
        message.set("body.data.buffer", bufferAddress, bufferCount);
        const std::size_t bufferCount2 = buffer.size();
        const std::uint32_t uint32_t3 = std::uint32_t(bufferCount2);
        message.set("body.data.length", uint32_t3);
    }

    void testUdpAndOracle(const std::string &folder)
    {
        message_manager::MessageManager sender;
        message_manager::MessageManager receiver;
        load(sender, folder);
        load(receiver, folder);
        message_manager::MessageInterface converter =
        message_manager::MessageInterface::fromFile(folder + "/interface_message.json", receiver);
        DatagramSocket transmitSocket;
        DatagramSocket receiveSocket;
        const std::vector<std::size_t> lengths = {
            0, 1, 2, 3, 4, 5, 7, 15, 16, 17, 31, 32, 63, 64, 65, 127, 255, 511, 1024};
        message_manager::Json cases = message_manager::Json::array();
        std::size_t iteration = 0;
        for (int repeat = 0; repeat < 10; ++repeat)
        {
            for (std::size_t length : lengths)
            {
                message_manager::Message &source = sender.message("link_message1", "message1");
                fill(source, iteration++, length);
                const std::vector<std::uint8_t> original = source.serialize();
                transmitSocket.send(original, receiveSocket);
                const std::vector<std::uint8_t> receivedBytes = receiveSocket.receive();
                check(receivedBytes == original, "actual UDP source datagram");

                const message_manager::MessageIdentification found = receiver.receive(receivedBytes);
                check(found._linkName == "link_message1" && found._msgcode == 1001, "automatic source routing");
                const message_manager::Message &received = receiver.message(found._linkName, found._messageName);
                const std::vector<std::uint8_t> serializedBytes = received.serialize();
                check(serializedBytes == original, "source receive exact bytes");
                message_manager::Message target = converter.convert(received);
                const std::vector<std::uint8_t> targetBytes = target.serialize();
                transmitSocket.send(targetBytes, receiveSocket);
                // 변환한 대상 메시지의 UDP 데이터를 한 번만 수신합니다.
                const std::vector<std::uint8_t> targetReceivedBytes = receiveSocket.receive();
                // 수신한 데이터를 대상 메시지에 반영합니다.
                const message_manager::MessageIdentification targetId = receiver.receive(targetReceivedBytes);
                check(targetId._linkName == "link_message2" && targetId._msgcode == 2001,
                    "converted UDP routing");
                const message_manager::Message &stored = receiver.message(targetId._linkName, targetId._messageName);
                const std::vector<std::uint8_t> serializedBytes2 = stored.serialize();
                check(serializedBytes2 == targetBytes, "converted receive exact bytes");
                message_manager::Message restored = converter.convert(stored);
                const std::vector<std::uint8_t> serializedBytes3 = restored.serialize();
                check(serializedBytes3 == original, "UDP forward/reverse conversion exact bytes");
                const std::vector<std::uint8_t> serializedBytes4 = source.serialize();
                check(serializedBytes4 == original, "sender source unchanged");
                const std::uint32_t convertedNumber = stored.get<std::uint32_t>("body.data.length");
                check(convertedNumber == length,
                    "manual length preserved");
                const std::string valueText = source.dump();
                const bool checkResult3 = valueText.find("INCOMPLETE") == std::string::npos;
                check(checkResult3, "ready current-data dump");
                message_manager::Json entry = message_manager::Json::object();
                entry["source_values"] = source.value("");
                entry["target_values"] = target.value("");
                entry["source_hex"] = message_manager::hexText(original, 0);
                entry["target_hex"] = message_manager::hexText(targetBytes, 0);
                cases.push_back(entry);
            }
        }
        const std::string valueText2 = cases.dump();
        message_manager::writeTextFile("audit_output/oracle_cases.json", valueText2);
        std::cout << "UDP cases: ";
        std::cout << cases.size();
        std::cout << ", datagrams: ";
        std::cout << cases.size() * 2;
        std::cout << '\n';

        // 고정 seed로 수신 데이터를 변형하여 실패 시 상태 보존과 재직렬화를 검사합니다.
        std::mt19937 random(0x20261029U);
        message_manager::Message &current = receiver.message("link_message1", "message1");
        const std::vector<std::uint8_t> base = current.serialize();
        std::size_t accepted = 0;
        std::size_t rejected = 0;
        for (int round = 0; round < 1024; ++round)
        {
            const std::string before = receiver.dumpAll();
            std::vector<std::uint8_t> damaged = base;
            if (round % 3 == 0)
            {
                const std::size_t baseCount = base.size();
                const std::uint32_t randomLength = random();
                const std::size_t newLength = randomLength % (baseCount + 10);
                damaged.resize(newLength);
            }
            else
            {
                const std::size_t index = random() % damaged.size();
                const std::uint32_t randomBit = random();
                const std::uint32_t bitIndex = randomBit % 8;
                const std::uint32_t bitValue = 1U << bitIndex;
                const std::uint8_t changedBit = static_cast<std::uint8_t>(bitValue);
                damaged[index] ^= changedBit;
            }
            try
            {
                const message_manager::MessageIdentification id = receiver.receive("link_message1", damaged);
                message_manager::Message &message = receiver.message(id._linkName, id._messageName);
                const std::vector<std::uint8_t> normalized = message.serialize();
                message.deserialize(normalized);
                const std::vector<std::uint8_t> serializedBytes5 = message.serialize();
                check(serializedBytes5 == normalized, "accepted frame normalizes stably");
                ++accepted;
            }
            catch (const std::exception &)
            {
                const bool checkResult4 = receiver.dumpAll() == before;
                check(checkResult4, "invalid frame preserves all message state");
                ++rejected;
            }
        }
        check(accepted + rejected == 1024, "all damaged frames accounted for");
        check(accepted != 0 && rejected != 0, "both valid and invalid mutation paths exercised");
        std::cout << "Mutations: ";
        std::cout << accepted;
        std::cout << " accepted, ";
        std::cout << rejected;
        std::cout << " rejected\n";
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
        std::filesystem::create_directories("audit_output");
        testCheckedReads();
        testSchemaBoundaries();
        testUdpAndOracle(folder);
        std::cout << "Passed ";
        std::cout << checks;
        std::cout << " audit integration checks\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what();
        std::cerr << '\n';
        return 1;
    }
    return 0;
}
