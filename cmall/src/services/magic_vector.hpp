
#pragma once

#include <vector>

template<typename T>
struct magic_vector : public std::vector<T>
{
public:
	magic_vector& operator += (const T& element)
	{
		std::vector<T>::push_back(element);
		return *this;
	}

	magic_vector& operator += (const magic_vector<T>& other_vector)
	{
		for (const T& element : other_vector)
			std::vector<T>::push_back(element);
		return *this;
	}
};
