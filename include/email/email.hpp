#ifndef __CIM_EMAIL_EMAIL_HPP__
#define __CIM_EMAIL_EMAIL_HPP__

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace CIM {

/**
 * @brief 邮件附件实体类
 * @details 该类用于表示邮件中的附件实体，包括附件的头部信息和内容
 */
class EMailEntity {
   public:
    /// 智能指针类型定义
    typedef std::shared_ptr<EMailEntity> ptr;

    /**
     * @brief 创建附件实体
     * @param[in] filename 附件文件路径
     * @return 附件实体智能指针
     */
    static EMailEntity::ptr CreateAttach(const std::string& filename);

    /**
     * @brief 添加头部信息
     * @param[in] key 头部键名
     * @param[in] val 头部值
     */
    void addHeader(const std::string& key, const std::string& val);

    /**
     * @brief 获取头部信息
     * @param[in] key 头部键名
     * @param[in] def 默认值
     * @return 头部值，如果不存在则返回默认值
     */
    std::string getHeader(const std::string& key, const std::string& def = "");

    /**
     * @brief 获取附件内容
     * @return 附件内容
     */
    const std::string& getContent() const { return m_content; }

    /**
     * @brief 设置附件内容
     * @param[in] v 附件内容
     */
    void setContent(const std::string& v) { m_content = v; }

    /**
     * @brief 转换为字符串表示
     * @return 字符串表示的附件实体
     */
    std::string toString() const;

   private:
    std::map<std::string, std::string> m_headers;  /// 头部信息映射表
    std::string m_content;                         /// 附件内容
};

/**
 * @brief 邮件类
 * @details 该类用于表示一封完整的邮件，包括发件人、收件人、主题、正文等信息
 */
class EMail {
   public:
    /// 智能指针类型定义
    typedef std::shared_ptr<EMail> ptr;

    /**
     * @brief 创建邮件对象
     * @param[in] from_address 发件人邮箱地址
     * @param[in] from_passwd 发件人邮箱密码
     * @param[in] title 邮件标题
     * @param[in] body 邮件正文
     * @param[in] to_address 收件人邮箱地址列表
     * @param[in] cc_address 抄送邮箱地址列表
     * @param[in] bcc_address 密送邮箱地址列表
     * @return 邮件对象智能指针
     */
    static EMail::ptr Create(const std::string& from_address, const std::string& from_passwd,
                             const std::string& title, const std::string& body,
                             const std::vector<std::string>& to_address,
                             const std::vector<std::string>& cc_address = {},
                             const std::vector<std::string>& bcc_address = {});

    /**
     * @brief 获取发件人邮箱地址
     * @return 发件人邮箱地址
     */
    const std::string& getFromEMailAddress() const { return m_fromEMailAddress; }

    /**
     * @brief 获取发件人邮箱密码
     * @return 发件人邮箱密码
     */
    const std::string& getFromEMailPasswd() const { return m_fromEMailPasswd; }

    /**
     * @brief 获取邮件标题
     * @return 邮件标题
     */
    const std::string& getTitle() const { return m_title; }

    /**
     * @brief 获取邮件正文
     * @return 邮件正文
     */
    const std::string& getBody() const { return m_body; }

    /**
     * @brief 设置发件人邮箱地址
     * @param[in] v 发件人邮箱地址
     */
    void setFromEMailAddress(const std::string& v) { m_fromEMailAddress = v; }

    /**
     * @brief 设置发件人邮箱密码
     * @param[in] v 发件人邮箱密码
     */
    void setFromEMailPasswd(const std::string& v) { m_fromEMailPasswd = v; }

    /**
     * @brief 设置邮件标题
     * @param[in] v 邮件标题
     */
    void setTitle(const std::string& v) { m_title = v; }

    /**
     * @brief 设置邮件正文
     * @param[in] v 邮件正文
     */
    void setBody(const std::string& v) { m_body = v; }

    /**
     * @brief 获取收件人邮箱地址列表
     * @return 收件人邮箱地址列表
     */
    const std::vector<std::string>& getToEMailAddress() const { return m_toEMailAddress; }

    /**
     * @brief 获取抄送邮箱地址列表
     * @return 抄送邮箱地址列表
     */
    const std::vector<std::string>& getCcEMailAddress() const { return m_ccEMailAddress; }

    /**
     * @brief 获取密送邮箱地址列表
     * @return 密送邮箱地址列表
     */
    const std::vector<std::string>& getBccEMailAddress() const { return m_bccEMailAddress; }

    /**
     * @brief 设置收件人邮箱地址列表
     * @param[in] v 收件人邮箱地址列表
     */
    void setToEMailAddress(const std::vector<std::string>& v) { m_toEMailAddress = v; }

    /**
     * @brief 设置抄送邮箱地址列表
     * @param[in] v 抄送邮箱地址列表
     */
    void setCcEMailAddress(const std::vector<std::string>& v) { m_ccEMailAddress = v; }

    /**
     * @brief 设置密送邮箱地址列表
     * @param[in] v 密送邮箱地址列表
     */
    void setBccEMailAddress(const std::vector<std::string>& v) { m_bccEMailAddress = v; }

    /**
     * @brief 添加附件实体
     * @param[in] entity 附件实体
     */
    void addEntity(EMailEntity::ptr entity);

    /**
     * @brief 获取附件实体列表
     * @return 附件实体列表
     */
    const std::vector<EMailEntity::ptr>& getEntitys() const { return m_entitys; }

   private:
    std::string m_fromEMailAddress;              /// 发件人邮箱地址
    std::string m_fromEMailPasswd;               /// 发件人邮箱密码
    std::string m_title;                         /// 邮件标题
    std::string m_body;                          /// 邮件正文
    std::vector<std::string> m_toEMailAddress;   /// 收件人邮箱地址列表
    std::vector<std::string> m_ccEMailAddress;   /// 抄送邮箱地址列表
    std::vector<std::string> m_bccEMailAddress;  /// 密送邮箱地址列表
    std::vector<EMailEntity::ptr> m_entitys;     /// 附件实体列表
};

}  // namespace CIM

#endif