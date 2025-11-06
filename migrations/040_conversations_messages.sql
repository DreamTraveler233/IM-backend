-- 会话表：存储私聊和群聊的会话信息
CREATE TABLE IF NOT EXISTS conversations (
  id              BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 会话ID，自增主键
  talk_mode       TINYINT UNSIGNED NOT NULL, -- 会话模式：1私聊 2群聊
  conversation_key VARCHAR(64) NOT NULL, -- 会话键：例如 1:uid1:uid2 或 2:group_id
  single_user_low BIGINT UNSIGNED, -- 私聊用户ID（较小者）
  single_user_high BIGINT UNSIGNED, -- 私聊用户ID（较大者）
  group_id        BIGINT UNSIGNED, -- 群聊ID（仅群聊时使用）
  created_at      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 创建时间
  UNIQUE KEY uq_conversation_key (conversation_key), -- 唯一键：会话键
  KEY idx_conversation_mode (talk_mode), -- 模式索引
  CONSTRAINT fk_conversation_group FOREIGN KEY (group_id) REFERENCES `groups` (id) ON DELETE CASCADE -- 群聊外键
) ENGINE=InnoDB;

-- 会话会话表：用户与会话的关联，存储用户的会话设置和状态
CREATE TABLE IF NOT EXISTS talk_sessions (
  id               BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 会话记录ID，自增主键
  user_id          BIGINT UNSIGNED NOT NULL, -- 用户ID
  conversation_id  BIGINT UNSIGNED NOT NULL, -- 会话ID
  talk_mode        TINYINT UNSIGNED NOT NULL, -- 会话模式：1私聊 2群聊
  target_user_id   BIGINT UNSIGNED, -- 目标用户ID（私聊时）
  target_group_id  BIGINT UNSIGNED, -- 目标群聊ID（群聊时）
  is_top           TINYINT UNSIGNED NOT NULL DEFAULT 0, -- 是否置顶：0否 1是
  is_disturb       TINYINT UNSIGNED NOT NULL DEFAULT 0, -- 是否免打扰：0否 1是
  is_robot         TINYINT UNSIGNED NOT NULL DEFAULT 0, -- 是否机器人：0否 1是
  unread_count     INT UNSIGNED NOT NULL DEFAULT 0, -- 未读消息数
  last_msg_id      VARCHAR(64), -- 最后消息ID
  last_msg_preview VARCHAR(255), -- 最后消息预览
  last_active_at   DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 最后活跃时间
  created_at       DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 创建时间
  updated_at       DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, -- 更新时间
  UNIQUE KEY uq_session_user_conversation (user_id, conversation_id), -- 唯一键：用户ID+会话ID
  KEY idx_session_unread (user_id, unread_count), -- 未读索引
  CONSTRAINT fk_talk_sessions_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE, -- 用户外键
  CONSTRAINT fk_talk_sessions_conversation FOREIGN KEY (conversation_id) REFERENCES conversations (id) ON DELETE CASCADE, -- 会话外键
  CONSTRAINT fk_talk_sessions_target_user FOREIGN KEY (target_user_id) REFERENCES users (id) ON DELETE SET NULL, -- 目标用户外键
  CONSTRAINT fk_talk_sessions_target_group FOREIGN KEY (target_group_id) REFERENCES `groups` (id) ON DELETE CASCADE -- 目标群聊外键
) ENGINE=InnoDB;

-- 消息表：存储所有消息内容
CREATE TABLE IF NOT EXISTS messages (
  id               BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 消息ID，自增主键
  conversation_id  BIGINT UNSIGNED NOT NULL, -- 会话ID
  msg_id           VARCHAR(64) NOT NULL, -- 消息唯一ID
  sequence         BIGINT UNSIGNED NOT NULL, -- 消息序列号
  msg_type         INT UNSIGNED NOT NULL, -- 消息类型
  sender_id        BIGINT UNSIGNED NOT NULL, -- 发送者用户ID
  receiver_id      BIGINT UNSIGNED, -- 接收者用户ID（私聊时）
  group_id         BIGINT UNSIGNED, -- 群聊ID（群聊时）
  status           TINYINT UNSIGNED NOT NULL DEFAULT 1, -- 消息状态：1成功 2发送中 3失败
  is_revoked       TINYINT UNSIGNED NOT NULL DEFAULT 0, -- 是否撤回：0否 1是
  quote_msg_id     VARCHAR(64), -- 引用消息ID
  body_json        JSON NOT NULL, -- 消息体JSON
  send_time        DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 发送时间
  updated_at       DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, -- 更新时间
  UNIQUE KEY uq_messages_msg_id (msg_id), -- 唯一键：消息ID
  UNIQUE KEY uq_messages_conversation_seq (conversation_id, sequence), -- 唯一键：会话ID+序列号
  KEY idx_messages_sender (sender_id, send_time), -- 发送者索引
  KEY idx_messages_receiver (receiver_id, send_time), -- 接收者索引
  KEY idx_messages_group (group_id, send_time), -- 群聊索引
  CONSTRAINT fk_messages_conversation FOREIGN KEY (conversation_id) REFERENCES conversations (id) ON DELETE CASCADE, -- 会话外键
  CONSTRAINT fk_messages_sender FOREIGN KEY (sender_id) REFERENCES users (id) ON DELETE CASCADE, -- 发送者外键
  CONSTRAINT fk_messages_receiver FOREIGN KEY (receiver_id) REFERENCES users (id) ON DELETE SET NULL, -- 接收者外键
  CONSTRAINT fk_messages_group FOREIGN KEY (group_id) REFERENCES `groups` (id) ON DELETE CASCADE, -- 群聊外键
  CONSTRAINT fk_messages_quote FOREIGN KEY (quote_msg_id) REFERENCES messages (msg_id) ON DELETE SET NULL -- 引用消息外键
) ENGINE=InnoDB;

