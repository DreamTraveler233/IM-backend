#include "app/contact_service.hpp"

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

    // 查询以前是否添加过好友记录
    CIM::dao::Contact contact;
    if (!CIM::dao::ContactDAO::GetByOwnerAndTarget(apply.target_id, apply.applicant_id, contact,
                                                   &err)) {
        if (!err.empty()) {
            CIM_LOG_ERROR(g_logger)
                << "HandleContactApply GetByOwnerAndTarget failed, apply_id=" << apply_id
                << ", err=" << err;
            result.code = 500;
            result.err = "获取好友记录失败！";
            return result;
        }
    }

    // 以前添加过，则为重复添加好友，因为删除好友为软删除，直接修改状态即可
    if (contact.id != 0) {
        if (!CIM::dao::ContactDAO::AddFriend(apply.target_id, apply.applicant_id, &err)) {
            CIM_LOG_ERROR(g_logger)
                << "HandleContactApply AgreeApplyAgain failed, apply_id=" << apply_id
                << ", err=" << err;
            result.code = 500;
            result.err = "处理好友申请失败！";
            return result;
        }
        if (!CIM::dao::ContactDAO::AddFriend(apply.applicant_id, apply.target_id, &err)) {
            CIM_LOG_ERROR(g_logger)
                << "HandleContactApply AgreeApplyAgain failed, apply_id=" << apply_id
                << ", err=" << err;
            result.code = 500;
            result.err = "处理好友申请失败！";
            return result;
        }
        result.ok = true;
        return result;
    }

    // 插入双向联系人记录
    std::vector<CIM::dao::Contact> contacts_to_create;

    // 目标用户添加申请人
    CIM::dao::Contact contact1;
    contact1.user_id = apply.target_id;
    contact1.contact_id = apply.applicant_id;
    contact1.relation = 2;
    contact1.group_id = 0;
    contact1.created_at = TimeUtil::NowToS();
    contact1.status = 1;
    contacts_to_create.push_back(contact1);

    // 申请人添加目标用户
    CIM::dao::Contact contact2;
    contact2.user_id = apply.applicant_id;
    contact2.contact_id = apply.target_id;
    contact2.relation = 2;
    contact2.group_id = 0;
    contact2.created_at = TimeUtil::NowToS();
    contact2.status = 1;
    contacts_to_create.push_back(contact2);

    for (const auto& contact : contacts_to_create) {
        if (!CIM::dao::ContactDAO::Create(contact, &err)) {
            CIM_LOG_ERROR(g_logger)
                << "CreateContactRecordsForApply failed, apply_id=" << apply_id << ", err=" << err;
            result.code = 500;
            result.err = "创建好友记录失败！";
            return result;
        }
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

    result.ok = true;
    return result;
}

}  // namespace CIM::app