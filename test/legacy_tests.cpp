#include "BinaryMessage.hpp"
#include <functional>
#include <iostream>
using message_manager::Json;
void check(bool ok)
{
    if (!ok)
    {
        throw std::runtime_error("test failure");
    }
}

Json schema(std::string type, std::string order = "big")
{
    Json field = Json::object();
    field["name"] = "x";
    field["type"] = type;
    Json fields = Json::array();
    fields.push_back(field);
    Json result = Json::object();
    result["endian"] = order;
    result["fields"] = fields;
    return result;
}
int integerBits(const std::string &type)
{
    if (type == "int8" || type == "uint8")
    {
        return 8;
    }
    if (type == "int16" || type == "uint16")
    {
        return 16;
    }
    if (type == "int32" || type == "uint32")
    {
        return 32;
    }
    return 64;
}

int main()
{
    const Json parse1283 = Json::parse(
        R"({"endian":"big","fields":[{"name":"id","type":"uint16"},{"name":"temperature","type":"float"},{"name":"name","type":"string","length":8}]})");
    message_manager::Message m(parse1283);
    {
        bool caughtException = false;
        try
        {
            m.serialize();

        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException);
    };
    m.set("id", 100);
    m.set("temperature", 25.5f);
    m.set("name", "DEV1");
    const std::vector<std::uint8_t> serializedBytes = m.serialize();
    const std::vector<std::uint8_t> copiedElements = std::vector<std::uint8_t>({0, 100, 0x41, 0xcc, 0, 0, 0x44, 0x45, 0x56, 0x31, 0, 0, 0, 0});
    check(serializedBytes ==
        copiedElements);
    const std::vector<std::uint8_t> serializedBytes2 = m.serialize();
    m.deserialize(serializedBytes2);
    const float convertedNumber = m.get<float>("temperature");
    check(convertedNumber == 25.5f);
    {
        bool caughtException = false;
        try
        {
            m.set("id", 65536);

        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException);
    };
    {
        bool caughtException = false;
        try
        {
            m.set("id", -1);

        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException);
    };
    {
        bool caughtException = false;
        try
        {
            m.set("id", 1.5);

        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException);
    };
    {
        bool caughtException = false;
        try
        {
            m.set("id", true);

        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException);
    };
    {
        bool caughtException = false;
        try
        {
            m.set("invalid", 1);

        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException);
    };
    {
        bool caughtException = false;
        try
        {
            m.set("name", "123456789");

        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException);
    };
    {
        bool caughtException = false;
        try
        {
            const std::string string1288 = std::string("a\0b", 3);
            m.set("name", string1288);

        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException);
    };
    {
        bool caughtException = false;
        try
        {
            m.deserialize({1});

        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException);
    };
    std::vector<std::uint8_t> broken = m.serialize();
    broken[13] = 1;
    {
        bool caughtException = false;
        try
        {
            m.deserialize(broken);

        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException);
    };
    const std::string mText = m.get<std::string>("name");
    check(mText == "DEV1");
    for (const char *order : {"big", "little"})
    {
        for (const char *t : {"int8", "int16", "int32", "int64"})
        {
            const Json schema1290 = schema(t, order);
            message_manager::Message s(schema1290);
            int bits = integerBits(t);
            std::int64_t low = bits == 64 ? INT64_MIN : -(std::int64_t{1} << (bits - 1));
            std::int64_t high = bits == 64 ? INT64_MAX : (std::int64_t{1} << (bits - 1)) - 1;
            for (std::int64_t n : {low, std::int64_t{-1}, std::int64_t{0}, high})
            {
                s.set("x", n);
                std::vector<std::uint8_t> b = s.serialize();
                s.deserialize(b);
                const std::int64_t convertedNumber2 = s.get<std::int64_t>("x");
                check(convertedNumber2 == n);
                if (n == -1)
                {
                    for (std::uint8_t byte : b)
                    {
                        check(byte == 255);
                    }
                }
            }
            if (bits < 64)
            {
                {
                    bool caughtException = false;
                    try
                    {
                        s.set("x", low - 1);

                    }
                    catch (const std::exception &)
                    {
                        caughtException = true;
                    }
                    check(caughtException);
                };
                {
                    bool caughtException = false;
                    try
                    {
                        s.set("x", high + 1);

                    }
                    catch (const std::exception &)
                    {
                        caughtException = true;
                    }
                    check(caughtException);
                };
            }
        }
        for (const char *t : {"uint8", "uint16", "uint32", "uint64"})
        {
            const Json schema1292 = schema(t, order);
            message_manager::Message s(schema1292);
            int bits = integerBits(t);
            const std::uint64_t high = UINT64_MAX >> (64 - bits);
            s.set("x", high);
            const std::vector<std::uint8_t> serializedBytes3 = s.serialize();
            s.deserialize(serializedBytes3);
            const std::uint64_t convertedNumber3 = s.get<std::uint64_t>("x");
            check(convertedNumber3 == high);
        }
        for (const char *t : {"float", "double"})
        {
            const Json schema1295 = schema(t, order);
            message_manager::Message s(schema1295);
            s.set("x", -25.5);
            const std::vector<std::uint8_t> serializedBytes4 = s.serialize();
            s.deserialize(serializedBytes4);
            const double convertedNumber4 = s.get<double>("x");
            check(convertedNumber4 == -25.5);
            {
                bool caughtException = false;
                try
                {
                    const double infiniteValue = std::numeric_limits<double>::infinity();
                    s.set("x", infiniteValue);

                }
                catch (const std::exception &)
                {
                    caughtException = true;
                }
                check(caughtException);
            };
        }
    }
    const Json schema1298 = schema("uint32", "little");
    message_manager::Message le(schema1298);
    le.set("x", 0x12345678);
    const std::vector<std::uint8_t> serializedBytes5 = le.serialize();
    const std::vector<std::uint8_t> copiedElements2 = std::vector<std::uint8_t>({0x78, 0x56, 0x34, 0x12});
    check(serializedBytes5 == copiedElements2);
    Json overrideSchema = schema("uint16", "big");
    overrideSchema["fields"][0]["endian"] = "little";
    message_manager::Message overridden(overrideSchema);
    overridden.set("x", 0x1234);
    const std::vector<std::uint8_t> serializedBytes6 = overridden.serialize();
    const std::vector<std::uint8_t> copiedElements3 = std::vector<std::uint8_t>({0x34, 0x12});
    check(serializedBytes6 == copiedElements3);
    Json dup = schema("int8");
    dup["fields"].push_back(dup["fields"][0]);
    {
        bool caughtException = false;
        try
        {
            message_manager::Message bad(dup);

        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException);
    };
    {
        bool caughtException = false;
        try
        {
            const Json schema1303 = schema("wrong");
            message_manager::Message bad(schema1303);

        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException);
    };
    {
        bool caughtException = false;
        try
        {
            const Json schema1304 = schema("uint8", "wrong");
            message_manager::Message bad(schema1304);

        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException);
    };
    Json str = schema("string");
    str["fields"][0]["length"] = -1;
    {
        bool caughtException = false;
        try
        {
            message_manager::Message bad(str);

        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException);
    };
    const Json schema1305 = schema("double");
    message_manager::Message dbl(schema1305);
    {
        bool caughtException = false;
        try
        {
            const std::vector<std::uint8_t> copiedElements4 = std::vector<std::uint8_t>(8, 255);
            dbl.deserialize(copiedElements4);

        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException);
    };
    Json nested = Json::parse(R"({
      "message":"Packet", "endian":"big", "types":{
        "Inner":{"fields":[{"name":"x","type":"int16_t"},{"name":"id","type":"uint32_t"}]},
        "Wrapper":{"fields":[{"name":"inner","type":"Inner"}]},
        "Packet":{"fields":[{"name":"a","type":"Wrapper"},{"name":"b","type":"Inner","endian":"little"}]}
        }})");
    message_manager::Message n(nested);
    n.set("a.inner.x", -2);
    n.set("a.inner.id", 0x12345678U);
    n.set("b.x", -2);
    n.set("b.id", 0x12345678U);
    const std::size_t nCount = n.size();
    check(nCount == 12);
    const std::vector<std::uint8_t> serializedBytes7 = n.serialize();
    const std::vector<std::uint8_t> copiedElements5 = std::vector<std::uint8_t>({255, 254, 0x12, 0x34, 0x56, 0x78, 254, 255, 0x78, 0x56, 0x34, 0x12});
    check(serializedBytes7 ==
        copiedElements5);
    const std::vector<std::uint8_t> serializedBytes8 = n.serialize();
    n.deserialize(serializedBytes8);
    const std::int16_t convertedNumber5 = n.get<std::int16_t>("a.inner.x");
    check(convertedNumber5 == -2);
    const std::uint32_t convertedNumber6 = n.get<std::uint32_t>("b.id");
    check(convertedNumber6 == 0x12345678U);
    {
        bool caughtException = false;
        try
        {
            n.set("a.inner.x", 32768);

        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException);
    };
    {
        bool caughtException = false;
        try
        {
            n.set("a.inner", 1);

        }
        catch (const std::exception &)
        {
            caughtException = true;
        }
        check(caughtException);
    };
    Json invalid = nested;
    invalid["types"]["Inner"]["fields"][0]["type"] = "Missing";
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
        check(caughtException);
    };
    invalid = nested;
    invalid["types"]["Inner"]["fields"][0]["type"] = "Packet";
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
        check(caughtException);
    };
    invalid = nested;
    invalid["types"]["Inner"]["fields"][0]["name"] = "a.b";
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
        check(caughtException);
    };
    invalid = nested;
    invalid["types"]["Unused"] = {{"fields", Json::array({{{"name", "x"}, {"type", "Missing"}}})}};
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
        check(caughtException);
    };
    for (const char *t :
    {
            "uint8_t", "uint16_t", "uint32_t", "uint64_t", "int8_t", "int16_t", "int32_t", "int64_t"
        }
        )
    {
        const Json schema1313 = schema(t);
        message_manager::Message c(schema1313);
        c.set("x", 7);
        const std::vector<std::uint8_t> serializedBytes9 = c.serialize();
        c.deserialize(serializedBytes9);
        const int convertedNumber7 = c.get<int>("x");
        check(convertedNumber7 == 7);
    }
    std::cout << "All tests passed\n";
}
