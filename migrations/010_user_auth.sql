-- 010_user_auth.sql
-- 用户 & 认证 & 验证相关表

-- 用户主档
CREATE TABLE IF NOT EXISTS `im_user` (
  `id` BIGINT UNSIGNED NOT NULL COMMENT '用户ID（雪花ID/分布式ID）',
  `mobile` VARCHAR(20) NOT NULL COMMENT '手机号（唯一，用于登录/找回）',
  `email` VARCHAR(100) NULL COMMENT '邮箱（唯一，可选，用于找回/通知）',
  `nickname` VARCHAR(64) NOT NULL COMMENT '昵称',
  `avatar` VARCHAR(255) NULL COMMENT '头像URL',
  `gender` TINYINT NOT NULL DEFAULT 0 COMMENT '性别：0=未知 1=男 2=女',
  `motto` VARCHAR(255) NULL COMMENT '个性签名',
  `birthday` DATE NULL COMMENT '生日 YYYY-MM-DD',
  `online_status` VARCHAR(1) NOT NULL DEFAULT 'N' COMMENT '在线状态：N:离线 Y:在线',
  `last_online_at` DATETIME NULL COMMENT '最后在线时间',
  `is_qiye` TINYINT NOT NULL DEFAULT 0 COMMENT '是否企业用户：0=否 1=是（用于 UserSettingResponse.user_info）',
  `is_robot` TINYINT NOT NULL DEFAULT 0 COMMENT '是否机器人账号：0=否 1=是',
  `is_disabled` TINYINT NOT NULL DEFAULT 0 COMMENT '是否禁用：0=否 1=是（冻结/封禁）',
  `created_at` DATETIME NOT NULL COMMENT '创建时间',
  `updated_at` DATETIME NOT NULL COMMENT '更新时间',
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_mobile` (`mobile`),
  UNIQUE KEY `uk_email` (`email`),
  KEY `idx_nickname` (`nickname`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户主档';

-- 本地账号密码
CREATE TABLE IF NOT EXISTS `im_user_auth` (
  `user_id` BIGINT UNSIGNED NOT NULL COMMENT '用户ID（im_user.id）',
  `password_hash` VARCHAR(255) NOT NULL COMMENT '密码哈希（bcrypt/argon2等）',
  `password_algo` VARCHAR(255) NOT NULL DEFAULT 'PBKDF2-HMAC-SHA256' COMMENT '哈希算法（bcrypt/argon2等）',
  `password_version` SMALLINT NOT NULL DEFAULT 1 COMMENT '密码版本（升级算法时便于区分）',
  `last_reset_at` DATETIME NULL COMMENT '最近重置时间（找回/修改）',
  `created_at` DATETIME NOT NULL COMMENT '创建时间',
  `updated_at` DATETIME NOT NULL COMMENT '更新时间',
  PRIMARY KEY (`user_id`),
  CONSTRAINT `fk_auth_user` FOREIGN KEY (`user_id`) REFERENCES `im_user` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户本地认证信息（密码等）';

-- 用户个性化设置（与前端类型对齐）
CREATE TABLE IF NOT EXISTS `im_user_setting` (
  `user_id` BIGINT UNSIGNED NOT NULL COMMENT '用户ID（im_user.id）',
  `theme_mode` VARCHAR(16) NOT NULL DEFAULT 'auto' COMMENT '主题模式：light/dark/auto',
  `theme_bag_img` VARCHAR(255) NOT NULL DEFAULT '' COMMENT '聊天背景图片URL',
  `theme_color` VARCHAR(32) NOT NULL DEFAULT '' COMMENT '主题主色调（#RRGGBB）',
  `notify_cue_tone` TINYINT NOT NULL DEFAULT 1 COMMENT '通知提示音开关：1=开 0=关（前端接受Y/N或true/false，接口转换）',
  `keyboard_event_notify` TINYINT NOT NULL DEFAULT 1 COMMENT '键盘事件通知：1=开 0=关（接口转换）',
  `created_at` DATETIME NOT NULL COMMENT '创建时间',
  `updated_at` DATETIME NOT NULL COMMENT '更新时间',
  PRIMARY KEY (`user_id`),
  CONSTRAINT `fk_setting_user` FOREIGN KEY (`user_id`) REFERENCES `im_user` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户个性化设置';

-- 登录日志（驱动登录提醒消息；便于风控）
CREATE TABLE IF NOT EXISTS `im_auth_login_log` (
  `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '主键ID',
  `user_id` BIGINT UNSIGNED NULL COMMENT '用户ID（登录前失败场景可能为空）',
  `mobile` VARCHAR(20) NULL COMMENT '尝试登录的手机号（便于失败审计）',
  `platform` VARCHAR(32) NOT NULL COMMENT '登录平台（来自请求）',
  `ip` VARCHAR(45) NOT NULL COMMENT '登录IP（IPv4/IPv6）',
  `address` VARCHAR(128) NULL COMMENT 'IP归属地（解析后）',
  `user_agent` VARCHAR(255) NULL COMMENT 'UA/设备指纹',
  `success` TINYINT NOT NULL DEFAULT 0 COMMENT '是否成功：1=成功 0=失败',
  `reason` VARCHAR(128) NULL COMMENT '失败原因（密码错误/验证码错误/账号禁用等）',
  `created_at` DATETIME NOT NULL COMMENT '记录时间',
  PRIMARY KEY (`id`),
  KEY `idx_user_time` (`user_id`,`created_at`),
  KEY `idx_mobile_time` (`mobile`,`created_at`),
  CONSTRAINT `fk_loginlog_user` FOREIGN KEY (`user_id`) REFERENCES `im_user` (`id`) ON DELETE SET NULL ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='认证登录日志';

-- 验证码（短信/邮箱）统一表
CREATE TABLE IF NOT EXISTS `im_verify_code` (
  `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '主键ID',
  `channel` TINYINT NOT NULL COMMENT '渠道：1=短信 2=邮箱',
  `scene` VARCHAR(32) NOT NULL COMMENT '场景：register/login/forget/update_mobile/update_email/oauth_bind 等',
  `target` VARCHAR(100) NOT NULL COMMENT '目标：手机号或邮箱',
  `code` VARCHAR(12) NOT NULL COMMENT '验证码',
  `status` TINYINT NOT NULL DEFAULT 1 COMMENT '状态：1=未使用 2=已使用 3=已过期',
  `attempts` INT NOT NULL DEFAULT 0 COMMENT '校验尝试次数',
  `max_attempts` INT NOT NULL DEFAULT 5 COMMENT '最大允许尝试次数',
  `sent_ip` VARCHAR(45) NULL COMMENT '发送请求IP',
  `sent_at` DATETIME NOT NULL COMMENT '发送时间',
  `expire_at` DATETIME NOT NULL COMMENT '过期时间',
  `used_at` DATETIME NULL COMMENT '使用时间',
  `created_at` DATETIME NOT NULL COMMENT '创建时间',
  PRIMARY KEY (`id`),
  KEY `idx_target_scene` (`target`,`scene`,`status`),
  KEY `idx_expire` (`expire_at`),
  KEY `idx_sent_at` (`sent_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='验证码记录（短信/邮箱统一）';

-- 会话/令牌管理（支持多端登录/注销/吊销）
CREATE TABLE IF NOT EXISTS `im_auth_session` (
  `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '主键ID',
  `user_id` BIGINT UNSIGNED NOT NULL COMMENT '用户ID（im_user.id）',
  `platform` VARCHAR(32) NOT NULL COMMENT '登录平台（web/ios/android/windows/mac 等）',
  `device_id` VARCHAR(64) NULL COMMENT '设备ID/指纹（可选）',
  `jti` VARCHAR(64) NOT NULL COMMENT '访问令牌ID（JWT jti 或自定义会话ID）',
  `token_hash` VARCHAR(255) NOT NULL COMMENT '访问令牌哈希（不存明文）',
  `refresh_token_hash` VARCHAR(255) NULL COMMENT '刷新令牌哈希（如使用刷新流）',
  `client_ip` VARCHAR(45) NULL COMMENT '登录IP',
  `user_agent` VARCHAR(255) NULL COMMENT 'UA/设备指纹',
  `issued_at` DATETIME NOT NULL COMMENT '签发时间',
  `expired_at` DATETIME NOT NULL COMMENT '过期时间',
  `revoked_at` DATETIME NULL COMMENT '吊销时间（注销/下线）',
  `revoke_reason` VARCHAR(128) NULL COMMENT '吊销原因',
  `created_at` DATETIME NOT NULL COMMENT '创建时间',
  `updated_at` DATETIME NOT NULL COMMENT '更新时间',
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_jti` (`jti`),
  KEY `idx_user_platform` (`user_id`,`platform`,`expired_at`),
  KEY `idx_expired` (`expired_at`),
  CONSTRAINT `fk_auth_session_user` FOREIGN KEY (`user_id`) REFERENCES `im_user` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户活跃会话/令牌管理';

-- 第三方登录绑定
CREATE TABLE IF NOT EXISTS `im_user_oauth` (
  `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '主键ID',
  `user_id` BIGINT UNSIGNED NOT NULL COMMENT '用户ID（im_user.id）',
  `provider_type` VARCHAR(32) NOT NULL COMMENT '提供方类型（github/wechat/google 等）',
  `open_id` VARCHAR(128) NOT NULL COMMENT '第三方 open_id（唯一标识）',
  `union_id` VARCHAR(128) NULL COMMENT '第三方 union_id（如微信全局ID）',
  `nickname` VARCHAR(64) NULL COMMENT '第三方昵称快照',
  `avatar` VARCHAR(255) NULL COMMENT '第三方头像快照',
  `access_token` VARCHAR(255) NULL COMMENT '第三方 access_token（可加密存）',
  `refresh_token` VARCHAR(255) NULL COMMENT '第三方 refresh_token（可加密存）',
  `expires_at` DATETIME NULL COMMENT '第三方 token 过期时间',
  `last_login_at` DATETIME NULL COMMENT '最近一次三方登录时间',
  `created_at` DATETIME NOT NULL COMMENT '绑定时间',
  `updated_at` DATETIME NOT NULL COMMENT '更新时间',
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_provider_open` (`provider_type`,`open_id`),
  KEY `idx_user_provider` (`user_id`,`provider_type`),
  KEY `idx_provider_union` (`provider_type`,`union_id`),
  CONSTRAINT `fk_oauth_user` FOREIGN KEY (`user_id`) REFERENCES `im_user` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户第三方账号绑定';

-- OAuth state 与防重放（授权中间态）
CREATE TABLE IF NOT EXISTS `im_oauth_state` (
  `state` VARCHAR(64) NOT NULL COMMENT 'OAuth state（随机串，作为主键）',
  `provider_type` VARCHAR(32) NOT NULL COMMENT '提供方类型',
  `redirect_uri` VARCHAR(255) NULL COMMENT '回跳地址（按需）',
  `created_at` DATETIME NOT NULL COMMENT '创建时间',
  `expire_at` DATETIME NOT NULL COMMENT '过期时间（短期）',
  `used_at` DATETIME NULL COMMENT '使用时间（回调后置位）',
  PRIMARY KEY (`state`),
  KEY `idx_provider_expire` (`provider_type`,`expire_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='OAuth state 暂存（防重放/CSRF）';

-- 未绑定账号的中间票据（bind_token）
CREATE TABLE IF NOT EXISTS `im_oauth_bind_token` (
  `bind_token` VARCHAR(64) NOT NULL COMMENT '绑定票据（返回给前端）',
  `provider_type` VARCHAR(32) NOT NULL COMMENT '提供方类型',
  `open_id` VARCHAR(128) NOT NULL COMMENT '第三方 open_id',
  `union_id` VARCHAR(128) NULL COMMENT '第三方 union_id',
  `profile_snapshot` JSON NULL COMMENT '三方用户信息快照（昵称/头像等）',
  `created_at` DATETIME NOT NULL COMMENT '创建时间',
  `expire_at` DATETIME NOT NULL COMMENT '过期时间（短期）',
  `used` TINYINT NOT NULL DEFAULT 0 COMMENT '是否已使用：0=否 1=是',
  `used_at` DATETIME NULL COMMENT '使用时间（绑定成功后置位）',
  PRIMARY KEY (`bind_token`),
  KEY `idx_provider_open` (`provider_type`,`open_id`),
  KEY `idx_expire` (`expire_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='第三方绑定中间票据（bind_token）';
