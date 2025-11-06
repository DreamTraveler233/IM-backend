-- 企业部门表：存储企业组织架构的部门信息
CREATE TABLE IF NOT EXISTS organize_departments (
  id           BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 部门ID，自增主键
  parent_id    BIGINT UNSIGNED, -- 父部门ID
  dept_name    VARCHAR(128) NOT NULL, -- 部门名称
  ancestors    VARCHAR(255), -- 祖先部门路径（如 1,2,3）
  sort         INT UNSIGNED NOT NULL DEFAULT 100, -- 排序权重
  created_at   DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 创建时间
  updated_at   DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, -- 更新时间
  CONSTRAINT fk_department_parent FOREIGN KEY (parent_id) REFERENCES organize_departments (id) ON DELETE SET NULL -- 父部门外键
) ENGINE=InnoDB;

-- 企业人事表：记录员工与部门的关联
CREATE TABLE IF NOT EXISTS organize_personnel (
  id           BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 人事记录ID，自增主键
  user_id      BIGINT UNSIGNED NOT NULL, -- 用户ID（员工）
  dept_id      BIGINT UNSIGNED NOT NULL, -- 部门ID
  position     VARCHAR(128), -- 职位
  is_leader    TINYINT UNSIGNED NOT NULL DEFAULT 0, -- 是否领导：0否 1是
  joined_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 加入时间
  status       TINYINT UNSIGNED NOT NULL DEFAULT 1, -- 状态：1在职 2离职
  CONSTRAINT fk_personnel_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE, -- 用户外键
  CONSTRAINT fk_personnel_dept FOREIGN KEY (dept_id) REFERENCES organize_departments (id) ON DELETE CASCADE, -- 部门外键
  UNIQUE KEY uq_personnel_user_dept (user_id, dept_id) -- 唯一键：用户ID+部门ID
) ENGINE=InnoDB;
