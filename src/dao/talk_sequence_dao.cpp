#include "dao/talk_sequence_dao.hpp"

namespace CIM::dao {

namespace {
inline void set_err(std::string* err, const std::string& v) {
    if (err) *err = v;
}
}  // namespace

bool TalkSequenceDao::nextSeq(const std::shared_ptr<CIM::MySQL>& db, uint64_t talk_id,
                              uint64_t& seq, std::string* err) {
    if (!db) {
        set_err(err, "TalkSequenceDao::nextSeq db is null");
        return false;
    }

    // Upsert + 自增
    {
        const char* sql =
            "INSERT INTO im_talk_sequence (talk_id, last_seq, created_at, updated_at) "
            "VALUES (?, 1, NOW(), NOW()) "
            "ON DUPLICATE KEY UPDATE last_seq = last_seq + 1, updated_at = NOW()";
        auto stmt = db->prepare(sql);
        if (!stmt) {
            set_err(err, std::string("prepare failed: ") + db->getErrStr());
            return false;
        }
        stmt->bindUint64(1, talk_id);
    if (stmt->execute() != 0) {
            set_err(err, std::string("execute failed: ") + db->getErrStr());
            return false;
        }
    }

    // 查询最新序列
    {
        const char* sql = "SELECT last_seq FROM im_talk_sequence WHERE talk_id = ? LIMIT 1";
        auto stmt = db->prepare(sql);
        if (!stmt) {
            set_err(err, std::string("prepare failed: ") + db->getErrStr());
            return false;
        }
        stmt->bindUint64(1, talk_id);
        auto res = stmt->query();
        if (!res) {
            set_err(err, std::string("query failed: ") + db->getErrStr());
            return false;
        }
        if (!res->next()) {
            set_err(err, "talk sequence row missing after upsert");
            return false;
        }
        seq = res->getUint64(0);
    }

    return true;
}

}  // namespace CIM::dao
