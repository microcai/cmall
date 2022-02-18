
#pragma once

#include <variant>
#include <optional>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/timer/timer.hpp>

#include <boost/json.hpp>
#include <boost/nowide/convert.hpp>

#include "utils/time_clock.hpp"
#include "cmall/misc.hpp"
#include "cmall/js_util.hpp"
#include "utils/logging.hpp"

using timer = boost::asio::basic_waitable_timer<time_clock::steady_clock>;


namespace eth_misc {

	//////////////////////////////////////////////////////////////////////////

	enum class TokenERC20
	{
		TRANSFER,
		TOTAL_SUPPLY,
		BALANCE_OF,
		DECIMALS,
		NAME,
		SYMBOL,
		OWNER,
		ENABLE,
		BAN,
		IS_BAN,
		MINT,
		BURN,
		PAUSE,
		PAUSED,
		UNPAUSE
	};

	enum class TokenFactoryErc20
	{
		CREATE,
		GET_TOKEN
	};

	enum class ChainDecimals
	{
		Ethchain = 18,
		Chschain = 18,
		TronChain = 6,
	};

	inline std::string get_function_hash(TokenERC20 flag)
	{
		static const std::map<TokenERC20, std::string> functions = {
			{ TokenERC20::TRANSFER,			"a9059cbb" },
			{ TokenERC20::TOTAL_SUPPLY,		"18160ddd" },
			{ TokenERC20::BALANCE_OF,		"70a08231" },
			{ TokenERC20::DECIMALS,			"313ce567" },
			{ TokenERC20::NAME,				"06fdde03" },
			{ TokenERC20::SYMBOL,			"95d89b41" },
			{ TokenERC20::OWNER,			"8da5cb5b" },
			{ TokenERC20::ENABLE,			"5bfa1b68" },
			{ TokenERC20::BAN,				"97c3ccd8" },
			{ TokenERC20::IS_BAN,			"2f7398fb" },
			{ TokenERC20::MINT,				"40c10f19" },
			{ TokenERC20::BURN,				"42966c68" },
			{ TokenERC20::PAUSE,			"8456cb59" },
			{ TokenERC20::PAUSED,			"5c975abb" },
			{ TokenERC20::UNPAUSE,			"3f4ba83a" }
		};

		auto it = functions.find(flag);
		BOOST_ASSERT(it != functions.end());

		return it->second;
	}

	inline std::string get_function_hash(TokenFactoryErc20 flag)
	{
		static const std::map<TokenFactoryErc20, std::string> functions = {
			{ TokenFactoryErc20::CREATE,	"502a81e6" },
			{ TokenFactoryErc20::GET_TOKEN,	"6e066c18" }
		};

		auto it = functions.find(flag);
		BOOST_ASSERT(it != functions.end());

		return it->second;
	}

	inline std::string get_function_hash(std::string signature)
	{
		auto r = ethash_keccak256((const uint8_t*)(signature.c_str()), signature.length());

		return to_hexstring(r.bytes, r.bytes + 4, "");
	}

	template<typename T>
	std::string make_hex_data_element(T t)
	{
		if constexpr (std::is_same_v<std::decay_t<T>, std::string>)
		{
			// 去除0x
			if (t.size() > 2 && t[0] == '0' && t[1] == 'x')
				t = t.substr(2);
			// 填充0.
			t = fill_hexstring(t);
			return t;
		}
		else if constexpr (std::is_same_v<std::decay_t<T>, cpp_int>)
		{
			return to_hexstring(t);
		}
		else
		{
			return "";
		}
	}

	inline std::string make_hex_data(std::string& str)
	{
		return str;
	}

	template<typename T>
	std::string make_hex_data(std::string& str, T a)
	{
		return str + make_hex_data_element(a);
	}

	template<typename T, typename ... Args>
	std::string make_hex_data(std::string& str, T t, Args ... data)
	{
		str += make_hex_data_element<std::decay_t<T>>(t);
		return make_hex_data(str, data ...);
	}

	template<typename ... Args>
	std::string make_data(TokenERC20 func, Args ... data)
	{
		std::string str = get_function_hash(func);
		return "0x" + make_hex_data(str, data ...);
	}

}

enum class Xstatus
{
	TS_FAIL,
	TS_SUCCEE,
	TS_NONE,
	TS_ERROR,
	TS_OUT_OF_ENERGY,
};

struct LogBoom
{
	std::string address;
	std::vector<std::string> topics;
	std::string data;
	long logIndex;
	bool removed;
};

struct txreceptinfo
{
	Xstatus ts;
	cpp_int gas_used;
	long block_number;
	std::string contract_address;

	std::vector<LogBoom> logs;
};

struct chainRAWTX
{
	std::string txhash;
	std::string from;
	std::string to;
	cpp_int nonce;
	std::string input;
	cpp_int value;
	cpp_int gas_price;
	long gas_limit;
};

