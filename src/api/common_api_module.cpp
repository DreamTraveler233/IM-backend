#include "api/common_api_module.hpp"

#include "app/common_service.hpp"
#include "app/user_service.hpp"
#include "base/macro.hpp"
#include "common/common.hpp"
#include "dao/user_dao.hpp"
#include "db/mysql.hpp"
#include "http/http_server.hpp"
#include "http/http_servlet.hpp"
#include "system/application.hpp"
#include "util/util.hpp"

namespace CIM::api {

static auto g_logger = CIM_LOG_NAME("root");

CommonApiModule::CommonApiModule() : Module("api.common", "0.1.0", "builtin") {}

bool CommonApiModule::onServerReady() {
    std::vector<CIM::TcpServer::ptr> httpServers;
    if (!CIM::Application::GetInstance()->getServer("http", httpServers)) {
        CIM_LOG_WARN(g_logger) << "no http servers found when registering common routes";
        return true;
    }

    // 初始化验证码清理定时器（幂等）
    CIM::app::CommonService::InitCleanupTimer();

    for (auto& s : httpServers) {
        auto http = std::dynamic_pointer_cast<CIM::http::HttpServer>(s);
        if (!http) continue;
        auto dispatch = http->getServletDispatch();

        // 发送短信验证码
        dispatch->addServlet("/api/v1/common/send-sms", [](CIM::http::HttpRequest::ptr req,
                                                           CIM::http::HttpResponse::ptr res,
                                                           CIM::http::HttpSession::ptr session) {
            res->setHeader("Content-Type", "application/json");

            std::string mobile, channel;
            Json::Value body;
            if (ParseBody(req->getBody(), body)) {
                mobile = CIM::JsonUtil::GetString(body, "mobile");
                channel = CIM::JsonUtil::GetString(body, "channel");
            }

            /*判断手机号是否已经注册*/
            auto ret = CIM::app::UserService::GetUserByMobile(mobile, channel);
            if (!ret.ok) {
                res->setStatus(ToHttpStatus(ret.code));
                res->setBody(Error(ret.code, ret.err));
                return 0;
            }

            /* 发送短信验证码 */
            auto result = CIM::app::CommonService::SendSmsCode(mobile, channel, session);
            if (!result.ok) {
                res->setStatus(ToHttpStatus(result.code));
                res->setBody(Error(result.code, result.err));
                return 0;
            }

            Json::Value data;
            data["sms_code"] = result.data.code;
            res->setBody(Ok(data));
            return 0;
        });

        // 注册邮箱服务（占位实现）
        dispatch->addServlet(
            "/api/v1/common/send-email",
            [](CIM::http::HttpRequest::ptr req, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                res->setBody("{\"code\":0,\"msg\":\"ok\",\"data\":{\"status\":\"running\"}} ");
                return 0;
            });

        // 测试接口（占位）
        dispatch->addServlet(
            "/api/v1/common/send-test",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                Json::Value data;
                data["echo"] = true;
                res->setBody(CIM::JsonUtil::ToString(data));
                return 0;
            });
    }
    return true;
}

}  // namespace CIM::api
