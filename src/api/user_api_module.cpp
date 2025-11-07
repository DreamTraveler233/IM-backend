#include "api/user_api_module.hpp"

#include "app/user_service.hpp"
#include "base/macro.hpp"
#include "common/common.hpp"
#include "http/http_server.hpp"
#include "http/http_servlet.hpp"
#include "system/application.hpp"
#include "util/util.hpp"

namespace CIM::api {

static auto g_logger = CIM_LOG_NAME("root");

UserApiModule::UserApiModule() : Module("api.user", "0.1.0", "builtin") {}

bool UserApiModule::onServerReady() {
    std::vector<CIM::TcpServer::ptr> httpServers;
    if (!CIM::Application::GetInstance()->getServer("http", httpServers)) {
        CIM_LOG_WARN(g_logger) << "no http servers found when registering user routes";
        return true;
    }

    for (auto& s : httpServers) {
        auto http = std::dynamic_pointer_cast<CIM::http::HttpServer>(s);
        if (!http) continue;
        auto dispatch = http->getServletDispatch();

        /*用户相关接口*/
        dispatch->addServlet("/api/v1/user/detail",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 CIM_LOG_DEBUG(g_logger) << "/api/v1/user/detail";
                                 /*设置响应头*/
                                 res->setHeader("Content-Type", "application/json");

                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(CIM::http::HttpStatus::UNAUTHORIZED);
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }

                                 /*根据用户 ID 加载用户信息*/
                                 auto result = CIM::app::UserService::LoadUserInfo(uid_result.data);
                                 if (!result.ok) {
                                     res->setStatus(CIM::http::HttpStatus::NOT_FOUND);
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 /*构造响应并返回*/
                                 Json::Value data;
                                 data["id"] = result.data.id;              // 用户ID
                                 data["mobile"] = result.data.mobile;      // 手机号
                                 data["nickname"] = result.data.nickname;  // 昵称
                                 data["email"] = result.data.email;        // 邮箱
                                 data["gender"] = result.data.gender;      // 性别
                                 data["motto"] = result.data.motto;        // 个性签名
                                 data["avatar"] = result.data.avatar;      // 头像URL
                                 data["birthday"] = result.data.birthday;  // 生日
                                 res->setBody(Ok(data));
                                 return 0;
                             });

        /*用户信息更新接口*/
        dispatch->addServlet("/api/v1/user/detail-update",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 CIM_LOG_DEBUG(g_logger) << "/api/v1/user/detail-update";
                                 res->setHeader("Content-Type", "application/json");

                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(CIM::http::HttpStatus::UNAUTHORIZED);
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }

                                 /*提取请求字段*/
                                 std::string nickname, avatar, motto, birthday;
                                 uint32_t gender;
                                 Json::Value body;
                                 if (CIM::ParseBody(req->getBody(), body)) {
                                     nickname = CIM::JsonUtil::GetString(body, "nickname");
                                     avatar = CIM::JsonUtil::GetString(body, "avatar");
                                     motto = CIM::JsonUtil::GetString(body, "motto");
                                     gender = CIM::JsonUtil::GetUint32(body, "gender");
                                     birthday = CIM::JsonUtil::GetString(body, "birthday");
                                 }

                                 /*更新用户信息*/
                                 auto result = CIM::app::UserService::UpdateUserInfo(
                                     uid_result.data, nickname, avatar, motto, gender, birthday);
                                 if (!result.ok) {
                                     res->setStatus(CIM::http::HttpStatus::INTERNAL_SERVER_ERROR);
                                     res->setBody(Error(500, "更新用户信息失败！"));
                                     return 0;
                                 }

                                 res->setBody(Ok());
                                 return 0;
                             });

        /*用户邮箱更新接口*/
        dispatch->addServlet("/api/v1/user/email-update",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 CIM_LOG_DEBUG(g_logger) << "/api/v1/user/detail-update";
                                 res->setHeader("Content-Type", "application/json");
                                 res->setBody(Ok());
                                 return 0;
                             });

        /*用户手机更新接口*/
        dispatch->addServlet("/api/v1/user/mobile-update",
                             [](CIM::http::HttpRequest::ptr, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 res->setBody(Ok());
                                 return 0;
                             });

        /*用户密码更新接口*/
        dispatch->addServlet("/api/v1/user/password-update",
                             [](CIM::http::HttpRequest::ptr, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 res->setBody(Ok());
                                 return 0;
                             });

        dispatch->addServlet("/api/v1/user/setting/save", [](CIM::http::HttpRequest::ptr req,
                                                             CIM::http::HttpResponse::ptr res,
                                                             CIM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");

            std::string theme_mode, theme_bag_img, theme_color, notify_cue_tone,
                keyboard_event_notify;
            Json::Value body;
            if (CIM::ParseBody(req->getBody(), body)) {
                theme_mode = CIM::JsonUtil::GetString(body, "theme_mode");
                theme_bag_img = CIM::JsonUtil::GetString(body, "theme_bag_img");
                theme_color = CIM::JsonUtil::GetString(body, "theme_color");
                notify_cue_tone = CIM::JsonUtil::GetString(body, "notify_cue_tone");
                keyboard_event_notify = CIM::JsonUtil::GetString(body, "keyboard_event_notify");
            }

            auto uid_result = GetUidFromToken(req, res);
            if (!uid_result.ok) {
                res->setStatus(CIM::http::HttpStatus::UNAUTHORIZED);
                res->setBody(Error(uid_result.code, uid_result.err));
                return 0;
            }

            auto save_result = CIM::app::UserService::SaveConfigInfo(
                uid_result.data, theme_mode, theme_bag_img, theme_color, notify_cue_tone,
                keyboard_event_notify);
            if (!save_result.ok) {
                res->setStatus(CIM::http::HttpStatus::INTERNAL_SERVER_ERROR);
                res->setBody(Error(save_result.code, save_result.err));
                return 0;
            }

            res->setBody(Ok());
            return 0;
        });

        /*用户设置接口*/
        dispatch->addServlet("/api/v1/user/setting", [](CIM::http::HttpRequest::ptr req,
                                                        CIM::http::HttpResponse::ptr res,
                                                        CIM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");

            auto uid_result = GetUidFromToken(req, res);
            if (!uid_result.ok) {
                res->setStatus(CIM::http::HttpStatus::UNAUTHORIZED);
                res->setBody(Error(uid_result.code, uid_result.err));
                return 0;
            }

            // 加载用户信息简版
            auto user_info_result = CIM::app::UserService::LoadUserInfoSimple(uid_result.data);
            if (!user_info_result.ok) {
                res->setStatus(CIM::http::HttpStatus::NOT_FOUND);
                res->setBody(Error(user_info_result.code, user_info_result.err));
                return 0;
            }

            // 加载用户设置
            auto config_info_result = CIM::app::UserService::LoadConfigInfo(uid_result.data);
            if (!config_info_result.ok) {
                res->setStatus(CIM::http::HttpStatus::NOT_FOUND);
                res->setBody(Error(config_info_result.code, config_info_result.err));
                return 0;
            }

            Json::Value user_info;
            user_info["uid"] = user_info_result.data.uid;
            user_info["nickname"] = user_info_result.data.nickname;
            user_info["avatar"] = user_info_result.data.avatar;
            user_info["motto"] = user_info_result.data.motto;
            user_info["gender"] = user_info_result.data.gender;
            user_info["is_qiye"] = user_info_result.data.is_qiye;
            user_info["mobile"] = user_info_result.data.mobile;
            user_info["email"] = user_info_result.data.email;
            Json::Value setting;
            setting["theme_mode"] = config_info_result.data.theme_mode;
            setting["theme_bag_img"] = config_info_result.data.theme_bag_img;
            setting["theme_color"] = config_info_result.data.theme_color;
            setting["notify_cue_tone"] = config_info_result.data.notify_cue_tone;
            setting["keyboard_event_notify"] = config_info_result.data.keyboard_event_notify;
            Json::Value data;
            data["user_info"] = user_info;
            data["setting"] = setting;
            res->setBody(Ok(data));
            return 0;
        });
    }

    CIM_LOG_INFO(g_logger) << "user routes registered";
    return true;
}

}  // namespace CIM::api
