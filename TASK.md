# TASK.md

## 当前版本
V6：从 select 迁移到单线程 epoll

## 当前目标
在保持 V2~V5 协议和业务行为基本不变的前提下，把服务端网络层从 select 重构为单线程 epoll。

## 必做功能
1. 使用 epoll_create1 / epoll_ctl / epoll_wait 重写事件循环
2. 所有 socket 改为非阻塞
3. accept() 循环处理，直到返回 EAGAIN / EWOULDBLOCK
4. 读事件中持续 read()，直到返回 EAGAIN / EWOULDBLOCK
5. 保留 V2 的 inbuf + framing 解析逻辑
6. 新增 outbuf
7. write() 写不完时，把剩余数据留在 outbuf
8. 有待发送数据时注册 EPOLLOUT
9. outbuf 清空后移除 EPOLLOUT
10. 保持 V3~V5 的 register/login/session/send/offline 行为仍然可用

## 建议新增/修改模块
- net/EpollReactor.h
- net/EpollReactor.cpp
- src/server.h
- src/server.cpp
- README.md

## 当前限制
1. 只允许完成 V6
2. 不要实现 V7 及之后内容
3. 不要做 timerfd
4. 不要做心跳
5. 不要做 signalfd
6. 不要做线程池
7. 不要改动数据库 schema
8. 不要扩展新业务协议

## 验收标准
1. 服务端可正常启动并接受多个客户端连接
2. register/login/session/send/offline 的原有行为保持可用
3. 半包 / 粘包处理仍然正确
4. 慢客户端存在时，服务端不会因同步阻塞写而卡死
5. 写缓冲区清空后，EPOLLOUT 会被正确移除
6. 客户端断开后，fd 和在线态能被正确清理

## 完成后必须输出
1. 修改文件列表
2. 关键实现说明
3. 编译命令
4. 运行命令
5. 验收步骤
6. 当前版本故意没做什么