#include "MessageInterface.hpp"

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

    static void testDirectOwnership(const std::string &folder)
    {
        MessageManager manager;
        manager.setLogHandler(nullptr);
        manager.loadEnums(folder + "/message_enums.json");
        manager.loadLink(folder + "/numeric_source.json");

        // Manager가 실제 객체를 직접 보관합니다. 링크를 나중에 추가해도 참조가 유지되어야 합니다.
        Message &original = manager.message("numeric_source", "packet");
        const Json document = readJsonFile(folder + "/source_values.json");
        const Json & bodyJson = document.at("body");
        original.set("body", bodyJson);
        const Message *address = &original;
        const std::vector<std::uint8_t> before = original.serialize();
        manager.loadLink(folder + "/numeric_target.json");
        Message &found = manager.message("numeric_source", "packet");
        check(&found == address, "existing message reference is stable");
        const std::vector<std::uint8_t> serializedBytes = found.serialize();
        check(serializedBytes == before, "loading another link preserves values");

        // create는 현재 데이터가 아니라 정의와 msgcode만 복사합니다.
        Message empty = manager.create("numeric_source", "packet");
        const int convertedNumber = empty.get<int>("header.msgcode");
        check(convertedNumber == 50, "create retains defined code");
        bool rejected = false;
        try
        {
            empty.value("body.position.y");
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        check(rejected, "create does not inherit mutable field values");

        // 같은 코드가 두 번 있는 새 링크는 등록 전체가 실패해야 합니다.
        Json bad = readJsonFile(folder + "/numeric_source.json");
        bad["link"] = "invalid_link";
        bad["messages"]["duplicate"] = bad["messages"]["packet"];
        const std::string dumpBefore = manager.dumpAll();
        rejected = false;
        try
        {
            manager.addLink(bad);
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        check(rejected, "duplicate code rejected");
        const bool checkResult1 = manager.dumpAll() == dumpBefore;
        check(checkResult1, "failed link load leaves no partial messages");
        const std::vector<std::uint8_t> serializedBytes2 = original.serialize();
        check(serializedBytes2 == before, "failed link load preserves existing reference and values");

        // 실패한 링크 이름을 정상 정의에 다시 쓰면 성공해야 합니다.
        bad["messages"].erase("duplicate");
        const std::string loadedName = manager.addLink(bad);
        check(loadedName == "invalid_link", "failed registration did not leave a stale link");
        const bool checkResult2 = manager.identify("invalid_link", before)._msgcode == 50;
        check(checkResult2, "new direct routing works");
        const std::string dumpAfter = manager.dumpAll();
        rejected = false;
        try
        {
            manager.message("missing", "packet");
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        const bool checkResult3 = rejected && manager.dumpAll() == dumpAfter;
        check(checkResult3, "lookup does not insert unknown messages");
    }
}

int main(int argc, char **argv)
{
    try
    {
        const std::string folder = argc > 1 ? argv[1] : "json/integer_formats";
        message_manager::testDirectOwnership(folder);
        std::cout << "Passed ";
        std::cout << message_manager::checks;
        std::cout << " simple-structure checks\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what();
        std::cerr << '\n';
        return 1;
    }
}
