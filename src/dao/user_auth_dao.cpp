#include "dao/user_auth_dao.hpp"

#include "db/mysql.hpp"

namespace CIM::dao {

static const char* kDBName = "default";

bool UserAuthDao::Create(const UserAuth& ua, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql =
        "INSERT INTO im_user_auth (user_id, password_hash, created_at, updated_at) "
        "VALUES (?, ?, NOW(), NOW())";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, ua.user_id);
    stmt->bindString(2, ua.password_hash);
    if (stmt->execute() != 0) {
        if (err) *err = "execute failed";
        return false;
    }
    return true;
}

}  // namespace CIM::dao
