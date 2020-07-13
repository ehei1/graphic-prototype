#include "stdafx.h"

#include <cassert>
#include <chrono>
#include <string>
#include <vector>

#include <d3d9.h>
#include <Shlwapi.h>

#define HAVE_OPENCV
#include "waifu2x\include\opencv2\core\hal\interface.h"

#include "DirectXTex\DirectXTex.h"
#include "waifu2x\src\w2xconv.h"
#include "ImageFilter.h"


namespace DotEngine
{
	Token::Index ImageFilter::Task::_unique_index{};


	Token::~Token()
	{
		if (_valid) {
			_imageFilter._remove_task(_index);
		}
	}

	class Waifu2xFilterImpl
	{
		std::vector<W2XConv*> _converters{ nullptr, nullptr, nullptr, nullptr };

	public:
		Waifu2xFilterImpl::Waifu2xFilterImpl() 
		{}

		Waifu2xFilterImpl::~Waifu2xFilterImpl()
		{
			auto destory_converter = [](W2XConv* converter) { 
				if (converter) { 
					w2xconv_fini(converter); 
				} 
			};
			std::for_each(std::begin(_converters), std::end(_converters), destory_converter);
		}

		HRESULT filter(LPVOID pBits, size_t width, size_t height, bool has_alpha, int denoise_level, float scale)
		{
			int block_size{};
			auto converter{ _get_converter(denoise_level) };

			if (auto error = w2xconv_convert_memory(converter, width, height, pBits, denoise_level, scale, block_size, has_alpha, CV_8UC4)) {
				assert(false);

				_check_for_errors(converter, error);
				return E_FAIL;
			}

			return S_OK;
		}

	private:
		W2XConv* _get_converter(int denoise_level)
		{
			assert(static_cast<int>(_converters.size()) > denoise_level);

			auto& converter = _converters[denoise_level];

			if (!converter) {
				auto job_number = 1;
				auto log_level = 0; // [0,4]
				converter = w2xconv_init(W2XConvGPUMode::W2XCONV_GPU_AUTO, job_number, log_level);

				TCHAR filePath[MAX_PATH]{};
				GetModuleFileName(NULL, filePath, _countof(filePath));

				PathRemoveFileSpec(filePath);
				PathAppend(filePath, TEXT("models_rgb"));

				if (auto error = w2xconv_load_model(denoise_level, converter, filePath)) {
					assert(false);

					_check_for_errors(converter, error);
					throw std::invalid_argument("invalid model path");
				}
			}

			return converter;
		}

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

	void ImageFilter::Task::start(DirectX::ScratchImage&& image)
	{
		if (_async_started) {
			throw std::runtime_error("async job started already");
		}
		else {
			_future = std::async(std::launch::async, _function, std::move(image));
			_async_started = true;
		}
	}


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

				__copy_from_surface_memory(highColorImage.GetPixels(), srcLockedRect.pBits, width, height, srcLockedRect.Pitch, 16);

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

				if (FAILED(_impl->filter(trueColorImage.GetPixels(), width, height, has_alpha, 3, 1.f))) {
					return E_FAIL;
				}

				auto filteringTime = getTime() - curTime;

				__copy_to_surface_memory(dstLockedRect.pBits, trueColorImage.GetPixels(), width, height, dstLockedRect.Pitch, 32);

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

				__copy_from_surface_memory(highColorImage.GetPixels(), lockedRect.pBits, width, height, lockedRect.Pitch, 16);

				auto format = (has_alpha ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_B8G8R8X8_UNORM);
				DirectX::ScratchImage trueColorImage;

				// 트루 컬러 그림으로 바꾼다
				if (FAILED(DirectX::Convert(highColorImage.GetImages(), highColorImage.GetImageCount(), highColorImage.GetMetadata(), format, DirectX::TEX_FILTER_FLAGS::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, trueColorImage))) {
					assert(false);
					return E_FAIL;
				}

				if (FAILED(_impl->filter(trueColorImage.GetPixels(), width, height, has_alpha, 3, 1.f))) {
					return E_FAIL;
				}

				// 다시 하이 컬러 그림으로 바꿈
				if (FAILED(DirectX::Convert(trueColorImage.GetImages(), trueColorImage.GetImageCount(), trueColorImage.GetMetadata(), DXGI_FORMAT_B4G4R4A4_UNORM, DirectX::TEX_FILTER_FLAGS::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, highColorImage))) {
					return E_FAIL;
				}

				__copy_to_surface_memory(lockedRect.pBits, trueColorImage.GetPixels(), width, height, lockedRect.Pitch, 16);
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

	std::shared_ptr<Token> ImageFilter::filter_async(LPDIRECT3DTEXTURE9 pSrcTexture, LPDIRECT3DTEXTURE9 pDstTexture, int denoise_level, float scale, Callback_type callback)
	{
		D3DSURFACE_DESC surface_desc{};
		
		if (FAILED(pSrcTexture->GetLevelDesc(0, &surface_desc))) {
			throw std::runtime_error("D3DSURFACE_DESC getting is failed");
		}

		bool has_alpha{};

		switch (surface_desc.Format) {
		case D3DFMT_A4R4G4B4:
			has_alpha = true;
		case D3DFMT_X4R4G4B4:
		{
			auto functor = std::bind(&ImageFilter::__apply_waifu2x_async, this, surface_desc.Width, surface_desc.Height, has_alpha, denoise_level, scale, std::placeholders::_1);
			auto task = std::make_shared<Task>(pSrcTexture, pDstTexture, functor, callback);
			_tasks[task->_index] = task;
			_task_indices.push(task->_index);

			return task->issue_token(*this);
		}	
		case D3DFMT_A8B8G8R8:
		case D3DFMT_X8B8G8R8:
			return nullptr;
		default:
			throw std::invalid_argument("some formats do not support");
		}
	}

