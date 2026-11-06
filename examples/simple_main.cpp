#include "MessageManager.hpp"

// 가장 먼저 읽어 볼 예제입니다. 람다, 사용자 템플릿, [] 접근 없이 사용합니다.
int main()
{
    try
    {
        // 1. 관리 객체를 만듭니다. 이 객체가 메시지들을 보관합니다.
        message_manager::MessageManager manager;

        // 2. enum -> 링크 순서로 정의를 읽습니다.
        manager.loadEnums("json/message_enums.json");
        manager.loadLink("json/link_message1.json");

        // 3. 관리 객체 안의 메시지를 참조합니다.
        // &를 빼면 복사본이 되어 manager 안의 메시지와 따로 바뀝니다.
        message_manager::Message &source = manager.message("link_message1", "message1");

        // 4. 점으로 연결한 경로에 값을 넣습니다.
        // 아직 다른 필드를 채우지 않았으므로 이 예제에서는 serialize를 호출하지 않습니다.
        source.set("header.id", 100);
        source.set("header.version", 1);
        source.set("header.state", 1);
        source.set("body.position.x", -10);
        source.set("body.position.y", 20);

        // 5. 원하는 C++ 타입으로 꺼냅니다. 범위를 넘으면 예외가 발생합니다.
        const int x = source.get<int>("body.position.x");
        std::cout << "x = ";
        std::cout << x;
        std::cout << '\n';

        // 6. 채운 필드와 미설정 필드를 같이 확인합니다.
        manager.dumpAll(std::cout);
        return 0;
    }
    catch (const std::exception &error)
    {
        // 어느 정의나 값에서 실패했는지 오류 메시지를 확인합니다.
        std::cerr << error.what();
        std::cerr << '\n';
        return 1;
    }
}
