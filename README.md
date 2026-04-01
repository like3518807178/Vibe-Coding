# TinyIM V8

当前版本只实现 V8：在 V7 的基础上，引入最小 `Logger` 和最小 `Config`，让服务端具备基础日志与配置能力，同时保持 `register / login / session / send / offline / heartbeat / timeout` 主链不变。

## 功能范围

- 保留 V7 的单线程 `epoll + timerfd` 主循环
- 保留 `register / login / send / offline / heartbeat / timeout` 业务链路
- 新增最小 `Logger`
- 支持 `INFO` / `ERROR`
- 新增最小 `Config`
- 启动时读取 `config/server.conf`
- 启动时打印生效配置
- 运行时使用配置项中的 `port / db_path / max_packet_size / idle_timeout / log_level`
- 在连接建立、登录成功/失败、注册成功/失败、路由投递、离线落库、离线补发、heartbeat、超时踢人、SQLite 错误等关键路径打日志

## 配置项

配置文件路径：

```bash
config/server.conf
```

支持的最小配置项：

- `port`：监听端口
- `db_path`：SQLite 数据库文件路径
- `max_packet_size`：协议包体最大长度，单位字节
- `idle_timeout`：空闲超时时间，单位秒
- `log_level`：日志级别，支持 `INFO` / `ERROR`

默认示例：

```ini
port = 7777
db_path = tinyim_v8.db
max_packet_size = 1048576
idle_timeout = 5
log_level = INFO
```

## 日志范围

- 新连接建立
- 客户端断开
- register 成功 / 失败
- login 成功 / 失败
- heartbeat 收到
- send 在线路由
- send 离线路由与离线落库
- 登录后的离线补发
- idle timeout 超时踢人
- 非法包 / 非法 JSON
- SQLite 打开失败、执行失败、离线消息相关数据库失败

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

## 编译

```bash
cmake -S . -B build
cmake --build build
```

## 运行

```bash
./build/tinyim_v8
```

## 验收

1. 启动服务端，确认首先打印生效配置。
2. 修改 `config/server.conf` 中的 `port / db_path / idle_timeout`，重启后确认配置生效。
3. 发送 `register` / `login`，确认主链仍正常，并且日志中能看到成功或失败信息。
4. 登录后每 1 秒发送一次 `heartbeat`，确认仍收到 `heartbeat_resp`，日志中能看到 heartbeat。
5. 停止发送 heartbeat，等待约 `idle_timeout + 1~3s`，确认连接被踢下线，日志中能看到超时关闭。
6. A、B 在线时发送 `send`，确认 B 仍能实时收到消息，日志中能看到在线路由。
7. B 离线时 A 再发送 `send`，确认消息仍入离线库，B 下次登录仍能收到 `offline_msg`，日志中能看到离线落库与补发。
8. 故意发送非法包或超长包，确认服务端有 `ERROR` 日志。
9. 把 `db_path` 改成一个无效目录，例如 `/no_such_dir/tinyim.db`，重启后确认启动失败并输出 `ERROR` 日志。

## 不在本版本内的内容

- 线程池
- `signalfd`
- ack 机制
- JSON 扩展
- 数据库 schema 重构
- 更复杂的日志切分、异步日志、配置热更新
