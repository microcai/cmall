//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <sstream>
#include <iostream>
#include <cstring> // std::memcpy
#include <unordered_map>


#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif

#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

using boost::multiprecision::cpp_int;
using cpp_numeric = boost::multiprecision::cpp_dec_float_100;

#include <odb/section.hxx>
#include <odb/nullable.hxx>

#ifdef _MSC_VER
#	pragma warning (push)
#	pragma warning (disable:4068)
#endif // _MSC_VER

#pragma db model version(8, 21, open)

#pragma db map type("numeric")			\
			as("TEXT")				\
			to("(?)::numeric")		\
			from("(?)::TEXT")

#pragma db map type("cidr")			\
			as("TEXT")			\
			to("(?)::cidr")		\
			from("(?)::TEXT")

#pragma db map type("INTEGER *\\[(\\d*)\\]") \
			as("TEXT")                    \
			to("(?)::INTEGER[$1]")        \
			from("(?)::TEXT")

#pragma db map type("NUMERIC *\\[(\\d*)\\]") \
			as("TEXT")                    \
			to("(?)::NUMERIC[$1]")        \
			from("(?)::TEXT")

#pragma db value(cpp_numeric) type("NUMERIC")
#pragma db value(cpp_int) type("BIGINT")


// 配置表.
#pragma db object
struct cmall_config {
#pragma db id auto
	std::uint64_t id_;
	// long long id_{ -1 };
};

#pragma db value
struct Recipient
{
	std::string name;
	std::string telephone;

	std::string address;

	odb::nullable<std::string> province;
	odb::nullable<std::string> city;
	odb::nullable<std::string> district;

	odb::nullable<std::string> specific_address;

	#pragma db default(false)
	bool as_default;
};

// 用户表.
#pragma db object pointer(std::shared_ptr)
struct cmall_user
{
#pragma db id auto
	std::uint64_t uid_;
	// long long id_{ -1 };

#pragma db index
	std::string name_; // 姓名.

#pragma db index
	// 手机号
	std::string active_phone;

	std::vector<std::string> used_phones;

	// 收货地址管理.
	std::vector<Recipient> recipients;

	bool verified_{ false }; // 是否验证通过.

	uint8_t state_{ 0 }; // 状态, 正常/停用/封禁.

	odb::nullable<std::string> desc_;

	boost::posix_time::ptime created_at_{ boost::posix_time::second_clock::local_time() };
	boost::posix_time::ptime updated_at_{ boost::posix_time::second_clock::local_time() };
	odb::nullable<boost::posix_time::ptime> deleted_at_;
};

#pragma db object
struct administrators
{
	#pragma db id auto
	std::uint64_t uid_;
	std::shared_ptr<cmall_user> user;
};

enum class merchant_state_t
{
	normal = 0,
	disabled, // 停用.
	banned, // 封禁.
};

#pragma db object
struct cmall_apptoken
{
	#pragma db id
	std::string apptoken;

	#pragma db index
	std::uint64_t uid_; // the token that the user belongs
};

// 商户表.
#pragma db object
struct cmall_merchant
{
#pragma db id
	std::uint64_t uid_;

#pragma db index
	std::string name_; // 商户名称.

	uint8_t state_{ 0 }; // 状态, 正常/停用/封禁.

	odb::nullable<std::string> desc_;

	odb::nullable<std::string> gitea_password;

	boost::posix_time::ptime created_at_{ boost::posix_time::second_clock::local_time() };
	boost::posix_time::ptime updated_at_{ boost::posix_time::second_clock::local_time() };
	odb::nullable<boost::posix_time::ptime> deleted_at_;

	std::string repo_path;
};

#pragma db value
struct goods_snapshot
{
	uint64_t merchant_id; // 商品发布者.
	std::string goods_id;
	std::string name_; // 商品名称.
	cpp_numeric price_;
	std::string description_; // 商品描述
	std::string good_version_git;
};

enum order_status_t
{
	order_unpay,
	order_payed,
	order_shipped,
	order_success,
	// TODO, more
};

#pragma db value
struct cmall_kuaidi_info
{
	std::string kuaidihao;
	std::string kuaidigongsi;
};

// 订单表
#pragma db object
struct cmall_order {
#pragma db id auto
	std::uint64_t id_;
	// long long id_{ -1 };

#pragma db index unique
	std::string oid_; // 订单号

#pragma db index
	uint64_t buyer_; // 购买者
#pragma db index
	uint64_t seller_; // 卖家

	cpp_numeric price_; // 下单时价格
 #pragma db type("numeric") default("0")
	cpp_numeric pay_amount_; // 支付数额

	std::vector<Recipient> recipient;

	uint8_t stage_; // 订单状态;
	odb::nullable<boost::posix_time::ptime> payed_at_;
	odb::nullable<boost::posix_time::ptime> close_at_;

	std::vector<goods_snapshot> bought_goods;
#pragma db index default("")
	std::string snap_git_version; // 下单时 商户的仓库 git 版本.

#pragma db index_column("kuaidihao")
	std::vector<cmall_kuaidi_info> kuaidi;

	odb::nullable<std::string> ext_; // 扩展字段

	boost::posix_time::ptime created_at_{ boost::posix_time::second_clock::local_time() };
	boost::posix_time::ptime updated_at_{ boost::posix_time::second_clock::local_time() };
	odb::nullable<boost::posix_time::ptime> deleted_at_;
};

// 购物车.
#pragma db object
struct cmall_cart
{
#pragma db id auto
	std::uint64_t id_;

#pragma db index
	std::uint64_t uid_;

	std::uint64_t merchant_id_;

	std::string goods_id_;

	std::uint64_t count_;

	boost::posix_time::ptime created_at_{ boost::posix_time::second_clock::local_time() };
	boost::posix_time::ptime updated_at_{ boost::posix_time::second_clock::local_time() };
};


enum class approve_state_t
{
	waiting = 0, // 未审批, 等待审批.
	approved, // 批准通过.
	denied, // 未批准.
};

// 申请表.
#pragma db object
struct cmall_apply_for_mechant
{
#pragma db id auto
	std::uint64_t id_;

#pragma db index
	std::shared_ptr<cmall_user> applicant_;

#pragma db default(0)
	uint8_t state_{0}; // 审批状态.

	std::string ext_;

	boost::posix_time::ptime created_at_{ boost::posix_time::second_clock::local_time() };
	boost::posix_time::ptime updated_at_{ boost::posix_time::second_clock::local_time() };
	odb::nullable<boost::posix_time::ptime> deleted_at_;
};
