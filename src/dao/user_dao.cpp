#include "dao/user_dao.hpp"

#include "db/mysql.hpp"

namespace CIM::dao {

static const char* kDBName = "default";

bool UserDAO::Create(const std::shared_ptr<CIM::MySQL>& db, const User& u, uint64_t& out_id,
                     std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "INSERT INTO im_user (mobile, email, nickname, avatar, motto, birthday, gender, "
        "online_status, last_online_at, is_qiye, is_robot, is_disabled, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NOW(), NOW())";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindString(1, u.mobile);
    if (!u.email.empty())
        stmt->bindString(2, u.email);
    else
        stmt->bindNull(2);
    stmt->bindString(3, u.nickname);
    if (!u.avatar.empty())
        stmt->bindString(4, u.avatar);
    else
        stmt->bindNull(4);
    if (!u.motto.empty())
        stmt->bindString(5, u.motto);
    else
        stmt->bindNull(5);
    if (!u.birthday.empty())
        stmt->bindString(6, u.birthday);
    else
        stmt->bindNull(6);
    stmt->bindUint8(7, u.gender);
    stmt->bindString(8, u.online_status);
    if (u.last_online_at != 0)
        stmt->bindTime(9, u.last_online_at);
    else
        stmt->bindNull(9);
    stmt->bindUint8(10, u.is_qiye);
    stmt->bindUint8(11, u.is_robot);
    stmt->bindUint8(12, u.is_disabled);

    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    out_id = stmt->getLastInsertId();
    return true;
}

bool UserDAO::GetByMobile(const std::string& mobile, User& out, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT id, mobile, email, nickname, avatar, motto, DATE_FORMAT(birthday, '%Y-%m-%d') as "
        "birthday, gender, online_status, last_online_at, is_qiye, is_robot, is_disabled, "
        "created_at, updated_at FROM im_user WHERE mobile = ? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindString(1, mobile);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    if (!res->next()) {
        if (err) *err = "no record found";
        return false;
    }

    out.id = res->getUint64(0);
    out.mobile = res->isNull(1) ? std::string() : res->getString(1);
    out.email = res->isNull(2) ? std::string() : res->getString(2);
    out.nickname = res->isNull(3) ? std::string() : res->getString(3);
    out.avatar = res->isNull(4) ? std::string() : res->getString(4);
    out.motto = res->isNull(5) ? std::string() : res->getString(5);
    out.birthday = res->isNull(6) ? std::string() : res->getString(6);
    out.gender = res->getUint8(7);
    out.online_status = res->isNull(8) ? "N" : res->getString(8);
    out.last_online_at = res->isNull(9) ? 0 : res->getTime(9);
    out.is_qiye = res->isNull(10) ? 0 : res->getUint8(10);
    out.is_robot = res->isNull(11) ? 0 : res->getUint8(11);
    out.is_disabled = res->isNull(12) ? 0 : res->getUint8(12);
    out.created_at = res->getTime(13);
    out.updated_at = res->getTime(14);

    return true;
}

bool UserDAO::GetById(uint64_t id, User& out, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT id, mobile, email, nickname, avatar, motto, DATE_FORMAT(birthday, "
        "'%Y-%m-%d') as birthday, gender, online_status, last_online_at, is_robot, is_qiye, "
        "is_disabled, created_at, updated_at FROM im_user WHERE id = ? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    if (!res->next()) {
        if (err) *err = "no record found";
        return false;
    }

    out.id = res->getUint64(0);
    out.mobile = res->isNull(1) ? std::string() : res->getString(1);
    out.email = res->isNull(2) ? std::string() : res->getString(2);
    out.nickname = res->isNull(3) ? std::string() : res->getString(3);
    out.avatar = res->isNull(4) ? std::string() : res->getString(4);
    out.motto = res->isNull(5) ? std::string() : res->getString(5);
    out.birthday = res->isNull(6) ? std::string() : res->getString(6);
    out.gender = res->getUint8(7);
    out.online_status = res->isNull(8) ? std::string("N") : res->getString(8);
    out.last_online_at = res->isNull(9) ? 0 : res->getTime(9);
    out.is_robot = res->isNull(10) ? 0 : res->getUint8(10);
    out.is_qiye = res->isNull(11) ? 0 : res->getUint8(11);
    out.is_disabled = res->isNull(12) ? 0 : res->getUint8(12);
    out.created_at = res->getTime(13);
    out.updated_at = res->getTime(14);

    return true;
}

