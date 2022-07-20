#pragma once

#include "IImageFilter.h"


namespace Flat
{
	class _ImageFilter;

	class _Token : public IToken
	{
		_ImageFilter& _imageFilter;
		size_t const _index{};
		bool _valid{ true };

	public:
		_Token(_ImageFilter& imageFilter, size_t index);
		~_Token();
		_Token(const _Token&) = delete;
		_Token& operator=(const _Token&) = delete;

		// 허상 포인터에 접근하는 걸 막기 위해 쓰인다
		inline void invalidate() { _valid = false; }
	};
}
