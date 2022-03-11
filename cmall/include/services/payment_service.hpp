
#pragma once

#include <memory>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

namespace services
{
	enum PAYMENT_GATEWAY {
		PAYMENT_ALIPAY,
		PAYMENT_WXPAY,
		PAYMENT_CHSPAY,
	};

	struct payment_url {
		std::string uri;
		boost::posix_time::ptime valid_until;
	};
	struct payment_impl;
	class payment
	{
	public:
		payment(boost::asio::io_context& io);
		~payment();

		// get payment url.
		boost::asio::awaitable<payment_url> get_payurl(std::string_view script_content, std::string orderid, int nthTry, std::string order_title, std::string order_amount, PAYMENT_GATEWAY gateway);
		// query payment success.
		boost::asio::awaitable<bool> query_pay(std::string orderid, PAYMENT_GATEWAY gateway);

	private:
		const payment_impl& impl() const;
		payment_impl& impl();

		std::array<char, 512> obj_stor;

	};
}
