#include "stdafx.h"

#include <cassert>
#include <chrono>
#include <string>

#include <Shlwapi.h>

#define HAVE_OPENCV
#include "waifu2x\include\opencv2\core\hal\interface.h"

#include "DirectXTex\DirectXTex.h"
#include "waifu2x\src\w2xconv.h"
#include "ImageFilter.h"




namespace DotEngine
{
	class Waifu2xFilterImpl
	{
		W2XConv* _converter{};
		const int _denoise_level = 3;

	public:
		Waifu2xFilterImpl::Waifu2xFilterImpl() : _converter{ w2xconv_init(W2XConvGPUMode::W2XCONV_GPU_AUTO, 1, 0) }
		{
			TCHAR filePath[MAX_PATH]{};
			GetModuleFileName(NULL, filePath, _countof(filePath));

			PathRemoveFileSpec(filePath);
			PathAppend(filePath, TEXT("models_rgb"));

			if (auto error = w2xconv_load_model(_denoise_level, _converter, filePath)) {
				assert(false);

				_check_for_errors(_converter, error);
				throw std::invalid_argument("invalid model path");
			}
		}

		Waifu2xFilterImpl::~Waifu2xFilterImpl()
		{
			w2xconv_fini(_converter);
		}

		HRESULT filter(LPVOID pBits, size_t width, size_t height, bool has_alpha) const
		{
			double scale = 1.f;
			int block_size{};

			if (auto error = w2xconv_convert_memory(_converter, width, height, pBits, _denoise_level, scale, block_size, has_alpha, CV_8UC4)) {
				assert(false);

				_check_for_errors(_converter, error);
				return E_FAIL;
			}

			return S_OK;
		}

	private:
		void _check_for_errors(W2XConv* converter, int error) const
		{
			if (error)
			{
				char *err = w2xconv_strerror(&converter->last_error);
				std::string errorMessage(err);
				w2xconv_free(err);
				throw std::runtime_error(errorMessage);
			}
		}

	};


	ImageFilter::ImageFilter() : _impl{ new Waifu2xFilterImpl }
	{}

	ImageFilter::~ImageFilter()
	{
		_impl.reset();
	}

	HRESULT ImageFilter::applyWaifu2x(const D3DLOCKED_RECT& srcLockedRect, D3DLOCKED_RECT& dstLockedRect, size_t width, size_t height, D3DFORMAT imageFormat) const
	{
		bool has_alpha{};

		switch (imageFormat) {
		case D3DFMT_A4R4G4B4:
			has_alpha = true;
		case D3DFMT_X4R4G4B4:
			{
				auto getTime = []() {
					return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
				};
				auto oldTime = getTime();

				// 32bpp 그림으로 바꾸기 위해 담는다
				DirectX::ScratchImage highColorImage;
				highColorImage.Initialize2D(DXGI_FORMAT_B4G4R4A4_UNORM, width, height, 1, 1);

				static_assert(sizeof(WORD) == 2, "invalid size");

				copyFromSurfaceMemory(highColorImage.GetPixels(), srcLockedRect.pBits, width, height, srcLockedRect.Pitch, 16);

				//DirectX::SaveToDDSFile(*highColorImage.GetImage(0, 0, 0), 0, std::wstring(L"D:\\study\\im.high.dds").c_str());

				auto format = (has_alpha ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_B8G8R8X8_UNORM);
				DirectX::ScratchImage trueColorImage;

				// 트루 컬러 그림으로 바꾼다
				if (FAILED(DirectX::Convert(highColorImage.GetImages(), highColorImage.GetImageCount(), highColorImage.GetMetadata(), format, DirectX::TEX_FILTER_FLAGS::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, trueColorImage))) {
					assert(false);
					return E_FAIL;
				}

				auto curTime = getTime();
				auto conversionTime = curTime - oldTime;

				//DirectX::SaveToDDSFile(*trueColorImage.GetImage(0, 0, 0), 0, std::wstring(L"D:\\study\\im.true.dds").c_str());

				if (FAILED(_impl->filter(trueColorImage.GetPixels(), width, height, has_alpha))) {
					return E_FAIL;
				}

				auto filteringTime = getTime() - curTime;

				copyToSurfaceMemory(dstLockedRect.pBits, trueColorImage.GetPixels(), width, height, dstLockedRect.Pitch, 32);

				std::string name = std::to_string(width) + "x" + std::to_string(height);
				std::string log = name + "\t" + std::to_string(conversionTime.count()) + "\t" + std::to_string(filteringTime.count()) + "\n";

				OutputDebugString(log.c_str());
				break;
			}
		}
		
		// format
		// width x height	convert		filter

		return S_OK;
	}

