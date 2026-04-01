# TASK.md

## 当前版本
V2：引入长度前缀 + JSON 消息体

## 当前目标
在 V1 的基础上，引入应用层协议，解决 TCP 半包/粘包问题。

## 必做功能
1. 定义 framing：
   - 4 字节长度前缀（网络字节序）
   - 后接 JSON 消息体
2. 新增输入缓冲区 inbuf
3. read() 读到的数据只追加到 inbuf
4. 循环解析 inbuf 中的完整 frame
5. 一次 read() 可能得到：
   - 半条消息
   - 一条完整消息
   - 多条消息
   都必须正确处理
6. 对超长包做保护（例如超过 1MB 直接拒绝）
7. 对非法 JSON 做可控处理，不能让服务端崩溃

## 建议新增/修改模块
- protocol/FramingCodec.h
- protocol/FramingCodec.cpp
- protocol/JsonCodec.h
- protocol/JsonCodec.cpp
- src/server.h
- src/server.cpp
- README.md

## 当前限制
1. 只允许完成 V2
2. 不要实现 V3 及之后内容
3. 不要加入 SQLite
4. 不要做登录注册
5. 不要做 SessionManager
6. 不要做 epoll
7. 不要做线程池

## 验收标准
1. 客户端发送一个完整 frame，服务端能正确解析
2. 客户端把一个 frame 分多次发送，服务端仍能正确解析
3. 客户端一次发送两个 frame，服务端能连续解析出两条消息
4. 发送非法 JSON，服务端不崩溃
5. 发送超长长度字段，服务端能拒绝并保持可控行为

## 完成后必须输出
1. 修改文件列表
2. 关键实现说明
3. 编译命令
4. 运行命令
5. 验收步骤
6. 当前版本故意没做什么