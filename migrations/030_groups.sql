-- 群聊表：存储群聊的基本信息，包括群名称、头像、简介、类型、成员限制等
CREATE TABLE IF NOT EXISTS `groups` (
  id            BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 群聊ID，自增主键
  name          VARCHAR(64) NOT NULL, -- 群聊名称
  avatar        VARCHAR(255), -- 群聊头像URL
  profile       VARCHAR(512), -- 群聊简介
  type          TINYINT UNSIGNED NOT NULL DEFAULT 1, -- 群聊类型：1私有 2公开
  max_members   INT UNSIGNED NOT NULL DEFAULT 200, -- 最大成员数
  creator_id    BIGINT UNSIGNED NOT NULL, -- 创建者用户ID
  owner_id      BIGINT UNSIGNED NOT NULL, -- 群主用户ID
  is_mute       TINYINT UNSIGNED NOT NULL DEFAULT 0, -- 是否全员禁言：0否 1是
  mute_until    DATETIME, -- 禁言结束时间
  is_overt      TINYINT UNSIGNED NOT NULL DEFAULT 0, -- 是否公开可见：0否 1是
  visit_card    VARCHAR(64), -- 群名片
  notice_id     BIGINT UNSIGNED, -- 最新公告ID
  status        TINYINT UNSIGNED NOT NULL DEFAULT 1, -- 群聊状态：1正常 2解散
  created_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 创建时间
  updated_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, -- 更新时间
  KEY idx_groups_owner (owner_id), -- 群主索引
  KEY idx_groups_status (status, created_at), -- 状态和创建时间索引
  CONSTRAINT fk_groups_creator FOREIGN KEY (creator_id) REFERENCES users (id) ON DELETE CASCADE, -- 创建者外键
  CONSTRAINT fk_groups_owner FOREIGN KEY (owner_id) REFERENCES users (id) ON DELETE CASCADE -- 群主外键
) ENGINE=InnoDB;

-- 群成员表：记录群聊成员信息，包括角色、权限等
CREATE TABLE IF NOT EXISTS group_members (
  id            BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 成员记录ID，自增主键
  group_id      BIGINT UNSIGNED NOT NULL, -- 群聊ID
  user_id       BIGINT UNSIGNED NOT NULL, -- 用户ID
  role          TINYINT UNSIGNED NOT NULL DEFAULT 3, -- 角色：1群主 2管理员 3成员
  is_mute       TINYINT UNSIGNED NOT NULL DEFAULT 0, -- 是否禁言：0否 1是
  mute_until    DATETIME, -- 禁言结束时间
  remark        VARCHAR(64), -- 备注
  alias         VARCHAR(64), -- 别名
  joined_at     DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 加入时间
  last_seen_seq BIGINT UNSIGNED NOT NULL DEFAULT 0, -- 最后看到的序列号
  status        TINYINT UNSIGNED NOT NULL DEFAULT 1, -- 状态：1正常 2退出
  UNIQUE KEY uq_group_member (group_id, user_id), -- 唯一键：群ID+用户ID
  KEY idx_group_member_role (group_id, role), -- 角色索引
  CONSTRAINT fk_group_members_group FOREIGN KEY (group_id) REFERENCES `groups` (id) ON DELETE CASCADE, -- 群聊外键
  CONSTRAINT fk_group_members_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE -- 用户外键
) ENGINE=InnoDB;

-- 群公告表：存储群聊公告内容
CREATE TABLE IF NOT EXISTS group_notices (
  id            BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 公告ID，自增主键
  group_id      BIGINT UNSIGNED NOT NULL, -- 群聊ID
  content       TEXT NOT NULL, -- 公告内容
  author_id     BIGINT UNSIGNED, -- 作者用户ID
  created_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 创建时间
  updated_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, -- 更新时间
  CONSTRAINT fk_group_notices_group FOREIGN KEY (group_id) REFERENCES `groups` (id) ON DELETE CASCADE, -- 群聊外键
  CONSTRAINT fk_group_notices_author FOREIGN KEY (author_id) REFERENCES users (id) ON DELETE SET NULL -- 作者外键
) ENGINE=InnoDB;