struct chainBlock
{
	std::string parenthash;
	std::string miner;
	long height;
	std::string hash;
	std::string difficulty;
	std::string extra_data;
	boost::posix_time::ptime blocktime;
	std::string gas_limit;
	cpp_int nonce;

	std::vector<std::variant<std::string, chainRAWTX>> transactions;
};

inline std::string json_to_string(const boost::json::value& jv, bool lf = true)
{
	if (lf) return boost::json::serialize(jv) + "\n";
	return boost::json::serialize(jv);
}

inline std::string make_rpc_json_string(long id, const std::string& method, boost::json::array params_array)
{
	boost::json::value tmp{
		{"jsonrpc", "2.0" },
		{"method", method },
		{"params", params_array },
		{"id", id}
	};

	return json_to_string(tmp, false);
}

namespace utf8util {
	inline std::string string_utf8(const std::string& str)
	{
		std::wstring wres = boost::nowide::widen(str);
		while ((!wres.empty()) && (* wres.rbegin() == 0))
		{
			wres.resize(wres.size() - 1);
		}
		return boost::nowide::narrow(wres);
	}
}

template<typename NextLayer>
class jsonrpc
{
	NextLayer ws;

	long id = 2;

	struct initiate_do_isContract
	{
		template<typename Handler>
		void operator()(Handler&& handler, jsonrpc* parent, const std::string& address)
		{
			boost::asio::spawn(parent->get_executor(), [handler = std::move(handler), parent, address](boost::asio::yield_context yield) mutable
			{
				boost::system::error_code ec;
				auto jv = parent->async_req("eth_getCode", {address, "latest" }, yield[ec]);
				if (ec)
				{
					handler(ec, false);
					return;
				}

				auto& obj = jv.as_object();
				auto result = jsutil::json_as_string(jsutil::json_accessor(obj).get("result", ""));
				if (result.empty())
				{
					handler(ec, false);
				}
				else
					handler(ec, !(result == "0x"));
			});
		}
	};

	struct initiate_do_cur_height
	{
		template<typename Handler>
		void operator()(Handler&& handler, jsonrpc* parent)
		{
			using namespace std::chrono_literals;
			boost::asio::spawn(parent->get_executor(), [handler = std::move(handler), parent](boost::asio::yield_context yield) mutable
			{
				boost::system::error_code ec;
				auto jv = parent->async_req("eth_blockNumber", {}, yield[ec]);
				if (ec)
				{
					handler(ec, 0);
					return;
				}

				try
				{
					auto& obj = jv.as_object();
					auto result = jsutil::json_as_string(jsutil::json_accessor(obj).get("result", ""));
					if (result.empty())
					{
						LOG_ERR << "fetch_cur_height, result is empty";
						handler(ec, 0);
						return;
					}

					handler(ec, cpp_int(result).convert_to<long>());
					return;
				}
				catch (const std::exception& e) {
					LOG_ERR << "fetch_cur_height, exception: " << e.what();
				}
				catch (...) {
				}

				handler(ec, 0);

			});
		}
	};

	struct initiate_do_contract_name
	{
		template<typename Handler>
		void operator()(Handler&& handler, jsonrpc* parent, const std::string& address)
		{
			using namespace std::chrono_literals;
			boost::asio::spawn(parent->get_executor(), [handler = std::move(handler), parent, address](boost::asio::yield_context yield) mutable
			{
				std::string balance_string;
				std::string hex_data = eth_misc::make_data(eth_misc::TokenERC20::NAME);

				boost::json::object params;
				params["to"] = address;
				params["data"] = hex_data;

				boost::system::error_code ec;
				auto jv = parent->async_req("eth_call", { params,"latest" }, yield[ec]);
				if (ec)
				{
					handler(ec, std::string{});
					return;
				}

				try {
					auto& obj = jv.as_object();
					auto result = jsutil::json_as_string(jsutil::json_accessor(obj).get("result", ""));
					if (result.empty())
					{
						LOG_ERR << "fetch_contract_name, result is empty";
						handler(ec, std::string{});
						return;
					}

					if (result.size() == 194)
					{
						result = result.substr(2);		// skip '0x'
						result = result.substr(64);		// skip hex, 0000000000000000000000000000000000000000000000000000000000000020
						auto hexstring_length = result.substr(0, 64);	// length
						auto hexstring = result.substr(64);

						auto val = from_contract_hexstring(hexstring_length, hexstring);
						if (val)
						{
							if (util::aux::utf8_check_is_valid(*val))
							{
								handler(ec, utf8util::string_utf8(*val));
								return;
							}
						}
					}

					handler(ec, result);
					return;
				}
				catch (const std::exception& e)
				{
					LOG_ERR << "fetch_contract_name, exception: " << e.what();
				}

				handler(ec, std::string{});
			});
		}
	};

