#include "app/message_service.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "dao/message_dao.hpp"
#include "dao/message_forward_map_dao.hpp"
#include "dao/message_mention_dao.hpp"
#include "dao/message_read_dao.hpp"
#include "dao/message_user_delete_dao.hpp"
#include "dao/talk_dao.hpp"
#include "dao/user_dao.hpp"
#include "macro.hpp"

namespace CIM::app {

static auto g_logger = CIM_LOG_NAME("root");

uint64_t MessageService::resolveTalkId(uint8_t talk_mode, uint64_t to_from_id) {
    std::string err;
    uint64_t talk_id = 0;

    if (talk_mode == 1) {
        // 单聊: to_from_id 是对端用户ID，需要与当前用户排序，这里无法确定 current_user_id -> 由调用方预先取得 talk_id 更合理。
        // 为简化：此方法仅用于群聊分支；单聊应外层自行处理。返回 0 表示未解析。
        return 0;
    } else if (talk_mode == 2) {
        if (!CIM::dao::TalkDao::getGroupTalkId(to_from_id, talk_id, &err)) {
            // 不存在直接返回 0
            return 0;
        }
        return talk_id;
    }
    return 0;
}

bool MessageService::buildRecord(const CIM::dao::Message& msg, CIM::dao::MessageRecord& out,
                                 std::string* err) {
    out.msg_id = std::to_string(msg.id);
    out.sequence = msg.sequence;
    out.msg_type = msg.msg_type;
    out.from_id = msg.sender_id;
    out.is_revoked = msg.is_revoked;
    out.send_time = TimeUtil::TimeToStr(msg.created_at);
    out.extra = msg.extra;  // 原样透传 JSON 字符串
    out.quote = "{}";

    // 加载用户信息（昵称/头像）
    CIM::dao::UserInfo ui;
    if (!CIM::dao::UserDAO::GetUserInfoSimple(msg.sender_id, ui, err)) {
        // 若加载失败仍返回基础字段
        out.nickname = "";
        out.avatar = "";
    } else {
        out.nickname = ui.nickname;
        out.avatar = ui.avatar;
    }

    // 引用消息
    if (msg.quote_msg_id) {
        CIM::dao::Message quoted;
        std::string qerr;
        if (CIM::dao::MessageDao::GetById(msg.quote_msg_id, quoted, &qerr)) {
            // 简化：仅回传被引用的文本/发送者ID
            std::ostringstream q;
            q << "{\"msg_id\":\"" << quoted.id << "\",\"from_id\":" << quoted.sender_id
              << ",\"text\":\"";
            // 转义内容_text 中的双引号与反斜杠
            for (char c : quoted.content_text) {
                if (c == '"' || c == '\\')
                    q << '\\' << c;
                else if (c == '\n')
                    q << "\\n";
                else
                    q << c;
            }
            q << "\"}";
            out.quote = q.str();
        }
    }
    return true;
}

MessageRecordPageResult MessageService::LoadRecords(uint64_t current_user_id, uint8_t talk_mode,
                                                    uint64_t to_from_id, uint64_t cursor,
                                                    uint32_t limit) {
    MessageRecordPageResult result;
    std::string err;
    if (limit == 0) {
        limit = 30;
    } else if (limit > 200) {
        limit = 200;
    }

    // 解析 talk_id
    uint64_t talk_id = 0;
    if (talk_mode == 1) {
        // 单聊，需要根据两个用户ID排序
        if (!CIM::dao::TalkDao::getSingleTalkId(current_user_id, to_from_id, talk_id, &err)) {
            result.ok = true;  // 无历史记录
            return result;
        }
    } else if (talk_mode == 2) {
        if (!CIM::dao::TalkDao::getGroupTalkId(to_from_id, talk_id, &err)) {
            result.ok = true;  // 群尚未产生消息
            return result;
        }
    } else {
        result.code = 400;
        result.err = "非法会话类型";
        return result;
    }

    CIM_LOG_DEBUG(g_logger) << "talk_id: " << talk_id;

    std::vector<CIM::dao::Message> msgs;
    if (!CIM::dao::MessageDao::ListRecentDesc(talk_id, cursor, limit, msgs, &err)) {
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "LoadRecords ListRecentDesc failed, talk_id=" << talk_id << ", err=" << err;
            result.code = 500;
            result.err = "加载消息失败";
            return result;
        }
    }

    CIM_LOG_DEBUG(g_logger) << "Loaded messages count: " << msgs.size();

    CIM::dao::MessagePage page;
    for (auto& m : msgs) {
        CIM::dao::MessageRecord rec;
        std::string rerr;
        buildRecord(m, rec, &rerr);
        page.items.push_back(std::move(rec));
    }
    if (!page.items.empty()) {
        // 下一游标为当前页最小 sequence
        uint64_t min_seq = page.items.back().sequence;
        page.cursor = min_seq;
    } else {
        page.cursor = cursor;  // 保持不变
    }
    result.data = std::move(page);
    result.ok = true;
    return result;
}

