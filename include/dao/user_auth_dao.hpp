#ifndef __CIM_DAO_USER_AUTH_DAO_HPP__
#define __CIM_DAO_USER_AUTH_DAO_HPP__

#include <cstdint>
#include <ctime>
#include <optional>
#include <string>

namespace CIM::dao {

struct UserAuth {
    uint64_t user_id = 0;                              // 用户ID (im_user.id)
    std::string password_hash;                         // 密码哈希（bcrypt/argon2 等）
    std::string password_algo = "PBKDF2-HMAC-SHA256";  // 哈希算法标识，默认为 PBKDF2-HMAC-SHA256
    int16_t password_version = 1;                      // 密码版本
    std::optional<std::time_t> last_reset_at;          // 最近重置时间，可空
    std::time_t created_at = 0;                        // 创建时间 (timestamp)
    std::time_t updated_at = 0;                        // 更新时间 (timestamp)
};

class UserAuthDao {
   public:
    static bool Create(const UserAuth& ua, std::string* err = nullptr);

    static bool GetByUserId(const uint64_t user_id, UserAuth& out, std::string* err = nullptr);

    static bool UpdatePasswordHash(const uint64_t user_id, const std::string& new_password_hash,
                                   std::string* err = nullptr);
};

}  // namespace CIM::dao

#endif  // __CIM_DAO_USER_AUTH_DAO_HPP__
