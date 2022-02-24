
#include "cmall/http_static_file_handle.hpp"
#include "bundle_file.hpp"
#include "cmall/internal.hpp"

boost::asio::awaitable<int> cmall::http_handle_static_file(size_t connection_id, boost::beast::http::request<boost::beast::http::string_body>& req, boost::beast::tcp_stream& client)
{
	unzFile zip_file = ::unzOpen2_64("internel_bundled_exe", &exe_bundled_file_ops);
	std::shared_ptr<void> auto_cleanup((void*)zip_file, unzClose);
	unz_global_info64 unzip_global_info64;
	std::time_t if_modified_since;
	bool have_if_modified_since = parse_gmt_time_fmt(
		req[boost::beast::http::field::if_modified_since].to_string().c_str(), &if_modified_since);

	boost::string_view accept_encoding = req[boost::beast::http::field::accept_encoding];
	bool accept_deflate				   = false;

	if (accept_encoding.find("deflate") != boost::string_view::npos)
		accept_deflate = true;

	if (unzGetGlobalInfo64(zip_file, &unzip_global_info64) != UNZ_OK)
		co_return 502;

	boost::string_view path_ = req.target();

	if (path_ == "/")
		path_ = "index.html";
	else
	{
		path_ = path_.substr(1);
	}

	std::string path{ path_.data(), path_.length() };

	if (unzLocateFile(zip_file, path.data(), false) != UNZ_OK)
	{
		path = "index.html";
		unzLocateFile(zip_file, "index.html", false);
	}

	unz_file_info64 target_file_info;

	unzGetCurrentFileInfo64(zip_file, &target_file_info, NULL, 0, NULL, 0, NULL, 0);

	std::time_t target_file_modifined_time = dos2unixtime(target_file_info.dosDate);

	using fields = boost::beast::http::fields;

	using response = boost::beast::http::response<boost::beast::http::buffer_body>;

	response res{ boost::beast::http::status::ok, req.version() };
	res.set(boost::beast::http::field::server, HTTPD_VERSION_STRING);
	res.set(boost::beast::http::field::expires, make_http_last_modified(std::time(0) + 60));
	res.set(boost::beast::http::field::last_modified, make_http_last_modified(target_file_modifined_time));
	res.set(boost::beast::http::field::content_type, mime_map[boost::filesystem::path(path).extension().string()]);
	res.keep_alive(req.keep_alive());

	if (have_if_modified_since && (target_file_modifined_time <= if_modified_since))
	{
		// 返回 304
		res.result(boost::beast::http::status::not_modified);

		res.set(boost::beast::http::field::expires, make_http_last_modified(std::time(0) + 60));
		boost::beast::http::response_serializer<boost::beast::http::buffer_body, fields> sr{ res };
		res.body().data = nullptr;
		res.body().more = false;
		co_await boost::beast::http::async_write(client, sr, boost::asio::use_awaitable);
		co_return 200;
	}

	int method = 0, level = 0;

	if (unzOpenCurrentFile2(zip_file, &method, &level, accept_deflate ? 1 : 0) != UNZ_OK)
		co_return 500;

	if (method == Z_DEFLATED && accept_deflate)
	{
		res.set(boost::beast::http::field::content_encoding, "deflate");
		res.content_length(target_file_info.compressed_size);
	}
	else
	{
		res.content_length(target_file_info.uncompressed_size);
	}

	res.body().data = nullptr;
	res.body().more = true;

	boost::system::error_code ec;
	boost::beast::http::response_serializer<boost::beast::http::buffer_body, fields> sr{ res };

	co_await boost::beast::http::async_write_header(client, sr, boost::asio::use_awaitable);

	char buffer[2048];

	do
	{
		auto bytes_transferred = unzReadCurrentFile(zip_file, buffer, 2048);
		if (bytes_transferred == 0)
		{
			res.body().data = nullptr;
			res.body().more = false;
		}
		else
		{
			res.body().data = buffer;
			res.body().size = bytes_transferred;
			res.body().more = true;
		}
		co_await boost::beast::http::async_write(
			client, sr, boost::asio::redirect_error(boost::asio::use_awaitable, ec));

		if (ec == boost::beast::http::error::need_buffer)
			continue;
		if (ec)
		{
			throw boost::system::system_error(ec);
		}

	} while (!sr.is_done());

	unzCloseCurrentFile(zip_file);

	co_return 200;

}