MessageRecordPageResult MessageService::LoadHistoryRecords(uint64_t current_user_id,
                                                           uint8_t talk_mode, uint64_t to_from_id,
                                                           uint16_t msg_type, uint64_t cursor,
                                                           uint32_t limit) {
    MessageRecordPageResult result;
    std::string err;
    if (limit == 0)
        limit = 30;
    else if (limit > 200)
        limit = 200;

    uint64_t talk_id = 0;
    if (talk_mode == 1) {
        if (!CIM::dao::TalkDao::getSingleTalkId(current_user_id, to_from_id, talk_id, &err)) {
            result.ok = true;
            return result;
        }
    } else if (talk_mode == 2) {
        if (!CIM::dao::TalkDao::getGroupTalkId(to_from_id, talk_id, &err)) {
            result.ok = true;
            return result;
        }
    } else {
        result.code = 400;
        result.err = "非法会话类型";
        return result;
    }

    // 先取一页，再过滤类型（简单实现；可优化为 SQL 条件）
    std::vector<CIM::dao::Message> msgs;
    if (!CIM::dao::MessageDao::ListRecentDesc(talk_id, cursor, limit * 3, msgs,
                                              &err)) {  // 加大抓取保证过滤后足够
        result.code = 500;
        result.err = "加载消息失败";
        return result;
    }

    CIM::dao::MessagePage page;
    for (auto& m : msgs) {
        if (msg_type != 0 && m.msg_type != msg_type) continue;
        CIM::dao::MessageRecord rec;
        std::string rerr;
        buildRecord(m, rec, &rerr);
        page.items.push_back(std::move(rec));
        if (page.items.size() >= limit) break;
    }
    if (!page.items.empty()) {
        page.cursor = page.items.back().sequence;
    } else {
        page.cursor = cursor;
    }
    result.data = std::move(page);
    result.ok = true;
    return result;
}

MessageRecordListResult MessageService::LoadForwardRecords(uint64_t current_user_id,
                                                           uint8_t talk_mode,
                                                           const std::vector<uint64_t>& msg_ids) {
    MessageRecordListResult result;
    std::string err;
    if (msg_ids.empty()) {
        result.ok = true;
        return result;
    }

    // 简化：直接批量拉取这些消息
    for (auto mid : msg_ids) {
        CIM::dao::Message m;
        std::string merr;
        if (!CIM::dao::MessageDao::GetById(mid, m, &merr)) continue;  // 忽略不存在
        CIM::dao::MessageRecord rec;
        std::string rerr;
        buildRecord(m, rec, &rerr);
        result.data.push_back(std::move(rec));
    }
    result.ok = true;
    return result;
}

VoidResult MessageService::DeleteMessages(uint64_t current_user_id, uint8_t talk_mode,
                                          uint64_t to_from_id,
                                          const std::vector<uint64_t>& msg_ids) {
    VoidResult result;
    std::string err;
    if (msg_ids.empty()) {
        result.ok = true;
        return result;
    }
    auto db = CIM::MySQLMgr::GetInstance()->get("default");
    if (!db) {
        result.code = 500;
        result.err = "数据库连接失败";
        return result;
    }

    // 验证会话存在（不严格校验每条消息归属以减少查询；生产可增强）
    uint64_t talk_id = 0;
    std::string terr;
    if (talk_mode == 1) {
        if (!CIM::dao::TalkDao::getSingleTalkId(current_user_id, to_from_id, talk_id, &terr)) {
            result.ok = true;
            return result;
        }
    } else if (talk_mode == 2) {
        if (!CIM::dao::TalkDao::getGroupTalkId(to_from_id, talk_id, &terr)) {
            result.ok = true;
            return result;
        }
    } else {
        result.code = 400;
        result.err = "非法会话类型";
        return result;
    }

    for (auto mid : msg_ids) {
        if (!CIM::dao::MessageUserDeleteDao::MarkUserDelete(mid, current_user_id, &err)) {
            CIM_LOG_WARN(g_logger)
                << "DeleteMessages MarkUserDelete failed msg_id=" << mid << " err=" << err;
        }
    }
    result.ok = true;
    return result;
}

VoidResult MessageService::RevokeMessage(uint64_t current_user_id, uint8_t talk_mode,
                                         uint64_t to_from_id, uint64_t msg_id) {
    VoidResult result;
    std::string err;

    CIM::dao::Message message;
    if (!CIM::dao::MessageDao::GetById(msg_id, message, &err)) {
        if (!err.empty()) {
            CIM_LOG_WARN(g_logger)
                << "RevokeMessage GetById error msg_id=" << msg_id << " err=" << err;
            result.code = 500;
            result.err = "消息加载失败";
            return result;
        }
    }

    // 基本权限：仅发送者可撤回
    if (message.sender_id != current_user_id) {
        result.code = 403;
        result.err = "无权限撤回";
        return result;
    }

    if (!CIM::dao::MessageDao::Revoke(msg_id, current_user_id, &err)) {
        if (!err.empty()) {
            result.code = 500;
            result.err = "撤回失败";
            return result;
        }
    }

    result.ok = true;
    return result;
}

}  // namespace CIM::app