-- 群加入申请表：记录用户申请加入群聊的请求
CREATE TABLE IF NOT EXISTS group_join_requests (
  id            BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 申请ID，自增主键
  group_id      BIGINT UNSIGNED NOT NULL, -- 群聊ID
  applicant_id  BIGINT UNSIGNED NOT NULL, -- 申请人用户ID
  remark        VARCHAR(255), -- 申请备注
  status        TINYINT UNSIGNED NOT NULL DEFAULT 0, -- 申请状态：0待审核 1同意 2拒绝
  handler_id    BIGINT UNSIGNED, -- 处理人用户ID
  handle_remark VARCHAR(255), -- 处理备注
  handled_at    DATETIME, -- 处理时间
  created_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 创建时间
  KEY idx_group_apply_status (group_id, status, created_at), -- 状态索引
  KEY idx_group_apply_applicant (applicant_id, created_at), -- 申请人索引
  CONSTRAINT fk_group_join_group FOREIGN KEY (group_id) REFERENCES `groups` (id) ON DELETE CASCADE, -- 群聊外键
  CONSTRAINT fk_group_join_applicant FOREIGN KEY (applicant_id) REFERENCES users (id) ON DELETE CASCADE, -- 申请人外键
  CONSTRAINT fk_group_join_handler FOREIGN KEY (handler_id) REFERENCES users (id) ON DELETE SET NULL -- 处理人外键
) ENGINE=InnoDB;

-- 群邀请表：记录群聊邀请信息
CREATE TABLE IF NOT EXISTS group_invitations (
  id            BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 邀请ID，自增主键
  group_id      BIGINT UNSIGNED NOT NULL, -- 群聊ID
  inviter_id    BIGINT UNSIGNED NOT NULL, -- 邀请人用户ID
  invitee_id    BIGINT UNSIGNED NOT NULL, -- 被邀请人用户ID
  status        TINYINT UNSIGNED NOT NULL DEFAULT 0, -- 邀请状态：0待接受 1已加入 2拒绝
  expire_at     DATETIME, -- 过期时间
  created_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 创建时间
  KEY idx_group_invite_status (group_id, status), -- 状态索引
  CONSTRAINT fk_group_invite_group FOREIGN KEY (group_id) REFERENCES `groups` (id) ON DELETE CASCADE, -- 群聊外键
  CONSTRAINT fk_group_invite_inviter FOREIGN KEY (inviter_id) REFERENCES users (id) ON DELETE CASCADE, -- 邀请人外键
  CONSTRAINT fk_group_invite_invitee FOREIGN KEY (invitee_id) REFERENCES users (id) ON DELETE CASCADE -- 被邀请人外键
) ENGINE=InnoDB;

-- 群投票表：存储群聊投票信息
CREATE TABLE IF NOT EXISTS group_votes (
  id             BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 投票ID，自增主键
  group_id       BIGINT UNSIGNED NOT NULL, -- 群聊ID
  title          VARCHAR(128) NOT NULL, -- 投票标题
  answer_mode    TINYINT UNSIGNED NOT NULL DEFAULT 1, -- 回答模式：1单选 2多选
  is_anonymous   TINYINT UNSIGNED NOT NULL DEFAULT 0, -- 是否匿名：0否 1是
  created_by     BIGINT UNSIGNED NOT NULL, -- 创建者用户ID
  status         TINYINT UNSIGNED NOT NULL DEFAULT 1, -- 投票状态：1进行中 2结束
  created_at     DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 创建时间
  closed_at      DATETIME, -- 结束时间
  CONSTRAINT fk_group_votes_group FOREIGN KEY (group_id) REFERENCES `groups` (id) ON DELETE CASCADE, -- 群聊外键
  CONSTRAINT fk_group_votes_creator FOREIGN KEY (created_by) REFERENCES users (id) ON DELETE CASCADE -- 创建者外键
) ENGINE=InnoDB;

-- 群投票选项表：存储投票的选项
CREATE TABLE IF NOT EXISTS group_vote_options (
  id         BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 选项ID，自增主键
  vote_id    BIGINT UNSIGNED NOT NULL, -- 投票ID
  option_key VARCHAR(32) NOT NULL, -- 选项键
  content    VARCHAR(255) NOT NULL, -- 选项内容
  sort       INT UNSIGNED NOT NULL DEFAULT 100, -- 排序
  UNIQUE KEY uq_group_vote_option (vote_id, option_key), -- 唯一键：投票ID+选项键
  CONSTRAINT fk_group_vote_options_vote FOREIGN KEY (vote_id) REFERENCES group_votes (id) ON DELETE CASCADE -- 投票外键
) ENGINE=InnoDB;

-- 群投票记录表：记录用户的投票选择
CREATE TABLE IF NOT EXISTS group_vote_records (
  id          BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 记录ID，自增主键
  vote_id     BIGINT UNSIGNED NOT NULL, -- 投票ID
  user_id     BIGINT UNSIGNED NOT NULL, -- 用户ID
  option_key  VARCHAR(32) NOT NULL, -- 选择的选项键
  answer_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 回答时间
  UNIQUE KEY uq_group_vote_record (vote_id, user_id, option_key), -- 唯一键：投票ID+用户ID+选项键
  CONSTRAINT fk_group_vote_record_vote FOREIGN KEY (vote_id) REFERENCES group_votes (id) ON DELETE CASCADE, -- 投票外键
  CONSTRAINT fk_group_vote_record_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE -- 用户外键
) ENGINE=InnoDB;
