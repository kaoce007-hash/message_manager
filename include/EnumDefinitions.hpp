#pragma once

#include "MessageCommon.hpp"

namespace message_manager
{
    // enum 정의는 메시지와 분리된 JSON에서 읽습니다.
    class EnumDefinitions
    {
    public:
        // enum JSON의 enums 아래 내용을 검사한 뒤 내부 사전에 추가합니다. 실패하면 기존 사전은 유지합니다.
        void load(const std::string &path);

        // 필드에서 참조한 enum이 존재하고 정수 타입도 같은지 확인합니다.
        void validate(const std::string &enumName, const std::string &fieldType) const;

        // 숫자 값에 해당하는 enum 표시 이름을 반환합니다. 예: 1 -> DeviceState::정상.
        std::string label(const std::string &enumName, const Json &value) const;

    protected:

    private:
    private:
        // enum 이름 -> {type, values}를 보관합니다. JSON의 최상위 enums 껍질을 뺀 내용입니다.
        Json _definitions = Json::object();
    };
}
