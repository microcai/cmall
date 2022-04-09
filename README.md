
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

源码里携带的 site.zip 为已经编写好的一个示例前端. 建议有能力的用户自己编写一套更符合需求的前端页面. 如果使用自己的前端页面, 可以替换 site.zip 并重新编译,
也可以使用 nginx 想用户提供前端页面, 并转发 websocket 到 cmall

```
root 自定义的cmall前端页面目录

location / {
    try_files $uri $uri/ /index.html;
}

location /api {
    proxy_pass_header Server;
    proxy_pass http://cmall;
    proxy_http_version 1.1;

    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection $connection_upgrade;

    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto $scheme;
}

location /repos {
    proxy_pass_header Server;
    proxy_pass http://cmall;
    proxy_http_version 1.1;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto $scheme;
}

location /goods {
    proxy_pass_header Server;
    proxy_pass http://cmall;
    proxy_http_version 1.1;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto $scheme;
}

location /scriptcallback {
    proxy_pass_header Server;
    proxy_pass http://cmall;
    proxy_http_version 1.1;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto $scheme;
}
```

这里是一个使用 nginx 托管前端页面并转发 api 请求到 cmall 的配置片段.

## 设计思路

### 前端 SPA 设计
cmall 的前端设计为 SPA. 同时API调用只需要向 cmall 维持单一的一条长连接即可. 前端使用长连接, 必然就是 websocket 了.

所有的API操作, 都需要 session 上下文环境. 由于有 cookie 可以恢复 session, 因此 websocket 连接中断可以保留会话信息.

前端的接口为 oor (Out-of-Order-RPC), 编写前端可以使用 [cmallsdk](https://git.codinge.cn/cmall/cmallsdk).

### 店铺管理

对店铺的商品管理, cmall 使用 git 进行. 商品的描述编写为一系列的 md 文件. md文件和其引用的 图片等资源文件一起纳入 git 进行版本控制.

具体的 git 仓库细节, 可以看文档 [doc/repos.md](doc/repos.md)


# FAQ

## 为什么使用 C++

因为 C++ 非常优秀, 能减少维护成本.

