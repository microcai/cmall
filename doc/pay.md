

# 收款方法
商户如果要收款, 就必须要在仓库的 scripts/ 文件夹下存放一个 getpayurl.js 的文件. 该文件必须是 nodejs 脚本, 但是不能引用任何 node_modules.
可以使用工具将 node 脚本打包为一个 js 文件.

对每个用户点击了"去付款"按钮的订单,getpayurl.js都会被调用. 因为用户可能重复点击 "去支付" 因此同一个订单号会被多次传递给 getpayurl.js, getpayurl.js 可以自己决定是否重用已经获取的支付链接, 还是每次点击都生成新的.

链接生成后, cmall 的 app 客户端会直接打开链接, 如果链接的格式为 alipayqr:// 或者 wxpay:// 则 支付宝或者微信的 app 就会被手机的操作系统自动唤醒.
也可以使用其他支付app的唤醒链接. 甚至是一个网页版本的支付页面也是没问题的.

# 确认支付
cmall 系统提供确认支付的 RPC 接口, 和相对应的管理页面. 商户既可以选择人工点击确认, 也可以使用脚本调用 rpc 确认.

# 实现细节

cmall 运行脚本的时候, 会在一个沙盒环境运行 js 文件, 这个环境将不存在任何文件, 无法执行任何文件读写操作. 但是可以访问白名单设定的网络.

* 网络白名单功能为后期的安全加固, 实现方式为基于 BPF 的系统调用过滤.目前不实现

* 文件沙盒功能靠 chroot 到一个空白目录实现

* chroot 的实现方式是先给 nodejs 发送一个 chroot 代码, 然后再发送 getpayurl.js

* 如果系统没有安装 nodejs , cmall 会调用内置的 js 引擎执行 getpayurl.js 脚本, 内置的引擎将失去绝大部分 node 支持的功能, 而只保留最简单的 js 函数.

```javascript
require('posix').chroot('sandbox');
process.chdir('/');
process.setgid(65535);
process.setuid(65535);
```

这个脚本会首先发送给 nodejs 进程的 stdin

接着, cmall 读取 git 仓库, 获取到 getpayurl.js 文件, 并将 getpayurl.js 文件写入 nodejs 进程的 stdin.

然后等待 nodejs 的 stdout 上输出获取到的 url.

nodejs 的进程会被允许最多 10s 的运行时间. 超过则被杀死, 用户无法支付.