	void ImageFilter::update()
	{
		if (_task_indices.empty()) {
			return;
		}

		auto task_index = _task_indices.front();
		auto task_iter = _tasks.find(task_index);

		// 이미 취소된 작업인다
		if (std::end(_tasks) == task_iter) {
			_task_indices.pop();
			return;
		}

		auto task = task_iter->second;

		// 시작된 작업
		if (task->_async_started) {
			// 작업 끝
			if (std::future_status::ready == task->_future.wait_until(std::chrono::system_clock::now())) {
				auto trueColorImage = task->_future.get();

				{
					D3DLOCKED_RECT locked_rect{};
					
					if (SUCCEEDED(task->_pDstTexture->LockRect(0, &locked_rect, NULL, 0))) {
						D3DSURFACE_DESC surface_desc{};
						
						if (SUCCEEDED(task->_pDstTexture->GetLevelDesc(0, &surface_desc))) {
							__copy_to_surface_memory(locked_rect.pBits, trueColorImage.GetPixels(), surface_desc.Width, surface_desc.Height, locked_rect.Pitch, 32);
							
							task->_callback(task->_pDstTexture);
						}
					}
				}

				_task_indices.pop();
				_tasks.erase(task_index);
			}
		}
		// 작업을 시작시킨다
		else {
			assert(false == task->_cancelled);

			D3DLOCKED_RECT locked_rect{};
			constexpr UINT mipmap_level{};

			if (SUCCEEDED(task->_pSrcTexture->LockRect(mipmap_level, &locked_rect, NULL, 0))) {
				D3DSURFACE_DESC surface_desc{};

				if (SUCCEEDED(task->_pSrcTexture->GetLevelDesc(mipmap_level, &surface_desc))) {
					// memory copy
					DirectX::ScratchImage highColorImage;
					{
						highColorImage.Initialize2D(DXGI_FORMAT_B4G4R4A4_UNORM, surface_desc.Width, surface_desc.Height, 1, 1);

						__copy_from_surface_memory(highColorImage.GetPixels(), locked_rect.pBits, surface_desc.Width, surface_desc.Height, locked_rect.Pitch, 16);
						task->_pSrcTexture->UnlockRect(mipmap_level);
					}

					task->start(std::move(highColorImage));
				}
			}
		}
	}

	void ImageFilter::_remove_task(Token::Index index)
	{
		auto task_iter = _tasks.find(index);

		if (std::end(_tasks) != task_iter) {
			auto& task = task_iter->second;

			// 비동기 작업이 시작된 상태에서 future를 소멸시키면 처리가 봉쇄된다. 결과 대기를 DirectX 장치가 생성된 스레드에서 해야한다. 따라서 이런 행위는 프레임 멈춤을 유발한다. 이를 우회하기 위해 한번 시작된 작업은 끝날 때까지 봐준다.
			if (task->_async_started) {
				task_iter->second->_cancelled = true;
			}
			else {
				_tasks.erase(task_iter);
			}
		}
	}

	DirectX::ScratchImage ImageFilter::__apply_waifu2x_async(size_t width, size_t height, bool has_alpha, int denoise_level, float scale, DirectX::ScratchImage&& highColorImage)
	{
		auto format = (has_alpha ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_B8G8R8X8_UNORM);
		DirectX::ScratchImage trueColorImage;

		// 트루 컬러 그림으로 바꾼다
		if (FAILED(DirectX::Convert(highColorImage.GetImages(), highColorImage.GetImageCount(), highColorImage.GetMetadata(), format, DirectX::TEX_FILTER_FLAGS::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, trueColorImage))) {
			throw std::runtime_error("image conversion is failed");
		}
		else if (FAILED(_impl->filter(trueColorImage.GetPixels(), width, height, has_alpha, denoise_level, scale))) {
			throw std::runtime_error("image conversion is failed");
		}

		return trueColorImage;
	}

	void ImageFilter::__copy_from_surface_memory(LPVOID pDst, LPVOID pSrc, size_t width, size_t height, UINT pitch, UINT bitPerPixel) const
	{
		auto byteSize = bitPerPixel / 8;
		auto recordSize = byteSize * width;

		assert(pitch >= recordSize);

		for (UINT i{}; i < height; ++i) {
			LPVOID pData = static_cast<LPBYTE>(pSrc) + pitch * i;

			memcpy(reinterpret_cast<LPBYTE>(pDst) + recordSize * i, pData, recordSize);
		}
	}

	void ImageFilter::__copy_to_surface_memory(LPVOID pDst, LPVOID pSrc, size_t width, size_t height, UINT pitch, UINT bitPerPixel) const
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