#ifndef __APP_SETTING_SERVICE_HPP__
#define __APP_SETTING_SERVICE_HPP__

#include <cstdint>
#include <optional>
#include <string>

#include "dao/user_dao.hpp"
#include "dao/user_login_log_dao.hpp"
#include "result.hpp"

namespace CIM::app {
using UserResult = Result<CIM::dao::User>;

class UserService {
   public:
    // 加载用户信息
    static UserResult LoadUserInfo(const uint64_t uid);

    // 更新用户信息
    static ResultVoid UpdateUserInfo(const uint64_t uid, const std::string& nickname,
                               const std::string& avatar, const std::string& motto,
                               const uint32_t gender, const std::string& birthday);
    // 注册新用户
    static UserResult Register(const std::string& nickname, const std::string& mobile,
                               const std::string& password, const std::string& platform);

    // 鉴权用户
    static UserResult Authenticate(const std::string& mobile, const std::string& password,
                                   const std::string& platform);

    // 找回密码
    static UserResult Forget(const std::string& mobile, const std::string& new_password);

    // 记录登录日志
    static ResultVoid LogLogin(const UserResult& result, const std::string& platform);

    // 用户上线
    static ResultVoid GoOnline(const uint64_t id);

    // 用户下线
    static ResultVoid Offline(const uint64_t id);
};

}  // namespace CIM::app

#endif  // __SETTING_SERVICE_HPP__