	struct initiate_do_contract_symbol
	{
		template<typename Handler>
		void operator()(Handler&& handler, jsonrpc* parent, const std::string& address)
		{
			using namespace std::chrono_literals;
			boost::asio::spawn(parent->get_executor(), [handler = std::move(handler), parent, address](boost::asio::yield_context yield) mutable
			{
				std::string balance_string;
				std::string hex_data = eth_misc::make_data(eth_misc::TokenERC20::SYMBOL);

				boost::json::object params;
				params["to"] = address;
				params["data"] = hex_data;

				boost::system::error_code ec;
				auto jv = parent->async_req("eth_call", { params, "latest" }, yield[ec]);
				if (ec)
				{
					handler(ec, std::string{});
					return;
				}

				try {
					auto& obj = jv.as_object();
					auto result = jsutil::json_as_string(jsutil::json_accessor(obj).get("result", ""));
					if (result.empty())
					{
						LOG_ERR << "fetch_contract_symbol, result is empty";
						handler(ec, std::string{});
						return;
					}

					if (result.size() == 194)
					{
						result = result.substr(2);		// skip '0x'
						result = result.substr(64);		// skip hex, 0000000000000000000000000000000000000000000000000000000000000020
						auto hexstring_length = result.substr(0, 64);	// length
						auto hexstring = result.substr(64);

						auto val = from_contract_hexstring(hexstring_length, hexstring);
						if (val)
						{
							if (util::aux::utf8_check_is_valid(*val))
							{
								handler(ec, utf8util::string_utf8(*val));
								return;
							}
						}
					}

					handler(ec, result);
					return;
				}
				catch (const std::exception& e)
				{
					LOG_ERR << "fetch_contract_symbol, exception: " << e.what();
				}

				handler(ec, std::string{});
			});
		}
	};

	struct initiate_do_contract_decimals
	{
		template<typename Handler>
		void operator()(Handler&& handler, jsonrpc* parent, const std::string& address)
		{
			using namespace std::chrono_literals;
			boost::asio::spawn(parent->get_executor(), [handler = std::move(handler), parent, address](boost::asio::yield_context yield) mutable
			{
				std::string balance_string;
				std::string hex_data = eth_misc::make_data(eth_misc::TokenERC20::DECIMALS);

				boost::json::object params;
				params["to"] = address;
				params["data"] = hex_data;

				boost::system::error_code ec;
				auto jv = parent->async_req("eth_call", { params, "latest" }, yield[ec]);
				if (ec)
				{
					handler(ec, 0);
					return;
				}

				try
				{
					auto& obj = jv.as_object();
					auto result = jsutil::json_as_string(jsutil::json_accessor(obj).get("result", ""));
					if (result.empty())
					{
						handler(ec, 0);
						return;
					}

					handler(ec, cpp_int(result).convert_to<long>());
					return;
				}
				catch (const std::exception& e) {
					LOG_ERR << "fetch_decimals, exception: " << e.what();
				}
				catch (...) {
				}

				handler(ec, 0);
			});
		}
	};

	struct initiate_do_tolen_balance
	{
		template<typename Handler>
		void operator()(Handler&& handler, jsonrpc* parent, const std::string& token, const std::string& address)
		{
			boost::asio::spawn(parent->get_executor(), [handler = std::move(handler), parent, token, address](boost::asio::yield_context yield) mutable
			{
				std::string balance_string;
				std::string hex_data = eth_misc::make_data(eth_misc::TokenERC20::BALANCE_OF, address);

				boost::json::object params;
				params["to"] = token;
				params["data"] = hex_data;

				boost::system::error_code ec;

				auto jv = parent->async_req("eth_call", { params, "latest"}, yield[ec]);
				if (ec)
				{
					handler(ec, 0);
					return;
				}

				try
				{
					auto& obj = jv.as_object();
					auto result = jsutil::json_as_string(jsutil::json_accessor(obj).get("result", ""));
					if (result.empty())
					{
						LOG_ERR << "fetch_token_balance, result is empty";
						handler(boost::asio::error::no_data, 0);
						return;
					}

					handler(ec, cpp_int(result));
					return;
				}
				catch (const std::exception& e) {
					LOG_ERR << "fetch_token_balance, exception: " << e.what();
				}
				catch (...) {
				}

				handler(ec, 0);
			});
		}
	};

	
	struct initiate_do_get_balance
	{
		template<typename Handler>
		void operator()(Handler&& handler, jsonrpc* parent, const std::string& address, const std::string& height_hex_str)
		{
			boost::asio::spawn(parent->get_executor(), [handler = std::move(handler), parent, height_hex_str, address](boost::asio::yield_context yield) mutable
			{
				boost::system::error_code ec;
				auto jv = parent->async_req("eth_getBalance", { address, height_hex_str }, yield[ec]);
				if (ec)
				{
					handler(ec, 0);
					return;
				}

				try
				{
					auto& obj = jv.as_object();
					auto result = jsutil::json_as_string(jsutil::json_accessor(obj).get("result", ""));
					if (result.empty())
					{
						LOG_ERR << "fetch_balance, result is empty";
						handler(boost::asio::error::no_data, 0);
						return;
					}

					handler(ec, cpp_int(result));
					return;
				}
				catch (const std::exception& e) {
					LOG_ERR << "fetch_balance, exception: " << e.what();
				}
				catch (...) {
				}

				handler(boost::asio::error::no_data, 0);
			});
		}
	};