bool UserDAO::GetById(const std::shared_ptr<CIM::MySQL>& db, const uint64_t id, User& out,
                      std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT id, mobile, email, nickname, avatar, motto, DATE_FORMAT(birthday, "
        "'%Y-%m-%d') as birthday, gender, online_status, last_online_at, is_robot, is_qiye, "
        "is_disabled, created_at, updated_at FROM im_user WHERE id = ? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    if (!res->next()) {
        if (err) *err = "no record found";
        return false;
    }

    out.id = res->getUint64(0);
    out.mobile = res->isNull(1) ? std::string() : res->getString(1);
    out.email = res->isNull(2) ? std::string() : res->getString(2);
    out.nickname = res->isNull(3) ? std::string() : res->getString(3);
    out.avatar = res->isNull(4) ? std::string() : res->getString(4);
    out.motto = res->isNull(5) ? std::string() : res->getString(5);
    out.birthday = res->isNull(6) ? std::string() : res->getString(6);
    out.gender = res->getUint8(7);
    out.online_status = res->isNull(8) ? std::string("N") : res->getString(8);
    out.last_online_at = res->isNull(9) ? 0 : res->getTime(9);
    out.is_robot = res->isNull(10) ? 0 : res->getUint8(10);
    out.is_qiye = res->isNull(11) ? 0 : res->getUint8(11);
    out.is_disabled = res->isNull(12) ? 0 : res->getUint8(12);
    out.created_at = res->getTime(13);
    out.updated_at = res->getTime(14);

    return true;
}
bool UserDAO::UpdateUserInfo(uint64_t id, const std::string& nickname, const std::string& avatar,
                             const std::string& motto, const uint8_t gender,
                             const std::string& birthday, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "UPDATE im_user SET nickname = ?, avatar = ?, motto = ?, gender = ?, birthday = ?, "
        "updated_at = NOW() WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindString(1, nickname);
    stmt->bindString(2, avatar);
    stmt->bindString(3, motto);
    stmt->bindUint8(4, gender);
    stmt->bindString(5, birthday);
    stmt->bindUint64(6, id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool UserDAO::UpdateMobile(const uint64_t id, const std::string& new_mobile, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql = "UPDATE users SET mobile = ?, updated_at = NOW() WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindString(1, new_mobile);
    stmt->bindUint64(2, id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool UserDAO::UpdateOnlineStatus(const uint64_t id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql = "UPDATE im_user SET online_status = ? WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindString(1, "Y");
    stmt->bindUint64(2, id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool UserDAO::UpdateOfflineStatus(const uint64_t id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql = "UPDATE im_user SET online_status = ?, last_online_at = NOW() WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    stmt->bindString(1, "N");
    stmt->bindUint64(2, id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool UserDAO::GetOnlineStatus(const uint64_t id, std::string& out_status, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql = "SELECT online_status FROM im_user WHERE id = ? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    stmt->bindUint64(1, id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    if (!res->next()) {
        if (err) *err = "no record found";
        return false;
    }

    out_status = res->isNull(0) ? std::string("N") : res->getString(0);

    return true;
}

bool UserDAO::GetUserInfoSimple(const uint64_t uid, UserInfo& out, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT id, nickname, avatar, motto, gender, is_qiye, mobile, email "
        "FROM im_user WHERE id = ? LIMIT 1";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, uid);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }

    if (!res->next()) {
        if (err) *err = "no record found";
        return false;
    }

    out.uid = res->isNull(0) ? 0 : res->getUint64(0);
    out.nickname = res->isNull(1) ? std::string() : res->getString(1);
    out.avatar = res->isNull(2) ? std::string() : res->getString(2);
    out.motto = res->isNull(3) ? std::string() : res->getString(3);
    out.gender = res->isNull(4) ? 0 : res->getUint8(4);
    out.is_qiye = res->isNull(5) ? false : (res->getUint8(5) != 0);
    out.mobile = res->isNull(6) ? std::string() : res->getString(6);
    out.email = res->isNull(7) ? std::string() : res->getString(7);

    return true;
}
}  // namespace CIM::dao
