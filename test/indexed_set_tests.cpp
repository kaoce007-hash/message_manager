#include "BinaryMessage.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

using message_manager::Json;
using message_manager::Message;

void require(bool condition)
{
    if (!condition) throw std::runtime_error("indexed set regression");
}

int main()
{
    const Json schema = Json::parse(R"({
        "message":"Data", "endian":"little", "types":{
            "Point":{"fields":[{"name":"x","type":"int16_t"},{"name":"y","type":"int16_t"}]},
            "Data":{"fields":[
                {"name":"samples","type":"uint16_t","count":3},
                {"name":"points","type":"Point","count":2},
                {"name":"length","type":"uint32_t"},
                {"name":"buffer","type":"char*","length_from":"length","max_length":32}
            ]}
        }
    })");
    Message message(schema);
    message.set("samples", 0, 10);
    std::size_t index = 1;
    message.set("samples", index, 20);
    message.set("samples", 2, 30);
    message.set("points", 0, Json{{"x", -1}, {"y", 2}});
    message.set("points", 1, Json{{"x", 3}, {"y", 4}});
    const char buffer[] = {'A', '\0', 'B'};
    message.set("buffer", buffer, sizeof(buffer));
    message.set("length", sizeof(buffer));
    const std::vector<std::uint8_t> expected = {
        10,0,20,0,30,0, 255,255,2,0,3,0,4,0, 3,0,0,0, 65,0,66
    };
    require(message.serialize() == expected);
    // 이전 방식과 배열 경로 방식이 같은 값과 바이트를 만드는지 확인합니다.
    message.set("samples[1]", 20);
    require(message.serialize() == expected);
    Message received = message.emptyCopy();
    received.deserialize(expected);
    require(received.get<int>("samples[1]") == 20);
    require(received.get<int>("points[0].x") == -1);
    require(received.getBuffer("buffer") == std::vector<char>(buffer, buffer + sizeof(buffer)));
    // 범위 초과나 잘못된 값은 예외가 발생하며 기존 데이터는 유지됩니다.
    for (std::size_t badIndex : {std::size_t(3), std::numeric_limits<std::size_t>::max()})
    {
        bool rejected = false;
        try { message.set("samples", badIndex, 40); }
        catch (const std::exception &) { rejected = true; }
        require(rejected && message.serialize() == expected);
    }
    bool rejected = false;
    try { message.set("samples", 1, 65536); }
    catch (const std::exception &) { rejected = true; }
    require(rejected && message.serialize() == expected);
    std::cout << "indexed set tests passed\n";
}