	struct initiate_do_get_block
	{
		template<typename Handler>
		void operator()(Handler&& handler, jsonrpc* parent, std::variant<long, std::string> block, bool contain_tx = false) const
		{
			boost::asio::spawn(parent->get_executor(), [handler = std::move(handler), parent, block, contain_tx](boost::asio::yield_context yield) mutable
			{
				boost::system::error_code ec;
				boost::json::value jv;

				if (std::holds_alternative<long>(block))
				{
					auto number_str = fmt::format("{:#x}", std::get<long>(block));
					jv = parent->async_req("eth_getBlockByNumber", { number_str, contain_tx }, yield[ec]);
				}
				else
				{
					jv = parent->async_req("eth_getBlockByHash", { std::get<std::string>(block), contain_tx }, yield[ec]);
				}

				if (ec)
				{
					LOG_ERR << "parse_block, json parse: " << ec.message();

					BOOST_ASIO_MOVE_OR_LVALUE(Handler)(handler)(ec, chainBlock{});
					return;
				}

				try
				{
					using namespace eth_misc;
					auto& obj = jv.as_object();
					if (!obj.contains("result") || obj.at("result").is_null())
					{
						LOG_ERR << "parse_block, bad json, no contain result";
						handler(boost::asio::error::no_data, chainBlock{});
						return;
					}
					obj = obj["result"].as_object();

					chainBlock thisblock;
					thisblock.parenthash = jsutil::json_as_string(jsutil::json_accessor(obj).get("parenthash", ""));
					thisblock.miner = jsutil::json_as_string(jsutil::json_accessor(obj).get("miner", ""));
					auto number_string = jsutil::json_as_string(jsutil::json_accessor(obj).get("number", ""));
					thisblock.hash = jsutil::json_as_string(jsutil::json_accessor(obj).get("hash", ""));
					thisblock.difficulty = jsutil::json_as_string(jsutil::json_accessor(obj).get("difficulty", ""));
					thisblock.extra_data = jsutil::json_as_string(jsutil::json_accessor(obj).get("extraData", ""));
					auto timestamp = jsutil::json_as_string(jsutil::json_accessor(obj).get("timestamp", ""));
					auto gas_limit = jsutil::json_as_string(jsutil::json_accessor(obj).get("gasLimit", ""));
					auto nonce_string = jsutil::json_as_string(jsutil::json_accessor(obj).get("nonce", ""));

					thisblock.blocktime = make_localtime(timestamp);
					cpp_int height(number_string);

					thisblock.height = height.convert_to<long>();
					cpp_int nonce(nonce_string);

					thisblock.nonce = nonce;

					auto transactions = obj["transactions"].as_array();

					for (auto& tx : transactions)
					{
						if (contain_tx)
						{
							auto& txobj = tx.as_object();
							chainRAWTX txlog;
							txlog.to = "0x0000000000000000000000000000000000000000";
							txlog.txhash = jsutil::json_as_string(jsutil::json_accessor(txobj).get("hash", ""));
							txlog.from = jsutil::json_as_string(jsutil::json_accessor(txobj).get("from", ""));
							if (txobj.contains("to") && !txobj.at("to").is_null())
								txlog.to = jsutil::json_as_string(jsutil::json_accessor(txobj).get("to", "0x0000000000000000000000000000000000000000"));
							auto nonce = jsutil::json_as_string(jsutil::json_accessor(txobj).get("nonce", ""));
							txlog.nonce = cpp_int(nonce).convert_to<long>();
							txlog.input = jsutil::json_as_string(jsutil::json_accessor(txobj).get("input", ""));

							std::string value = jsutil::json_as_string(jsutil::json_accessor(txobj).get("value", ""));
							cpp_int transfer_value = cpp_int(value);
							txlog.value = transfer_value;
							txlog.gas_price = cpp_int(jsutil::json_as_string(jsutil::json_accessor(txobj).get("gasPrice", "")));
							txlog.gas_limit = cpp_int(jsutil::json_as_string(jsutil::json_accessor(txobj).get("gas", ""))).convert_to<int>();

							thisblock.transactions.push_back(txlog);
						}
						else
						{
							thisblock.transactions.push_back(jsutil::json_as_string(tx));
						}
					}

					handler(ec, thisblock);
					return;
				}
				catch (const std::exception& e) {
					LOG_ERR << "fetch_balance, exception: " << e.what();
				}
				catch (...) {
				}

				handler(boost::asio::error::no_data, chainBlock{});
			});
		}
	};

