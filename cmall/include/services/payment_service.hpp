
#pragma once

#include <memory>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

using boost::asio::awaitable;

namespace services
{
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
		awaitable<payment_url> get_payurl(std::string_view script_content, std::string orderid, int nthTry, std::string order_amount, std::string paymentmethod);

	private:
		const payment_impl& impl() const;
		payment_impl& impl();

		std::array<char, 512> obj_stor;

	};
}
