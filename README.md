# TinyIM V3

当前版本只实现 V3：在 V2 协议基础上，引入 SQLite 用户持久化和 `register/login`。

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

## 不在本版本内的内容

- SessionManager
- `epoll`
- 线程池
- 在线态
- 单聊 / 离线消息

## 协议格式

- frame = `4` 字节长度前缀 + JSON body
- 长度前缀为网络字节序 `uint32_t`
- 当前 V3 只支持最小 JSON 子集：
- 顶层对象
- key 为字符串
- value 暂时只支持字符串
- 注册请求：`{"type":"register","username":"alice","password":"123456"}`
- 登录请求：`{"type":"login","username":"alice","password":"123456"}`
- 普通消息仍可继续广播

## 编译

```bash
cmake -S . -B build
cmake --build build
```

## 运行

```bash
./build/tinyim_v3
```

## 验收

1. 启动服务端。
2. 发送 `register` 请求，确认返回成功响应。
3. 使用 `sqlite3 tinyim_v3.db 'select username from users;'` 确认表中有记录。
4. 重复注册相同用户，确认返回“用户已存在”。
5. 发送正确密码的 `login` 请求，确认返回成功响应。
6. 发送错误密码的 `login` 请求，确认返回失败响应且连接不断开。
7. 重启服务端后再次 `login`，确认已注册用户仍然存在。
