#include "dao/contact_group_dao.hpp"

#include "db/mysql.hpp"

namespace CIM::dao {

static const char* kDBName = "default";

bool ContactGroupDAO::CreateWithConn(const std::shared_ptr<CIM::MySQL>& db, const ContactGroup& g,
                                     uint64_t& out_id, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "INSERT INTO im_contact_group (user_id, name, sort, contact_count, created_at, updated_at) "
        "VALUES (?, ?, "
        "?, ?, NOW(), NOW())";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, g.user_id);
    stmt->bindString(2, g.name);
    stmt->bindUint32(3, g.sort);
    stmt->bindUint32(4, g.contact_count);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    out_id = static_cast<uint64_t>(stmt->getLastInsertId());
    return true;
}

bool ContactGroupDAO::ListByUserId(const uint64_t user_id, std::vector<ContactGroupItem>& outs,
                                   std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT id, name, contact_count, sort FROM im_contact_group "
        "WHERE user_id = ? ORDER BY sort ASC, id ASC";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, user_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    while (res->next()) {
    ContactGroupItem g;
    g.id = res->getUint64(0);
    g.name = res->isNull(1) ? std::string() : res->getString(1);
        g.contact_count = res->getUint32(2);
        g.sort = res->getUint32(3);
        outs.push_back(std::move(g));
    }
    return true;
}

bool ContactGroupDAO::ListByUserIdWithConn(const std::shared_ptr<CIM::MySQL>& db,
                                           const uint64_t user_id,
                                           std::vector<ContactGroupItem>& outs, std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql =
        "SELECT id, name, contact_count, sort FROM im_contact_group "
        "WHERE user_id = ? ORDER BY sort ASC, id ASC";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, user_id);
    auto res = stmt->query();
    if (!res) {
        if (err) *err = "query failed";
        return false;
    }
    while (res->next()) {
    ContactGroupItem g;
    g.id = res->getUint64(0);
    g.name = res->isNull(1) ? std::string() : res->getString(1);
        g.contact_count = res->getUint32(2);
        g.sort = res->getUint32(3);
        outs.push_back(std::move(g));
    }
    return true;
}
bool ContactGroupDAO::UpdateWithConn(const std::shared_ptr<CIM::MySQL>& db, const uint64_t id,
                                     const uint32_t sort, const std::string& name,
                                     std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql = "UPDATE im_contact_group SET name = ?, sort = ? WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindString(1, name);
    stmt->bindUint32(2, sort);
    stmt->bindUint64(3, id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ContactGroupDAO::DeleteWithConn(const std::shared_ptr<CIM::MySQL>& db, const uint64_t id,
                                     std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql = "DELETE FROM im_contact_group WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}

bool ContactGroupDAO::UpdateContactCountWithConn(const std::shared_ptr<CIM::MySQL>& db,
                                           const uint64_t group_id, bool increase,
                                           std::string* err) {
    if (!db) {
        if (err) *err = "get mysql connection failed";
        return false;
    }
    const char* sql;
    if (!increase) {
        // avoid unsigned underflow
        sql =
            "UPDATE im_contact_group SET contact_count = GREATEST(contact_count - 1, 0) WHERE id = "
            "?";
    } else {
        sql = "UPDATE im_contact_group SET contact_count = contact_count + 1 WHERE id = ?";
    }
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare sql failed";
        return false;
    }
    stmt->bindUint64(1, group_id);
    if (stmt->execute() != 0) {
        if (err) *err = stmt->getErrStr();
        return false;
    }
    return true;
}
}  // namespace CIM::dao
