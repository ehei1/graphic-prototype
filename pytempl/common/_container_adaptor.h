#pragma once

#include <type_traits>

namespace pytempl
{
	template<typename T>
	class __Container_adaptor
	{
	protected:
		using iterator_type = typename T::iterator;

	public:
		iterator_type begin(T& t)
		{
			return std::begin(t);
		}

		iterator_type end(T& t)
		{
			return std::end(t);
		}
	};

	template<typename T>
	class __Container_adaptor<T const>
	{
	protected:
		using iterator_type = typename T::const_iterator;

	public:
		iterator_type begin(T const& t)
		{
			return std::cbegin(t);
		}

		iterator_type end(T const& t)
		{
			return std::cend(t);
		}
	};

	template<typename T, typename U=std::remove_reference_t<T>>
	class _Container_adaptor : public __Container_adaptor<U>
	{};
}