	HRESULT ImageFilter::applyWaifu2x(D3DLOCKED_RECT& lockedRect, size_t width, size_t height, D3DFORMAT imageFormat) const
	{
		bool has_alpha{};

		switch (imageFormat) {
		case D3DFMT_A4R4G4B4:
			has_alpha = true;
		case D3DFMT_X4R4G4B4:
			{
				// 32bpp 그림으로 바꾸기 위해 담는다
				DirectX::ScratchImage highColorImage;
				highColorImage.Initialize2D(DXGI_FORMAT_B4G4R4A4_UNORM, width, height, 1, 1);

				copyFromSurfaceMemory(highColorImage.GetPixels(), lockedRect.pBits, width, height, lockedRect.Pitch, 16);

				auto format = (has_alpha ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_B8G8R8X8_UNORM);
				DirectX::ScratchImage trueColorImage;

				// 트루 컬러 그림으로 바꾼다
				if (FAILED(DirectX::Convert(highColorImage.GetImages(), highColorImage.GetImageCount(), highColorImage.GetMetadata(), format, DirectX::TEX_FILTER_FLAGS::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, trueColorImage))) {
					assert(false);
					return E_FAIL;
				}

				if (FAILED(_impl->filter(trueColorImage.GetPixels(), width, height, has_alpha))) {
					return E_FAIL;
				}

				// 다시 하이 컬러 그림으로 바꿈
				if (FAILED(DirectX::Convert(trueColorImage.GetImages(), trueColorImage.GetImageCount(), trueColorImage.GetMetadata(), DXGI_FORMAT_B4G4R4A4_UNORM, DirectX::TEX_FILTER_FLAGS::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, highColorImage))) {
					return E_FAIL;
				}

				copyToSurfaceMemory(lockedRect.pBits, trueColorImage.GetPixels(), width, height, lockedRect.Pitch, 16);
			}
			// 32bpp 그림은 디더링되지 않은 것으로 간주하고 필터 처리도 하지 않는다
		case D3DFMT_A8R8G8B8:
		case D3DFMT_X8R8G8B8:
			return S_OK;
		default:
			assert(false);
			return E_FAIL;
		}

		return S_OK;
	}

	void ImageFilter::copyFromSurfaceMemory(LPVOID pDst, LPVOID pSrc, size_t width, size_t height, UINT pitch, UINT bitPerPixel) const
	{
		auto byteSize = bitPerPixel / 8;
		auto recordSize = byteSize * width;

		assert(pitch >= recordSize);

		for (UINT i{}; i < height; ++i) {
			LPVOID pData = static_cast<LPBYTE>(pSrc) + pitch * i;

			memcpy(reinterpret_cast<LPBYTE>(pDst) + recordSize * i, pData, recordSize);
		}
	}

	void ImageFilter::copyToSurfaceMemory(LPVOID pDst, LPVOID pSrc, size_t width, size_t height, UINT pitch, UINT bitPerPixel) const
	{
		auto byteSize = bitPerPixel / 8;
		auto recordSize = byteSize * width;

		assert(pitch >= recordSize);

		for (UINT i{}; i < height; ++i) {
			LPVOID pData = static_cast<LPBYTE>(pDst) + recordSize * i;

			memcpy(pData, static_cast<LPBYTE>(pSrc) + recordSize * i, recordSize);
		}
	}
}