-- 消息接收者表：记录每条消息的接收状态
CREATE TABLE IF NOT EXISTS message_recipients (
  id             BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 接收记录ID，自增主键
  msg_id         VARCHAR(64) NOT NULL, -- 消息ID
  user_id        BIGINT UNSIGNED NOT NULL, -- 接收者用户ID
  conversation_id BIGINT UNSIGNED NOT NULL, -- 会话ID
  receive_status TINYINT UNSIGNED NOT NULL DEFAULT 0, -- 接收状态：0未读 1已读
  is_deleted     TINYINT UNSIGNED NOT NULL DEFAULT 0, -- 是否删除：0否 1是
  read_at        DATETIME, -- 阅读时间
  deleted_at     DATETIME, -- 删除时间
  UNIQUE KEY uq_message_recipient (msg_id, user_id), -- 唯一键：消息ID+用户ID
  KEY idx_message_recipient_user (user_id, receive_status), -- 用户状态索引
  CONSTRAINT fk_message_recipient_msg FOREIGN KEY (msg_id) REFERENCES messages (msg_id) ON DELETE CASCADE, -- 消息外键
  CONSTRAINT fk_message_recipient_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE, -- 用户外键
  CONSTRAINT fk_message_recipient_conversation FOREIGN KEY (conversation_id) REFERENCES conversations (id) ON DELETE CASCADE -- 会话外键
) ENGINE=InnoDB;

-- 消息附件表：存储消息的附件信息
CREATE TABLE IF NOT EXISTS message_attachments (
  id            BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 附件ID，自增主键
  msg_id        VARCHAR(64) NOT NULL, -- 消息ID
  file_id       BIGINT UNSIGNED NOT NULL, -- 文件ID
  attachment_type TINYINT UNSIGNED NOT NULL DEFAULT 1, -- 附件类型：1文件 2图片 3音频 4视频
  created_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 创建时间
  UNIQUE KEY uq_message_attachment (msg_id, file_id), -- 唯一键：消息ID+文件ID
  CONSTRAINT fk_message_attachment_msg FOREIGN KEY (msg_id) REFERENCES messages (msg_id) ON DELETE CASCADE, -- 消息外键
  CONSTRAINT fk_message_attachment_file FOREIGN KEY (file_id) REFERENCES storage_files (id) ON DELETE CASCADE -- 文件外键
) ENGINE=InnoDB;

-- 转发消息表：记录消息转发关系
CREATE TABLE IF NOT EXISTS forwarded_messages (
  id             BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 转发记录ID，自增主键
  root_msg_id    VARCHAR(64) NOT NULL, -- 根消息ID
  child_msg_id   VARCHAR(64) NOT NULL, -- 子消息ID
  created_at     DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 创建时间
  UNIQUE KEY uq_forward_relation (root_msg_id, child_msg_id), -- 唯一键：根消息ID+子消息ID
  CONSTRAINT fk_forward_root FOREIGN KEY (root_msg_id) REFERENCES messages (msg_id) ON DELETE CASCADE, -- 根消息外键
  CONSTRAINT fk_forward_child FOREIGN KEY (child_msg_id) REFERENCES messages (msg_id) ON DELETE CASCADE -- 子消息外键
) ENGINE=InnoDB;

-- 混合消息项表：存储混合消息的子项
CREATE TABLE IF NOT EXISTS mixed_message_items (
  id            BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 项ID，自增主键
  msg_id        VARCHAR(64) NOT NULL, -- 消息ID
  item_order    INT UNSIGNED NOT NULL DEFAULT 1, -- 项顺序
  item_type     INT UNSIGNED NOT NULL, -- 项类型
  content       TEXT, -- 内容
  link_url      VARCHAR(255), -- 链接URL
  created_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 创建时间
  UNIQUE KEY uq_mixed_item (msg_id, item_order), -- 唯一键：消息ID+顺序
  CONSTRAINT fk_mixed_item_msg FOREIGN KEY (msg_id) REFERENCES messages (msg_id) ON DELETE CASCADE -- 消息外键
) ENGINE=InnoDB;