	struct initiate_do_get_transaction
	{
		static bool parse_tx(chainRAWTX& txlog, boost::json::value& jv)
		{
			try
			{
				auto& obj = jv.as_object();
				if (obj.contains("result") && obj.at("result").is_null())
				{
					LOG_WARN << "fetch_transaction_receipt:  result is null";
					return false;
				}
				if (obj.contains("error") || !obj.contains("result"))
				{
					return false;
				}

				auto& txobj = obj["result"].as_object();

				txlog.to = "0x0000000000000000000000000000000000000000";
				txlog.txhash = jsutil::json_as_string(jsutil::json_accessor(txobj).get("hash", ""));
				txlog.from = jsutil::json_as_string(jsutil::json_accessor(txobj).get("from", ""));
				if (txobj.contains("to") && !txobj.at("to").is_null())
					txlog.to = jsutil::json_as_string(jsutil::json_accessor(txobj).get("to", "0x0000000000000000000000000000000000000000"));
				auto nonce = jsutil::json_as_string(jsutil::json_accessor(txobj).get("nonce", ""));
				txlog.nonce = cpp_int(nonce).convert_to<long>();
				txlog.input = jsutil::json_as_string(jsutil::json_accessor(txobj).get("input", ""));
				std::string value = jsutil::json_as_string(jsutil::json_accessor(txobj).get("value", ""));
				cpp_int transfer_value = cpp_int(value);
				txlog.value = transfer_value;
				txlog.gas_price = cpp_int(jsutil::json_as_string(jsutil::json_accessor(txobj).get("gasPrice", "")));
				txlog.gas_limit = cpp_int(jsutil::json_as_string(jsutil::json_accessor(txobj).get("gas", ""))).convert_to<int>();
				return true;
			}
			catch (const std::exception& e) {
				LOG_ERR << "fetch_balance, exception: " << e.what();
			}
			catch (...) {
			}

			return false;
		}

		template<typename Handler>
		void operator()(Handler&& handler, jsonrpc* parent, std::string txhash)
		{
			boost::asio::spawn(parent->get_executor(), [handler = std::move(handler), parent, txhash](boost::asio::yield_context yield) mutable
			{
				boost::system::error_code ec;
				auto jv = parent->async_req("eth_getTransactionByHash", { txhash }, yield[ec]);
				if (ec)
				{
					LOG_ERR << "fetch_transaction, json parse: " << ec.message();
					handler(ec, {});
					return;
				}

				chainRAWTX txlog;

				if (initiate_do_get_transaction::parse_tx(txlog, jv))
				{
					handler(ec, txlog);
				}
				else
				{
					handler(boost::asio::error::no_data, {});
				}
			});
		}

		template<typename Handler>
		void operator()(Handler&& handler, jsonrpc* parent, long block_height, long index)
		{
			std::string height_hex_str = to_hexstring(static_cast<int64_t>(block_height), 0, "0x");
			std::string index_hex_str = to_hexstring(static_cast<int64_t>(index), 0, "0x");

			boost::asio::spawn(parent->get_executor(), [handler = std::move(handler), parent, height_hex_str, index_hex_str](boost::asio::yield_context yield) mutable
			{
				boost::system::error_code ec;
				auto jv = parent->async_req("eth_getTransactionByBlockNumberAndIndex", { height_hex_str, index_hex_str }, yield[ec]);
				if (ec)
				{
					LOG_ERR << "fetch_transaction, json parse: " << ec.message();
					handler(ec, chainRAWTX{});
					return;
				}

				chainRAWTX txlog;

				if (initiate_do_get_transaction::parse_tx(txlog, jv))
				{
					handler(ec, txlog);
				}
				else
				{
					handler(boost::asio::error::no_data, chainRAWTX{});
				}
			});
		}
	};

