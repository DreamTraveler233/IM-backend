## MySQL 使用说明

此文档说明工程中封装的 MySQL 客户端接口，位于 `include/db/mysql.hpp` 与实现文件 `src/db/mysql.cpp`。内容包括配置、连接、查询、预处理语句、事务、结果遍历、错误处理、连接池、时间处理与常见注意事项，并提供若干 C++ 使用示例。

> 约定：本库中对索引大多为 0 基（例如查询结果列索引），请参考示例。

---

## 一、概述

仓库提供了一个对 MySQL C API（libmysqlclient）的轻量封装：

- `CIM::MySQL`：表示单个连接封装，提供 `execute` / `query` / `prepare` / `openTransaction` 等方法。
- `CIM::MySQLStmt`：准备语句（prepared statement），支持多种类型绑定并执行。
- `CIM::MySQLRes`：普通 SQL 查询返回（`mysql_store_result`）的封装，支持 `next()` 与按类型读取列。
- `CIM::MySQLStmtRes`：Prepared Statement 的结果封装（绑定返回列缓冲并 fetch）。
- `CIM::MySQLManager`：连接池管理器（按名字分配连接），并封装库初始化与回收。
- `CIM::MySQLUtil`：工具静态方法，快捷访问 `MySQLManager` 的查询/执行接口。

该封装会在 `MySQLManager` 构造时调用 `mysql_library_init`，并在析构时调用 `mysql_library_end`。

## 二、配置

连接配置来源：`Config::Lookup("mysql.dbs", ...)`（见 `mysql.cpp` 顶部：`g_mysql_dbs`）。

每个数据库节点的参数为一个 map，通常支持的键：

- `host`：主机名或 IP
- `port`：端口（整数字符串）
- `user`：用户名
- `passwd`：密码
- `dbname`：默认数据库名（可选，连接时也可切换）
- `pool`：连接池大小（可选，默认 5）

示例：在配置（yaml/json/代码）中应类似：

```
mysql:
  dbs:
    default:
      host: 127.0.0.1
      port: 3306
      user: root
      passwd: pwd
      dbname: mydb
      pool: 8
```

## 三、获取连接

通常使用单例管理器：

- `MySQLMgr::GetInstance()->get("name")`：获取一个 `MySQL::ptr`（智能指针）。返回的智能指针带有自定义 deleter，当智能指针析构时对象会被归还到 `MySQLManager` 的连接池（或被销毁，如果池已满）。

示例：

```cpp
auto db = MySQLMgr::GetInstance()->get("default");
if(!db) {
    // 获取失败：可能配置缺失或连接失败
}
```

注意：`MySQL::connect()` 内部由 `mysql_init` 和 `mysql_real_connect` 实现；如果连接处于错误状态会标记 `m_hasError`。

## 四、直接执行 SQL（非预处理）

方法：

- `int MySQL::execute(const char* fmt, ...)` / `int MySQL::execute(const std::string &sql)`：执行不返回结果的 SQL（例如 INSERT/UPDATE/DELETE）。返回 0 表示成功。
- `ISQLData::ptr MySQL::query(const char* fmt, ...)` / `query(std::string)`：执行 SELECT 风格查询并返回 `MySQLRes` 封装。

示例：

```cpp
auto db = MySQLMgr::GetInstance()->get("default");
if(db) {
    // execute
    int r = db->execute("INSERT INTO t(a,b) VALUES('%s', %d)", "abc", 123);

    // query
    auto res = db->query("SELECT id, name FROM t WHERE id = %d", 1);
    while(res && res->next()){
        int id = res->getInt32(0);
        std::string name = res->getString(1);
        // ...
    }
}
```

也可以使用 `MySQLManager` 的封装：

```cpp
MySQLMgr::GetInstance()->execute("default", "UPDATE ...");
auto rows = MySQLMgr::GetInstance()->query("default", "SELECT ...");
```

或工具类快捷调用 `MySQLUtil::Query/Execute`。

## 五、预处理语句（Prepared Statement）

