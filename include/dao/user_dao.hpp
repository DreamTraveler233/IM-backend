#ifndef __CIM_DAO_USER_DAO_HPP__
#define __CIM_DAO_USER_DAO_HPP__

#include <cstdint>
#include <ctime>
#include <optional>
#include <string>

namespace CIM::dao {

struct User {
    uint64_t id = 0;                            // 用户ID（雪花ID/分布式ID）
    std::string mobile;                         // 手机号（唯一，用于登录/找回）
    std::optional<std::string> email;           // 邮箱（唯一，可选，用于找回/通知）
    std::string nickname;                       // 昵称
    std::optional<std::string> avatar;          // 头像URL，可选
    std::optional<std::string> motto;           // 个性签名，可选
    std::optional<std::string> birthday;        // 生日 YYYY-MM-DD，可选
    uint8_t gender = 0;                         // 性别：0=未知 1=男 2=女
    std::string online_status = "N";            // 在线状态：N:离线 Y:在线
    std::optional<std::time_t> last_online_at;  // 最后在线时间，可空
    uint8_t is_qiye = 0;                        // 是否企业用户：0=否 1=是
    uint8_t is_robot = 0;                       // 是否机器人账号：0=否 1=是
    uint8_t is_disabled = 0;                    // 是否禁用：0=否 1=是
    std::time_t created_at = 0;                 // 创建时间
    std::time_t updated_at = 0;                 // 更新时间
};

struct UserInfo {
    uint64_t uid;          // 用户ID
    std::string nickname;  // 昵称
    std::string avatar;    // 头像URL
    std::string motto;     // 个性签名
    uint32_t gender;       // 性别：0未知 1男 2女
    bool is_qiye;          // 是否企业用户
    std::string mobile;    // 手机号
    std::string email;     // 邮箱
};

class UserDAO {
   public:
    // 创建新用户
    static bool Create(const User& u, uint64_t& out_id, std::string* err = nullptr);

    // 根据手机号获取用户信息
    static bool GetByMobile(const std::string& mobile, User& out, std::string* err = nullptr);

    // 根据用户ID获取用户信息
    static bool GetById(const uint64_t id, User& out, std::string* err = nullptr);

    // 更新用户密码
    static bool UpdatePassword(const uint64_t id, const std::string& new_password_hash,
                               std::string* err = nullptr);

    // 更新用户状态
    static bool UpdateStatus(const uint64_t id, int32_t status, std::string* err = nullptr);

    // 更新用户信息（昵称、头像、签名等）
    static bool UpdateUserInfo(const uint64_t id, const std::string& nickname,
                               const std::string& avatar, const std::string& motto,
                               const uint32_t gender, const std::string& birthday,
                               std::string* err = nullptr);

    // 更新用户手机号
    static bool UpdateMobile(const uint64_t id, const std::string& new_mobile,
                             std::string* err = nullptr);

    // 用户上线
    static bool UpdateOnlineStatus(const uint64_t id, std::string* err = nullptr);

    // 用户下线
    static bool UpdateOfflineStatus(const uint64_t id, std::string* err = nullptr);

    // 获取用户在线状态
    static bool GetOnlineStatus(const uint64_t id, std::string& out_status,
                                std::string* err = nullptr);

    // 获取用户配置信息
    static bool GetUserInfoSimple(const uint64_t uid, UserInfo& out, std::string* err = nullptr);
};

}  // namespace CIM::dao

#endif  // __CIM_DAO_USER_DAO_HPP__