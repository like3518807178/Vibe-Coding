# TASK.md

## 当前版本
V7：心跳与超时回收（timerfd 集成 epoll）

## 当前目标
在 V6 的 epoll 基础上，引入 heartbeat、last_active 和超时回收机制。

## 必做功能
1. 为每个连接维护 last_active_ms
2. 收到业务数据或 heartbeat 时更新 last_active_ms
3. 新增 heartbeat 消息类型，例如：
   - {"type":"heartbeat","ts":"..."}
4. 服务端可选回复 pong
5. 引入 timerfd_create / timerfd_settime
6. 把 timerfd 加入 epoll 事件循环
7. 定时扫描连接：
   - 超过 idle_timeout 的连接主动关闭
   - 同时清理在线态和 SessionManager 映射
8. 保持 V3~V6 的 register/login/session/send/offline 行为仍可用

## 建议新增/修改模块
- net/Timer.h
- net/Timer.cpp
- service/Heartbeat.h
- service/Heartbeat.cpp
- src/server.h
- src/server.cpp
- README.md

## 当前限制
1. 只允许完成 V7
2. 不要实现 V8 及之后内容
3. 不要做 signalfd（可选项先不做）
4. 不要做线程池
5. 不要扩展新业务协议
6. 不要改数据库 schema，除非确实必要

## 验收标准
1. idle_timeout=5s 时，客户端不发 heartbeat，会在约 6~8s 内被服务端回收
2. 客户端每 1s 发 heartbeat，不会被回收
3. 停止 heartbeat 后，会在超时后被回收
4. 被回收连接的在线态会被清理
5. 清理后同账号可以重新登录
6. 原有 register/login/send/offline 主链仍可用

## 完成后必须输出
1. 修改文件列表
2. 关键实现说明
3. 编译命令
4. 运行命令
5. 验收步骤
6. 当前版本故意没做什么