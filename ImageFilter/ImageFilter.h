#pragma once

#include <future>
#include <memory>
#include <winnt.h>
#include <d3d9types.h>


namespace DotEngine
{
	class Waifu2xFilterImpl;

	class ImageFilter
	{
		std::unique_ptr< Waifu2xFilterImpl > _impl;

	public:
		ImageFilter();
		~ImageFilter();

		HRESULT applyWaifu2x(LPVOID pBits, size_t width, size_t height, D3DFORMAT) const;
	};
}