#include "MessageInterface.hpp"
#include <iostream>
#include <random>

using message_manager::Json;
static unsigned checks = 0;
static void check(bool ok)
{
    ++checks;
    if (!ok)
    {
        const std::string numberText = std::to_string(checks);
        throw std::runtime_error("bitfield check " + numberText);
    }
}

static Json entry(const std::string &name, const std::string &type, unsigned bits)
{
    return Json{{"name",name},{"type",type},{"bits",bits}};
}
int main(int argc, char **argv)
{
    try
    {
        const std::string folder = argc > 1 ? argv[1] : "json/bitfields";
        std::mt19937_64 rng(42);
        // Independent integer packing oracle, including the 64-bit shift boundary.
        for (const char *endian : {"little", "big"})
        {
            for (unsigned width : {8,16,32,64})
            {
                const std::string type = "uint" + std::to_string(width) + "_t";
                for (unsigned a = 1; a < width; ++a)
                {
                    const Json entry1317 = entry("a",type,a);
                    const Json entry1318 = entry("b",type,width-a);
                    Json fields = Json::array({entry1317,entry1318});
                    message_manager::Message m(Json{{"endian",endian},{"fields",fields}});
                    m.setLogHandler({});
                    for (int n=0; n<6; ++n)
                    {
                        std::uint64_t av = rng() & (UINT64_MAX >> (64-a));
                        std::uint64_t bv = rng() & (UINT64_MAX >> (64-(width-a)));
                        m.set("a", av);
                        m.set("b", bv);
                        std::vector<std::uint8_t> bytes = m.serialize();
                        const std::size_t bytesCount = bytes.size();
                        const bool checkResult1 = bytesCount==width/8 && m.fixedSize()==width/8;
                        check(checkResult1);
                        const std::uint64_t word = av | (bv << a);
                        for (unsigned i=0;i<width/8;++i)
                        {
                            unsigned j = std::string(endian)=="little" ? i : width/8-1-i;
                            const std::uint8_t convertedValue = static_cast<std::uint8_t>(word >> (8*j));
                            check(bytes[i]==convertedValue);
                        }
                        message_manager::Message copy=m.emptyCopy();
                        copy.deserialize(bytes);
                        const std::uint64_t convertedNumber = copy.get<std::uint64_t>("a");
                        const std::uint64_t convertedNumber2 = copy.get<std::uint64_t>("b");
                        check(convertedNumber==av && convertedNumber2==bv);
                        {
                            bool caughtException = false;
                            try
                            {
                                std::vector<std::uint8_t>::const_iterator firstByte = bytes.begin();
                                std::vector<std::uint8_t>::const_iterator lastByte = bytes.end();
                                --lastByte;
                                const std::vector<std::uint8_t> copiedElements(firstByte,lastByte);
                                copy.deserialize(copiedElements);
                            }
                            catch (const std::exception &)
                            {
                                caughtException = true;
                            }
                            check(caughtException);
                        };
                        const std::vector<std::uint8_t> serializedBytes = copy.serialize();
                        check(serializedBytes==bytes);
                    }
                    const std::vector<std::uint8_t> previous=m.serialize();
                    {
                        bool caughtException = false;
                        try
                        {
                            m.set("a", UINT64_MAX >> (63-a));
                        }
                        catch (const std::exception &)
                        {
                            caughtException = true;
                        }
                        check(caughtException);
                    };
                    const std::vector<std::uint8_t> serializedBytes2 = m.serialize();
                    check(serializedBytes2==previous);
                }
                const Json entry1326 = entry("v",type,width);
                const Json array1327 = Json::array({entry1326});
                message_manager::Message full(Json{{"fields",array1327}});
                const std::uint64_t maximum=UINT64_MAX >> (64-width);
                full.set("v", maximum);
                std::vector<std::uint8_t> bytes=full.serialize();
                full.deserialize(bytes);
                const std::uint64_t convertedNumber3 = full.get<std::uint64_t>("v");
                check(convertedNumber3==maximum);
            }
        }
        // A partial unit is closed by a type change, overflow, ordinary field or struct boundary.
        const Json entry1329 = entry("a","uint16_t",5);
        const Json entry1330 = entry("d","uint16_t",5);
        const Json entry1331 = entry("e","uint16_t",1);
        const Json entry1332 = entry("b","uint16_t",11);
        const Json entry1333 = entry("c","uint16_t",12);
        Json fields=Json::array({entry1329,entry1332,
                entry1333,entry1330,
                Json{{"name","x"},{"type","uint8_t"}},entry1331});
        message_manager::Message aligned(Json{{"alignment",4},{"endian","little"},{"fields",fields}});
        aligned.set("a",31);
        aligned.set("b",2047);
        aligned.set("c",4095);
        aligned.set("d",31);
        aligned.set("x",0xAB);
        aligned.set("e",1);
        const std::vector<std::uint8_t> serializedBytes3 = aligned.serialize();
        const std::vector<std::uint8_t> copiedElements2 = std::vector<std::uint8_t>({255,255,255,15,31,0,0xAB,0,1,0});
        check(serializedBytes3==copiedElements2);
        std::vector<std::uint8_t> before=aligned.serialize();
        {
            bool caughtException = false;
            try
            {
                aligned.set("a",32);
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
                aligned.set("b",2048);
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
                aligned.set("a",-1);
            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException);
        };
        const std::vector<std::uint8_t> serializedBytes4 = aligned.serialize();
        check(before==serializedBytes4);
        const std::string valueText = aligned.dump();
        const bool checkResult2 = valueText.find("11 bits @5")!=std::string::npos;
        check(checkResult2);
        for (Json bad : {entry("a","uint16_t",0), entry("a","uint16_t",17),entry("a","int16_t",5),entry("a","float",5),entry("a","Unknown",5)})
        {
            bool caughtException = false;
            try
            {
                const Json array1338 = Json::array({bad});
                message_manager::Message m(Json{{"fields",array1338}});
            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException);
        };
        Json bad=entry("a","uint16_t",5);
        bad["count"]=2;
        {
            bool caughtException = false;
            try
            {
                const Json array1339 = Json::array({bad});
                message_manager::Message m(Json{{"fields",array1339}});
            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException);
        };
        // Nested struct arrays never share units across elements or with their parent.
        const Json entry1340 = entry("a","uint16_t",5);
        const Json entry1341 = entry("b","uint16_t",11);
        const Json entry1342 = entry("prefix","uint16_t",1);
        const Json entry1343 = entry("suffix","uint16_t",1);
        Json types={{"Flags",{{"fields",Json::array({entry1340,entry1341})}}},
            {
                "Root",{{"fields",Json::array({entry1342,Json{{"name","items"},{"type","Flags"},{"count",2}},entry1343})}}
            }
        };
        message_manager::Message nested(Json{{"message","Root"},{"types",types},{"endian","little"}});
        nested.set("prefix",1);
        nested.set("suffix",1);
        nested.set("items[0]", Json{{"a",1},{"b",1}});
        nested.set("items[1]", Json{{"a",2},{"b",2}});
        const std::vector<std::uint8_t> serializedBytes5 = nested.serialize();
        const std::vector<std::uint8_t> copiedElements3 = std::vector<std::uint8_t>({1,0,33,0,66,0,1,0});
        check(serializedBytes5==copiedElements3);
        message_manager::Message nestedCopy=nested.emptyCopy();
        const std::vector<std::uint8_t> serializedBytes6 = nested.serialize();
        nestedCopy.deserialize(serializedBytes6);
        const Json itemsSetting = nested.value("items");
        const Json itemsSetting2 = nestedCopy.value("items");
        check(itemsSetting2==itemsSetting);
        // Three members share one unit; an endian change starts a fresh unit.
        const Json entry1349 = entry("a","uint16_t",3);
        const Json entry1350 = entry("b","uint16_t",4);
        const Json entry1351 = entry("c","uint16_t",9);
        const Json array1352 = Json::array({entry1349,entry1350,entry1351});
        message_manager::Message triple(Json{{"endian","little"},{"fields",array1352}});
        triple.set("a",5);
        triple.set("b",9);
        triple.set("c",300);
        const std::vector<std::uint8_t> serializedBytes7 = triple.serialize();
        const std::vector<std::uint8_t> copiedElements4 = std::vector<std::uint8_t>({0x4D,0x96});
        check(serializedBytes7==copiedElements4);
        message_manager::Message tripleCopy=triple.emptyCopy();
        const std::vector<std::uint8_t> serializedBytes8 = triple.serialize();
        tripleCopy.deserialize(serializedBytes8);
        const unsigned convertedNumber4 = tripleCopy.get<unsigned>("a");
        const unsigned convertedNumber5 = tripleCopy.get<unsigned>("b");
        const unsigned convertedNumber6 = tripleCopy.get<unsigned>("c");
        check(convertedNumber4==5 && convertedNumber5==9 && convertedNumber6==300);
        const Json entry1359 = entry("c","uint8_t",3);
        const Json entry1360 = entry("a","uint16_t",5);
        const Json entry1361 = entry("b","uint16_t",11);
        Json mixedFields=Json::array({entry1360,entry1361,entry1359});
        mixedFields[1]["endian"]="big";
        message_manager::Message mixed(Json{{"endian","little"},{"fields",mixedFields}});
        mixed.set("a",1);
        mixed.set("b",0x123);
        mixed.set("c",7);
        const std::vector<std::uint8_t> serializedBytes9 = mixed.serialize();
        const std::vector<std::uint8_t> copiedElements5 = std::vector<std::uint8_t>({1,0,1,0x23,7});
        check(serializedBytes9==copiedElements5);
        // Actual GCC layout for the requested unsigned 5+11 example on this host.
#if defined(__GNUC__) && defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        struct Native
        {
            std::uint16_t val1 : 5;
            std::uint16_t val2 : 11;
        };
        Native native{};
        native.val1=1;
        native.val2=100;
        check(sizeof(native)==2);
        std::uint8_t nativeBytes[2];
        // 데이터 내용을 지정한 바이트 수만큼 복사합니다. 포인터 주소 자체를 전송하지 않습니다.
        std::memcpy(nativeBytes,&native,2);
        check(nativeBytes[0]==0x81 && nativeBytes[1]==0x0C);
#endif
        // Shared enum labels and external named mappings for both individual bit fields.
        message_manager::MessageManager catalog;
        catalog.loadEnums(folder+"/message_enums.json");
        catalog.loadLink(folder+"/bit_source.json");
        catalog.loadLink(folder+"/bit_target.json");
        message_manager::Message source=catalog.create("bit_source","message1");
        source.set("flags.val1", 1);
        source.set("flags.val2", 100);
        const std::vector<std::uint8_t> serializedBytes10 = source.serialize();
        const std::vector<std::uint8_t> copiedElements6 = std::vector<std::uint8_t>({1,0x81,0x0C});
        check(serializedBytes10==copiedElements6);
        const std::string valueText2 = source.dump();
        const std::string valueText3 = source.dump();
        const bool checkResult3 = valueText2.find("Val1::START")!=std::string::npos && valueText3.find("Val2::NORMAL")!=std::string::npos;
        check(checkResult3);
        message_manager::MessageInterface converter=message_manager::MessageInterface::fromFile(folder+"/interface_message.json",catalog);
        message_manager::Message target=converter.convert(source);
        const unsigned convertedNumber7 = target.get<unsigned>("flags.val1");
        const unsigned convertedNumber8 = target.get<unsigned>("flags.val2");
        check(convertedNumber7==3 && convertedNumber8==300);
        const std::vector<std::uint8_t> serializedBytes11 = target.serialize();
        const std::vector<std::uint8_t> copiedElements7 = std::vector<std::uint8_t>({2,0x83,0x25});
        check(serializedBytes11==copiedElements7);
        const std::string valueText4 = target.dump();
        const bool checkResult4 = valueText4.find("Val2::MAPPED")!=std::string::npos;
        check(checkResult4);
        message_manager::Message parsed=catalog.create("bit_target","message1");
        const std::vector<std::uint8_t> serializedBytes12 = target.serialize();
        parsed.deserialize(serializedBytes12);
        const Json flagsSetting = parsed.value("flags");
        const Json flagsSetting2 = target.value("flags");
        check(flagsSetting==flagsSetting2);
        Json definition=message_manager::readJsonFile(folder+"/interface_message.json");
        definition.erase("mapping_file");
        for(Json &f:definition["interfaces"][0]["fields"])
        {
            f.erase("mapping");
        }
        const Json flagsSetting3 = source.value("flags");
        const message_manager::MessageInterface convertedMessageConverter(definition,catalog);
        const message_manager::Message convertedMessage = convertedMessageConverter.convert(source);
        const Json flagsSetting4 = convertedMessage.value("flags");
        check(flagsSetting4==flagsSetting3);
        Json mappings=message_manager::readJsonFile(folder+"/field_mapping.json");
        mappings["mappings"]["val2_map"]["values"][0]["to"]=2048;
        definition=message_manager::readJsonFile(folder+"/interface_message.json");
        {
            bool caughtException = false;
            try
            {
                const message_manager::MessageInterface converterObject = message_manager::MessageInterface(definition,mappings,catalog);
                converterObject.convertInto("bits_to_bits",source,target);
            }
            catch (const std::exception &)
            {
                caughtException = true;
            }
            check(caughtException);
        };
        const unsigned convertedNumber9 = target.get<unsigned>("flags.val2");
        check(convertedNumber9==300);
        // Dispatch can select a bit field in the same unit as another header value.
        const Json entry1381 = entry("flags","uint16_t",5);
        const Json entry1382 = entry("code","uint16_t",11);
        Json link={{"link","dispatch_bits"},{"endian","little"},{"dispatch",{{"field","code"}}},
            {
                "messages",{{"one",{{"msgcode",3},{"fields",Json::array({entry1381,entry1382})}}}}
            }
        };
        message_manager::MessageManager dispatch(link);
        message_manager::Message msg=dispatch.create("one");
        msg.set("flags",31);
        const std::vector<std::uint8_t> serializedBytes13 = msg.serialize();
        const bool checkResult5 = dispatch.identify(serializedBytes13)._msgcode==3;
        check(checkResult5);
        message_manager::Message rx=dispatch.create("one");
        const std::vector<std::uint8_t> serializedBytes14 = msg.serialize();
        rx.deserialize(serializedBytes14);
        const unsigned convertedNumber10 = rx.get<unsigned>("flags");
        check(convertedNumber10==31);
        // A bit-field length may control a manually sized binary payload.
        const Json entry1386 = entry("flags","uint8_t",3);
        const Json entry1387 = entry("length","uint8_t",5);
        const Json array1388 = Json::array({entry1386,entry1387,Json{{"name","buffer"},{"type","char*"},{"length_from","length"}}});
        message_manager::Message payload(Json{{"fields",array1388}});
        payload.set("flags",7);
        payload.set("length",3);
        const unsigned char buffer[]={0,255,42};
        payload.set("buffer",buffer,3);
        const std::vector<std::uint8_t> serializedBytes15 = payload.serialize();
        const std::vector<std::uint8_t> copiedElements8 = std::vector<std::uint8_t>({31,0,255,42});
        check(serializedBytes15==copiedElements8);
        message_manager::Message payloadCopy=payload.emptyCopy();
        const std::vector<std::uint8_t> serializedBytes16 = payload.serialize();
        payloadCopy.deserialize(serializedBytes16);
        const Json bufferSetting = payload.value("buffer");
        const Json bufferSetting2 = payloadCopy.value("buffer");
        check(bufferSetting2==bufferSetting);
        std::cout << "bitfield checks passed: ";
        std::cout << checks;
        std::cout << '\n';
    }
    catch(const std::exception &e)
    {
        std::cerr << e.what();
        std::cerr << '\n';
        return 1;
    }
}
