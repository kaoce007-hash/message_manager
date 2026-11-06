#include "DumpTable.hpp"

namespace message_manager
{
    // UTF-8 문자 하나를 읽고 index를 다음 문자 위치로 옮깁니다.
    std::uint32_t nextCodePoint(const std::string &text, std::size_t &index)
    {
        const unsigned char first = static_cast<unsigned char>(text[index++]);
        if (first < 0x80)
        {
            return first;
        }
        std::size_t extra = 0;
        std::uint32_t code = first;
        if ((first & 0xE0) == 0xC0)
        {
            extra = 1;
            code = first & 0x1F;
        }
        else
        {
            if ((first & 0xF0) == 0xE0)
            {
                extra = 2;
                code = first & 0x0F;
            }
            else
            {
                if ((first & 0xF8) == 0xF0)
                {
                    extra = 3;
                    code = first & 0x07;
                }
            }
        }
        for (std::size_t count = 0; count < extra && index < text.size(); ++count)
        {
            code = (code << 6) | (static_cast<unsigned char>(text[index++]) & 0x3F);
        }
        return code;
    }

    // 한글 등 넓은 문자와 영문자의 화면 표시 폭을 구분합니다.
    std::size_t codeWidth(std::uint32_t code)
    {
        if ((code >= 0x0300 && code <= 0x036F) || code == 0x200D || (code >= 0xFE00 && code <= 0xFE0F))
        {
            return 0;
        }
        if ((code >= 0x1100 && code <= 0x115F) || (code >= 0x2E80 && code <= 0xA4CF) ||
            (code >= 0xAC00 && code <= 0xD7A3) || (code >= 0xF900 && code <= 0xFAFF) ||
            (code >= 0xFE10 && code <= 0xFE6F) || (code >= 0xFF01 && code <= 0xFF60) ||
            (code >= 0x1F300 && code <= 0x1FAFF))
        {
            return 2;
        }
        return 1;
    }

    // 문자열의 화면 표시 폭을 더합니다. UTF-8 바이트 수와는 다릅니다.
    std::size_t displayWidth(const std::string &text)
    {
        std::size_t width = 0;
        std::size_t index = 0;
        while (index < text.size())
        {
            // UTF-8 문자 하나를 읽고 다음 문자 위치로 이동합니다.
            const std::uint32_t nextCodePoint95 = nextCodePoint(text, index);
            width += codeWidth(nextCodePoint95);
        }
        return width;
    }

    // 표의 셀에 줄바꿈이 들어가지 않도록 제어문자를 표시용 텍스트로 바꿉니다.
    std::string singleLine(const std::string &text)
    {
        std::string result;
        for (char character : text)
        {
            if (character == '\n')
            {
                result += "\\n";
            }
            else
            {
                if (character == '\r')
                {
                    result += "\\r";
                }
                else
                {
                    if (character == '\t')
                    {
                        result += "\\t";
                    }
                    else
                    {
                        bool matchesCondition = static_cast<unsigned char>(character) < 0x20;
                        if (!matchesCondition)
                        {
                            matchesCondition = character == 0x7F;
                        }
                        if (matchesCondition)
                        {
                            result += '?';
                        }
                        else
                        {
                            result += character;
                        }
                    }
                }
            }
        }
        return result;
    }

    // 너무 긴 셀 내용을 지정한 표시 폭에 맞춰 줄입니다.
    std::string shorten(const std::string &text, std::size_t limit)
    {
        if (displayWidth(text) <= limit)
        {
            return text;
        }
        std::size_t index = 0;
        std::size_t end = 0;
        std::size_t width = 0;
        while (index < text.size())
        {
            // UTF-8 문자 하나를 읽고 다음 문자 위치로 이동합니다.
            const std::uint32_t nextCodePoint96 = nextCodePoint(text, index);
            width += codeWidth(nextCodePoint96);
            if (width > limit - 3)
            {
                break;
            }
            end = index;
        }
        const std::string prefix = text.substr(0, end);
        const std::string result = prefix + "...";
        return result;
    }

    // 바이트 배열을 16진수 문자열로 만듭니다. limit가 0이면 전체를 표시합니다.
    // ostringstream는 메모리에 문자열을 만드는 출력 스트림입니다. cout처럼 <<를 쓰되 화면 대신 문자열에 쌓습니다.
    // std::hex/uppercase는 이후 숫자의 표시 방식, setfill은 빈칸 문자, setw(2)는 다음 항목의 최소 폭입니다.
    // 숫자 10을 0A로 표시하려고 씁니다. 원본 바이트 자체가 달라지는 것은 아닙니다.
    std::string hexText(const std::vector<std::uint8_t> &bytes, std::size_t limit)
    {
        std::ostringstream output;
        std::size_t count = bytes.size();
        if (limit != 0 && count > limit)
        {
            count = limit;
        }
        for (std::size_t index = 0; index < count; ++index)
        {
            if (index != 0)
            {
                output << ' ';
            }
            output << std::hex;
            output << std::uppercase;
            output << std::setw(2);
            output << std::setfill('0');
            output << static_cast<unsigned int>(bytes[index]);
        }
        if (count < bytes.size())
        {
            output << " ...";
        }
        if (bytes.empty())
        {
            output << "(empty)";
        }
        const std::string result = output.str();
        return result;
    }

}