	struct initiate_do_get_transaction_receipt
	{
		template<typename Handler>
		void operator()(Handler&& handler, jsonrpc* parent, std::string txhash)
		{
			using namespace std::chrono_literals;
			boost::asio::spawn(parent->get_executor(), [handler = std::move(handler), parent, txhash](boost::asio::yield_context yield) mutable
			{
				txreceptinfo ret;
				boost::system::error_code ec;
				auto jv = parent->async_req("eth_getTransactionReceipt", {txhash}, yield[ec]);
				if (ec)
				{
					LOG_ERR << "fetch_transaction_receipt, json parse: " << ec.message();
					handler(ec, ret);
					return;
				}

				try
				{
					auto& obj = jv.as_object();
					if (obj.contains("result") && obj.at("result").is_null())
					{
						LOG_WARN << "fetch_transaction_receipt: " << std::string(txhash) << ", result is null";
						ret.ts = Xstatus::TS_NONE;
						handler(ec, ret);
						return ;
					}
					if (obj.contains("error") || !obj.contains("result"))
					{
						LOG_WARN << "fetch_transaction_receipt: " << std::string(txhash);
						ret.ts = Xstatus::TS_NONE;
						handler(ec, ret);
						return;
					}
					
					auto& result_obj = obj["result"].as_object();

					std::string block_number_string = jsutil::json_as_string(jsutil::json_accessor(result_obj).get("blockNumber", ""));
					if (block_number_string.empty())
					{
						ret.ts = Xstatus::TS_NONE;
						handler(ec, ret);
						return;
					}

					long block_number = (long)cpp_int(block_number_string).convert_to<int64_t>();
					std::string gas_used = jsutil::json_as_string(jsutil::json_accessor(result_obj).get("gasUsed", ""));
					std::string contract_address;
					if (result_obj.contains("contractAddress") && !result_obj["contractAddress"].is_null())
						contract_address = jsutil::json_as_string(jsutil::json_accessor(result_obj).get("contractAddress", ""));

					auto logs = jsutil::json_accessor(result_obj).get("logs", boost::json::array{});

					Xstatus ts = Xstatus::TS_FAIL;
					if (result_obj.contains("status"))
					{
						auto status = jsutil::json_as_string(jsutil::json_accessor(result_obj).get("status", ""));
						if (cpp_int(status) == 1)
							ts = Xstatus::TS_SUCCEE;
					}
					else
					{
						auto status = jsutil::json_as_string(jsutil::json_accessor(result_obj).get("root", ""));
						if (!status.empty())
							ts = Xstatus::TS_SUCCEE;
					}

					ret.ts = ts;
					ret.gas_used = cpp_int(gas_used);
					ret.block_number = block_number;
					ret.contract_address = contract_address;

					for (const auto& log : logs.as_array())
					{
						LogBoom logevent;

						logevent.address = jsutil::json_as_string(jsutil::json_accessor(log).get("address", ""));
						logevent.removed = jsutil::json_accessor(log).get("removed", false).as_bool();
						logevent.data = jsutil::json_as_string(jsutil::json_accessor(log).get("data", ""));
						logevent.logIndex = cpp_int(jsutil::json_as_string(jsutil::json_accessor(log).get("logIndex", "0x0"))).convert_to<long>();
						boost::json::value topics = jsutil::json_accessor(log).get("topics", boost::json::array{});

						for (const auto& topic : topics.as_array())
						{
							logevent.topics.push_back(jsutil::json_as_string(topic));
						}

						ret.logs.push_back(logevent);
					}

					handler(ec, ret);
					return;

				}
				catch (const std::exception& e) {
					LOG_ERR << "fetch_balance, exception: " << e.what();
				}
				catch (...) {
				}

				handler(boost::asio::error::no_data, ret);
			});
		}
	};

	struct initiate_do_req
	{
		template<typename Handler>
		void operator()(Handler&& handler, jsonrpc* parent, std::string method, boost::json::array params)
		{
			using namespace std::chrono_literals;
			boost::asio::spawn(parent->get_executor(), [handler = std::move(handler), parent, method, params](boost::asio::yield_context yield) mutable
			{
				auto context = make_rpc_json_string(parent->id++, method, params);

				boost::beast::multi_buffer buf;
				boost::system::error_code ec;
				boost::beast::get_lowest_layer(parent->ws).expires_after(60s);
				parent->ws.async_write(boost::asio::buffer(context), yield[ec]);
				if (ec)
				{
					handler(ec, {});
					return;
				}
				boost::beast::get_lowest_layer(parent->ws).expires_after(60s);
				parent->ws.async_read(buf, yield[ec]);
				if (ec)
				{
					handler(ec, {});
					return;
				}
				auto body = boost::beast::buffers_to_string(buf.data());
				auto jv = boost::json::parse(body, ec, {}, { 64, false, false, true });
				if (ec)
				{
					LOG_ERR << "jsonrpc replay parse error, json parse: " << ec.message() << ", block: " << body;
					handler(ec, {});
					return;
				}

				handler(ec, jv);
			});
		}
	};

	struct initiate_do_parall_req
	{
		template<typename Handler>
		void operator()(Handler&& handler, jsonrpc* parent, std::string method, boost::json::array params)
		{
			using namespace std::chrono_literals;

			boost::asio::spawn(parent->get_executor(), [handler = std::move(handler), parent, method, params](boost::asio::yield_context yield) mutable
			{
				boost::system::error_code ec;

				auto this_call_id = parent->id++;

				auto context = make_rpc_json_string(this_call_id, method, params);

				// 把 handler 挂入 handler_map
				parent->handler_map.insert({ this_call_id , { std::move(handler) , method, boost::timer::cpu_timer{} } });

				parent->write_queue.push_back(context);
				parent->m_write_timer->cancel(ec);
			});
		}
	};

	std::shared_ptr<bool> stop_flag;
	boost::asio::coroutine reader, writer;
	boost::beast::multi_buffer reader_buf;
	struct handler_t
	{
		boost::function<void(boost::system::error_code, boost::json::value)> callback;
		std::string method;
		boost::timer::cpu_timer eclipsed_timer;
	};

	std::map<std::int64_t, handler_t> handler_map;

	std::deque<std::string> write_queue;

	std::unique_ptr<timer> m_write_timer;

