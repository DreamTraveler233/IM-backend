#include "app/message_service.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "api/ws_gateway_module.hpp"
#include "app/talk_service.hpp"
#include "base/macro.hpp"
#include "common/message_preview_map.hpp"
#include "common/message_type_map.hpp"
#include "dao/contact_dao.hpp"
#include "dao/message_dao.hpp"
#include "dao/message_forward_map_dao.hpp"
#include "dao/message_mention_dao.hpp"
#include "dao/message_read_dao.hpp"
#include "dao/message_user_delete_dao.hpp"
#include "dao/talk_dao.hpp"
#include "dao/talk_sequence_dao.hpp"
#include "dao/talk_session_dao.hpp"
#include "dao/user_dao.hpp"
#include "util/hash_util.hpp"

namespace CIM::app {

static auto g_logger = CIM_LOG_NAME("root");
static constexpr const char* kDBName = "default";

uint64_t MessageService::resolveTalkId(const uint8_t talk_mode, const uint64_t to_from_id) {
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
    out.msg_id = msg.id;
    out.sequence = msg.sequence;
    out.msg_type = msg.msg_type;
    out.from_id = msg.sender_id;
    out.is_revoked = msg.is_revoked;
    out.send_time = TimeUtil::TimeToStr(msg.created_at);
    out.extra = msg.extra;  // 原样透传 JSON 字符串
    out.quote = "{}";

    // 标准化 extra 输出：对于文本在 extra.content 中补齐 content；
    // 同时统一把 mentions 列表放入 extra.mentions 中，方便前端渲染/高亮
    Json::Value extraJson(Json::objectValue);
    if (msg.msg_type == 1) {
        extraJson["content"] = msg.content_text;
    } else {
        if (!msg.extra.empty()) {
            Json::CharReaderBuilder rb;
            std::string errs;
            std::istringstream in(msg.extra);
            if (Json::parseFromStream(rb, in, &extraJson, &errs)) {
                // parsed ok
            } else {
                extraJson = Json::objectValue;  // 保持空对象
            }
        }
    }
    // 补齐 mentions
    std::vector<uint64_t> mentioned;
    std::string merr;
    if (CIM::dao::MessageMentionDao::GetMentions(msg.id, mentioned, &merr)) {
        if (!mentioned.empty()) {
            Json::Value arr(Json::arrayValue);
            for (auto id : mentioned) arr.append((Json::UInt64)id);
            extraJson["mentions"] = arr;
        }
    }
    Json::StreamWriterBuilder wb;
    out.extra = Json::writeString(wb, extraJson);

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
    if (!msg.quote_msg_id.empty()) {
        CIM::dao::Message quoted;
        std::string qerr;
        if (CIM::dao::MessageDao::GetById(msg.quote_msg_id, quoted, &qerr)) {
            // 适配前端结构：{"quote_id":"...","content":"...","from_id":...}
            Json::Value qjson;
            qjson["quote_id"] = quoted.id;
            qjson["from_id"] = (Json::UInt64)quoted.sender_id;
            qjson["content"] = quoted.content_text;  // 仅文本简化
            Json::StreamWriterBuilder wbq;
            out.quote = Json::writeString(wbq, qjson);
        }
    }
    return true;
}

MessageRecordPageResult MessageService::LoadRecords(const uint64_t current_user_id,
                                                    const uint8_t talk_mode,
                                                    const uint64_t to_from_id, uint64_t cursor,
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

    std::vector<CIM::dao::Message> msgs;
    // 使用带过滤的查询，过滤掉已被当前用户删除的消息（im_message_user_delete）
    if (!CIM::dao::MessageDao::ListRecentDescWithFilter(talk_id, cursor, limit,
                                                        /*user_id=*/current_user_id,
                                                        /*msg_type=*/0, msgs, &err)) {
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "LoadRecords ListRecentDescWithFilter failed, talk_id=" << talk_id
                << ", err=" << err;
            result.code = 500;
            result.err = "加载消息失败";
            return result;
        }
    }

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

MessageRecordPageResult MessageService::LoadHistoryRecords(const uint64_t current_user_id,
                                                           const uint8_t talk_mode,
                                                           const uint64_t to_from_id,
                                                           const uint16_t msg_type, uint64_t cursor,
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

MessageRecordListResult MessageService::LoadForwardRecords(
    const uint64_t current_user_id, const uint8_t talk_mode,
    const std::vector<std::string>& msg_ids) {
    MessageRecordListResult result;
    std::string err;
    if (msg_ids.empty()) {
        result.ok = true;
        return result;
    }

    // 简化：直接批量拉取这些消息
    for (auto& mid : msg_ids) {
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

VoidResult MessageService::DeleteMessages(const uint64_t current_user_id, const uint8_t talk_mode,
                                          const uint64_t to_from_id,
                                          const std::vector<std::string>& msg_ids) {
    VoidResult result;
    std::string err;

    // 1. 如果 msg_ids 为空，直接返回成功
    if (msg_ids.empty()) {
        result.ok = true;
        return result;
    }

    // 2. 开启事务。
    auto trans = CIM::MySQLMgr::GetInstance()->openTransaction(kDBName, false);
    if (!trans) {
        CIM_LOG_DEBUG(g_logger) << "DeleteMessages openTransaction failed";
        result.code = 500;
        result.err = "数据库事务创建失败";
        return result;
    }

    // 3. 获取数据库连接
    auto db = trans->getMySQL();
    if (!db) {
        CIM_LOG_DEBUG(g_logger) << "DeleteMessages getMySQL failed";
        result.code = 500;
        result.err = "数据库连接获取失败";
        return result;
    }

    // 4. 验证会话存在（不严格校验每条消息归属以减少查询；生产可增强）
    uint64_t talk_id = 0;
    if (talk_mode == 1) {
        if (!CIM::dao::TalkDao::getSingleTalkId(current_user_id, to_from_id, talk_id, &err)) {
            if (!err.empty()) {
                CIM_LOG_WARN(g_logger) << "DeleteMessages getSingleTalkId failed, err=" << err;
                result.code = 500;
                result.err = "删除消息失败";
                return result;
            }
        }
    } else if (talk_mode == 2) {
        if (!CIM::dao::TalkDao::getGroupTalkId(to_from_id, talk_id, &err)) {
            if (!err.empty()) {
                CIM_LOG_WARN(g_logger) << "DeleteMessages getGroupTalkId failed, err=" << err;
                result.code = 500;
                result.err = "删除消息失败";
                return result;
            }
        }
    } else {
        result.code = 400;
        result.err = "非法会话类型";
        return result;
    }

    // 5. 标记删除(针对当前用户视角的软删除)
    for (auto& mid : msg_ids) {
        if (!CIM::dao::MessageUserDeleteDao::MarkUserDelete(db, mid, current_user_id, &err)) {
            if (!err.empty()) {
                trans->rollback();
                CIM_LOG_WARN(g_logger) << "DeleteMessages MarkUserDelete failed err=" << err;
                result.code = 500;
                result.err = "删除消息失败";
                return result;
            }
        }
    }

    // 6. 标记删除后，需要更新会话的最后消息摘要（仅影响当前用户的会话视图）
    std::vector<CIM::dao::Message> remain_msgs;
    std::string digest;
    if (!CIM::dao::MessageDao::ListRecentDescWithFilter(db, talk_id, /*anchor_seq=*/0,
                                                        /*limit=*/1, current_user_id,
                                                        /*msg_type=*/0, remain_msgs, &err)) {
        if (!err.empty()) {
            CIM_LOG_WARN(g_logger) << "ListRecentDescWithFilter failed: " << err;
            result.code = 500;
            result.err = "删除消息失败";
            return result;
        }
    } else {
        if (!remain_msgs.empty()) {
            const auto& lm = remain_msgs[0];
            auto mtype = static_cast<CIM::common::MessageType>(lm.msg_type);
            if (mtype == CIM::common::MessageType::Text) {
                // 如果是文本消息，直接截取前 255 字符作为摘要
                digest = lm.content_text;
                if (digest.size() > 255) digest = digest.substr(0, 255);
            } else {
                // 非文本消息使用统一预览文本
                auto it = CIM::common::kMessagePreviewMap.find(mtype);
                if (it != CIM::common::kMessagePreviewMap.end()) {
                    digest = it->second;
                } else {
                    digest = "[非文本消息]";
                }
            }

            // 更新最后消息
            if (!CIM::dao::TalkSessionDAO::updateLastMsgForUser(
                    db, current_user_id, talk_id, std::optional<std::string>(lm.id),
                    std::optional<uint16_t>(lm.msg_type), std::optional<uint64_t>(lm.sender_id),
                    std::optional<std::string>(digest), &err)) {
                if (!err.empty()) {
                    trans->rollback();
                    CIM_LOG_WARN(g_logger) << "updateLastMsgForUser failed: " << err;
                    result.code = 500;
                    result.err = "删除消息失败";
                    return result;
                }
            }
        } else {
            // 没有剩余消息，清空最后消息字段
            if (!CIM::dao::TalkSessionDAO::updateLastMsgForUser(
                    db, current_user_id, talk_id, std::optional<std::string>(),
                    std::optional<uint16_t>(), std::optional<uint64_t>(),
                    std::optional<std::string>(), &err)) {
                if (!err.empty()) {
                    trans->rollback();
                    CIM_LOG_WARN(g_logger) << "updateLastMsgForUser failed: " << err;
                    result.code = 500;
                    result.err = "删除消息失败";
                    return result;
                }
            }
        }
    }

    // 7. 提交事务
    if (!trans->commit()) {
        const auto commit_err = db->getErrStr();
        trans->rollback();
        CIM_LOG_WARN(g_logger) << "DeleteMessages transaction commit failed err=" << commit_err;
        result.code = 500;
        result.err = "数据库事务提交失败";
        return result;
    }

    // 8. 通知客户端更新消息预览
    if (!digest.empty()) {
        Json::Value payload;
        payload["talk_mode"] = talk_mode;
        payload["to_from_id"] = to_from_id;
        payload["msg_text"] = digest;
        payload["updated_at"] = CIM::TimeUtil::NowToMS();
        CIM::api::WsGatewayModule::PushToUser(current_user_id, "im.session.update", payload);
    } else {
        Json::Value payload;
        payload["talk_mode"] = talk_mode;
        payload["to_from_id"] = to_from_id;
        payload["msg_text"] = Json::Value();
        payload["updated_at"] = CIM::TimeUtil::NowToMS();
        CIM::api::WsGatewayModule::PushToUser(current_user_id, "im.session.update", payload);
    }

    result.ok = true;
    return result;
}

VoidResult MessageService::DeleteAllMessagesInTalkForUser(const uint64_t current_user_id,
                                                          const uint8_t talk_mode,
                                                          const uint64_t to_from_id) {
    VoidResult result;
    std::string err;

    // 1. 开启事务
    auto trans = CIM::MySQLMgr::GetInstance()->openTransaction(kDBName, false);
    if (!trans) {
        CIM_LOG_DEBUG(g_logger) << "DeleteAllMessagesInTalkForUser openTransaction failed";
        result.code = 500;
        result.err = "数据库事务创建失败";
        return result;
    }

    // 2. 获取数据库连接
    auto db = trans->getMySQL();
    if (!db) {
        CIM_LOG_DEBUG(g_logger) << "DeleteAllMessagesInTalkForUser getMySQL failed";
        result.code = 500;
        result.err = "数据库连接获取失败";
        return result;
    }

    // 3. 解析 talk_id
    uint64_t talk_id = 0;
    if (talk_mode == 1) {
        if (!CIM::dao::TalkDao::getSingleTalkId(current_user_id, to_from_id, talk_id, &err)) {
            if (!err.empty()) {
                CIM_LOG_WARN(g_logger)
                    << "DeleteAllMessagesInTalkForUser getSingleTalkId failed, err=" << err;
                result.code = 500;
                result.err = "删除消息失败";
                return result;
            }
        }
    } else if (talk_mode == 2) {
        if (!CIM::dao::TalkDao::getGroupTalkId(to_from_id, talk_id, &err)) {
            if (!err.empty()) {
                CIM_LOG_WARN(g_logger)
                    << "DeleteAllMessagesInTalkForUser getGroupTalkId failed, err=" << err;
                result.code = 500;
                result.err = "删除消息失败";
                return result;
            }
        }
    } else {
        result.code = 400;
        result.err = "非法会话类型";
        return result;
    }

    // 4. 批量标记会话中的所有消息为当前用户删除
    if (!CIM::dao::MessageUserDeleteDao::MarkAllMessagesDeletedByUserInTalk(
            db, talk_id, current_user_id, &err)) {
        if (!err.empty()) {
            trans->rollback();
            CIM_LOG_WARN(g_logger)
                << "MarkAllMessagesDeletedByUserInTalk failed, talk_id=" << talk_id
                << ", err=" << err;
            result.code = 500;
            result.err = "删除消息失败";
            return result;
        }
    }

    // 5. 清空会话最后消息
    if (!CIM::dao::TalkSessionDAO::updateLastMsgForUser(
            db, current_user_id, talk_id, std::optional<std::string>(), std::optional<uint16_t>(),
            std::optional<uint64_t>(), std::optional<std::string>(), &err)) {
        if (!err.empty()) {
            trans->rollback();
            CIM_LOG_WARN(g_logger) << "updateLastMsgForUser failed: " << err;
            result.code = 500;
            result.err = "删除消息失败";
            return result;
        }
    }

    // 6. 删除会话视图
    if (!dao::TalkSessionDAO::deleteSession(db, current_user_id, to_from_id, talk_mode, &err)) {
        if (!err.empty()) {
            trans->rollback();
            CIM_LOG_ERROR(g_logger)
                << "DeleteAllMessagesInTalkForUser deleteSession failed, err=" << err;
            result.code = 500;
            result.err = "删除会话失败";
            return result;
        }
    }

    // 7. 提交事务
    if (!trans->commit()) {
        const auto commit_err = db->getErrStr();
        CIM_LOG_ERROR(g_logger) << "DeleteAllMessagesInTalkForUser commit failed, err="
                                << commit_err;
        result.code = 500;
        result.err = "删除消息失败";
        return result;
    }

    // 8. 向客户端推送更新消息预览
    Json::Value payload;
    payload["talk_mode"] = talk_mode;
    payload["to_from_id"] = to_from_id;
    payload["msg_text"] = Json::Value();
    payload["updated_at"] = CIM::TimeUtil::NowToMS();
    CIM::api::WsGatewayModule::PushToUser(current_user_id, "im.session.update", payload);

    result.ok = true;
    return result;
}

VoidResult MessageService::RevokeMessage(const uint64_t current_user_id, const uint8_t talk_mode,
                                         const uint64_t to_from_id, const std::string& msg_id) {
    VoidResult result;
    std::string err;

    // 1. 开启事务
    auto trans = CIM::MySQLMgr::GetInstance()->openTransaction(kDBName, false);
    if (!trans) {
        CIM_LOG_DEBUG(g_logger) << "DeleteAllMessagesInTalkForUser openTransaction failed";
        result.code = 500;
        result.err = "数据库事务创建失败";
        return result;
    }

    // 2. 获取数据库连接
    auto db = trans->getMySQL();
    if (!db) {
        CIM_LOG_DEBUG(g_logger) << "DeleteAllMessagesInTalkForUser getMySQL failed";
        result.code = 500;
        result.err = "数据库连接获取失败";
        return result;
    }

    // 3. 加载消息
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

    // 4. 撤回消息
    if (!CIM::dao::MessageDao::Revoke(db, msg_id, current_user_id, &err)) {
        if (!err.empty()) {
            trans->rollback();
            CIM_LOG_ERROR(g_logger) << "RevokeMessage Revoke failed err=" << err;
            result.code = 500;
            result.err = "撤回失败";
            return result;
        }
    }

    // 5. 撤回成功后：若该消息为会话快照中的最后消息，则需要为受影响的用户重建/清空会话摘要
    // 先确定该消息所属的 talk_id（之前已加载到 message）
    uint64_t talk_id = message.talk_id;
    std::vector<uint64_t> affected_users;
    std::vector<uint64_t> update_uids;
    std::vector<uint64_t> clear_uids;
    std::vector<std::string> digest_vec;  // 用于推送内容
    if (!CIM::dao::TalkSessionDAO::listUsersByLastMsg(db, talk_id, msg_id, affected_users, &err)) {
        if (!err.empty()) {
            trans->rollback();
            CIM_LOG_WARN(g_logger) << "listUsersByLastMsg failed: " << err;
            result.code = 500;
            result.err = "撤回失败";
            return result;
        }
    } else {
        // 为每个受影响用户重建最后消息摘要
        for (auto uid : affected_users) {
            std::vector<CIM::dao::Message> remain_msgs;
            if (!CIM::dao::MessageDao::ListRecentDescWithFilter(db, talk_id, /*anchor_seq=*/0,
                                                                /*limit=*/1, uid,
                                                                /*msg_type=*/0, remain_msgs,
                                                                &err)) {
                if (!err.empty()) {
                    trans->rollback();
                    CIM_LOG_ERROR(g_logger)
                        << "ListRecentDescWithFilter failed for uid=" << uid << " err=" << err;
                    result.code = 500;
                    result.err = "撤回失败";
                    return result;
                }
            }
            if (!remain_msgs.empty()) {
                update_uids.push_back(uid);
                const auto& lm = remain_msgs[0];
                std::string digest;
                auto mtype = static_cast<CIM::common::MessageType>(lm.msg_type);
                if (mtype == CIM::common::MessageType::Text) {
                    digest = lm.content_text;
                    if (digest.size() > 255) digest = digest.substr(0, 255);
                } else {
                    auto it = CIM::common::kMessagePreviewMap.find(mtype);
                    if (it != CIM::common::kMessagePreviewMap.end()) {
                        digest = it->second;
                    } else {
                        digest = "[非文本消息]";
                    }
                }
                digest_vec.push_back(digest);
                if (!CIM::dao::TalkSessionDAO::updateLastMsgForUser(
                        db, uid, talk_id, std::optional<std::string>(lm.id),
                        std::optional<uint16_t>(lm.msg_type), std::optional<uint64_t>(lm.sender_id),
                        std::optional<std::string>(digest), &err)) {
                    if (!err.empty()) {
                        trans->rollback();
                        CIM_LOG_ERROR(g_logger)
                            << "updateLastMsgForUser failed uid=" << uid << " err=" << err;
                        result.code = 500;
                        result.err = "撤回失败";
                        return result;
                    }
                }
            } else {
                clear_uids.push_back(uid);
                // 没有剩余消息，清空最后消息字段
                if (!CIM::dao::TalkSessionDAO::updateLastMsgForUser(
                        db, uid, talk_id, std::optional<std::string>(), std::optional<uint16_t>(),
                        std::optional<uint64_t>(), std::optional<std::string>(), &err)) {
                    if (!err.empty()) {
                        trans->rollback();
                        CIM_LOG_ERROR(g_logger)
                            << "clear last msg for user failed uid=" << uid << " err=" << err;
                        result.code = 500;
                        result.err = "撤回失败";
                        return result;
                    }
                }
            }
        }
    }

    // 6. 提交事务
    if (!trans->commit()) {
        const auto commit_err = db->getErrStr();
        trans->rollback();
        CIM_LOG_ERROR(g_logger) << "RevokeMessage transaction commit failed err=" << commit_err;
        result.code = 500;
        result.err = "数据库事务提交失败";
        return result;
    }

    // 7. 通知客户端更新消息预览
    int index = 0;
    if (!digest_vec.empty()) {
        for (const auto& uid : update_uids) {
            Json::Value payload;
            payload["talk_mode"] = talk_mode;
            // For single chat, compute to_from_id per receiver: it's always the other party's id
            if (talk_mode == 1) {
                payload["to_from_id"] = (Json::UInt64)(uid == current_user_id ? to_from_id
                                                                                    : current_user_id);
            } else {
                payload["to_from_id"] = (Json::UInt64)to_from_id;
            }
            payload["msg_text"] = digest_vec[index++];
            payload["updated_at"] = CIM::TimeUtil::NowToMS();
            CIM::api::WsGatewayModule::PushToUser(uid, "im.session.update", payload);
        }
    } else {
        for (const auto& uid : clear_uids) {
            Json::Value payload;
            payload["talk_mode"] = talk_mode;
            if (talk_mode == 1) {
                payload["to_from_id"] = (Json::UInt64)(uid == current_user_id ? to_from_id
                                                                                    : current_user_id);
            } else {
                payload["to_from_id"] = (Json::UInt64)to_from_id;
            }
            payload["msg_text"] = Json::Value();
            payload["updated_at"] = CIM::TimeUtil::NowToMS();
            CIM::api::WsGatewayModule::PushToUser(uid, "im.session.update", payload);
        }
    }

    // 8. 广播撤回事件给会话中的在线用户，方便他们更新对端消息状态
    try {
        std::vector<uint64_t> talk_users;
        std::string lerr;
        if (CIM::dao::TalkSessionDAO::listUsersByTalkId(talk_id, talk_users, &lerr)) {
            Json::Value ev;
            ev["talk_mode"] = talk_mode;
            ev["to_from_id"] = (Json::UInt64)to_from_id;
            ev["from_id"] = (Json::UInt64)message.sender_id;
            ev["msg_id"] = msg_id;
            for (auto uid : talk_users) {
                CIM::api::WsGatewayModule::PushToUser(uid, "im.message.revoke", ev);
            }
        }
    } catch (const std::exception& ex) {
        CIM_LOG_WARN(g_logger) << "broadcast revoke event failed: " << ex.what();
    }
    
    result.ok = true;
    return result;
}

MessageRecordResult MessageService::SendMessage(
    const uint64_t current_user_id, const uint8_t talk_mode, const uint64_t to_from_id,
    const uint16_t msg_type, const std::string& content_text, const std::string& extra,
    const std::string& quote_msg_id, const std::string& msg_id,
    const std::vector<uint64_t>& mentioned_user_ids) {
    MessageRecordResult result;
    std::string err;

    // 1. 开启事务。
    auto trans = CIM::MySQLMgr::GetInstance()->openTransaction(kDBName, false);
    if (!trans) {
        CIM_LOG_DEBUG(g_logger) << "SendMessage openTransaction failed";
        result.code = 500;
        result.err = "数据库事务创建失败";
        return result;
    }

    // 2. 获取数据库连接
    auto db = trans->getMySQL();
    if (!db) {
        CIM_LOG_DEBUG(g_logger) << "SendMessage getMySQL failed";
        result.code = 500;
        result.err = "数据库连接获取失败";
        return result;
    }

    // 3. 查询或者创建 talk_id
    uint64_t talk_id = 0;
    if (talk_mode == 1) {
        // 单聊不存在则创建会话
        if (!CIM::dao::TalkDao::findOrCreateSingleTalk(db, current_user_id, to_from_id, talk_id,
                                                       &err)) {
            if (!err.empty()) {
                trans->rollback();
                CIM_LOG_ERROR(g_logger) << "SendMessage findOrCreateSingleTalk failed, err=" << err;
                result.code = 500;
                result.err = "创建单聊会话失败";
                return result;
            }
        }
    } else if (talk_mode == 2) {
        // 群聊不存在则创建会话（不校验群合法性，这里假设外层已校验）
        if (!CIM::dao::TalkDao::findOrCreateGroupTalk(db, to_from_id, talk_id, &err)) {
            if (!err.empty()) {
                trans->rollback();
                CIM_LOG_ERROR(g_logger) << "SendMessage findOrCreateGroupTalk failed, err=" << err;
                result.code = 500;
                result.err = "创建群聊会话失败";
                return result;
            }
        }
    } else {
        trans->rollback();
        result.code = 400;
        result.err = "非法会话类型";
        return result;
    }

    // 4. 计算 sequence
    uint64_t next_seq = 0;
    if (!CIM::dao::TalkSequenceDao::nextSeq(db, talk_id, next_seq, &err)) {
        if (!err.empty()) {
            trans->rollback();
            CIM_LOG_ERROR(g_logger) << "SendMessage nextSeq failed, err=" << err;
            result.code = 500;
            result.err = "分配消息序列失败";
            return result;
        }
    }

    // 5. 创建消息记录
    // 说明：不同消息类型（text, image, forward 等）会将不同字段写入：
    //  - 文本: content_text 存储正文；extra 可为空
    //  - 非文本: payload 序列化到 extra 字段，前端也会从 extra 解析出对应信息
    //  - 引用: quote_msg_id 记录被引用消息的 ID
    CIM::dao::Message m;
    m.talk_id = talk_id;
    m.sequence = next_seq;
    m.talk_mode = talk_mode;
    m.msg_type = msg_type;
    m.sender_id = current_user_id;
    if (talk_mode == 1) {  // 单聊
        m.receiver_id = to_from_id;
        m.group_id = 0;
    } else {  // 群聊
        m.receiver_id = 0;
        m.group_id = to_from_id;
    }
    m.content_text = content_text;  // 文本类存这里，其它类型留空
    m.extra = extra;                // 非文本 JSON 或补充字段
    // 若为转发：在服务器端补充 preview records（方便前端短缩显示）
    if (m.msg_type == static_cast<uint16_t>(CIM::common::MessageType::Forward) &&
        !m.extra.empty()) {
        Json::CharReaderBuilder rb;
        Json::Value payload;
        std::string errs;
        std::istringstream in(m.extra);
        if (Json::parseFromStream(rb, in, &payload, &errs)) {
            std::vector<std::string> src_ids;
            if (payload.isMember("msg_ids") && payload["msg_ids"].isArray()) {
                for (auto& v : payload["msg_ids"]) {
                    if (v.isString()) {
                        src_ids.push_back(v.asString());
                    } else if (v.isUInt64()) {
                        src_ids.push_back(std::to_string(v.asUInt64()));
                    } else if (v.isInt()) {
                        src_ids.push_back(std::to_string(v.asInt()));
                    }
                }
            }

            // 限制 preview 数量，防止超大 payload
            const size_t kMaxPreview = 50;
            if (!src_ids.empty()) {
                if (src_ids.size() > kMaxPreview) src_ids.resize(kMaxPreview);
                std::vector<CIM::dao::Message> src_msgs;
                if (CIM::dao::MessageDao::GetByIds(src_ids, src_msgs, &err)) {
                    Json::Value arr(Json::arrayValue);
                    for (auto& s : src_msgs) {
                        Json::Value it;
                        // 获取发送者昵称
                        CIM::dao::UserInfo ui;
                        if (CIM::dao::UserDAO::GetUserInfoSimple(s.sender_id, ui, &err)) {
                            it["nickname"] = ui.nickname;
                        } else {
                            it["nickname"] = Json::Value();
                        }
                        // 仅使用文本内容作为预览（其它类型可扩展）
                        it["content"] = s.content_text;
                        arr.append(it);
                    }
                    payload["records"] = arr;
                    Json::StreamWriterBuilder wb;
                    m.extra = Json::writeString(wb, payload);
                } else {
                    CIM_LOG_WARN(g_logger)
                        << "MessageDao::GetByIds failed when build preview records: " << err;
                }
            }
        }
    }
    m.quote_msg_id = quote_msg_id;
    m.is_revoked = 2;  // 正常
    m.revoke_by = 0;
    m.revoke_time = 0;
    // 使用前端传入的消息ID；若为空则服务端生成一个16进制32位随机字符串
    if (msg_id.empty()) {
        // 随机生成 32 长度 hex id
        m.id = CIM::random_string(32, "0123456789abcdef");
    } else {
        m.id = msg_id;
    }

    if (!CIM::dao::MessageDao::Create(db, m, &err)) {
        if (!err.empty()) {
            trans->rollback();
            CIM_LOG_ERROR(g_logger) << "MessageDao::Create failed: " << err;
            result.code = 500;
            result.err = "消息写入失败";
            return result;
        }
    }

    // 处理 @ 提及：把提及映射写入 im_message_mention 表，便于快速查询“被@到的记录”。
    if (!mentioned_user_ids.empty()) {
        if (!CIM::dao::MessageMentionDao::AddMentions(db, m.id, mentioned_user_ids, &err)) {
            if (!err.empty()) {
                trans->rollback();
                CIM_LOG_WARN(g_logger) << "AddMentions failed: " << err;
                result.code = 500;
                result.err = "消息发送成功，但提及记录保存失败";
                return result;
            }
        }
    }

    // 若为转发消息，记录转发原始消息映射表（im_message_forward_map）
    // 说明：当前实现将 `extra` 作为 JSON 保存（其中包含 `msg_ids`），服务端会把被转发消息的
    // id 列表查出并写入 im_message_forward_map，便于后续回溯/搜索/统计等功能。
    if (m.msg_type == static_cast<uint16_t>(CIM::common::MessageType::Forward) &&
        !m.extra.empty()) {
        // extra 在 API 层已被写成 JSON 字符串
        Json::CharReaderBuilder rb;
        Json::Value payload;
        std::string errs;
        std::istringstream in(m.extra);
        if (Json::parseFromStream(rb, in, &payload, &errs)) {
            std::vector<std::string> src_ids;
            if (payload.isMember("msg_ids") && payload["msg_ids"].isArray()) {
                for (auto& v : payload["msg_ids"]) {
                    if (v.isString()) {
                        src_ids.push_back(v.asString());
                    } else if (v.isUInt64()) {
                        src_ids.push_back(std::to_string(v.asUInt64()));
                    }
                }
            }

            if (!src_ids.empty()) {
                std::vector<CIM::dao::Message> src_msgs;
                if (CIM::dao::MessageDao::GetByIds(src_ids, src_msgs, &err)) {
                    std::vector<CIM::dao::ForwardSrc> srcs;
                    for (auto& s : src_msgs) {
                        CIM::dao::ForwardSrc fs;
                        fs.src_msg_id = s.id;
                        fs.src_talk_id = s.talk_id;
                        fs.src_sender_id = s.sender_id;
                        srcs.push_back(std::move(fs));
                    }
                    if (!CIM::dao::MessageForwardMapDao::AddForwardMap(db, m.id, srcs, &err)) {
                        CIM_LOG_WARN(g_logger) << "AddForwardMap failed: " << err;
                        // 非关键业务，继续处理并返回成功消息发送
                    }
                } else {
                    CIM_LOG_WARN(g_logger) << "MessageDao::GetByIds failed: " << err;
                }
            }
        } else {
            CIM_LOG_WARN(g_logger) << "Parse forward extra payload failed: " << errs;
        }
    }

    // 生成最后一条消息摘要（用于会话列表预览文案）并更新会话表的 last_msg_* 字段
    // 说明：该摘要用于会话列表展示，text 类型直接使用内容，其他类型用占位字符串避免泄露内部结构。
    std::string last_msg_digest;
    auto mtype = static_cast<CIM::common::MessageType>(m.msg_type);
    if (mtype == CIM::common::MessageType::Text) {
        last_msg_digest = m.content_text;
        if (last_msg_digest.size() > 255) last_msg_digest = last_msg_digest.substr(0, 255);
    } else {
        auto it = CIM::common::kMessagePreviewMap.find(mtype);
        if (it != CIM::common::kMessagePreviewMap.end()) {
            last_msg_digest = it->second;
        } else {
            last_msg_digest = "[非文本消息]";
        }
    }

    // 在同一事务连接中更新会话的最后消息信息，保证会话列表能及时显示预览
    // 在发送消息之前，保证目标用户（单聊）存在会话视图（im_talk_session），如果不存在或被软删除则创建/恢复
    if (talk_mode == 1) {
        // 为接收方创建/恢复会话视图（以便对端也能在会话列表中看到消息）
        try {
            CIM::dao::ContactDetails cd;
            std::string cerr;
            // 不要因为 contact 查不到就退出：会话的 name/avatar 只是用于展示
            (void)CIM::dao::ContactDAO::GetByOwnerAndTargetWithConn(db, m.receiver_id,
                                                                    current_user_id, cd, &cerr);
            CIM::dao::TalkSession session;
            session.user_id = m.receiver_id;  // 接收者
            session.talk_id = talk_id;
            session.to_from_id = current_user_id;  // 会话视图中的对象（发送者）
            session.talk_mode = 1;
            if (!cd.nickname.empty()) session.name = cd.nickname;
            if (!cd.avatar.empty()) session.avatar = cd.avatar;
            if (!cd.contact_remark.empty()) session.remark = cd.contact_remark;

            std::string sErr;
            if (!CIM::dao::TalkSessionDAO::createSession(db, session, &sErr)) {
                // 记录日志但不阻塞发送：创建会话失败也不影响消息写入
                CIM_LOG_WARN(g_logger) << "createSession for receiver failed: " << sErr;
            }
            // 同时为发送者创建/恢复会话视图（当前用户），保证发送方在会话列表可见
            CIM::dao::ContactDetails cd2;
            std::string cerr2;
            (void)CIM::dao::ContactDAO::GetByOwnerAndTargetWithConn(db, current_user_id,
                                                                    m.receiver_id, cd2, &cerr2);
            CIM::dao::TalkSession session2;
            session2.user_id = current_user_id;
            session2.talk_id = talk_id;
            session2.to_from_id = m.receiver_id;
            session2.talk_mode = 1;
            if (!cd2.nickname.empty()) session2.name = cd2.nickname;
            if (!cd2.avatar.empty()) session2.avatar = cd2.avatar;
            if (!cd2.contact_remark.empty()) session2.remark = cd2.contact_remark;
            std::string sErr2;
            if (!CIM::dao::TalkSessionDAO::createSession(db, session2, &sErr2)) {
                CIM_LOG_WARN(g_logger) << "createSession for sender failed: " << sErr2;
            }
        } catch (const std::exception& ex) {
            CIM_LOG_WARN(g_logger) << "createSession exception: " << ex.what();
        }
    }

    if (!CIM::dao::TalkSessionDAO::bumpOnNewMessage(db, talk_id, current_user_id, m.id,
                                                    static_cast<uint16_t>(m.msg_type),
                                                    last_msg_digest, &err)) {
        if (!err.empty()) {
            trans->rollback();
            CIM_LOG_ERROR(g_logger) << "bumpOnNewMessage failed: " << err;
            result.code = 500;
            result.err = "更新会话摘要失败";
            return result;
        }
    }

    // 通知客户端更新会话预览：单聊推送给接收方，群聊推送给群中会话存在的所有用户
    {
        Json::Value payload;
        payload["talk_mode"] = talk_mode;
        payload["to_from_id"] = to_from_id;
        payload["sender_id"] = (Json::UInt64)current_user_id;
        payload["msg_text"] = last_msg_digest;
        payload["updated_at"] = (Json::UInt64)CIM::TimeUtil::NowToMS();

        if (talk_mode == 1) {
            // 单聊：不再在服务端做 ID 交换，统一推送标准 payload
            // 前端根据 (talk_mode=1 && to_from_id == my_uid) ? sender_id : to_from_id 来判断会话索引
            CIM::api::WsGatewayModule::PushToUser(to_from_id, "im.session.update", payload);
            CIM::api::WsGatewayModule::PushToUser(current_user_id, "im.session.update", payload);
        } else {
            // 群聊：查出本群拥有会话快照的用户，并逐个推送
            std::vector<uint64_t> talk_users;
            std::string lerr;
            if (CIM::dao::TalkSessionDAO::listUsersByTalkId(talk_id, talk_users, &lerr)) {
                for (auto uid : talk_users) {
                    CIM::api::WsGatewayModule::PushToUser(uid, "im.session.update", payload);
                }
            }
        }
    }

    // 提交事务
    if (!trans->commit()) {
        const auto commit_err = db->getErrStr();
        trans->rollback();
        CIM_LOG_ERROR(g_logger) << "Transaction commit failed: " << commit_err;
        result.code = 500;
        result.err = "事务提交失败";
        return result;
    }

    // 构建返回记录（补充昵称头像与引用信息）
    // 说明：SendMessage 返回的 MessageRecord 已经包裹好前端需要的字段：msg_id/sequence/msg_type/from_id/nickname/avatar/is_revoked/send_time/extra/quote
    // 前端可以直接把这个对象渲染为会话一条消息，不需要额外的网路请求。
    CIM::dao::MessageRecord rec;
    buildRecord(m, rec, &err);
    result.data = std::move(rec);
    result.ok = true;
    return result;
}

}  // namespace CIM::app
