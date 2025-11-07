#include "app/contact_service.hpp"

#include <unordered_set>
#include <vector>

#include "dao/contact_apply_dao.hpp"
#include "dao/contact_dao.hpp"
#include "dao/user_dao.hpp"
#include "macro.hpp"

namespace CIM::app {
static auto g_logger = CIM_LOG_NAME("system");

UserResult ContactService::SearchByMobile(const std::string& mobile) {
    UserResult result;
    std::string err;

    if (!CIM::dao::UserDAO::GetByMobile(mobile, result.data, &err)) {
        CIM_LOG_ERROR(g_logger) << "SearchByMobile failed, mobile=" << mobile << ", err=" << err;
        result.code = 404;
        result.err = "联系人不存在！";
        return result;
    }
    result.ok = true;
    return result;
}

ContactDetailsResult ContactService::GetContactDetail(const uint64_t owner_id,
                                                      const uint64_t target_id) {
    ContactDetailsResult result;
    std::string err;

    if (!CIM::dao::ContactDAO::GetByOwnerAndTarget(owner_id, target_id, result.data, &err)) {
        CIM_LOG_ERROR(g_logger) << "GetContactDetail failed, owner_id=" << owner_id
                                << ", target_id=" << target_id << ", err=" << err;
        result.code = 404;
        result.err = "用户不存在！";
        return result;
    }
    result.ok = true;
    return result;
}

ContactListResult ContactService::ListFriends(const uint64_t user_id) {
    ContactListResult result;
    std::string err;

    if (!CIM::dao::ContactDAO::ListByUser(user_id, result.data, &err)) {
        CIM_LOG_ERROR(g_logger) << "ListFriends failed, user_id=" << user_id << ", err=" << err;
        result.code = 500;
        result.err = "获取好友列表失败！";
        return result;
    }
    result.ok = true;
    return result;
}

ResultVoid ContactService::CreateContactApply(uint64_t from_id, uint64_t to_id,
                                              const std::string& remark) {
    ResultVoid result;
    std::string err;

    CIM::dao::ContactApply apply;
    apply.applicant_id = from_id;
    apply.target_id = to_id;
    apply.remark = remark;
    apply.created_at = TimeUtil::NowToS();
    if (!CIM::dao::ContactApplyDAO::Create(apply, &err)) {
        CIM_LOG_ERROR(g_logger) << "CreateContactApply failed, from_id=" << from_id
                                << ", to_id=" << to_id << ", err=" << err;
        result.code = 500;
        result.err = "创建好友申请失败！";
        return result;
    }

    result.ok = true;
    return result;
}

ApplyCountResult ContactService::GetPendingContactApplyCount(uint64_t user_id) {
    ApplyCountResult result;
    std::string err;

    if (!CIM::dao::ContactApplyDAO::GetPendingCountById(user_id, result.data, &err)) {
        CIM_LOG_ERROR(g_logger) << "GetPendingContactApplyCount failed, user_id=" << user_id
                                << ", err=" << err;
        result.code = 500;
        result.err = "获取未处理的好友申请数量失败！";
        return result;
    }

    result.ok = true;
    return result;
}

ContactApplyListResult ContactService::ListContactApplies(uint64_t user_id) {
    ContactApplyListResult result;
    std::string err;

    if (!CIM::dao::ContactApplyDAO::GetItemById(user_id, result.data, &err)) {
        CIM_LOG_ERROR(g_logger) << "ListContactApplies failed, user_id=" << user_id
                                << ", err=" << err;
        result.code = 500;
        result.err = "获取好友申请列表失败！";
        return result;
    }
    result.ok = true;
    return result;
}

ResultVoid ContactService::AgreeApply(const uint64_t apply_id, const std::string& remark) {
    ResultVoid result;
    std::string err;

    // 更新申请状态为已同意
    if (!CIM::dao::ContactApplyDAO::AgreeApply(apply_id, remark, &err)) {
        CIM_LOG_ERROR(g_logger) << "HandleContactApply AgreeApply failed, apply_id=" << apply_id
                                << ", err=" << err;
        result.code = 500;
        result.err = "处理好友申请失败！";
        return result;
    }

    // 获取申请详情
    CIM::dao::ContactApply apply;
    if (!CIM::dao::ContactApplyDAO::GetDetailById(apply_id, apply, &err)) {
        CIM_LOG_ERROR(g_logger) << "HandleContactApply GetDetailById failed, apply_id=" << apply_id
                                << ", err=" << err;
        result.code = 500;
        result.err = "获取好友申请详情失败！";
        return result;
    }

    // 使用 SQL 级别的 upsert（插入或更新）简化：无记录则创建，有记录则把 status/relation 恢复为好友状态
    CIM::dao::Contact c1;
    c1.user_id = apply.target_id;  // 目标用户添加申请人
    c1.contact_id = apply.applicant_id;
    c1.relation = 2;
    c1.created_at = TimeUtil::NowToS();
    c1.status = 1;
    if (!CIM::dao::ContactDAO::Upsert(c1, &err)) {
        CIM_LOG_ERROR(g_logger) << "AgreeApply Upsert failed for target->applicant, apply_id="
                                << apply_id << ", err=" << err;
        result.code = 500;
        result.err = "创建/更新好友记录失败！";
        return result;
    }

    CIM::dao::Contact c2;
    c2.user_id = apply.applicant_id;  // 申请人添加目标用户
    c2.contact_id = apply.target_id;
    c2.relation = 2;
    c2.group_id = 0;
    c2.created_at = TimeUtil::NowToS();
    c2.status = 1;
    if (!CIM::dao::ContactDAO::Upsert(c2, &err)) {
        CIM_LOG_ERROR(g_logger) << "AgreeApply Upsert failed for applicant->target, apply_id="
                                << apply_id << ", err=" << err;
        result.code = 500;
        result.err = "创建/更新好友记录失败！";
        return result;
    }
    result.ok = true;
    return result;
}

ResultVoid ContactService::RejectApply(const uint64_t apply_id, const std::string& remark) {
    ResultVoid result;
    std::string err;

    if (!CIM::dao::ContactApplyDAO::RejectApply(apply_id, remark, &err)) {
        CIM_LOG_ERROR(g_logger) << "HandleContactApply RejectApply failed, apply_id=" << apply_id
                                << ", err=" << err;
        result.code = 500;
        result.err = "处理好友申请失败！";
        return result;
    }

    result.ok = true;
    return result;
}

ResultVoid ContactService::EditContactRemark(const uint64_t user_id, const uint64_t contact_id,
                                             const std::string& remark) {
    ResultVoid result;
    std::string err;

    if (!CIM::dao::ContactDAO::EditRemark(user_id, contact_id, remark, &err)) {
        CIM_LOG_ERROR(g_logger) << "EditContactRemark failed, user_id=" << user_id
                                << ", err=" << err;
        result.code = 500;
        result.err = "修改联系人备注失败！";
        return result;
    }

    result.ok = true;
    return result;
}

ResultVoid ContactService::DeleteContact(const uint64_t user_id, const uint64_t contact_id) {
    ResultVoid result;
    std::string err;

    // 查询联系人所在分组
    uint64_t group_id = 0;
    if (!CIM::dao::ContactDAO::GetOldGroupId(user_id, contact_id, group_id, &err)) {
        CIM_LOG_ERROR(g_logger) << "DeleteContact GetGroup failed, user_id=" << user_id
                                << ", contact_id=" << contact_id << ", err=" << err;
        result.code = 500;
        result.err = "获取联系人分组失败！";
        return result;
    }

    // 如果在分组中，更新分组下的联系人数量 -1
    if (group_id != 0) {
        if (!CIM::dao::ContactGroupDAO::UpdateContactCount(group_id, false, &err)) {
            CIM_LOG_ERROR(g_logger)
                << "DeleteContact UpdateContactCount failed, user_id=" << user_id
                << ", contact_id=" << contact_id << ", group_id=" << group_id << ", err=" << err;
            result.code = 500;
            result.err = "更新联系人分组数量失败！";
            return result;
        }
    }

    // 删除 user_id -> contact_id
    if (!CIM::dao::ContactDAO::Delete(user_id, contact_id, &err)) {
        CIM_LOG_ERROR(g_logger) << "DeleteContact failed, user_id=" << user_id
                                << ", contact_id=" << contact_id << ", err=" << err;
        result.code = 500;
        result.err = "删除联系人失败！";
        return result;
    }

    // 删除 contact_id -> user_id（双向）
    if (!CIM::dao::ContactDAO::Delete(contact_id, user_id, &err)) {
        CIM_LOG_ERROR(g_logger) << "DeleteContact failed, user_id=" << user_id
                                << ", contact_id=" << contact_id << ", err=" << err;
        result.code = 500;
        result.err = "删除联系人失败！";
        return result;
    }

    // 从分组中移除联系人
    if (!CIM::dao::ContactDAO::RemoveFromGroup(user_id, contact_id, &err)) {
        CIM_LOG_ERROR(g_logger) << "DeleteContact failed, user_id=" << user_id
                                << ", contact_id=" << contact_id << ", err=" << err;
        result.code = 500;
        result.err = "从分组中移除联系人失败！";
        return result;
    }

    result.ok = true;
    return result;
}

ResultVoid ContactService::SaveContactGroup(
    const uint64_t user_id,
    const std::vector<std::tuple<uint64_t, uint64_t, std::string>>& groupItems) {
    ResultVoid result;
    std::string err;

    std::unordered_set<uint64_t> ids_seen;

    for (const auto& item : groupItems) {
        if (std::get<0>(item) == 0) {
            // id 为0，表示新建分组
            CIM::dao::ContactGroup new_group;
            new_group.user_id = user_id;
            new_group.name = std::get<2>(item);
            new_group.sort = std::get<1>(item);
            new_group.created_at = TimeUtil::NowToS();
            if (!CIM::dao::ContactGroupDAO::Create(new_group, new_group.id, &err)) {
                CIM_LOG_ERROR(g_logger) << "SaveContactGroup failed, user_id=" << user_id
                                        << ", id=" << std::get<0>(item) << ", err=" << err;
                result.code = 500;
                result.err = "保存联系人分组失败！";
                return result;
            }
            ids_seen.insert(new_group.id);  // 记录已见ID
        } else {
            ids_seen.insert(std::get<0>(item));  // 记录已见ID

            // id 不为0，表示更新分组
            if (!CIM::dao::ContactGroupDAO::Update(std::get<0>(item), std::get<1>(item),
                                                   std::get<2>(item), &err)) {
                CIM_LOG_ERROR(g_logger) << "SaveContactGroup failed, user_id=" << user_id
                                        << ", id=" << std::get<0>(item) << ", err=" << err;
                result.code = 500;
                result.err = "保存联系人分组失败！";
                return result;
            }
        }
    }

    // 查询用户现有的分组列表
    std::vector<CIM::dao::ContactGroupItem> existing_groups;
    if (!CIM::dao::ContactGroupDAO::ListByUserId(user_id, existing_groups, &err)) {
        CIM_LOG_ERROR(g_logger) << "SaveContactGroup ListByUserId failed, user_id=" << user_id
                                << ", err=" << err;
        result.code = 500;
        result.err = "保存联系人分组失败！";
        return result;
    }

    // 删除不在传入列表中的分组
    for (const auto& group : existing_groups) {
        if (ids_seen.find(group.id) == ids_seen.end()) {
            // 分组ID不在已见ID集合中，执行删除

            // 将分组中的联系人移除
            if (!CIM::dao::ContactDAO::RemoveFromGroupByGroupId(user_id, group.id, &err)) {
                CIM_LOG_ERROR(g_logger)
                    << "SaveContactGroup RemoveFromGroupByGroupId failed, user_id=" << user_id
                    << ", id=" << group.id << ", err=" << err;
                result.code = 500;
                result.err = "保存联系人分组失败！";
                return result;
            }

            // 删除分组
            if (!CIM::dao::ContactGroupDAO::Delete(group.id, &err)) {
                CIM_LOG_ERROR(g_logger) << "SaveContactGroup Delete failed, user_id=" << user_id
                                        << ", id=" << group.id << ", err=" << err;
                result.code = 500;
                result.err = "保存联系人分组失败！";
                return result;
            }
        }
    }

    result.ok = true;
    return result;
}

ContactGroupListResult ContactService::GetContactGroupLists(const uint64_t user_id) {
    ContactGroupListResult result;
    std::string err;

    if (!CIM::dao::ContactGroupDAO::ListByUserId(user_id, result.data, &err)) {
        CIM_LOG_ERROR(g_logger) << "ListContactGroups failed, user_id=" << user_id
                                << ", err=" << err;
        result.code = 500;
        result.err = "获取联系人分组列表失败！";
        return result;
    }
    result.ok = true;
    return result;
}

ResultVoid ContactService::ChangeContactGroup(const uint64_t user_id, const uint64_t contact_id,
                                              const uint64_t group_id) {
    ResultVoid result;
    std::string err;

    // 查询好友原先的分组
    uint64_t old_group_id = 0;
    if (!CIM::dao::ContactDAO::GetOldGroupId(user_id, contact_id, old_group_id, &err)) {
        CIM_LOG_ERROR(g_logger) << "ChangeContactGroup GetGroup failed, contact_id=" << contact_id
                                << ", err=" << err;
        result.code = 500;
        result.err = "获取联系人分组失败！";
        return result;
    }

    // 修改联系人分组
    if (!CIM::dao::ContactDAO::ChangeGroup(user_id, contact_id, group_id, &err)) {
        CIM_LOG_ERROR(g_logger) << "ChangeContactGroup failed, contact_id=" << contact_id
                                << ", group_id=" << group_id << ", err=" << err;
        result.code = 500;
        result.err = "修改联系人分组失败！";
        return result;
    }

    if (old_group_id != 0) {
        // 原先分组下的联系人数量 -1
        if (!CIM::dao::ContactGroupDAO::UpdateContactCount(old_group_id, false, &err)) {
            CIM_LOG_ERROR(g_logger)
                << "ChangeContactGroup UpdateContactCount failed, contact_id=" << contact_id
                << ", group_id=" << old_group_id << ", err=" << err;
            result.code = 500;
            result.err = "修改联系人分组失败！";
            return result;
        }
    }

    // 当前分组下的联系人数量 +1
    if (!CIM::dao::ContactGroupDAO::UpdateContactCount(group_id, true, &err)) {
        CIM_LOG_ERROR(g_logger) << "ChangeContactGroup UpdateContactCount failed, contact_id="
                                << contact_id << ", group_id=" << group_id << ", err=" << err;
        result.code = 500;
        result.err = "修改联系人分组失败！";
        return result;
    }

    result.ok = true;
    return result;
}

}  // namespace CIM::app