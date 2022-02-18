
#pragma once

#include <boost/json.hpp>

#include <odb/nullable.hxx>

#include "cmall/misc.hpp"
#include "cmall/db.hpp"

namespace jsutil
{
	template <typename T, typename = void>
	struct is_vector : std::false_type {};

	template <typename T>
	struct is_vector<T
	, std::void_t<decltype(std::declval<T>().data())
		, decltype(std::declval<T>().size())>> : std::true_type {};

	template<>
	struct is_vector<std::string> : std::false_type {};

	template<typename T>
	inline boost::json::value value_to_json(const T& t)
	{
		return boost::json::value(t);
	}

	template<typename T>
	inline boost::json::value vector_to_json(std::vector<T>& vector_of_t)
	{
		boost::json::array array;

		for (const auto& t :  vector_of_t)
			array.push_back(value_to_json<T>(t));

		return array;
	}

	template<typename T>
	inline boost::json::value to_json(T&& t)
	{
		if constexpr ( is_vector<T>::value )
			return vector_to_json(t);
		else
			return value_to_json(t);
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
		boost::json::string j(to_string(t));
		return j;
	}

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

		inline json::value get(char const* key, json::value default_value) const
		{
			try {
				if (obj_.contains(key))
					return obj_.at(key);
			}
			catch (const std::invalid_argument&)
			{}

			return default_value;
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

}
