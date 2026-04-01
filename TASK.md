# TASK.md

## 当前版本
V3：注册 / 登录 + SQLite 落地用户

## 当前目标
先检查必要组件与环境，组件与环境完整就在 V2 协议基础上，引入用户系统和 SQLite 持久化。
若组件与环境不完整就直接告知用户，让用户安装好在开始

## 必做功能
1. 引入 SQLite 连接层
2. 服务端启动时自动创建 users 表
3. 支持 register 消息
4. 支持 login 消息
5. 重复注册返回明确错误
6. 登录失败不影响连接稳定
7. 服务端重启后用户仍存在
8. 设置 sqlite3_busy_timeout

## 建议新增/修改模块
- storage/SqliteDB.h
- storage/SqliteDB.cpp
- storage/UserDao.h
- storage/UserDao.cpp
- service/AuthService.h
- service/AuthService.cpp
- src/server.h
- src/server.cpp
- README.md

## 当前限制
1. 只允许完成 V3
2. 不要实现 V4 及之后内容
3. 不要做 SessionManager
4. 不要做在线态
5. 不要做单聊 / 离线消息
6. 不要做 epoll
7. 不要做线程池

## 验收标准
1. register 成功后，users 表有记录
2. 重复注册，返回“用户已存在”
3. login 成功后返回成功响应
4. 错误密码登录失败，但连接不断
5. 服务端重启后，已注册用户仍可登录

## 完成后必须输出
1. 修改文件列表
2. 关键实现说明
3. 编译命令
4. 运行命令
5. 验收步骤
6. 当前版本故意没做什么