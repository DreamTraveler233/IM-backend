#include "dao/user_dao.hpp"

#include "db/mysql.hpp"

namespace CIM::dao {

static const char* kDBName = "default";

bool UserDAO::Create(const User& u, uint64_t& out_id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql =
        "INSERT INTO im_user (mobile, email, nickname, avatar, motto, birthday, gender, "
        "online_status, last_online_at, is_qiye, is_robot, is_disabled, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NOW(), NOW())";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    int idx = 1;
    stmt->bindString(idx++, u.mobile);
    if (u.email.has_value())
        stmt->bindString(idx++, *u.email);
    else
        stmt->bindNull(idx++);
    stmt->bindString(idx++, u.nickname);
    if (u.avatar.has_value())
        stmt->bindString(idx++, *u.avatar);
    else
        stmt->bindNull(idx++);
    if (u.motto.has_value())
        stmt->bindString(idx++, *u.motto);
    else
        stmt->bindNull(idx++);
    if (u.birthday.has_value())
        stmt->bindString(idx++, *u.birthday);
    else
        stmt->bindNull(idx++);
    stmt->bindInt32(idx++, u.gender);
    stmt->bindString(idx++, u.online_status);
    if (u.last_online_at.has_value())
        stmt->bindTime(idx++, *u.last_online_at);
    else
        stmt->bindNull(idx++);
    stmt->bindInt32(idx++, u.is_qiye);
    stmt->bindInt32(idx++, u.is_robot);
    stmt->bindInt32(idx++, u.is_disabled);

    if (stmt->execute() != 0) {
        if (err) *err = "execute failed";
        return false;
    }
    out_id = static_cast<uint64_t>(stmt->getLastInsertId());
    return true;
}

bool UserDAO::GetByMobile(const std::string& mobile, User& out, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql =
        "SELECT id, mobile, email, nickname, avatar, motto, DATE_FORMAT(birthday, '%Y-%m-%d') as "
        "birthday, "
        "gender, online_status, last_online_at, is_qiye, is_robot, is_disabled, created_at, "
        "updated_at "
        "FROM im_user WHERE mobile = ? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindString(1, mobile);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    if (!res->next()) {
        if (err) *err = "user not found";
        return false;
    }
    int idx = 0;
    out.id = static_cast<uint64_t>(res->getUint64(idx++));
    out.mobile = res->getString(idx++);
    out.email = res->isNull(idx++) ? std::nullopt : std::make_optional(res->getString(idx++));
    out.nickname = res->getString(idx++);
    out.avatar = res->isNull(idx++) ? std::nullopt : std::make_optional(res->getString(idx++));
    out.motto = res->isNull(idx++) ? std::nullopt : std::make_optional(res->getString(idx++));
    out.birthday = res->isNull(idx++) ? std::nullopt : std::make_optional(res->getString(idx++));
    out.gender = static_cast<uint8_t>(res->getInt32(idx++));
    out.online_status = res->isNull(idx++) ? "N" : res->getString(idx++);
    out.last_online_at =
        res->isNull(idx++) ? std::nullopt : std::make_optional(res->getTime(idx++));
    out.is_qiye = res->isNull(idx++) ? 0 : res->getInt32(idx++);
    out.is_robot = res->isNull(idx++) ? 0 : res->getInt32(idx++);
    out.is_disabled = res->isNull(idx++) ? 0 : res->getInt32(idx++);
    out.created_at = res->getTime(idx++);
    out.updated_at = res->getTime(idx++);
    return true;
}

bool UserDAO::GetById(uint64_t id, User& out, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql =
        "SELECT id, mobile, email, nickname, password_hash, avatar, motto, DATE_FORMAT(birthday, "
        "'%Y-%m-%d') as birthday, gender, online_status, last_online_at, is_robot, is_qiye, "
        "status, created_at, updated_at FROM users WHERE id = ? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    if (!res->next()) {
        if (err) *err = "user not found";
        return false;
    }
    out.id = static_cast<uint64_t>(res->getUint64(0));
    out.mobile = res->getString(1);
    out.email = res->isNull(2) ? std::string() : res->getString(2);
    out.nickname = res->getString(3);
    out.password_hash = res->getString(4);
    out.avatar = res->isNull(5) ? std::string() : res->getString(5);
    out.motto = res->isNull(6) ? std::string() : res->getString(6);
    out.birthday = res->isNull(7) ? std::string() : res->getString(7);
    out.gender = static_cast<uint32_t>(res->getInt32(8));
    out.online_status = res->isNull(9) ? "N" : res->getString(9);
    out.last_online_at = res->isNull(10) ? 0 : res->getTime(10);
    out.is_robot = res->isNull(11) ? 0 : res->getInt32(11);
    out.is_qiye = res->isNull(12) ? 0 : res->getInt32(12);
    out.status = res->isNull(13) ? 1 : res->getInt32(13);
    out.created_at = res->getTime(14);
    out.updated_at = res->getTime(15);
    return true;
}

