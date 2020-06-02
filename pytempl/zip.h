#pragma once

#include <set>
#include <tuple>
#include <type_traits>

#include "common\_container_adaptor.h"

/*
https://docs.python.org/3/library/functions.html#zip

// mission
iterator supports not yet

// usage
std::vector<int> iv{ 1, 2, 3, 4 ,5 };
std::vector<float> fv{ 1, 2, 3, 4, 5 };
auto z = pytempl::zip(iv, fv);

for(auto v : z ) {
int i{};
float f{};

std::tie( i, f ) = v;
auto& v = std::get<0>; // int&
}
*/
namespace pytempl {
	template<typename T>
	class _Container_iterator
	{
	public:
		T m_iterator;

		using value_type = typename T::reference;

	public:
		_Container_iterator( T& t ) : m_iterator{ t }
		{}

		value_type get_value()
		{
			return *m_iterator;
		}

		bool iterate()
		{
			++m_iterator;

			return true;
		}

		bool operator==( _Container_iterator<T>& _iterator )
		{
			return _iterator != m_iterator;
		}
	};

	template<typename...Ts>
	class Container_iterator : _Container_iterator<Ts>...
	{
	public:
		using value_type = std::tuple<typename _Container_iterator<Ts>::value_type...>;
		using iterator_category = std::forward_iterator_tag;

	public:
		Container_iterator( Ts&&...ts ) : _Container_iterator<Ts>( ts )...
		{}

		value_type operator*()
		{
			return{ _Container_iterator<Ts>::get_value()... };
		}

		void operator++()
		{
			std::initializer_list<bool>{ _Container_iterator<Ts>::iterate()... };
		}

		bool operator==( Container_iterator<Ts...>& iterator )
		{
			return _is_equal<Ts...>( iterator );
		}

		bool operator!=( Container_iterator<Ts...>& iterator )
		{
			return !operator==( iterator );
		}

	private:
		template<typename A, typename...As>
		bool _is_equal( Container_iterator<Ts...>& iterator )
		{
			auto lhs{ _Container_iterator<A>::m_iterator };
			auto rhs{ static_cast<_Container_iterator<A>>( iterator ).m_iterator };

			if ( lhs == rhs ) {
				return true;
			}

			auto r{ std::set<bool>{ _is_equal<As>( iterator )... } };

			if ( r.count( true ) ) {
				return true;
			}
			else {
				return false;
			}
		}
	};

	template<typename T>
	class _Reference_container : public _Container_adaptor<T>
	{
		T& m_container;
		using iterator_type = typename _Container_adaptor<T>::iterator_type;

	public:
		_Reference_container( T& t ) : m_container{ t }
		{}

		iterator_type begin()
		{
			return _Container_adaptor<T>::begin( m_container );
		}

		iterator_type end()
		{
			return _Container_adaptor<T>::end( m_container );
		}
	};

	template<typename...Ts>
	class _Zip : public _Reference_container<Ts>...
	{
	public:
		using iterator = Container_iterator<typename _Reference_container<Ts>::iterator_type...>;

	public:
		_Zip( Ts&...ts ) : _Reference_container<Ts>( ts )...
		{}

		iterator begin()
		{
			return iterator{ _Reference_container<Ts>::begin()... };
		}

		iterator end()
		{
			return iterator{ _Reference_container<Ts>::end()... };
		}
	};

	template<typename...Ts>
	auto zip( Ts&&...ts ) noexcept
	{
		return _Zip<Ts...>{ ts... };
	}
};