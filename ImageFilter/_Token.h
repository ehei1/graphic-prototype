#pragma once

#include "IImageFilter.h"


namespace DotEngine
{
	class _ImageFilter;

	class _Token : public IToken
	{
		_ImageFilter& _imageFilter;
		size_t const _index{};
		bool _valid{ true };

	public:
		_Token(_ImageFilter& imageFilter, size_t index);
		virtual ~_Token();

		// ��� �����Ϳ� �����ϴ� �� ���� ���� ���δ�
		inline void invalidate() { _valid = false; }
	};
}
