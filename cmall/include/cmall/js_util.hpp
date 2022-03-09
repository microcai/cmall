
#pragma once

#include <concepts>
#include <vector>
#include <boost/json.hpp>

#include <odb/nullable.hxx>

#include "cmall/misc.hpp"
#include "cmall/db.hpp"

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

	inline std::string json_to_string(const boost::json::value& jv, bool lf = true)
	{
		if (lf) return boost::json::serialize(jv) + "\n";
		return boost::json::serialize(jv);
	}
}
