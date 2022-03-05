
# C++/git powered 商城

cmall 是全球唯一一款使用 C++ 代码开发的在线商城. 并使用 git 进行商品管理.

## 快速上手
### 编译 

需要使用支持 c++20 的编译器, 推荐使用最新版的 clang, 并使用 libc++ 而不是 libstdc++ 的 标准库.

```bash
mkdir build && cd build && cmake -G Ninja .. && ninja 
```

### 运行 

```bash
systemctl start cmall
```

或者直接运行

```
./bin/cmall --ws "[::]:80"
```

## 设计思路

cmall 由前端驱动. 前端只需要向 cmall 维持单一的一条长连接即可. 前端使用长连接, 必然就是 websocket 了.

cmall 的前端连接服务器后, 立即进行恢复session的操作. 如果无 session 可恢复, 服务端会返回一个新的 session. 前端需要在本地使用 localstorage 持久化 session token.

所有的API操作, 都需要 session 上下文环境. 由于有 session 恢复机制, 因此 websocket 连接中断可以保留会话信息.




# FAQ

## 为什么使用 C++

因为 C++ 非常优秀, 能减少维护成本.