	void clear_out(boost::system::error_code ec)
	{
		for (auto&& handler : handler_map)
		{
			boost::asio::post(get_executor(), boost::asio::detail::bind_handler(handler.second.callback, ec, boost::json::value{}));
		}
		handler_map.clear();
	}

	void parallel_mode_readloop(boost::system::error_code ec, std::size_t bytes_transfered, std::shared_ptr<bool> stop_flag)
	{
		if (*stop_flag)
			return;
		if (ec == boost::asio::error::operation_aborted)
			return;
		BOOST_ASIO_CORO_REENTER(reader)
		{
			while (!*stop_flag)
			{
				BOOST_ASIO_CORO_YIELD this->ws.async_read(reader_buf, boost::bind(&jsonrpc::parallel_mode_readloop, this, boost::placeholders::_1, boost::placeholders::_2, stop_flag));
				if (ec)
					return clear_out(ec);
				{
					auto body = boost::beast::buffers_to_string(reader_buf.data());
					auto jv = boost::json::parse(body, ec, {}, { 64, false, false, true });
					if (ec)
					{
						boost::beast::get_lowest_layer(ws).close();
						return clear_out(ec);
					}
					reader_buf.consume(bytes_transfered);
					if (jsutil::json_accessor(jv).get("jsonrpc", "1") != "2.0")
					{
						ec = boost::asio::error::invalid_argument;
						return clear_out(ec);
					}

					if ( ! jv.as_object().contains("id"))
					{
						// TODO 发送通知消息
						continue;
					}

					// extract id from replay
					auto replay_id = jsutil::json_accessor(jv).get("id", 0).to_number<std::int64_t>();
					// lookup replay_id in handler map
					auto handler_it = handler_map.find(replay_id);
					if (handler_it != handler_map.end())
					{
						auto eclipsed_time = handler_it->second.eclipsed_timer.elapsed().wall;
						std::string method = handler_it->second.method;
						boost::asio::post(get_executor(), boost::asio::detail::bind_handler(handler_it->second.callback, ec, jv));
						handler_map.erase(handler_it);
						if (eclipsed_time > 1000000000)
						{
							LOG_WARN << "rpc call toke too long for req: " << method;
						}
					}
				}
			}
		}
	}

	void parallel_mode_writeloop(boost::system::error_code ec, std::size_t bytes_transfered, std::shared_ptr<bool> stop_flag)
	{
		BOOST_ASIO_CORO_REENTER(writer)
		{
			while (!*stop_flag)
			{
				while (write_queue.empty())
				{
					using namespace std::chrono_literals;
					this->m_write_timer->expires_from_now(60s);
					BOOST_ASIO_CORO_YIELD this->m_write_timer->async_wait(boost::bind(&jsonrpc::parallel_mode_writeloop, this, boost::placeholders::_1, 0, stop_flag));
					if (*stop_flag)
						return;
				}

				while (!write_queue.empty())
				{
					BOOST_ASIO_CORO_YIELD ws.async_write(boost::asio::buffer(*write_queue.begin()), boost::bind(&jsonrpc::parallel_mode_writeloop, this, boost::placeholders::_1, boost::placeholders::_2, stop_flag));
					if (ec)
					{
						clear_out(ec);
						return;
					}
					if (*stop_flag)
						return;
					write_queue.pop_front();
				}
			}
		}
	}

	bool parallel_mode = false;

public:
	using executor_type = typename std::decay<NextLayer>::type::executor_type;
	jsonrpc(NextLayer&& t) : ws(t) {}

	auto get_executor() noexcept
	{
		return ws.get_executor();
	}

	void switch_to_parallel_mode()
	{
		parallel_mode = true;
		stop_flag = std::make_shared<bool>(false);
		m_write_timer.reset(new timer(get_executor()));

		boost::asio::post(get_executor(), boost::bind(&jsonrpc::parallel_mode_writeloop, this, boost::system::error_code(), 0, stop_flag));
		boost::asio::post(get_executor(), boost::bind(&jsonrpc::parallel_mode_readloop, this, boost::system::error_code(), 0, stop_flag));
	}

	~jsonrpc()
	{
		if (!parallel_mode)
			return;
		*stop_flag = true;
		boost::system::error_code ignore_ec;
		m_write_timer->cancel(ignore_ec);
		boost::beast::get_lowest_layer(ws).cancel();
		clear_out(boost::asio::error::operation_aborted);
	}

