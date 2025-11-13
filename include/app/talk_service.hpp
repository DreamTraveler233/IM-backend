#ifndef __CIM_APP_TALK_SESSION_HPP__
#define __CIM_APP_TALK_SESSION_HPP__

#include "dao/talk_session_dao.hpp"
#include "result.hpp"

namespace CIM::app {

class TalkService {
   public:
    // 获取用户的会话列表
    static TalkSessionListResult getSessionListByUserId(const uint64_t user_id);
    // 设置会话置顶/取消置顶
    static VoidResult setSessionTop(const uint64_t user_id, const uint64_t to_from_id,
                                    const uint8_t talk_mode, const uint8_t action);
    // 设置会话免打扰/取消免打扰
    static VoidResult setSessionDisturb(const uint64_t user_id, const uint64_t to_from_id,
                                        const uint8_t talk_mode, const uint8_t action);

    // 创建会话
    static TalkSessionResult createSession(const uint64_t user_id, const uint64_t to_from_id,
                                           const uint8_t talk_mode);
    // 删除会话
    static VoidResult deleteSession(const uint64_t user_id, const uint64_t to_from_id,
                                    const uint8_t talk_mode);

    // 清除会话未读消息数
    static VoidResult clearSessionUnreadNum(const uint64_t user_id, const uint64_t to_from_id,
                                            const uint8_t talk_mode);
};

}  // namespace CIM::app

#endif  // __CIM_APP_TALK_SESSION_HPP__