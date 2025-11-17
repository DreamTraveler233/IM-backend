#include "dao/user_login_log_dao.hpp"

#include "db/mysql.hpp"

namespace CIM::dao {

static constexpr const char* kDBName = "default";

bool UserLoginLogDAO::Create(const UserLoginLog& log, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "INSERT INTO im_auth_login_log (user_id, mobile, platform, ip, address, user_agent, "
        "success, reason, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, NOW())";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    if (log.user_id != 0)
        stmt->bindUint64(1, log.user_id);
    else
        stmt->bindNull(1);
    if (!log.mobile.empty())
        stmt->bindString(2, log.mobile);
    else
        stmt->bindNull(2);
    stmt->bindString(3, log.platform);
    stmt->bindString(4, log.ip);
    if (!log.address.empty())
        stmt->bindString(5, log.address);
    else
        stmt->bindNull(5);
    if (!log.user_agent.empty())
        stmt->bindString(6, log.user_agent);
    else
        stmt->bindNull(6);
    stmt->bindInt32(7, log.success);
    if (!log.reason.empty())
        stmt->bindString(8, log.reason);
    else
        stmt->bindNull(8);

    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

}  // namespace CIM::dao