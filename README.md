# TinyIM V4

当前版本只实现 V4：在 V3 的注册/登录基础上，引入连接状态机和在线 SessionManager。

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

## 不在本版本内的内容

- `epoll`
- 线程池
- 单聊 / 离线消息

## 协议格式

- frame = `4` 字节长度前缀 + JSON body
- 长度前缀为网络字节序 `uint32_t`
- 当前 V4 只支持最小 JSON 子集：
- 顶层对象
- key 为字符串
- value 暂时只支持字符串
- 注册请求：`{"type":"register","username":"alice","password":"123456"}`
- 登录请求：`{"type":"login","username":"alice","password":"123456"}`
- 业务消息示例：`{"type":"chat","text":"hello"}`
- 只有登录成功后的连接才允许发送业务消息

## 编译

```bash
cmake -S . -B build
cmake --build build
```

## 运行

```bash
./build/tinyim_v4
```

## 验收

1. 启动服务端。
2. 发送 `register` 请求，确认返回成功响应。
3. 发送未登录业务消息，例如 `{"type":"chat","text":"hello"}`，确认被拒绝。
4. 发送正确密码的 `login` 请求，确认返回成功响应。
5. 登录后再发送业务消息，确认允许继续广播。
6. 用第二个客户端再次登录同一账号，确认返回“用户已在线，当前策略为拒绝新登录”。
7. 断开已登录客户端后，再次登录同一账号，确认可以重新登录。
