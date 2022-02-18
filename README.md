
# C++/git powered 商城

cmall 是全球唯一一款使用 C++ 代码开发的在线商城. 并使用 git 进行商品管理.

# 编译 

需要使用支持 c++20 的编译器, 推荐使用最新版的 clang, 并使用 libc++ 而不是 libstdc++ 的 标准库.

```bash
mkdir build && cd build && cmake -G Ninja .. && ninja 
```

# 运行 

```bash
systemctl start cmall
```

或者直接运行

```
./bin/cmall --ws "[::]:80"
```

# FAQ

## 为什么使用 C++

因为 C++ 非常优秀, 能减少维护成本.

