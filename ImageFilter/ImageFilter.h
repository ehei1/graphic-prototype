#pragma once

#include <future>
#include <memory>
#include <winnt.h>
#include <d3d9types.h>



namespace DotEngine
{
	class Waifu2xFilterImpl;

	struct Token
	{
		size_t _index{};

		Token(size_t index) : _index{ index }
		{}
	};

	class ImageFilter
	{
		friend Token;

		std::unique_ptr< Waifu2xFilterImpl > _impl;

	public:
		ImageFilter();
		~ImageFilter();

		HRESULT applyWaifu2x(D3DLOCKED_RECT& lockedRect, size_t width, size_t height, D3DFORMAT) const;
		
		HRESULT applyWaifu2x(const D3DLOCKED_RECT& srcLockedRect, D3DLOCKED_RECT& dstLockedRect, size_t width, size_t height, D3DFORMAT) const;

	private:
		void copyFromSurfaceMemory(LPVOID pDst, LPVOID pSrc, size_t width, size_t height, UINT pitch, UINT bitPerPixel) const;
		void copyToSurfaceMemory(LPVOID pDst, LPVOID pSrc, size_t width, size_t height, UINT pitch, UINT bitPerPixel) const;
	};
}