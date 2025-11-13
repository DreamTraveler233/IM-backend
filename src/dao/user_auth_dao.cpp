#include "dao/user_auth_dao.hpp"

#include "db/mysql.hpp"

namespace CIM::dao {

static const char* kDBName = "default";

bool UserAuthDao::Create(const std::shared_ptr<CIM::MySQL>& db, const UserAuth& ua,
                         std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "INSERT INTO im_user_auth (user_id, password_hash, password_algo, password_version "
        ", last_reset_at, created_at, updated_at) VALUES (?, ?, ?, ?, ?, NOW(), NOW())";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, ua.user_id);
    stmt->bindString(2, ua.password_hash);
    stmt->bindString(3, ua.password_algo);
    stmt->bindInt16(4, ua.password_version);
    if (ua.last_reset_at != 0)
        stmt->bindTime(5, ua.last_reset_at);
    else
        stmt->bindNull(5);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool UserAuthDao::GetByUserId(const uint64_t user_id, UserAuth& out, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT user_id, password_hash, password_algo, password_version, last_reset_at, "
        "created_at, updated_at "
        "FROM im_user_auth WHERE user_id = ? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, user_id);
    auto res = stmt->query();
    if (!res || !res->next()) {
        if (err) *err = "query failed";
        return false;
    }

    out.user_id = res->getUint64(0);
    out.password_hash = res->isNull(1) ? std::string() : res->getString(1);
    out.password_algo = res->isNull(2) ? std::string("PBKDF2-HMAC-SHA256") : res->getString(2);
    out.password_version = res->isNull(3) ? 1 : res->getInt16(3);
    out.last_reset_at = res->isNull(4) ? 0 : res->getTime(4);
    out.created_at = res->getTime(5);
    out.updated_at = res->getTime(6);

    return true;
}

bool UserAuthDao::UpdatePasswordHash(const uint64_t user_id, const std::string& new_password_hash,
                                     std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_user_auth SET password_hash = ?, updated_at = NOW() WHERE user_id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    stmt->bindString(1, new_password_hash);
    stmt->bindUint64(2, user_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

}  // namespace CIM::dao