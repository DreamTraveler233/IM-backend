-- 文章分类表：用户自定义的文章分类
CREATE TABLE IF NOT EXISTS article_classifies (
  id          BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 分类ID，自增主键
  user_id     BIGINT UNSIGNED NOT NULL, -- 用户ID
  name        VARCHAR(64) NOT NULL, -- 分类名称
  sort        INT UNSIGNED NOT NULL DEFAULT 100, -- 排序权重
  is_default  TINYINT UNSIGNED NOT NULL DEFAULT 0, -- 是否默认分类：0否 1是
  created_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 创建时间
  updated_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, -- 更新时间
  UNIQUE KEY uq_article_classify (user_id, name), -- 唯一键：用户ID+名称
  CONSTRAINT fk_article_classify_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE -- 用户外键
) ENGINE=InnoDB;

-- 文章标签表：用户自定义的文章标签
CREATE TABLE IF NOT EXISTS article_tags (
  id          BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 标签ID，自增主键
  user_id     BIGINT UNSIGNED NOT NULL, -- 用户ID
  tag_name    VARCHAR(64) NOT NULL, -- 标签名称
  sort        INT UNSIGNED NOT NULL DEFAULT 100, -- 排序权重
  created_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 创建时间
  UNIQUE KEY uq_article_tag (user_id, tag_name), -- 唯一键：用户ID+标签名
  CONSTRAINT fk_article_tags_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE -- 用户外键
) ENGINE=InnoDB;

-- 文章表：存储用户的笔记或文章
CREATE TABLE IF NOT EXISTS articles (
  id            BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 文章ID，自增主键
  user_id       BIGINT UNSIGNED NOT NULL, -- 作者用户ID
  classify_id   BIGINT UNSIGNED, -- 分类ID
  title         VARCHAR(128) NOT NULL, -- 文章标题
  abstract      VARCHAR(255), -- 摘要
  md_content    LONGTEXT NOT NULL, -- Markdown内容
  cover_image   VARCHAR(255), -- 封面图片URL
  status        TINYINT UNSIGNED NOT NULL DEFAULT 1, -- 状态：1正常 2草稿 3归档
  is_asterisk   TINYINT UNSIGNED NOT NULL DEFAULT 0, -- 是否星标：0否 1是
  created_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 创建时间
  updated_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, -- 更新时间
  deleted_at    DATETIME, -- 删除时间（软删除）
  KEY idx_articles_classify (classify_id, status), -- 分类索引
  KEY idx_articles_user (user_id, status), -- 用户索引
  FULLTEXT KEY ft_articles_title_content (title, md_content), -- 全文索引：标题和内容
  CONSTRAINT fk_articles_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE, -- 用户外键
  CONSTRAINT fk_articles_classify FOREIGN KEY (classify_id) REFERENCES article_classifies (id) ON DELETE SET NULL -- 分类外键
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  ROW_FORMAT=DYNAMIC;

-- 文章标签关联表：文章与标签的多对多关系
CREATE TABLE IF NOT EXISTS article_tag_relations (
  id          BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 关联ID，自增主键
  article_id  BIGINT UNSIGNED NOT NULL, -- 文章ID
  tag_id      BIGINT UNSIGNED NOT NULL, -- 标签ID
  UNIQUE KEY uq_article_tag_relation (article_id, tag_id), -- 唯一键：文章ID+标签ID
  CONSTRAINT fk_article_tag_rel_article FOREIGN KEY (article_id) REFERENCES articles (id) ON DELETE CASCADE, -- 文章外键
  CONSTRAINT fk_article_tag_rel_tag FOREIGN KEY (tag_id) REFERENCES article_tags (id) ON DELETE CASCADE -- 标签外键
) ENGINE=InnoDB;

-- 文章附件文件表：文章关联的文件
CREATE TABLE IF NOT EXISTS article_annex_files (
  id           BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 附件ID，自增主键
  article_id   BIGINT UNSIGNED NOT NULL, -- 文章ID
  file_id      BIGINT UNSIGNED NOT NULL, -- 文件ID
  created_at   DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 创建时间
  deleted_at   DATETIME, -- 删除时间
  CONSTRAINT fk_article_annex_article FOREIGN KEY (article_id) REFERENCES articles (id) ON DELETE CASCADE, -- 文章外键
  CONSTRAINT fk_article_annex_file FOREIGN KEY (file_id) REFERENCES storage_files (id) ON DELETE CASCADE -- 文件外键
) ENGINE=InnoDB;

-- 文章收藏表：用户收藏的文章
CREATE TABLE IF NOT EXISTS article_favorites (
  id          BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 收藏ID，自增主键
  user_id     BIGINT UNSIGNED NOT NULL, -- 用户ID
  article_id  BIGINT UNSIGNED NOT NULL, -- 文章ID
  created_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 收藏时间
  UNIQUE KEY uq_article_favorite (user_id, article_id), -- 唯一键：用户ID+文章ID
  CONSTRAINT fk_article_fav_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE, -- 用户外键
  CONSTRAINT fk_article_fav_article FOREIGN KEY (article_id) REFERENCES articles (id) ON DELETE CASCADE -- 文章外键
) ENGINE=InnoDB;

-- 文章回收日志表：记录文章的删除和恢复操作
CREATE TABLE IF NOT EXISTS article_recycle_logs (
  id           BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 日志ID，自增主键
  article_id   BIGINT UNSIGNED NOT NULL, -- 文章ID
  user_id      BIGINT UNSIGNED NOT NULL, -- 操作用户ID
  deleted_at   DATETIME NOT NULL, -- 删除时间
  restored_at  DATETIME, -- 恢复时间
  action       TINYINT UNSIGNED NOT NULL DEFAULT 1, -- 操作类型：1删除 2恢复 3彻底删除
  CONSTRAINT fk_article_recycle_article FOREIGN KEY (article_id) REFERENCES articles (id) ON DELETE CASCADE, -- 文章外键
  CONSTRAINT fk_article_recycle_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE -- 用户外键
) ENGINE=InnoDB;
