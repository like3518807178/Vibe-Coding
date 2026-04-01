cat > docs/roadmap_v0_v8.md <<'EOF'
# TinyIM V0 ~ V8 路线图

## 项目目标
这是一个 Linux + C++17 的 TinyIM 迭代项目。
按版本逐步推进，每一版都必须：
1. 有明确目标
2. 有明确边界
3. 有最小可运行实现
4. 有可执行验收步骤

---

## V0：单连接 Echo
目标：
- 单进程服务端接受 1 个 TCP 连接
- 客户端发什么，服务端回什么
- 客户端断开后服务端不崩溃
- 服务端继续等待下一个连接

关键词：
- socket / bind / listen / accept / read / writeAll

---

## V1：select 多客户端聊天室
目标：
- 多客户端连接
- 广播消息
- 断线清理

关键词：
- select
- fd_set
- maxfd
- 客户端管理

---

## V2：长度前缀 + JSON 协议
目标：
- 正确处理半包/粘包
- framing：4 字节长度前缀 + JSON body
- Connection 增加输入缓冲

关键词：
- framing
- inbuf
- tryPopFrame
- JSON

---

## V3：注册 / 登录 + SQLite
目标：
- 用户注册登录
- SQLite 落地
- 重启后用户仍存在

关键词：
- SqliteDB
- UserDao
- prepare / bind / step / finalize

---

## V4：SessionManager 与在线态
目标：
- 连接状态机
- user <-> fd 双向映射
- 重复登录策略

关键词：
- SessionManager
- state
- bindUser / unbind

---

## V5：单聊 + 离线消息
目标：
- 在线直投
- 离线落库
- 登录后拉取离线消息

关键词：
- MessageDao
- OfflineService
- msg_id
- ack

---

## V6：epoll 重构
目标：
- select 迁移到单线程 epoll
- 非阻塞 socket
- outbuf + EPOLLOUT

关键词：
- epoll
- O_NONBLOCK
- EPOLLIN / EPOLLOUT
- EAGAIN

---

## V7：心跳与超时回收
目标：
- heartbeat
- 超时踢下线
- timerfd 集成 epoll

关键词：
- timerfd
- last_active_ms
- idle_timeout

---

## V8：日志与配置工程化
目标：
- Logger
- Config
- 错误可观测
- 启动配置可打印

关键词：
- INFO / ERROR
- config
- db_path
- idle_timeout
- max_packet_size

---

## 重要规则
1. Codex 当前只能完成 TASK.md 指定的版本。
2. 不允许为了“后续扩展”提前实现后面版本。
3. 必须优先保证当前版本可编译、可运行、可验收。
EOF