使用 `MySQL::prepare(sql)` 或 `MySQLStmt::Create` 创建 `MySQLStmt`。封装支持按索引绑定多种类型：

绑定函数（示例）：

- `bindInt32 / bindUint32 / bindInt64 / bindString / bindBlob / bindTime / bindNull` 等
- 也可以使用通用 `bind(int idx, T)` 重载（内部会分派到具体 bindXXX）。

示例：插入示例

```cpp
auto db = MySQLMgr::GetInstance()->get("default");
auto st = db->prepare("INSERT INTO user(name, age, created) VALUES(?, ?, ?)");
st->bindString(1, std::string("alice"));
st->bindInt32(2, 30);
st->bindTime(3, time(nullptr));
int rt = st->execute();
if(rt != 0) {
    // 失败，可通过 st->getErrNo / getErrStr 获取错误信息
}
```

模板辅助（便捷调用）:

- `MySQL::execStmt(const char* stmt, Args&&... args)`：内部会创建 `MySQLStmt` 并通过模板 `bindX` 递归绑定参数，然后执行。
- `MySQL::queryStmt`：同理但返回 `ISQLData::ptr`（`MySQLStmtRes`）。

示例使用模板接口：

```cpp
int r = db->execStmt("INSERT INTO t(a,b) VALUES(?,?)", "hello", 10);
auto res = db->queryStmt("SELECT id, a FROM t WHERE b>?", 5);
```

注意：模板绑定支持的类型见 `include/db/mysql.hpp` 中的 `XX(...)` 宏扩展：`char*`, `const char*`, `std::string`, `int8_t/uint8_t/.../int64_t/uint64_t`, `float`, `double`。

## 六、Prepared Statement 返回结果

当使用 `MySQLStmt::query()` 时，会返回 `MySQLStmtRes`：

- 在 `Create` 过程中，会调用 `mysql_stmt_result_metadata` 获取字段信息并为每个字段分配缓冲（由 `MySQLStmtRes::Data::alloc` 分配）。
- 结果读取通过 `next()`（内部调用 `mysql_stmt_fetch`）和 `getInt32/getString/getTime` 等方法。

注意：`MySQLStmtRes::Data::alloc` 在分配缓冲后将 `length` 初始化为 0（驱动在 fetch 后回填真实长度），避免把 buffer 的容量误当作有效长度。

示例：

```cpp
auto st = db->prepare("SELECT id, name FROM user WHERE age>? ");
st->bindInt32(1, 20);
auto res = st->query();
while(res && res->next()){
    int id = res->getInt32(0);
    std::string name = res->getString(1);
}
```

## 七、事务

通过 `MySQLManager::openTransaction(name, auto_commit)` 获取 `MySQLTransaction`：

- `auto_commit = true`：在 `MySQLTransaction` 析构时会尝试 `commit()`。
- `auto_commit = false`：析构时会尝试 `rollback()`（因此若你想在析构时自动提交，请传 `true`）。

示例：

```cpp
auto trans = MySQLMgr::GetInstance()->openTransaction("default", false);
if(!trans) { /* error */ }
if(trans->execute("INSERT ...") != 0) {
    trans->rollback();
} else {
    trans->commit();
}
```

注意：如果事务已结束（commit/rollback），再次调用 `execute` 会返回错误并在日志中记录。

## 八、结果读取细节

- `MySQLRes`（普通查询）提供 `next()`、`getInt32/getString/getTime` 等方法，底层基于 `mysql_store_result` 与 `mysql_fetch_row`。
- `MySQLRes::foreach` 提供回调遍历行，每行含字段数与行号。回调返回 `false` 可提前中止。
- 字符串/二进制数据读取应使用 `getColumnBytes` / `getBlob` 等，以避免截断或包含额外的 \0。

示例遍历：

```cpp
auto res = db->query("SELECT id, data FROM t");
res->foreach([](MYSQL_ROW row, int field_count, int row_no){
    // 注意：该回调是 mysql_fetch_row 返回的原始数据接口
    return true; // 返回 false 则停止 foreach
});

while(res && res->next()){
    // 使用 getXXX
}
```

