#include "api/auth_api_module.hpp"

#include <jwt-cpp/jwt.h>

#include "app/auth_service.hpp"
#include "app/common_service.hpp"
#include "app/user_service.hpp"
#include "base/macro.hpp"
#include "common/common.hpp"
#include "config/config.hpp"
#include "dao/user_dao.hpp"
#include "http/http_server.hpp"
#include "http/http_servlet.hpp"
#include "system/application.hpp"
#include "util/util.hpp"

namespace CIM::api {

static auto g_logger = CIM_LOG_NAME("root");

// JWT过期时间(秒)
static auto g_jwt_expires_in =
    CIM::Config::Lookup<uint32_t>("auth.jwt.expires_in", 3600, "jwt expires in seconds");

AuthApiModule::AuthApiModule() : Module("api.auth", "0.1.0", "builtin") {}

/* 服务器准备就绪时注册认证相关路由 */
bool AuthApiModule::onServerReady() {
    std::vector<CIM::TcpServer::ptr> httpServers;
    if (!CIM::Application::GetInstance()->getServer("http", httpServers)) {
        CIM_LOG_WARN(g_logger) << "no http servers found when registering auth routes";
        return true;
    }

    for (auto& s : httpServers) {
        auto http = std::dynamic_pointer_cast<CIM::http::HttpServer>(s);
        if (!http) continue;
        auto dispatch = http->getServletDispatch();

        /*登录接口*/
        dispatch->addServlet("/api/v1/auth/login", [](CIM::http::HttpRequest::ptr req,
                                                      CIM::http::HttpResponse::ptr res,
                                                      CIM::http::HttpSession::ptr session) {
            res->setHeader("Content-Type", "application/json");

            std::string mobile, password, platform;
            Json::Value body;
            if (ParseBody(req->getBody(), body)) {
                mobile = CIM::JsonUtil::GetString(body, "mobile");
                password = CIM::JsonUtil::GetString(body, "password");
                platform = CIM::JsonUtil::GetString(body, "platform");
            }

            // 执行登陆业务
            auto result = CIM::app::AuthService::Authenticate(mobile, password, platform);

            /*只要用户存在，无论鉴权成功还是失败都要记录登录日志*/
            std::string err;
            if (result.data.id != 0) {
                auto LogLogin_res = CIM::app::AuthService::LogLogin(result, platform, session);
                if (!LogLogin_res.ok) {
                    res->setStatus(ToHttpStatus(LogLogin_res.code));
                    res->setBody(Error(LogLogin_res.code, LogLogin_res.err));
                    return 0;
                }
            }
            if (!result.ok) {
                res->setStatus(ToHttpStatus(result.code));
                res->setBody(Error(result.code, result.err));
                return 0;
            }

            /* 签发JWT */
            auto token_result =
                SignJwt(std::to_string(result.data.id), g_jwt_expires_in->getValue());
            if (!token_result.ok) {
                res->setStatus(ToHttpStatus(token_result.code));
                res->setBody(Error(token_result.code, token_result.err));
                return 0;
            }

            // 更新在线状态为在线
            auto goOnline_res = CIM::app::AuthService::GoOnline(result.data.id);
            if (!goOnline_res.ok) {
                res->setStatus(ToHttpStatus(goOnline_res.code));
                res->setBody(Error(goOnline_res.code, goOnline_res.err));
                return 0;
            }

            Json::Value data;
            data["type"] = "Bearer";                   // token类型，固定值Bearer
            data["access_token"] = token_result.data;  // 访问令牌
            data["expires_in"] =
                static_cast<Json::UInt>(g_jwt_expires_in->getValue());  // 过期时间(秒)
            res->setBody(Ok(data));
            return 0;
        });

        /*注册接口*/
        dispatch->addServlet("/api/v1/auth/register", [](CIM::http::HttpRequest::ptr req,
                                                         CIM::http::HttpResponse::ptr res,
                                                         CIM::http::HttpSession::ptr session) {
            res->setHeader("Content-Type", "application/json");

            std::string nickname, mobile, password, sms_code, platform;
            Json::Value body;
            if (ParseBody(req->getBody(), body)) {
                nickname = CIM::JsonUtil::GetString(body, "nickname");
                mobile = CIM::JsonUtil::GetString(body, "mobile");
                password = CIM::JsonUtil::GetString(body, "password");
                sms_code = CIM::JsonUtil::GetString(body, "sms_code");
                platform = CIM::JsonUtil::GetString(body, "platform");
            }

            /* 验证短信验证码 */
            auto verifyResult =
                CIM::app::CommonService::VerifySmsCode(mobile, sms_code, "register");
            if (!verifyResult.ok) {
                res->setStatus(ToHttpStatus(verifyResult.code));
                res->setBody(Error(verifyResult.code, verifyResult.err));
                return 0;
            }

            /* 注册用户 */
            auto result = CIM::app::AuthService::Register(nickname, mobile, password, platform);

            /*只要创建用户成功就记录登录日志*/
            std::string err;
            if (result.data.id != 0) {
                auto LogLogin_res = CIM::app::AuthService::LogLogin(result, platform, session);
                if (!LogLogin_res.ok) {
                    res->setStatus(ToHttpStatus(LogLogin_res.code));
                    res->setBody(Error(LogLogin_res.code, LogLogin_res.err));
                    return 0;
                }
            }
            if (!result.ok) {
                res->setStatus(ToHttpStatus(result.code));
                res->setBody(Error(result.code, result.err));
                return 0;
            }

            /* 签发JWT */
            auto token_result =
                SignJwt(std::to_string(result.data.id), g_jwt_expires_in->getValue());
            if (!token_result.ok) {
                res->setStatus(ToHttpStatus(token_result.code));
                res->setBody(Error(token_result.code, token_result.err));
                return 0;
            }

            // 更新在线状态为在线
            auto goOnline_res = CIM::app::AuthService::GoOnline(result.data.id);
            if (!goOnline_res.ok) {
                res->setStatus(ToHttpStatus(goOnline_res.code));
                res->setBody(Error(goOnline_res.code, goOnline_res.err));
                return 0;
            }

            Json::Value data;
            data["type"] = "Bearer";
            data["access_token"] = token_result.data;
            data["expires_in"] = static_cast<Json::UInt>(g_jwt_expires_in->getValue());
            res->setBody(Ok(data));
            return 0;
        });

        /*找回密码接口*/
        dispatch->addServlet("/api/v1/auth/forget", [](CIM::http::HttpRequest::ptr req,
                                                       CIM::http::HttpResponse::ptr res,
                                                       CIM::http::HttpSession::ptr session) {
            res->setHeader("Content-Type", "application/json");

            std::string mobile, password, sms_code;
            Json::Value body;
            if (ParseBody(req->getBody(), body)) {
                mobile = CIM::JsonUtil::GetString(body, "mobile");
                password = CIM::JsonUtil::GetString(body, "password");
                sms_code = CIM::JsonUtil::GetString(body, "sms_code");
            }

            /* 验证短信验证码 */
            auto verifyResult =
                CIM::app::CommonService::VerifySmsCode(mobile, sms_code, "forget_account");
            if (!verifyResult.ok) {
                res->setStatus(ToHttpStatus(verifyResult.code));
                res->setBody(Error(verifyResult.code, verifyResult.err));
                return 0;
            }

            /* 找回密码 */
            auto forgetResult = CIM::app::AuthService::Forget(mobile, password);
            if (!forgetResult.ok) {
                res->setStatus(ToHttpStatus(forgetResult.code));
                res->setBody(Error(forgetResult.code, forgetResult.err));
                return 0;
            }

            res->setBody(Ok());
            return 0;
        });

        /*获取 oauth2.0 跳转地址*/
        dispatch->addServlet("/api/v1/auth/oauth", [](CIM::http::HttpRequest::ptr /*req*/,
                                                      CIM::http::HttpResponse::ptr res,
                                                      CIM::http::HttpSession::ptr /*session*/) {
            res->setHeader("Content-Type", "application/json");
            res->setBody(Ok());
            return 0;
        });

        /*绑定第三方登录接口*/
        dispatch->addServlet(
            "/api/v1/auth/oauth/bind",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                res->setBody(Ok());
                return 0;
            });

        /*第三方登录接口*/
        dispatch->addServlet(
            "/api/v1/auth/oauth/login",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                res->setBody(Ok());
                return 0;
            });
    }
    return true;
}

}  // namespace CIM::api
