# TASK.md

## 当前版本
V8：日志与配置模块化

## 当前目标
在 V7 的基础上，引入最小 Logger 和最小 Config，使服务端具备基本可观测性和可配置性。

## 必做功能
1. 新增 Logger，至少支持：
   - INFO
   - ERROR
2. 关键路径必须打日志：
   - 新连接建立
   - 登录成功 / 失败
   - register 成功 / 失败
   - send 路由
   - 离线落库
   - 登录后离线补发
   - heartbeat / 超时踢人
   - SQLite 错误
3. 新增 Config，至少支持读取：
   - port
   - db_path
   - max_packet_size
   - idle_timeout
   - log_level
4. 启动时打印当前生效配置
5. 服务端运行时使用 Config 中的端口、DB 路径、包长、超时参数
6. 保持 V3~V7 的主链仍可用

## 建议新增/修改模块
- common/Logger.h
- common/Logger.cpp
- common/Config.h
- common/Config.cpp
- src/main.cpp
- src/server.h
- src/server.cpp
- README.md

## 当前限制
1. 只允许完成 V8
2. 不要再扩展新业务功能
3. 不要做线程池
4. 不要做 signalfd
5. WAL 可以作为可选项，但不是必须
6. 不要大改现有协议和数据库 schema

## 验收标准
1. 启动服务端时打印生效配置
2. register/login/send/offline/heartbeat/timeout 主链仍可用
3. 故意发送非法包时，有 ERROR 日志
4. DB 打开失败或 SQL 出错时，有 ERROR 日志
5. 修改配置文件中的端口、db_path、idle_timeout 后，重启服务端生效
6. README 明确写清配置项与日志范围

## 完成后必须输出
1. 修改文件列表
2. 关键实现说明
3. 编译命令
4. 运行命令
5. 验收步骤
6. 当前版本故意没做什么