## 九、错误处理与日志

- 对于 `MySQL` 和 `MySQLStmt`，可使用 `getErrno()` 和 `getErrStr()` 获取最后错误代码与字符串。
- 出错时库会在日志（`system` logger）输出相关信息（例如 mysql_error 或 mysql_stmt_error）。

## 十、连接池与线程安全

- `MySQLManager` 管理以名字分类的连接池，返回 `MySQL::ptr` 时使用自定义 deleter 将连接归还池。池大小由每个 MySQL 对象的 `pool` 参数控制。
- 当获取连接后会检测 `isNeedCheck()` 与 `ping()`，如果需要会尝试重连。
- 在多线程环境中，请使用 `MySQLManager` 统一分配连接。内部在 `mysql_init` 使用了 `mysql_thread_init()`（线程局部），而 `MySQLManager` 在全局构造时调用 `mysql_library_init`。

## 十一、时间类型处理

- 提供 `mysql_time_to_time_t` 与 `time_t_to_mysql_time` 辅助函数，但 `MySQLStmt::bindTime` 当前以字符串形式绑定（`TimeUtil::TimeToStr`）。
- `MySQLStmtRes::getTime` 会把 `MYSQL_TIME` 转换为 `time_t`（使用 `mysql_time_to_time_t`）。

## 十二、类型绑定对照表（常用）

- MYSQL_TYPE_TINY -> int8_t / uint8_t
- MYSQL_TYPE_SHORT -> int16_t / uint16_t
- MYSQL_TYPE_LONG -> int32_t / uint32_t
- MYSQL_TYPE_LONGLONG -> int64_t / uint64_t
- MYSQL_TYPE_FLOAT -> float
- MYSQL_TYPE_DOUBLE -> double
- 其它（字符串/二进制）-> 分配字段长度作为缓冲

## 十三、常见问题与注意事项

- Prepared Statement 的结果缓冲会在 metadata 获取阶段分配固定容量，实际有效长度由 MySQL 在 fetch 时通过 `length` 回填。
- `MySQLStmt::bindTime` 目前将时间以字符串绑定，请确保数据库字段类型与字符串格式兼容（或扩展为直接绑定 `MYSQL_TIME`）。
- 当大量短连接操作时，优先使用 `MySQLManager` 的连接池以减少连接/握手开销。
- 请注意本库对索引的基准（大部分方法以 0 为列索引起点）。

## 十四、完整示例

示例 1：简单查询

```cpp
auto db = MySQLMgr::GetInstance()->get("default");
if(!db) return;
auto res = db->query("SELECT id, name, created FROM user WHERE id=%d", 1);
while(res && res->next()){
    int id = res->getInt32(0);
    std::string name = res->getString(1);
    time_t t = res->getTime(2);
}
```

示例 2：预处理插入

```cpp
auto db = MySQLMgr::GetInstance()->get("default");
if(!db) return;
int r = db->execStmt("INSERT INTO user(name, age) VALUES(?, ?)", std::string("bob"), 28);
if(r != 0) {
    // 处理错误
}
```

示例 3：事务

```cpp
auto trans = MySQLMgr::GetInstance()->openTransaction("default", false);
if(!trans) return;
if(trans->execute("UPDATE account SET balance=balance-100 WHERE id=1") != 0) {
    trans->rollback();
} else if(trans->execute("UPDATE account SET balance=balance+100 WHERE id=2") != 0) {
    trans->rollback();
} else {
    trans->commit();
}
```

## 十五、参考文件

- 实现：`src/db/mysql.cpp`
- 头文件：`include/db/mysql.hpp`

---

文档结束。如需把 `bindTime` 改为直接绑定 `MYSQL_TIME` 或增加更多类型支持（例如 JSON），可在 `MySQLStmt` 中添加对应 bind 重载并在 `MySQLStmt::Create` / `MySQLStmtRes::Create` 中调整元信息分配逻辑。
