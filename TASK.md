# TASK.md

## 当前版本
V5：单聊与离线消息

## 当前目标
在 V4 的注册/登录和在线态基础上，实现单聊与离线消息：

- A -> B
- B 在线时实时投递
- B 离线时写入 SQLite
- B 下次登录后自动拉取 / 下发离线消息

## 必做功能
1. 新增离线消息表 offline_messages
2. 至少包含这些字段：
   - msg_id
   - from_user
   - to_user
   - body
   - ts
   - delivered
3. 新增 MessageDao：
   - InsertOffline(...)
   - ListUndelivered(...)
   - MarkDelivered(...)
4. 新增 OfflineService：
   - 如果目标用户在线，直接投递
   - 如果目标用户不在线，写入离线表
5. 新增单聊消息类型，例如：
   - {"type":"send","to":"bob","text":"hello"}
6. 用户登录成功后，自动检查并下发其未投递离线消息
7. 下发后把消息标记为 delivered

## 建议新增/修改模块
- storage/MessageDao.h
- storage/MessageDao.cpp
- service/OfflineService.h
- service/OfflineService.cpp
- src/server.h
- src/server.cpp
- README.md

## 当前限制
1. 只允许完成 V5
2. 不要实现 V6 及之后内容
3. 不要做 epoll
4. 不要做非阻塞改造
5. 不要做线程池
6. ack 机制如果没把握，可以先不做，只完成 delivered 即可

## 验收标准
1. A、B 都登录，A 发 send 给 B，B 能实时收到
2. B 不登录，A 发 send 给 B，服务端返回“已入离线”或等价成功响应
3. 执行数据库查询，确认 offline_messages 中有记录
4. B 登录后，服务端自动下发离线消息
5. 下发后数据库记录被标记 delivered
6. 重复登录 B，不应重复收到已经 delivered 的离线消息

## 完成后必须输出
1. 修改文件列表
2. 关键实现说明
3. 编译命令
4. 运行命令
5. 验收步骤
6. 当前版本故意没做什么