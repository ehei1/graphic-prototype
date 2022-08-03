#include "stdafx.h"

#include <cassert>
#include <string>
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
			if (auto error = w2xconv_load_model(_denoise_level, _converter, TEXT(".\\models_rgb"))) {
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

	HRESULT ImageFilter::applyWaifu2x(LPVOID pBits, size_t width, size_t height, D3DFORMAT imageFormat) const
	{
		bool has_alpha{};

		switch (imageFormat) {
		case D3DFMT_A4R4G4B4:
			has_alpha = true;
		case D3DFMT_X4R4G4B4:
			{
				// 32bpp �׸����� �ٲٱ� ���� ��´�
				DirectX::ScratchImage highColorImage;
				highColorImage.Initialize2D(DXGI_FORMAT_B4G4R4A4_UNORM, width, height, 1, 1);
				memcpy(highColorImage.GetPixels(), pBits, highColorImage.GetPixelsSize());

				auto format = (has_alpha ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_B8G8R8X8_UNORM);
				DirectX::ScratchImage trueColorImage;

				// Ʈ�� �÷� �׸����� �ٲ۴�
				if (FAILED(DirectX::Convert(highColorImage.GetImages(), highColorImage.GetImageCount(), highColorImage.GetMetadata(), format, DirectX::TEX_FILTER_FLAGS::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, trueColorImage))) {
					assert(false);
					return E_FAIL;
				}

				if (FAILED(_impl->filter(trueColorImage.GetPixels(), width, height, has_alpha))) {
					return E_FAIL;
				}

				// �ٽ� ���� �÷� �׸����� �ٲ�
				if (FAILED(DirectX::Convert(trueColorImage.GetImages(), trueColorImage.GetImageCount(), trueColorImage.GetMetadata(), DXGI_FORMAT_B4G4R4A4_UNORM, DirectX::TEX_FILTER_FLAGS::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, highColorImage))) {
					return E_FAIL;
				}

				memcpy(pBits, highColorImage.GetPixels(), highColorImage.GetPixelsSize());
				break;
			}
			// 32bpp �׸��� ��������� ���� ������ �����ϰ� ���� ó���� ���� �ʴ´�
		case D3DFMT_A8R8G8B8:
		case D3DFMT_X8R8G8B8:
			return S_OK;
		default:
			assert(false);
			return E_FAIL;
		}

		return S_OK;
	}
}