#include "stdafx.h"
#include "_Token.h"
#include "_ImageFilter.h"


namespace DotEngine
{
	_Token::_Token(_ImageFilter& imageFilter, size_t index) : _imageFilter{ imageFilter }, _index{ index }
	{}

	_Token::~_Token()
	{
		if (_valid) {
			_imageFilter._remove_task(_index);
		}
	}
}
