#include "MessageInterface.hpp"
#include "MessageManager.hpp"

#include <iostream>

// 전체 예제: 정의 읽기 → 값 설정 → 변환 → 바이너리 수신.
// 한 줄에 호출을 이어 붙이지 않고 중간 결과를 변수로 확인합니다.
int main(int argc, char **argv)
{
    try
    {
        std::string jsonFolder = "json";
        if (argc > 1)
        {
            jsonFolder = argv[1];
        }

        // 1. types의 공통 msgheader와 messages의 일반 구조체 필드를 읽습니다.
        message_manager::MessageManager manager;
        manager.loadEnums(jsonFolder + "/message_enums.json");
        manager.loadLink(jsonFolder + "/link_message1.json");
        manager.loadLink(jsonFolder + "/link_message2.json");
        message_manager::MessageInterface converter =
        message_manager::MessageInterface::fromFile(jsonFolder + "/interface_message.json", manager);

        // 2. 원본 메시지의 현재 값을 설정합니다. msgcode는 JSON 값으로 자동 설정됩니다.
        message_manager::Message &source = manager.message("link_message1", "message1");
        const std::uint32_t uint32_t2 = std::uint32_t(100);
        source.set("header.id", uint32_t2);
        const std::uint16_t uint16_t2 = std::uint16_t(1);
        source.set("header.version", uint16_t2);
        const std::uint8_t uint8_t2 = std::uint8_t(1);
        source.set("header.state", uint8_t2);
        const std::int16_t int16_t1564 = std::int16_t(-10);
        source.set("body.position.x", int16_t1564);
        const std::int16_t int16_t1565 = std::int16_t(20);
        source.set("body.position.y", int16_t1565);
        source.set("body.temperature", 25.5f);
        source.set("body.name", "DEV1");

        // 3. 배열 원소도 전체 경로로 접근합니다.
        source.set("body.points[0].x", -1);
        source.set("body.points[0].y", 2);
        source.set("body.points[1].x", 0);
        source.set("body.points[1].y", 3);
        source.set("body.samples", 0, 10);
        source.set("body.samples", 1, 20);
        source.set("body.samples", 2, 30);

        // 4. 버퍼 내용과 길이는 각각 설정합니다. 0x00도 데이터로 복사됩니다.
        char buffer[] = {'A', '\0', 'B', 'C', 'D'};
        const std::uint32_t length = sizeof(buffer);
        source.set("body.data.buffer", buffer, length);
        source.set("body.data.length", length);

        // 5. 메시지 한 개의 현재 값, enum 숫자/이름, 실제 버퍼 HEX를 출력합니다.
        std::cout << "=== SOURCE: CURRENT VALUES ===\n";
        source.dump(std::cout);
        // 이 메시지의 표를 파일로 저장합니다.
        source.dumpToFile("source_dump.txt");

        // 6. JSON의 변환 규칙에 따라 새 대상 메시지를 반환합니다. 원본은 유지합니다.
        message_manager::Message target = converter.convert(source);
        std::cout << "Source link=";
        std::cout << source.getLinkName();
        std::cout << " message=";
        std::cout << source.getMessageName();
        std::cout << '\n';
        std::cout << "Target link=";
        std::cout << target.getLinkName();
        std::cout << " message=";
        std::cout << target.getMessageName();
        std::cout << '\n';
        std::cout << "\n=== TARGET: CONVERTED VALUES ===\n";
        target.dump(std::cout);
        // 이 메시지의 표를 파일로 저장합니다.
        target.dumpToFile("target_dump.txt");
        // 반환된 target은 독립 객체입니다. 전체 덤프에 포함하려면 명시적으로 보관합니다.
        const std::string targetLink = target.getLinkName();
        const std::string targetName = target.getMessageName();
        message_manager::Message &storedTarget = manager.message(targetLink, targetName);
        storedTarget = target;
        // 전체 메시지의 표를 파일로 저장합니다.
        manager.dumpAllToFile("all_messages_dump.txt");

        // 7. 수신 바이너리의 헤더 코드로 링크와 메시지를 자동 판별합니다.
        const std::vector<std::uint8_t> binary = source.serialize();
        message_manager::MessageManager receiver;
        receiver.loadEnums(jsonFolder + "/message_enums.json");
        receiver.loadLink(jsonFolder + "/link_message1.json");
        receiver.loadLink(jsonFolder + "/link_message2.json");
        const message_manager::MessageIdentification result = receiver.receive(binary);
        const message_manager::Message &received = receiver.message(result._linkName, result._messageName);
        // 버퍼 내용을 독립된 char 벡터로 복사해 읽습니다.
        const std::vector<char> receivedBuffer = received.getBuffer("body.data.buffer");
        std::cout << "Received message=";
        std::cout << result._messageName;
        std::cout << " msgcode=";
        std::cout << result._msgcode;
        std::cout << " buffer length=";
        std::cout << receivedBuffer.size();
        std::cout << '\n';
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what();
        std::cerr << '\n';
        return 1;
    }
    return 0;
}
