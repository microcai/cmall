
API 分成 6 组, 未特殊说明, api 都是以 jsonrpc 在同一个连接里调用.
文档里 id 隐去不提.

# 浏览组

这个组里的 api 不需要授权, 可供游客使用.

功能有, 枚举商户下的商品列表, 获取商品详情, 拉取详情页面, 搜索商品

其中, 拉取详情页面的操作使用 HTTP GET /goods/merchantid/prodid/ 的方式，获取 rtl 格式的描述，不是完整 html 文件, 需要嵌入前端.

```json
{
	method: "user_list_products",
	params: {
		merchant: "0"
	}
}
```
回
```json
{
	result: [
		{
			merchant: 0,
			name : "定位书包",
			pic: "picutureid", // 图片的 ID
			price: "1000.00",
			detial: "0/dingwei_shubao"
		},
		{
			merchant: 0,
			name : "小宝nas",
			pic: "picutureid", // 图片的 ID
			price: "4000.00",
			detial: "0/nas"
		}
	]
}
```

====




# 购物车组

## 添加到购物车[`cart_add`]

## 从购物车删除[`cart_del`]

## 购物车列表[`cart_list`]


# 订单组

## 从商品创建订单[`order_create_direct`]

## 从购物车创建订单[`order_create_cart`]

~~获取支付链接~~

## 查询订单状态[`order_status`]

## 取消订单[`order_close`]

# 用户组


## 登录[`user_login`]

params

```json
{ "type": "b", "phone": "12345678910", "code": "7894125" }
or
{ "type": "c", "phone": "12345678910", "code": "7894125" }
```
type表示作为B端或C端用户登录


## 快速登录[`user_login`]

params

```json
{ "type": "b", "sessid": "Kai1shaexe2Vea6u" }
or
{ "type": "c", "sessid": "Kai1shaexe2Vea6u" }
```

## 查看商品列表[`user_list_products`]

## 申请成为商户[`user_apply_merchant`]

## 注销[`user_logout`]


# 管理组

## 用户列表[`admin_user_list`]

## 商户列表[`admin_merchant_list`]

## 商品列表[`admin_product_list`]

## 禁用用户[`admin_user_ban`]

## 禁用商户[`admin_merchant_ban`]

## 下架商品[`admin_product_withdraw`]

## 强制退款[`admin_order_force_refund`]

# 商户组

## 添加商品/修改[`merchant_product_add`/`merchant_product_mod`]

## 商品上架[`merchant_product_launch`]

## 商品下架[`merchant_product_withdraw`]


## 物流管理
