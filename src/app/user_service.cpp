#include "app/user_service.hpp"

#include "crypto_module.hpp"
#include "dao/user_dao.hpp"
#include "macro.hpp"
#include "util/hash_util.hpp"
#include "util/password.hpp"

namespace CIM::app {

static auto g_logger = CIM_LOG_NAME("root");
UserResult UserService::LoadUserInfo(uint64_t uid) {
    UserResult result;
    std::string err;
    if (!CIM::dao::UserDAO::GetById(uid, result.data, &err)) {
        CIM_LOG_ERROR(g_logger) << "LoadUserInfo failed, uid=" << uid << ", err=" << err;
        result.code = 404;
        result.err = "用户不存在！";
        return result;
    }
    result.ok = true;
    return result;
}

ResultVoid UserService::UpdateUserInfo(const uint64_t uid, const std::string& nickname,
                                       const std::string& avatar, const std::string& motto,
                                       const uint32_t gender, const std::string& birthday) {
    ResultVoid result;
    std::string err;
    if (!CIM::dao::UserDAO::UpdateUserInfo(uid, nickname, avatar, motto, gender, birthday, &err)) {
        CIM_LOG_ERROR(g_logger) << "UpdateUserInfo failed, uid=" << uid << ", err=" << err;
        result.code = 500;
        result.err = "更新用户信息失败！";
        return result;
    }
    result.ok = true;
    return result;
}

UserResult UserService::Register(const std::string& nickname, const std::string& mobile,
                                 const std::string& password, const std::string& platform) {
    UserResult result;
    std::string err;

    /*使用Base64解码密码并RSA解密*/
    // Base64 解码
    std::string cipher_bin = CIM::base64decode(password);
    if (cipher_bin.empty()) {
        result.err = "密码解码失败！";
        return result;
    }
    // 私钥解密
    auto cm = CIM::CryptoModule::Get();
    if (!cm || !cm->isReady()) {
        result.err = "密钥模块未加载！";
        return result;
    }
    std::string decrypted_pwd = "";
    if (!cm->PrivateDecrypt(cipher_bin, decrypted_pwd)) {
        result.err = "密码解密失败！";
        return result;
    }

    /*生成密码哈希*/
    auto ph = CIM::util::Password::Hash(decrypted_pwd);
    if (ph.empty()) {
        result.err = "密码哈希生成失败！";
        return result;
    }

    /*创建用户*/
    CIM::dao::User u;
    u.nickname = nickname;
    u.mobile = mobile;
    u.password_hash = ph;

    if (!CIM::dao::UserDAO::Create(u, u.id, &err)) {
        CIM_LOG_ERROR(g_logger) << "Register Create user failed, mobile=" << mobile
                                << ", err=" << err;
        result.code = 500;
        result.err = "创建用户失败！";
        return result;
    }

    result.ok = true;
    result.data = std::move(u);
    return result;
}

UserResult UserService::Authenticate(const std::string& mobile, const std::string& password,
                                     const std::string& platform) {
    UserResult result;
    std::string err;

    /*密码解密*/
    // Base64 解码
    std::string cipher_bin = CIM::base64decode(password);
    if (cipher_bin.empty()) {
        result.err = "密码解码失败！";
        return result;
    }

    // 私钥解密
    auto cm = CIM::CryptoModule::Get();
    if (!cm || !cm->isReady()) {
        result.err = "密钥模块未加载！";
        return result;
    }
    std::string decrypted_pwd = "";
    if (!cm->PrivateDecrypt(cipher_bin, decrypted_pwd)) {
        result.err = "密码解密失败！";
        return result;
    }

    /*获取用户信息*/
    if (!CIM::dao::UserDAO::GetByMobile(mobile, result.data, &err)) {
        CIM_LOG_ERROR(g_logger) << "Authenticate GetByMobile failed, mobile=" << mobile
                                << ", err=" << err;
        result.code = 404;
        result.err = "手机号或密码错误！";
        return result;
    }

    /*验证密码*/
    if (!CIM::util::Password::Verify(decrypted_pwd, result.data.password_hash)) {
        result.err = "手机号或密码错误";
        return result;
    }

    /*检查用户状态*/
    if (result.data.status != 1) {
        result.err = "账户被禁用!";
        return result;
    }

    result.ok = true;
    return result;
}

UserResult UserService::Forget(const std::string& mobile, const std::string& new_password) {
    UserResult result;
    std::string err;

    /*密码解密*/
    // Base64 解码
    std::string cipher_bin = CIM::base64decode(new_password);
    if (cipher_bin.empty()) {
        result.err = "密码解码失败！";
        return result;
    }

    // 私钥解密
    auto cm = CIM::CryptoModule::Get();
    if (!cm || !cm->isReady()) {
        result.err = "密钥模块未加载！";
        return result;
    }
    std::string decrypted_pwd = "";
    if (!cm->PrivateDecrypt(cipher_bin, decrypted_pwd)) {
        result.err = "密码解密失败！";
        return result;
    }

    /*检查手机号是否存在*/
    if (!CIM::dao::UserDAO::GetByMobile(mobile, result.data, &err)) {
        CIM_LOG_ERROR(g_logger) << "Forget GetByMobile failed, mobile=" << mobile
                                << ", err=" << err;
        result.code = 404;
        result.err = "手机号不存在！";
        return result;
    }

    /*生成新密码哈希*/
    auto ph = CIM::util::Password::Hash(decrypted_pwd);
    if (ph.empty()) {
        result.err = "密码哈希生成失败！";
        return result;
    }

    /*更新用户密码*/
    if (!CIM::dao::UserDAO::UpdatePassword(result.data.id, ph, &err)) {
        CIM_LOG_ERROR(g_logger) << "Forget UpdatePassword failed, mobile=" << mobile
                                << ", err=" << err;
        result.code = 500;
        result.err = "更新密码失败！";
        return result;
    }

    result.ok = true;
    return result;
}

ResultVoid UserService::LogLogin(const UserResult& user_result, const std::string& platform) {
    ResultVoid result;
    std::string err;

    CIM::dao::UserLoginLog log;
    log.user_id = user_result.data.id;
    log.platform = platform;
    log.login_at = TimeUtil::NowToS();
    log.status = user_result.ok ? 1 : 2;

    if (!CIM::dao::UserLoginLogDAO::Create(log, &err)) {
        CIM_LOG_ERROR(g_logger) << "LogLogin Create UserLoginLog failed, user_id="
                                << user_result.data.id << ", err=" << err;
        result.code = 500;
        result.err = "记录登录日志失败！";
        return result;
    }

    result.ok = true;
    return result;
}

ResultVoid UserService::GoOnline(const uint64_t id) {
    ResultVoid result;
    std::string err;

    if (!CIM::dao::UserDAO::UpdateOnlineStatus(id, &err)) {
        CIM_LOG_ERROR(g_logger) << "UpdateOnlineStatus failed, user_id=" << id << ", err=" << err;
        result.code = 500;
        result.err = "更新在线状态失败！";
        return result;
    }

    result.ok = true;
    return result;
}

ResultVoid UserService::Offline(const uint64_t id) {
    ResultVoid result;
    std::string err;

    if (!CIM::dao::UserDAO::UpdateOfflineStatus(id, &err)) {
        CIM_LOG_ERROR(g_logger) << "UpdateOfflineStatus failed, user_id=" << id << ", err=" << err;
        result.code = 500;
        result.err = "更新在线状态失败！";
        return result;
    }

    result.ok = true;
    return result;
}
}  // namespace CIM::app
