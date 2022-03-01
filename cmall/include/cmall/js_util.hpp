
#pragma once

#include <concepts>
#include <vector>
#include <boost/json.hpp>

#include <odb/nullable.hxx>

#include "cmall/misc.hpp"
#include "cmall/db.hpp"

#include "cmall/rpc_defs.hpp"
#include "./myconcepts"

namespace jsutil
{
	namespace json = boost::json;
	class json_accessor
	{
	public:
		json_accessor(const json::object& obj)
			: obj_(obj)
		{}

        json_accessor(const json::value& obj)
            : obj_(obj.as_object())
        {}

		inline json::value get(char const* key, json::value default_value) const noexcept
		{
			if (obj_.contains(key))
				return obj_.at(key);

			return default_value;
		}

		inline json::object get_obj(char const* key) const noexcept
		{
			if (obj_.contains(key))
				return obj_.at(key).as_object();

			return json::object{};
		}

		inline std::string get_string(char const* key) const noexcept
		{
			if (obj_.contains(key))
			{
				auto value = obj_.at(key);
				if (value.is_string())
				{
					auto ref = value.as_string();
					return std::string(ref.begin(), ref.end());
				}
			}

			return std::string{};
		}

	private:
		const json::object& obj_;
	};

	inline std::string json_as_string(const json::value& value, std::string default_value = "")
	{
		try {
			auto ref = value.as_string();
			return std::string(ref.begin(), ref.end());
		}
		catch (const std::invalid_argument&)
		{}

		return default_value;
	}

	template<typename T>
	inline boost::json::value value_to_json(const T& t)
	{
		return boost::json::value(t);
	}

	template<typename T>
	inline boost::json::value to_json(T&& t)
	{
		return value_to_json(std::forward<T>(t));
	}

	template<typename T>
	requires concepts::isString<T>
	inline boost::json::value to_json(T&& str)
	{
		return boost::json::value(str);
	}

	template<typename T>
	requires concepts::isVector<T> && (!concepts::isString<T>)
	inline boost::json::value to_json(T&& vector_of_t)
	{
		boost::json::array array;

		for (const auto& t :  vector_of_t)
			array.push_back(value_to_json(t));

		return array;
	}

	template<>
	inline boost::json::value value_to_json(const cpp_numeric& t)
	{
		boost::json::string j(to_string(t));
		return j;
	}

	template<>
	inline boost::json::value value_to_json(const cpp_int& t)
	{
		boost::json::string j(to_string(t));
		return j;
	}

	template<>
	inline boost::json::value value_to_json(const odb::nullable<std::string>& t)
	{
		boost::json::string j(t.get());
		return j;
	}

	template<>
	inline boost::json::value value_to_json(const odb::nullable<long>& t)
	{
		boost::json::value j(t.get());
		return j;
	}

	template<>
	inline boost::json::value value_to_json(const boost::posix_time::ptime& t)
	{
		boost::json::string j([](auto && t) -> std::string
		{
			if (t.is_not_a_date_time())
				return "";
			return boost::posix_time::to_iso_extended_string(t);
		}(t));
		return j;
	}

	template<>
	inline boost::json::value value_to_json(const cmall_product& t)
	{
		boost::json::object j;

		j.insert_or_assign("name", t.name_ );
		j.insert_or_assign("merchant", t.owner_);
		j.insert_or_assign("id", t.id_ );
		j.insert_or_assign("price", to_json(t.price_) );
		j.insert_or_assign("describe", t.description_ );

		return j;
	}

	template<>
	inline boost::json::value value_to_json(const Recipient& t)
	{
		boost::json::object j;

		j.insert_or_assign("name", t.name );
		j.insert_or_assign("address", t.address);
		j.insert_or_assign("telephone", to_json(t.telephone) );

		return j;
	}
}
