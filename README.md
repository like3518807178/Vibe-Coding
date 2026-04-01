# TinyIM V7

当前版本只实现 V7：在 V6 单线程 `epoll` 的基础上，引入 `heartbeat`、`last_active`、`idle_timeout` 和 `timerfd` 超时回收。

## 功能范围

- 监听 `7777` 端口
- 支持多个客户端同时连接
- 每个客户端都有输入缓冲区 `inbuf`
- 使用 `4` 字节网络字节序长度前缀解决半包/粘包
- 一次 `read()` 可正确处理半条、一条或多条 frame
- 启动时自动创建 `users` 表
- 支持 `register` / `login`
- 设置了 `sqlite3_busy_timeout`
- 服务端重启后，已注册用户仍可登录
- 为每个连接维护 `Connected / Authed / Closed` 状态
- 维护 `user -> fd` 和 `fd -> user` 双向映射
- 未登录用户不能发送业务消息
- 重复登录策略：拒绝新登录，保留旧连接
- 支持单聊消息类型 `send`
- 目标用户在线时实时投递
- 目标用户离线时写入 SQLite
- 用户登录成功后自动拉取并下发未投递离线消息
- 网络事件循环使用单线程 `epoll`
- 所有 socket 均为非阻塞
- 每个连接维护 `outbuf`
- 写不完的数据会留在 `outbuf` 中，并通过 `EPOLLOUT` 继续发送
- `outbuf` 清空后会移除 `EPOLLOUT`
- 每个连接维护 `last_active_ms`
- 成功收到客户端数据后会刷新 `last_active_ms`
- 成功处理 `heartbeat` 后会再次刷新 `last_active_ms`
- 新增 `heartbeat` 消息类型
- 服务端通过 `timerfd_create` / `timerfd_settime` 创建周期定时器
- `timerfd` 已集成进 `epoll`
- 定时扫描超时连接，超过 `idle_timeout=5s` 会主动关闭连接
- 超时关闭时会清理在线态和 `SessionManager` 映射

## 不在本版本内的内容

- `signalfd`
- 线程池
- ack 机制
- 日志系统
- 配置系统
- V8 及之后内容

## 协议格式

- frame = `4` 字节长度前缀 + JSON body
- 长度前缀为网络字节序 `uint32_t`
- 当前只支持最小 JSON 子集
- 顶层对象
- key 为字符串
- value 暂时只支持字符串
- 注册请求：`{"type":"register","username":"alice","password":"123456"}`
- 登录请求：`{"type":"login","username":"alice","password":"123456"}`
- 心跳请求：`{"type":"heartbeat","ts":"1710000000"}`
- 心跳响应：`{"type":"heartbeat_resp","message":"pong","ok":"true"}`
- 单聊请求：`{"type":"send","to":"bob","text":"hello"}`
- 实时接收消息：`{"type":"recv","from":"alice","to":"bob","text":"hello","ts":"..."}`
- 离线补发消息：`{"type":"offline_msg","msg_id":"1","from":"alice","to":"bob","text":"hello","ts":"..."}`
- 只有登录成功后的连接才允许发送业务消息

## 编译

```bash
cmake -S . -B build
cmake --build build
```

## 运行

```bash
./build/tinyim_v7
```

## 验收

1. 启动服务端。
2. 发送 `register` 请求，确认返回成功响应。
3. 发送 `login` 请求，确认返回 `login_resp`。
4. 连续每 `1s` 发送一次 `heartbeat`，确认服务端持续返回 `heartbeat_resp`，连接不会因空闲超时被回收。
5. 停止发送 `heartbeat`，等待约 `6~8s`，确认服务端主动关闭连接。
6. 使用同一账号重新登录，确认在线态已清理，能够重新登录成功。
7. A、B 都登录，A 发送 `{"type":"send","to":"bob","text":"hello"}`，确认 B 实时收到 `recv` 消息。
8. 让 B 离线，A 再发 `send` 给 B，确认 A 收到“已入离线”或等价成功响应。
9. 执行 `sqlite3 tinyim_v7.db 'select msg_id,from_user,to_user,body,ts,delivered from offline_messages;'`，确认有离线记录。
10. B 登录，确认服务端自动下发 `offline_msg`，并且对应记录被标记为已投递。
