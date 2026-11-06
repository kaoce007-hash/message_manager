#include "BinaryMessage.hpp"
#include <iostream>
#include <stdexcept>

using message_manager::Json;
using message_manager::Message;

// 중첩 구조체의 끝 정렬을 다음 필드에 합쳐도 전송 바이트가 같아야 합니다.
// 특히 가변 버퍼 뒤의 패딩은 정의를 읽을 때가 아니라 실제 값을 쓸 때 결정해야 합니다.
int main()
{
    const Json schema = Json::parse(R"({
        "message":"Outer", "alignment":4, "endian":"little", "types":{
            "Inner":{"fields":[
                {"name":"mark","type":"uint32_t"},
                {"name":"length","type":"uint16_t"},
                {"name":"data","type":"bytes","length_from":"length","max_length":32}
            ]},
            "Outer":{"fields":[
                {"name":"prefix","type":"uint8_t"},
                {"name":"items","type":"Inner","count":2},
                {"name":"tail","type":"uint8_t"}
            ]}
        }
    })");
    Message source(schema);
    source.set("prefix", 1);
    source.set("items[0].mark", 0x11223344);
    source.set("items[0].data", std::vector<std::uint8_t>{0xAA});
    source.set("items[1].mark", 0x55667788);
    source.set("items[1].data", std::vector<std::uint8_t>{0xBB, 0xCC, 0xDD});
    source.set("tail", 0xEE);
    // 직접 계산한 아래 바이트 배치를 기대값으로 고정합니다.
    // 0:prefix / 4:items[0] / 12:items[1] / 24:tail / 28:message end
    const std::vector<std::uint8_t> expected = {
        1,0,0,0, 0x44,0x33,0x22,0x11, 1,0,0xAA,0,
        0x88,0x77,0x66,0x55, 3,0,0xBB,0xCC,0xDD,0,0,0,
        0xEE,0,0,0
    };
    if (source.serialize() != expected || source.size() != 28 || source.fixedSize() != 24)
    {
        throw std::runtime_error("nested dynamic alignment bytes/size mismatch");
    }
    Message received = source.emptyCopy();
    received.deserialize(expected);
    if (received.serialize() != expected)
    {
        throw std::runtime_error("nested dynamic round trip mismatch");
    }
    // 어느 위치에서 잘려도 수신을 거부하고 이전 값을 유지해야 합니다.
    for (std::size_t length = 0; length < expected.size(); ++length)
    {
        bool rejected = false;
        try
        {
            received.deserialize(std::vector<std::uint8_t>(expected.begin(), expected.begin() + length));
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        if (!rejected || received.serialize() != expected)
        {
            throw std::runtime_error("truncation rollback mismatch");
        }
    }
    std::vector<std::uint8_t> dirty = expected;
    for (std::size_t index : {1,2,3,11,21,22,23,25,26,27})
    {
        dirty[index] = 0xFF;
    }
    received.deserialize(dirty);
    if (received.serialize() != expected)
    {
        throw std::runtime_error("padding normalization mismatch");
    }
    std::cout << "Direct alignment regression: exact bytes, sizes, round trip, 28 truncations, padding normalization passed\n";
}
