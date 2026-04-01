# TinyIM V5

当前版本只实现 V5：在 V4 的在线态基础上，实现单聊与离线消息。

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
- 新增单聊消息类型：`send`
- 目标用户在线时实时投递
- 目标用户离线时写入 SQLite
- 用户登录成功后自动拉取并下发未投递离线消息
- 离线消息表包含 `msg_id / from_user / to_user / body / ts / delivered`

## 不在本版本内的内容

- `epoll`
- 线程池
- ack 机制

## 协议格式

- frame = `4` 字节长度前缀 + JSON body
- 长度前缀为网络字节序 `uint32_t`
- 当前 V5 只支持最小 JSON 子集：
- 顶层对象
- key 为字符串
- value 暂时只支持字符串
- 注册请求：`{"type":"register","username":"alice","password":"123456"}`
- 登录请求：`{"type":"login","username":"alice","password":"123456"}`
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
./build/tinyim_v5
```

## 验收

1. 启动服务端。
2. 发送 `register` 请求，确认返回成功响应。
3. A、B 都登录，A 发送 `{"type":"send","to":"bob","text":"hello"}`，确认 B 实时收到 `recv` 消息。
4. 让 B 离线，A 再发 `send` 给 B，确认 A 收到“已入离线”或等价成功响应。
5. 执行 `sqlite3 tinyim_v5.db 'select msg_id,from_user,to_user,body,ts,delivered from offline_messages;'`，确认有离线记录。
6. B 登录，确认服务端自动下发 `offline_msg`。
7. 再次查询数据库，确认对应记录的 `delivered` 已变成 `1`。
8. 重复登录 B，不应重复收到已经 `delivered` 的离线消息。
