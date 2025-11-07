#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace CIM::dao {

struct ContactGroup {
    uint64_t id = 0;             // 分组ID
    uint64_t user_id = 0;        // 所属用户ID
    std::string name;            // 分组名称
    uint32_t sort = 100;         // 排序值
    uint32_t contact_count = 0;  // 分组下联系人数量
    std::time_t created_at = 0;  // 创建时间
    std::time_t updated_at = 0;  // 更新时间
};

struct ContactGroupItem {
    uint64_t id = 0;             // 分组ID
    std::string name;            // 分组名称
    uint32_t contact_count = 0;  // 分组下联系人数量
    uint32_t sort = 0;           // 排序值
};

class ContactGroupDAO {
   public:
    // 创建新的联系人分组
    static bool Create(const ContactGroup& g, uint64_t& out_id, std::string* err = nullptr);
    // 根据ID获取联系人分组信息
    static bool GetById(const uint64_t id, ContactGroup& out, std::string* err = nullptr);
    // 根据用户ID获取联系人分组列表
    static bool ListByUserId(const uint64_t user_id, std::vector<ContactGroupItem>& outs,
                             std::string* err = nullptr);
    // 更新联系人分组信息
    static bool Update(const uint64_t id, uint32_t sort, const std::string& name,
                       std::string* err = nullptr);
    // 删除联系人分组
    static bool Delete(const uint64_t id, std::string* err = nullptr);
    // 更新分组下的联系人数量
    static bool UpdateContactCount(const uint64_t group_id, bool increase,
                                   std::string* err = nullptr);
};

}  // namespace CIM::dao
