#ifndef __CIM_DAO_TALK_SEQUENCE_DAO_HPP__
#define __CIM_DAO_TALK_SEQUENCE_DAO_HPP__

#include <cstdint>
#include <memory>
#include <string>

#include "db/mysql.hpp"

namespace CIM::dao {

// 会话序列分配：基于表 im_talk_sequence(talk_id PK, last_seq)
// 建议在外层事务中调用，确保和消息入库/会话推进的一致性。
class TalkSequenceDao {
   public:
    // 递增并返回新的序列号（从 1 开始）。
    // 若不存在对应 talk_id 的行，则插入并返回 1。
    static bool nextSeq(const std::shared_ptr<CIM::MySQL>& db, uint64_t talk_id, uint64_t& seq,
                        std::string* err = nullptr);
};

}  // namespace CIM::dao

#endif  // __CIM_DAO_TALK_SEQUENCE_DAO_HPP__