	template <
		BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, boost::json::value)) Handler
		BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
	BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, boost::json::value))
		async_req(std::string method, boost::json::array req_params, BOOST_ASIO_MOVE_ARG(Handler) handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
	{
		if (parallel_mode)
			return boost::asio::async_initiate<Handler, void(boost::system::error_code, boost::json::value)>
				(initiate_do_parall_req{}, handler, this, method, req_params);
		else
			return boost::asio::async_initiate<Handler, void(boost::system::error_code, boost::json::value)>
				(initiate_do_req{}, handler, this, method, req_params);
	}

	template <
		BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, bool)) Handler
		BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
	BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, bool))
		async_isContract(const std::string contract_address, BOOST_ASIO_MOVE_ARG(Handler) handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
	{
		return boost::asio::async_initiate<Handler, void(boost::system::error_code, bool)>
			(initiate_do_isContract{}, handler, this, contract_address);
	}

	template <
		BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, long)) Handler
			BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
	BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, long))
		async_cur_height(BOOST_ASIO_MOVE_ARG(Handler) handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
	{
		return boost::asio::async_initiate<Handler, void(boost::system::error_code, long)>
			(initiate_do_cur_height{}, handler, this);
	}

	template <
		BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, std::string)) Handler
		BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
	BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, std::string))
		async_contract_name(const std::string contract_address, BOOST_ASIO_MOVE_ARG(Handler) handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
	{
		return boost::asio::async_initiate<Handler, void(boost::system::error_code, std::string)>
			(initiate_do_contract_name{}, handler, this, contract_address);
	}

	template <
		BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, std::string)) Handler
		BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
	BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, std::string))
		async_contract_symbol(const std::string contract_address, BOOST_ASIO_MOVE_ARG(Handler) handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
	{
		return boost::asio::async_initiate<Handler, void(boost::system::error_code, std::string)>
			(initiate_do_contract_symbol{}, handler, this, contract_address);
	}

	template <
		BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, int)) Handler
		BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
	BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, int))
		async_contract_decimals(const std::string& contract_address, BOOST_ASIO_MOVE_ARG(Handler) handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
	{
		return boost::asio::async_initiate<Handler, void(boost::system::error_code, int)>
			(initiate_do_contract_decimals{}, handler, this, contract_address);
	}

	template <
		BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, cpp_int)) Handler
		BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
	BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, cpp_int))
		async_token_balance(const std::string& token, const std::string& address, BOOST_ASIO_MOVE_ARG(Handler) handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
	{
		return boost::asio::async_initiate<Handler, void(boost::system::error_code, cpp_int)>
			(initiate_do_tolen_balance{}, handler, this, token, address);
	}

	template <
		BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, cpp_int)) Handler
		BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
	BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, cpp_int))
		async_get_balance(const std::string& address, BOOST_ASIO_MOVE_ARG(Handler) handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
	{
		return boost::asio::async_initiate<Handler, void(boost::system::error_code, cpp_int)>
			(initiate_do_get_balance{}, handler, this, address, "latest");
	}

	template <
		BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, cpp_int)) Handler
		BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
	BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, cpp_int))
		async_get_balance(const std::string& address, long block_height, BOOST_ASIO_MOVE_ARG(Handler) handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
	{
		std::string height_hex_str = to_hexstring(static_cast<int64_t>(block_height), 0, "0x");
		return boost::asio::async_initiate<Handler, void(boost::system::error_code, cpp_int)>
			(initiate_do_get_balance{}, handler, this, address, height_hex_str);
	}

	template <
		BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, chainBlock)) Handler
		BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
	BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, chainBlock))
		async_get_block(const std::string& blockhash, BOOST_ASIO_MOVE_ARG(Handler) handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
	{
		return boost::asio::async_initiate<Handler, void(boost::system::error_code, chainBlock)>
			(initiate_do_get_block{}, handler, this, blockhash, true);
	}
	
	template <
		BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, chainBlock)) Handler
		BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
	BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, chainBlock))
		async_get_block(const long block_height, BOOST_ASIO_MOVE_ARG(Handler) handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
	{
		return boost::asio::async_initiate<Handler, void(boost::system::error_code, chainBlock)>
			(initiate_do_get_block{}, handler, this, block_height);
	}

	template <
		BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, chainRAWTX)) Handler
		BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
	BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, chainRAWTX))
		async_get_transaction(const std::string& txhash, BOOST_ASIO_MOVE_ARG(Handler) handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
	{
		return boost::asio::async_initiate<Handler, void(boost::system::error_code, chainRAWTX)>
			(initiate_do_get_transaction{}, handler, this, txhash);
	}

	template <
		BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, chainRAWTX)) Handler
		BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
	BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, chainRAWTX))
		async_get_transaction(const long blocknumber, const int index, BOOST_ASIO_MOVE_ARG(Handler) handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
	{
		return boost::asio::async_initiate<Handler, void(boost::system::error_code, chainRAWTX)>
			(initiate_do_get_transaction{}, handler, this, blocknumber, index);
	}
		
	template <
		BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, txreceptinfo)) Handler
		BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
	BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(Handler, void(boost::system::error_code, txreceptinfo))
		async_get_transaction_receipt(const std::string& txhash, BOOST_ASIO_MOVE_ARG(Handler) handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
	{
		return boost::asio::async_initiate<Handler, void(boost::system::error_code, txreceptinfo)>
			(initiate_do_get_transaction_receipt{}, handler, this, txhash);
	}
};
