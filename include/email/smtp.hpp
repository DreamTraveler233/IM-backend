#ifndef __CIM_EMAIL_SMTP_HPP__
#define __CIM_EMAIL_SMTP_HPP__

#include <sstream>

#include "email.hpp"
#include "streams/socket_stream.hpp"

namespace CIM {

/**
 * @brief SMTP结果类
 * @details 用于封装SMTP操作的结果状态和消息
 */
struct SmtpResult {
    /// 智能指针类型定义
    typedef std::shared_ptr<SmtpResult> ptr;

    /// 结果枚举
    enum Result {
        OK = 0,        /// 操作成功
        IO_ERROR = -1  /// IO错误
    };

    /**
     * @brief 构造函数
     * @param[in] r 结果码
     * @param[in] m 消息内容
     */
    SmtpResult(int r, const std::string& m) : result(r), msg(m) {}

    int result;       /// 结果码
    std::string msg;  /// 消息内容
};

/**
 * @brief SMTP客户端类
 * @details 用于通过SMTP协议发送邮件的客户端实现
 */
class SmtpClient : public SocketStream {
   public:
    /// 智能指针类型定义
    typedef std::shared_ptr<SmtpClient> ptr;

    /**
     * @brief 创建SMTP客户端
     * @param[in] host SMTP服务器主机地址
     * @param[in] port SMTP服务器端口
     * @param[in] ssl 是否使用SSL加密连接
     * @return SMTP客户端智能指针
     */
    static SmtpClient::ptr Create(const std::string& host, uint32_t port, bool ssl = false);

    /**
     * @brief 发送邮件
     * @param[in] email 邮件对象
     * @param[in] timeout_ms 超时时间（毫秒）
     * @param[in] debug 是否开启调试模式
     * @return SMTP操作结果
     */
    SmtpResult::ptr send(EMail::ptr email, int64_t timeout_ms = 1000, bool debug = false);

    /**
     * @brief 获取调试信息
     * @return 调试信息字符串
     */
    std::string getDebugInfo();

   private:
    /**
     * @brief 执行SMTP命令
     * @param[in] cmd SMTP命令
     * @param[in] debug 是否开启调试模式
     * @return SMTP操作结果
     */
    SmtpResult::ptr doCmd(const std::string& cmd, bool debug);

   private:
    /**
     * @brief 构造函数
     * @param[in] sock 套接字对象
     */
    SmtpClient(Socket::ptr sock);

   private:
    std::string m_host;      /// SMTP服务器主机地址
    std::stringstream m_ss;  /// 调试信息流
    bool m_authed = false;   /// 是否已认证
};

}  // namespace CIM

#endif