#ifndef __CIM_APP_MESSAGE_SERVICE_HPP__
#define __CIM_APP_MESSAGE_SERVICE_HPP__

#include <cstdint>
#include <string>
#include <vector>

#include "app/result.hpp"

namespace CIM::app {

// 消息服务：提供消息列表/历史/转发/删除/撤回等业务操作。
// 说明：
// - 仅封装 DAO 与简单权限校验；复杂业务（敏感词、风控、群成员校验）应由更上层处理。
// - 分页逻辑：cursor=0 表示获取最新；返回的 cursor=当前页最小 sequence，用于下一次继续向旧消息翻页。
class MessageService {
   public:
    // 获取当前会话消息（按 sequence DESC 返回最新 -> 较旧）。
    static MessageRecordPageResult LoadRecords(uint64_t current_user_id, uint8_t talk_mode,
                                               uint64_t to_from_id, uint64_t cursor,
                                               uint32_t limit);

    // 获取当前会话历史消息（支持按 msg_type 过滤，0=全部）。
    static MessageRecordPageResult LoadHistoryRecords(uint64_t current_user_id, uint8_t talk_mode,
                                                      uint64_t to_from_id, uint16_t msg_type,
                                                      uint64_t cursor, uint32_t limit);

    // 获取转发消息记录（传入一组消息ID，返回消息详情；不分页）。
    static MessageRecordListResult LoadForwardRecords(uint64_t current_user_id, uint8_t talk_mode,
                                                      const std::vector<uint64_t>& msg_ids);

    // 删除聊天记录（仅对本人视图生效）。
    static VoidResult DeleteMessages(uint64_t current_user_id, uint8_t talk_mode,
                                     uint64_t to_from_id, const std::vector<uint64_t>& msg_ids);

    // 撤回消息（仅发送者可撤回，后续可扩展管理员权限）。
    static VoidResult RevokeMessage(uint64_t current_user_id, uint8_t talk_mode,
                                    uint64_t to_from_id, uint64_t msg_id);

   private:
    // 根据 talk_mode 与对象ID 获取会话 talk_id（不存在返回 0）。
    static uint64_t resolveTalkId(uint8_t talk_mode, uint64_t to_from_id);

    // 将 DAO Message 转换为前端需要的记录结构（补充用户昵称头像、引用）。
    static bool buildRecord(const CIM::dao::Message& msg, CIM::dao::MessageRecord& out,
                            std::string* err);
};

}  // namespace CIM::app

#endif  // __CIM_APP_MESSAGE_SERVICE_HPP__