-- 用户自定义表情表：存储用户上传或设置的表情包
CREATE TABLE IF NOT EXISTS user_emoticons (
  id          BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 表情ID，自增主键
  user_id     BIGINT UNSIGNED NOT NULL, -- 用户ID
  file_id     BIGINT UNSIGNED, -- 文件ID（本地上传）
  remote_url  VARCHAR(255), -- 远程URL（网络表情）
  drive       TINYINT UNSIGNED NOT NULL DEFAULT 1, -- 存储驱动：1本地 2远程等
  name        VARCHAR(64), -- 表情名称
  created_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 创建时间
  UNIQUE KEY uq_user_emoticon (user_id, remote_url, file_id), -- 唯一键：用户ID+远程URL+文件ID
  CONSTRAINT fk_user_emoticon_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE, -- 用户外键
  CONSTRAINT fk_user_emoticon_file FOREIGN KEY (file_id) REFERENCES storage_files (id) ON DELETE SET NULL -- 文件外键
) ENGINE=InnoDB;