bool UserDAO::UpdatePassword(uint64_t id, const std::string& new_password_hash, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql = "UPDATE users SET password_hash = ?, updated_at = NOW() WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindString(1, new_password_hash);
    stmt->bindUint64(2, id);
    if (stmt->execute() != 0) {
        if (err) *err = "execute failed";
        return false;
    }
    return true;
}

bool UserDAO::UpdateStatus(uint64_t id, int32_t status, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql = "UPDATE users SET status = ?, updated_at = NOW() WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindInt32(1, status);
    stmt->bindUint64(2, id);
    if (stmt->execute() != 0) {
        if (err) *err = "execute failed";
        return false;
    }
    return true;
}

bool UserDAO::UpdateUserInfo(uint64_t id, const std::string& nickname, const std::string& avatar,
                             const std::string& motto, const uint32_t gender,
                             const std::string& birthday, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql =
        "UPDATE users SET nickname = ?, avatar = ?, motto = ?, gender = ?, birthday = ?, "
        "updated_at = NOW() WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindString(1, nickname);
    stmt->bindString(2, avatar);
    stmt->bindString(3, motto);
    stmt->bindUint32(4, gender);
    stmt->bindString(5, birthday);
    stmt->bindUint64(6, id);
    if (stmt->execute() != 0) {
        if (err) *err = "execute failed";
        return false;
    }
    return true;
}

bool UserDAO::UpdateMobile(const uint64_t id, const std::string& new_mobile, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql = "UPDATE users SET mobile = ?, updated_at = NOW() WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindString(1, new_mobile);
    stmt->bindUint64(2, id);
    if (stmt->execute() != 0) {
        if (err) *err = "execute failed";
        return false;
    }
    return true;
}

bool UserDAO::UpdateOnlineStatus(const uint64_t id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql = "UPDATE users SET online_status = ? WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindString(1, "Y");
    stmt->bindUint64(2, id);
    if (stmt->execute() != 0) {
        if (err) *err = "execute failed";
        return false;
    }
    return true;
}

bool UserDAO::UpdateOfflineStatus(const uint64_t id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql = "UPDATE users SET online_status = ?, last_online_at = ? WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindString(1, "N");
    stmt->bindTime(2, TimeUtil::NowToS());
    stmt->bindUint64(3, id);
    if (stmt->execute() != 0) {
        if (err) *err = "execute failed";
        return false;
    }
    return true;
}

bool UserDAO::GetOnlineStatus(const uint64_t id, std::string& out_status, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql = "SELECT online_status FROM users WHERE id = ? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    if (!res->next()) {
        if (err) *err = "user not found";
        return false;
    }
    out_status = res->getString(0);
    return true;
}

bool UserDAO::GetUserInfoSimple(const uint64_t uid, UserInfo& out, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql =
        "SELECT id, nickname, avatar, motto, gender, is_qiye, mobile, email FROM users WHERE id = "
        "? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, uid);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    if (!res->next()) {
        if (err) *err = "user not found";
        return false;
    }
    out.uid = res->isNull(0) ? 0 : res->getUint64(0);
    out.nickname = res->isNull(1) ? std::string() : res->getString(1);
    out.avatar = res->isNull(2) ? std::string() : res->getString(2);
    out.motto = res->isNull(3) ? std::string() : res->getString(3);
    out.gender = res->isNull(4) ? 0 : res->getUint32(4);
    out.is_qiye = res->isNull(5) ? false : (res->getUint32(5) != 0);
    out.mobile = res->isNull(6) ? std::string() : res->getString(6);
    out.email = res->isNull(7) ? std::string() : res->getString(7);
    return true;
}
}  // namespace CIM::dao
