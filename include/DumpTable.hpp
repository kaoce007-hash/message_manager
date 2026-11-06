#pragma once

#include "MessageCommon.hpp"

namespace message_manager
{
    // UTF-8의 코드 포인트를 읽어 한글 enum 이름도 열을 맞춥니다.
    std::uint32_t nextCodePoint(const std::string &text, std::size_t &index);

    // 한글 등 넓은 문자와 영문자의 화면 표시 폭을 구분합니다.
    std::size_t codeWidth(std::uint32_t code);

    // 문자열의 화면 표시 폭을 더합니다. UTF-8 바이트 수와는 다릅니다.
    std::size_t displayWidth(const std::string &text);

    // 표의 셀에 줄바꿈이 들어가지 않도록 제어문자를 표시용 텍스트로 바꿉니다.
    std::string singleLine(const std::string &text);

    // 너무 긴 셀 내용을 지정한 표시 폭에 맞춰 줄입니다.
    std::string shorten(const std::string &text, std::size_t limit = 48);

    // 바이트 배열을 16진수 문자열로 만듭니다. limit가 0이면 전체를 표시합니다.
    std::string hexText(const std::vector<std::uint8_t> &bytes, std::size_t limit);

    class DumpTable
    {
    public:
        void add(const std::vector<std::string> &row)
        {
            std::vector<std::string> cleaned;
            for (const std::string &cell : row)
            {
                // 표 안에서 줄바꿈이 생기지 않도록 표시 문자를 정리합니다.
                const std::string singleLineText = singleLine(cell);
                cleaned.push_back(singleLineText);
            }
            _rows.push_back(cleaned);
        }

        void write(std::ostream &output) const
        {
            if (_rows.empty())
            {
                return;
            }
            const std::vector<std::string> &firstRow = _rows.front();
            const std::size_t firstRowCount = firstRow.size();
            std::vector<std::size_t> widths(firstRowCount, 0);
            for (const std::vector<std::string> &row : _rows)
            {
                for (std::size_t column = 0; column < row.size(); ++column)
                {
                    const std::string &cellText = row[column];
                    // 문자열의 화면 표시 폭을 계산합니다.
                    const std::size_t cellWidth = displayWidth(cellText);
                    if (cellWidth > widths[column])
                    {
                        widths[column] = cellWidth;
                    }
                }
            }
            for (std::size_t index = 0; index < _rows.size(); ++index)
            {
                const std::vector<std::string> &row = _rows[index];
                for (std::size_t column = 0; column < row.size(); ++column)
                {
                    if (column != 0)
                    {
                        output << " | ";
                    }
                    // 문자열의 화면 표시 폭을 계산합니다.
                    const std::size_t displayWidth213 = displayWidth(row[column]);
                    output << row[column];
                    output << std::string(widths[column] - displayWidth213, ' ');
                }
                output << '\n';
                if (index == 0)
                {
                    for (std::size_t column = 0; column < widths.size(); ++column)
                    {
                        if (column != 0)
                        {
                            output << "-+-";
                        }
                        output << std::string(widths[column], '-');
                    }
                    output << '\n';
                }
            }
        }

    protected:

    private:
    private:
        // 표에 출력할 행과 셀 문자열입니다.
        std::vector<std::vector<std::string>> _rows;
    };

}
