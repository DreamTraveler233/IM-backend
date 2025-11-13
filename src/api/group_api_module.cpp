#include "api/group_api_module.hpp"

#include "base/macro.hpp"
#include "common/common.hpp"
#include "http/http_server.hpp"
#include "http/http_servlet.hpp"
#include "system/application.hpp"
#include "util/util.hpp"

namespace CIM::api {

static auto g_logger = CIM_LOG_NAME("root");

GroupApiModule::GroupApiModule() : Module("api.group", "0.1.0", "builtin") {}

bool GroupApiModule::onServerReady() {
    std::vector<CIM::TcpServer::ptr> httpServers;
    if (!CIM::Application::GetInstance()->getServer("http", httpServers)) {
        CIM_LOG_WARN(g_logger) << "no http servers found when registering group routes";
        return true;
    }

    for (auto& s : httpServers) {
        auto http = std::dynamic_pointer_cast<CIM::http::HttpServer>(s);
        if (!http) continue;
        auto dispatch = http->getServletDispatch();

        // group-apply
        dispatch->addServlet(
            "/api/v1/group-apply/agree",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                res->setBody(Ok());
                return 0;
            });
        dispatch->addServlet(
            "/api/v1/group-apply/all",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                Json::Value d;
                d["list"] = Json::Value(Json::arrayValue);
                res->setBody(Ok(d));
                return 0;
            });
        dispatch->addServlet(
            "/api/v1/group-apply/create",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                res->setBody(Ok());
                return 0;
            });
        dispatch->addServlet(
            "/api/v1/group-apply/decline",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                res->setBody(Ok());
                return 0;
            });
        dispatch->addServlet(
            "/api/v1/group-apply/delete",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                res->setBody(Ok());
                return 0;
            });
        dispatch->addServlet(
            "/api/v1/group-apply/list",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                Json::Value d;
                d["list"] = Json::Value(Json::arrayValue);
                res->setBody(Ok(d));
                return 0;
            });
        dispatch->addServlet(
            "/api/v1/group-apply/unread-num",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                Json::Value d;
                d["count"] = 0;
                res->setBody(Ok(d));
                return 0;
            });

        // group-notice
        dispatch->addServlet(
            "/api/v1/group-notice/edit",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                res->setBody(Ok());
                return 0;
            });

        // group-vote
        dispatch->addServlet(
            "/api/v1/group-vote/create",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                res->setBody(Ok());
                return 0;
            });
        dispatch->addServlet(
            "/api/v1/group-vote/detail",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                Json::Value d(Json::objectValue);
                res->setBody(Ok(d));
                return 0;
            });
        dispatch->addServlet(
            "/api/v1/group-vote/submit",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                res->setBody(Ok());
                return 0;
            });

        // group main
        dispatch->addServlet(
            "/api/v1/group/assign-admin",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                res->setBody(Ok());
                return 0;
            });
        dispatch->addServlet("/api/v1/group/create", [](CIM::http::HttpRequest::ptr /*req*/,
                                                        CIM::http::HttpResponse::ptr res,
                                                        CIM::http::HttpSession::ptr /*session*/) {
            res->setHeader("Content-Type", "application/json");
            Json::Value d(Json::objectValue);
            d["group_id"] = static_cast<Json::Int64>(0);
            res->setBody(Ok(d));
            return 0;
        });
        dispatch->addServlet("/api/v1/group/detail", [](CIM::http::HttpRequest::ptr /*req*/,
                                                        CIM::http::HttpResponse::ptr res,
                                                        CIM::http::HttpSession::ptr /*session*/) {
            res->setHeader("Content-Type", "application/json");
            Json::Value d(Json::objectValue);
            res->setBody(Ok(d));
            return 0;
        });
        dispatch->addServlet("/api/v1/group/dismiss", [](CIM::http::HttpRequest::ptr /*req*/,
                                                         CIM::http::HttpResponse::ptr res,
                                                         CIM::http::HttpSession::ptr /*session*/) {
            res->setHeader("Content-Type", "application/json");
            res->setBody(Ok());
            return 0;
        });
        dispatch->addServlet(
            "/api/v1/group/get-invite-friends",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                Json::Value d;
                d["list"] = Json::Value(Json::arrayValue);
                res->setBody(Ok(d));
                return 0;
            });
        dispatch->addServlet("/api/v1/group/handover", [](CIM::http::HttpRequest::ptr /*req*/,
                                                          CIM::http::HttpResponse::ptr res,
                                                          CIM::http::HttpSession::ptr /*session*/) {
            res->setHeader("Content-Type", "application/json");
            res->setBody(Ok());
            return 0;
        });
        dispatch->addServlet("/api/v1/group/invite", [](CIM::http::HttpRequest::ptr /*req*/,
                                                        CIM::http::HttpResponse::ptr res,
                                                        CIM::http::HttpSession::ptr /*session*/) {
            res->setHeader("Content-Type", "application/json");
            res->setBody(Ok());
            return 0;
        });
        dispatch->addServlet("/api/v1/group/list", [](CIM::http::HttpRequest::ptr /*req*/,
                                                      CIM::http::HttpResponse::ptr res,
                                                      CIM::http::HttpSession::ptr /*session*/) {
            res->setHeader("Content-Type", "application/json");
            Json::Value d;
            d["list"] = Json::Value(Json::arrayValue);
            res->setBody(Ok(d));
            return 0;
        });
        dispatch->addServlet(
            "/api/v1/group/member-list",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                Json::Value d;
                d["list"] = Json::Value(Json::arrayValue);
                res->setBody(Ok(d));
                return 0;
            });
        dispatch->addServlet("/api/v1/group/mute", [](CIM::http::HttpRequest::ptr /*req*/,
                                                      CIM::http::HttpResponse::ptr res,
                                                      CIM::http::HttpSession::ptr /*session*/) {
            res->setHeader("Content-Type", "application/json");
            res->setBody(Ok());
            return 0;
        });
        dispatch->addServlet("/api/v1/group/no-speak", [](CIM::http::HttpRequest::ptr /*req*/,
                                                          CIM::http::HttpResponse::ptr res,
                                                          CIM::http::HttpSession::ptr /*session*/) {
            res->setHeader("Content-Type", "application/json");
            res->setBody(Ok());
            return 0;
        });
        dispatch->addServlet("/api/v1/group/overt", [](CIM::http::HttpRequest::ptr /*req*/,
                                                       CIM::http::HttpResponse::ptr res,
                                                       CIM::http::HttpSession::ptr /*session*/) {
            res->setHeader("Content-Type", "application/json");
            res->setBody(Ok());
            return 0;
        });
        dispatch->addServlet(
            "/api/v1/group/overt-list",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                Json::Value d;
                d["list"] = Json::Value(Json::arrayValue);
                res->setBody(Ok(d));
                return 0;
            });
        dispatch->addServlet(
            "/api/v1/group/remark-update",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                res->setBody(Ok());
                return 0;
            });
        dispatch->addServlet(
            "/api/v1/group/remove-member",
            [](CIM::http::HttpRequest::ptr /*req*/, CIM::http::HttpResponse::ptr res,
               CIM::http::HttpSession::ptr /*session*/) {
                res->setHeader("Content-Type", "application/json");
                res->setBody(Ok());
                return 0;
            });
        dispatch->addServlet("/api/v1/group/secede", [](CIM::http::HttpRequest::ptr /*req*/,
                                                        CIM::http::HttpResponse::ptr res,
                                                        CIM::http::HttpSession::ptr /*session*/) {
            res->setHeader("Content-Type", "application/json");
            res->setBody(Ok());
            return 0;
        });
        dispatch->addServlet("/api/v1/group/setting", [](CIM::http::HttpRequest::ptr /*req*/,
                                                         CIM::http::HttpResponse::ptr res,
                                                         CIM::http::HttpSession::ptr /*session*/) {
            res->setHeader("Content-Type", "application/json");
            Json::Value d;
            d["notify"] = true;
            d["mute"] = false;
            res->setBody(Ok(d));
            return 0;
        });
    }
    return true;
}

}  // namespace CIM::api
