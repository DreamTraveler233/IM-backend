#include "dao/contact_group_dao.hpp"

#include "db/mysql.hpp"

namespace CIM::dao {

static const char* kDBName = "default";

bool ContactGroupDAO::Create(const ContactGroup& g, uint64_t& out_id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql =
        "INSERT INTO contact_groups (user_id, name, sort, contact_count, created_at) VALUES (?, ?, "
        "?, ?, ?)";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, g.user_id);
    stmt->bindString(2, g.name);
    stmt->bindUint32(3, g.sort);
    stmt->bindUint32(4, g.contact_count);
    stmt->bindTime(5, g.created_at);
    if (stmt->execute() != 0) {
        if (err) *err = "execute failed";
        return false;
    }
    out_id = static_cast<uint64_t>(stmt->getLastInsertId());
    return true;
}

bool ContactGroupDAO::GetById(const uint64_t id, ContactGroup& out, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql =
        "SELECT id, user_id, name, sort, contact_count, created_at, updated_at FROM contact_groups "
        "WHERE id = ? LIMIT 1";
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
    while (res->next()) {
        out.id = res->getUint64(0);
        out.user_id = res->getUint64(1);
        out.name = res->getString(2);
        out.sort = res->getUint32(3);
        out.contact_count = res->getUint32(4);
        out.created_at = res->getTime(5);
        out.updated_at = res->getTime(6);
        return true;
    }
    return false;
}

bool ContactGroupDAO::ListByUserId(const uint64_t user_id, std::vector<ContactGroupItem>& outs,
                                   std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql =
        "SELECT id, name, contact_count, sort FROM contact_groups "
        "WHERE user_id = ? ORDER BY sort ASC, id ASC";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
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
        g.name = res->getString(1);
        g.contact_count = res->getUint32(2);
        g.sort = res->getUint32(3);
        outs.push_back(std::move(g));
    }
    return true;
}

bool ContactGroupDAO::Update(const uint64_t id, uint32_t sort, const std::string& name,
                             std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql = "UPDATE contact_groups SET name = ?, sort = ? WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindString(1, name);
    stmt->bindUint32(2, sort);
    stmt->bindUint64(3, id);
    if (stmt->execute() != 0) {
        if (err) *err = "execute failed";
        return false;
    }
    return true;
}

bool ContactGroupDAO::Delete(const uint64_t id, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql = "DELETE FROM contact_groups WHERE id = ?";
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, id);
    if (stmt->execute() != 0) {
        if (err) *err = "execute failed";
        return false;
    }
    return true;
}

bool ContactGroupDAO::UpdateContactCount(const uint64_t group_id, bool increase, std::string* err) {
    auto db = CIM::MySQLMgr::GetInstance()->get(kDBName);
    if (!db) {
        if (err) *err = "no mysql connection";
        return false;
    }
    const char* sql;
    if (!increase) {
        sql = "UPDATE contact_groups SET contact_count = contact_count - 1 WHERE id = ?";
    } else {
        sql = "UPDATE contact_groups SET contact_count = contact_count + 1 WHERE id = ?";
    }
    auto stmt = db->prepare(sql);
    if (!stmt) {
        if (err) *err = "prepare failed";
        return false;
    }
    stmt->bindUint64(1, group_id);
    if (stmt->execute() != 0) {
        if (err) *err = "execute failed";
        return false;
    }
    return true;
}
}  // namespace CIM::dao
