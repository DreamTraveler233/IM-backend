#include "api/user_api_module.hpp"

#include "app/common_service.hpp"
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
                                 res->setHeader("Content-Type", "application/json");

                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
                                     res->setBody(Error(uid_result.code, uid_result.err));
                                     return 0;
                                 }

                                 /*根据用户 ID 加载用户信息*/
                                 auto result = CIM::app::UserService::LoadUserInfo(uid_result.data);
                                 if (!result.ok) {
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 /*构造响应并返回*/
                                 Json::Value data;
                                 data["id"] = result.data.id;
                                 data["mobile"] = result.data.mobile;
                                 data["nickname"] = result.data.nickname;
                                 data["email"] = result.data.email;
                                 data["gender"] = result.data.gender;
                                 data["motto"] = result.data.motto;
                                 data["avatar"] = result.data.avatar;
                                 data["birthday"] = result.data.birthday;
                                 res->setBody(Ok(data));
                                 return 0;
                             });

        /*用户信息更新接口*/
        dispatch->addServlet("/api/v1/user/detail-update",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");

                                 auto uid_result = GetUidFromToken(req, res);
                                 if (!uid_result.ok) {
                                     res->setStatus(ToHttpStatus(uid_result.code));
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
                                     res->setStatus(ToHttpStatus(result.code));
                                     res->setBody(Error(result.code, result.err));
                                     return 0;
                                 }

                                 res->setBody(Ok());
                                 return 0;
                             });

        /*用户邮箱更新接口*/
        dispatch->addServlet("/api/v1/user/email-update",
                             [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
                                CIM::http::HttpSession::ptr) {
                                 res->setHeader("Content-Type", "application/json");
                                 res->setBody(Ok());
                                 return 0;
                             });

        /*用户手机更新接口*/
        dispatch->addServlet("/api/v1/user/mobile-update", [](CIM::http::HttpRequest::ptr req,
                                                              CIM::http::HttpResponse::ptr res,
                                                              CIM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");

            auto uid_result = GetUidFromToken(req, res);
            if (!uid_result.ok) {
                res->setStatus(ToHttpStatus(uid_result.code));
                res->setBody(Error(uid_result.code, uid_result.err));
                return 0;
            }

            std::string new_mobile, password, sms_code;
            Json::Value body;
            if (CIM::ParseBody(req->getBody(), body)) {
                new_mobile = CIM::JsonUtil::GetString(body, "mobile");
                password = CIM::JsonUtil::GetString(body, "password");
                sms_code = CIM::JsonUtil::GetString(body, "sms_code");
            }

            auto sms_result =
                CIM::app::CommonService::VerifySmsCode(new_mobile, sms_code, "mobile_update");
            if (!sms_result.ok) {
                res->setStatus(ToHttpStatus(sms_result.code));
                res->setBody(Error(sms_result.code, sms_result.err));
                return 0;
            }

            auto update_result =
                CIM::app::UserService::UpdateMobile(uid_result.data, password, new_mobile);
            if (!update_result.ok) {
                res->setStatus(ToHttpStatus(update_result.code));
                res->setBody(Error(update_result.code, update_result.err));
                return 0;
            }

            res->setBody(Ok());
            return 0;
        });

        /*用户密码更新接口*/
        dispatch->addServlet("/api/v1/user/password-update", [](CIM::http::HttpRequest::ptr req,
                                                                CIM::http::HttpResponse::ptr res,
                                                                CIM::http::HttpSession::ptr) {
            res->setHeader("Content-Type", "application/json");

            std::string old_password, new_password;
            Json::Value body;
            if (CIM::ParseBody(req->getBody(), body)) {
                old_password = CIM::JsonUtil::GetString(body, "old_password");
                new_password = CIM::JsonUtil::GetString(body, "new_password");
            }

            auto uid_result = GetUidFromToken(req, res);
            if (!uid_result.ok) {
                res->setStatus(ToHttpStatus(uid_result.code));
                res->setBody(Error(uid_result.code, uid_result.err));
                return 0;
            }

            auto result =
                CIM::app::UserService::UpdatePassword(uid_result.data, old_password, new_password);
            if (!result.ok) {
                res->setStatus(ToHttpStatus(result.code));
                res->setBody(Error(result.code, result.err));
                return 0;
            }

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
                res->setStatus(ToHttpStatus(uid_result.code));
                res->setBody(Error(uid_result.code, uid_result.err));
                return 0;
            }

            auto save_result = CIM::app::UserService::SaveConfigInfo(
                uid_result.data, theme_mode, theme_bag_img, theme_color, notify_cue_tone,
                keyboard_event_notify);
            if (!save_result.ok) {
                res->setStatus(ToHttpStatus(save_result.code));
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
                res->setStatus(ToHttpStatus(uid_result.code));
                res->setBody(Error(uid_result.code, uid_result.err));
                return 0;
            }

            // 加载用户信息简版
            auto user_info_result = CIM::app::UserService::LoadUserInfoSimple(uid_result.data);
            if (!user_info_result.ok) {
                res->setStatus(ToHttpStatus(user_info_result.code));
                res->setBody(Error(user_info_result.code, user_info_result.err));
                return 0;
            }

            // 加载用户设置
            auto config_info_result = CIM::app::UserService::LoadConfigInfo(uid_result.data);
            if (!config_info_result.ok) {
                res->setStatus(ToHttpStatus(config_info_result.code));
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
    return true;
}

}  // namespace CIM::api
