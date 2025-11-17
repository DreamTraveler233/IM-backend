#include "api/message_api_module.hpp"

#include "app/message_service.hpp"
#include "base/macro.hpp"
#include "common/common.hpp"
#include "http/http_server.hpp"
#include "http/http_servlet.hpp"
#include "system/application.hpp"
#include "util/util.hpp"

namespace CIM::api {

static auto g_logger = CIM_LOG_NAME("root");

MessageApiModule::MessageApiModule() : Module("api.message", "0.1.0", "builtin") {}

bool MessageApiModule::onServerReady() {
    std::vector<CIM::TcpServer::ptr> httpServers;
    if (!CIM::Application::GetInstance()->getServer("http", httpServers)) {
        CIM_LOG_WARN(g_logger) << "no http servers found when registering message routes";
        return true;
    }

    for (auto& s : httpServers) {
        auto http = std::dynamic_pointer_cast<CIM::http::HttpServer>(s);
        if (!http) continue;
        auto dispatch = http->getServletDispatch();

        // 删除消息（仅影响本人视图）
        dispatch->addServlet("/api/v1/message/delete", [](CIM::http::HttpRequest::ptr req,
                                                          CIM::http::HttpResponse::ptr res,
                                                          CIM::http::HttpSession::ptr) {
            CIM_LOG_DEBUG(g_logger) << "Received /api/v1/message/delete request";
            res->setHeader("Content-Type", "application/json");
            Json::Value body;
            uint8_t talk_mode = 0;
            uint64_t to_from_id = 0;
            std::vector<uint64_t> msg_ids;
            if (ParseBody(req->getBody(), body)) {
                talk_mode = CIM::JsonUtil::GetUint8(body, "talk_mode");
                to_from_id = CIM::JsonUtil::GetUint64(body, "to_from_id");
                if (body.isMember("msg_ids") && body["msg_ids"].isArray()) {
                    for (auto& v : body["msg_ids"]) {
                        if (v.isString()) {
                            try {
                                msg_ids.push_back(std::stoull(v.asString()));
                            } catch (...) {
                            }
                        } else if (v.isUInt64()) {
                            msg_ids.push_back(v.asUInt64());
                        }
                    }
                }
            }
            auto uid_ret = GetUidFromToken(req, res);
            if (!uid_ret.ok) {
                res->setStatus(ToHttpStatus(uid_ret.code));
                res->setBody(Error(uid_ret.code, uid_ret.err));
                return 0;
            }
            auto svc_ret = CIM::app::MessageService::DeleteMessages(uid_ret.data, talk_mode,
                                                                    to_from_id, msg_ids);
            if (!svc_ret.ok) {
                res->setStatus(ToHttpStatus(svc_ret.code));
                res->setBody(Error(svc_ret.code, svc_ret.err));
                return 0;
            }
            res->setBody(Ok());
            return 0;
        });

        // 转发消息记录查询（不分页）
        dispatch->addServlet("/api/v1/message/forward-records", [](CIM::http::HttpRequest::ptr req,
                                                                   CIM::http::HttpResponse::ptr res,
                                                                   CIM::http::HttpSession::ptr) {
            CIM_LOG_DEBUG(g_logger) << "Received /api/v1/message/forward-records request";
            res->setHeader("Content-Type", "application/json");
            Json::Value body;
            uint8_t talk_mode = 0;
            std::vector<uint64_t> msg_ids;
            if (ParseBody(req->getBody(), body)) {
                talk_mode = CIM::JsonUtil::GetUint8(body, "talk_mode");
                if (body.isMember("msg_ids") && body["msg_ids"].isArray()) {
                    for (auto& v : body["msg_ids"]) {
                        if (v.isString()) {
                            try {
                                msg_ids.push_back(std::stoull(v.asString()));
                            } catch (...) {
                            }
                        } else if (v.isUInt64()) {
                            msg_ids.push_back(v.asUInt64());
                        }
                    }
                }
            }
            auto uid_ret = GetUidFromToken(req, res);
            if (!uid_ret.ok) {
                res->setStatus(ToHttpStatus(uid_ret.code));
                res->setBody(Error(uid_ret.code, uid_ret.err));
                return 0;
            }
            auto svc_ret =
                CIM::app::MessageService::LoadForwardRecords(uid_ret.data, talk_mode, msg_ids);
            if (!svc_ret.ok) {
                res->setStatus(ToHttpStatus(svc_ret.code));
                res->setBody(Error(svc_ret.code, svc_ret.err));
                return 0;
            }
            Json::Value root;
            Json::Value items(Json::arrayValue);
            for (auto& r : svc_ret.data) {
                Json::Value it;
                it["msg_id"] = r.msg_id;
                it["sequence"] = (Json::UInt64)r.sequence;
                it["msg_type"] = r.msg_type;
                it["from_id"] = (Json::UInt64)r.from_id;
                it["nickname"] = r.nickname;
                it["avatar"] = r.avatar;
                it["is_revoked"] = r.is_revoked;
                it["send_time"] = r.send_time;
                it["extra"] = r.extra;
                it["quote"] = r.quote;
                items.append(it);
            }
            root["items"] = items;
            res->setBody(Ok(root));
            return 0;
        });

        // 历史消息分页（按类型过滤）
        dispatch->addServlet("/api/v1/message/history-records", [](CIM::http::HttpRequest::ptr req,
                                                                   CIM::http::HttpResponse::ptr res,
                                                                   CIM::http::HttpSession::ptr) {
            CIM_LOG_DEBUG(g_logger) << "Received /api/v1/message/history-records request";
            res->setHeader("Content-Type", "application/json");
            Json::Value body;
            uint8_t talk_mode = 0;
            uint64_t to_from_id = 0;
            uint64_t cursor = 0;
            uint32_t limit = 0;
            uint16_t msg_type = 0;
            if (ParseBody(req->getBody(), body)) {
                talk_mode = CIM::JsonUtil::GetUint8(body, "talk_mode");
                to_from_id = CIM::JsonUtil::GetUint64(body, "to_from_id");
                cursor = CIM::JsonUtil::GetUint64(body, "cursor");
                limit = CIM::JsonUtil::GetUint32(body, "limit");
                msg_type = CIM::JsonUtil::GetUint16(body, "msg_type");
            }
            auto uid_ret = GetUidFromToken(req, res);
            if (!uid_ret.ok) {
                res->setStatus(ToHttpStatus(uid_ret.code));
                res->setBody(Error(uid_ret.code, uid_ret.err));
                return 0;
            }
            auto svc_ret = CIM::app::MessageService::LoadHistoryRecords(
                uid_ret.data, talk_mode, to_from_id, msg_type, cursor, limit);
            if (!svc_ret.ok) {
                res->setStatus(ToHttpStatus(svc_ret.code));
                res->setBody(Error(svc_ret.code, svc_ret.err));
                return 0;
            }
            Json::Value root;
            Json::Value items(Json::arrayValue);
            for (auto& r : svc_ret.data.items) {
                Json::Value it;
                it["msg_id"] = r.msg_id;
                it["sequence"] = (Json::UInt64)r.sequence;
                it["msg_type"] = r.msg_type;
                it["from_id"] = (Json::UInt64)r.from_id;
                it["nickname"] = r.nickname;
                it["avatar"] = r.avatar;
                it["is_revoked"] = r.is_revoked;
                it["send_time"] = r.send_time;
                it["extra"] = r.extra;
                it["quote"] = r.quote;
                items.append(it);
            }
            root["items"] = items;
            root["cursor"] = (Json::UInt64)svc_ret.data.cursor;
            res->setBody(Ok(root));
            return 0;
        });

        /*获取会话消息记录*/
        dispatch->addServlet("/api/v1/message/records", [](CIM::http::HttpRequest::ptr req,
                                                           CIM::http::HttpResponse::ptr res,
                                                           CIM::http::HttpSession::ptr) {
            CIM_LOG_DEBUG(g_logger) << "Received /api/v1/message/records request";
            res->setHeader("Content-Type", "application/json");

            Json::Value body;
            uint8_t talk_mode = 0;    // 会话类型
            uint64_t to_from_id = 0;  // 会话对象ID
            uint64_t cursor = 0;      // 游标
            uint32_t limit = 0;       // 每次请求返回的消息数量上限
            if (ParseBody(req->getBody(), body)) {
                talk_mode = CIM::JsonUtil::GetUint8(body, "talk_mode");
                to_from_id = CIM::JsonUtil::GetUint64(body, "to_from_id");
                cursor = CIM::JsonUtil::GetUint64(body, "cursor");
                limit = CIM::JsonUtil::GetUint32(body, "limit");
            }

            CIM_LOG_DEBUG(g_logger)
                << "Parameters - talk_mode: " << static_cast<int>(talk_mode)
                << ", to_from_id: " << to_from_id << ", cursor: " << cursor << ", limit: " << limit;

            auto uid_ret = GetUidFromToken(req, res);
            if (!uid_ret.ok) {
                res->setStatus(ToHttpStatus(uid_ret.code));
                res->setBody(Error(uid_ret.code, uid_ret.err));
                return 0;
            }

            CIM_LOG_DEBUG(g_logger) << "Token valid for uid: " << uid_ret.data;

            auto svc_ret = CIM::app::MessageService::LoadRecords(uid_ret.data, talk_mode,
                                                                 to_from_id, cursor, limit);
            if (!svc_ret.ok) {
                res->setStatus(ToHttpStatus(svc_ret.code));
                res->setBody(Error(svc_ret.code, svc_ret.err));
                return 0;
            }

            Json::Value root;
            Json::Value items(Json::arrayValue);
            for (auto& r : svc_ret.data.items) {
                Json::Value it;
                it["msg_id"] = r.msg_id;
                it["sequence"] = r.sequence;
                it["msg_type"] = r.msg_type;
                it["from_id"] = r.from_id;
                it["nickname"] = r.nickname;
                it["avatar"] = r.avatar;
                it["is_revoked"] = r.is_revoked;
                it["send_time"] = r.send_time;
                it["extra"] = r.extra;
                it["quote"] = r.quote;
                items.append(it);
            }
            root["items"] = items;
            root["cursor"] = svc_ret.data.cursor;

            // 避免将整页消息完整序列化到日志，防止在包含超大 extra 时导致内存暴涨
            CIM_LOG_DEBUG(g_logger)
                << "records count=" << static_cast<int>(svc_ret.data.items.size())
                << ", cursor=" << svc_ret.data.cursor;
            res->setBody(Ok(root));
            return 0;
        });

        /*消息撤回接口*/
        dispatch->addServlet("/api/v1/message/revoke", [](CIM::http::HttpRequest::ptr req,
                                                          CIM::http::HttpResponse::ptr res,
                                                          CIM::http::HttpSession::ptr) {
            CIM_LOG_DEBUG(g_logger) << "Received /api/v1/message/revoke request";
            res->setHeader("Content-Type", "application/json");

            Json::Value body;
            uint8_t talk_mode = 0;               // 会话类型
            uint64_t to_from_id = 0;             // 会话对象ID
            std::string msg_id = std::string();  // 消息ID
            if (ParseBody(req->getBody(), body)) {
                talk_mode = CIM::JsonUtil::GetUint8(body, "talk_mode");
                to_from_id = CIM::JsonUtil::GetUint64(body, "to_from_id");
                msg_id = CIM::JsonUtil::GetString(body, "msg_id");
            }

            auto uid_ret = GetUidFromToken(req, res);
            if (!uid_ret.ok) {
                res->setStatus(ToHttpStatus(uid_ret.code));
                res->setBody(Error(uid_ret.code, uid_ret.err));
                return 0;
            }

            auto svc_ret = CIM::app::MessageService::RevokeMessage(uid_ret.data, talk_mode,
                                                                   to_from_id, std::stoull(msg_id));
            if (!svc_ret.ok) {
                res->setStatus(ToHttpStatus(svc_ret.code));
                res->setBody(Error(svc_ret.code, svc_ret.err));
                return 0;
            }

            res->setBody(Ok());
            return 0;
        });
    }
    return true;
}

}  // namespace CIM::api
