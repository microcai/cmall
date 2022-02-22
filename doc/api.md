
API 分成 6 组, 未特殊说明, api 都是以 jsonrpc 在同一个连接里调用.
文档里 id 隐去不提.

# 浏览组

这个组里的 api 不需要授权, 可供游客使用.

功能有, 枚举商户下的商品列表, 获取商品详情, 拉取详情页面, 搜索商品

其中, 拉取详情页面的操作使用 HTTP GET /goods/merchantid/prodid/ 的方式，获取 rtl 格式的描述，不是完整 html 文件, 需要嵌入前端.

```json
{
	method: "list_goods",
	merchant: "0"
}
```
回
```json
{
	results : [
		{
			merchant: 0,
			name : "定位书包",
			pic: "picutureid", // 图片的 ID
			price: 1000.00,
			detial: "0/dingwei_shubao"
		},
		{
			merchant: 0,
			name : "小宝nas",
			pic: "picutureid", // 图片的 ID
			price: 4000.00,
			detial: "0/nas"
		}
	]
}
```

====




# 购物车组

添加购物车

删除购物车


# 订单组

从商品创建订单

从购物车创建订单

获取支付链接

查询订单状态

取消订单

# 用户组


登录

快速登录

注销


# 管理组

用户列表

商户列表

商品列表

禁用用户

下架商品

强制退款

# 商户组

添加商品/修改

商品上架

商品下架

物流管理
