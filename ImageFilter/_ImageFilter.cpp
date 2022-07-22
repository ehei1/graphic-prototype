#include "stdafx.h"

#include <cassert>
#include <chrono>
#include <string>
#include <vector>

#include "_ImageFilter.h"
#include "_Task.h"
#include "_Token.h"


namespace Flat
{
	class _Waifu2xImpl
	{
		std::vector<W2XConv*> _converters{ nullptr, nullptr, nullptr, nullptr };

	public:
		_Waifu2xImpl() 
		{}

		~_Waifu2xImpl()
		{
			auto destory_converter = [](W2XConv* converter) { 
				if (converter) { 
					w2xconv_fini(converter); 
				} 
			};
			std::for_each(std::begin(_converters), std::end(_converters), destory_converter);
		}

		_Waifu2xImpl(_Waifu2xImpl const&) = delete;
		_Waifu2xImpl(_Waifu2xImpl&&) = delete;
		_Waifu2xImpl& operator=(_Waifu2xImpl&) = delete;
		_Waifu2xImpl& operator=(_Waifu2xImpl&&) = delete;

		DirectX::ScratchImage filter(DirectX::ScratchImage&& sourceImage, bool has_alpha, int denoise_level, float scale)
		{
			int block_size{};
			auto converter{ _get_converter(denoise_level) };
			auto& metaData = sourceImage.GetMetadata();
			DirectX::ScratchImage destImage;
			destImage.Initialize2D(metaData.format, metaData.width * 2, metaData.height * 2, 1, 1);

			if (auto error = w2xconv_convert_memory2(converter, metaData.width, metaData.height, destImage.GetPixels(), sourceImage.GetPixels(), denoise_level, scale, block_size, has_alpha, CV_8UC4)) {
				assert(false);

				_check_for_errors(converter, error);
				throw std::exception();
			}

			return destImage;
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


	_ImageFilter::_ImageFilter() : _impl{ std::make_unique<_Waifu2xImpl>() }
	{}

	std::shared_ptr<IToken> _ImageFilter::filter_async(LPDIRECT3DTEXTURE9 pTexture, int denoise_level, float scale, Filter_callback_type callback)
	{
		D3DSURFACE_DESC surface_desc{};
		
		if (FAILED(pTexture->GetLevelDesc(0, &surface_desc))) {
			throw std::runtime_error("D3DSURFACE_DESC getting is failed");
		}

		bool has_alpha{};

		switch (surface_desc.Format) {
		case D3DFMT_A4R4G4B4:
		case D3DFMT_A8R8G8B8:
			has_alpha = true;
		case D3DFMT_X4R4G4B4:
		case D3DFMT_X8R8G8B8:
		{
			if(_log_callback) {
				std::string log = "[" + std::to_string(surface_desc.Width) + "x" + std::to_string(surface_desc.Height) + "]" + "waifu2x reserved";
				_log_callback(log);
			}

			auto functor = std::bind(&_ImageFilter::__apply_waifu2x_async, this, has_alpha, denoise_level, scale, std::placeholders::_1);
			auto task = std::make_shared<_Task>(pTexture, functor, callback);
			_tasks[task->_index] = task;
			_task_indices.push(task->_index);

			return task->issue_token(*this);
		}
		default:
			assert(false);
			return nullptr;
		}
	}

	void _ImageFilter::update(LPDIRECT3DDEVICE9 pDevice)
	{
		if (_task_indices.empty()) {
			return;
		}

		auto task_index = _task_indices.front();
		auto task_iter = _tasks.find(task_index);

		// it already was cancelled
		if (std::end(_tasks) == task_iter) {
			_task_indices.pop();
			return;
		}

		auto task = task_iter->second;

		// started task
		if (task->_async_started) {
			// it finished
			if (std::future_status::ready == task->_future.wait_until(std::chrono::steady_clock::now())) {
				if (!task->_cancelled) {
					// if token is alive then invoke callback
					if (auto token_ptr = task->_weak_token_ptr.lock()) {
						token_ptr->invalidate();

						D3DSURFACE_DESC surface_desc{};

						if (SUCCEEDED(task->_pTexture->GetLevelDesc(0, &surface_desc))) {
							auto trueColorImage = task->_future.get();
							DirectX::Blob blob;

							if (SUCCEEDED(DirectX::SaveToDDSMemory(*trueColorImage.GetImages(), 0, blob))) {
								LPDIRECT3DTEXTURE9 pOutTexture{};

								auto multiply_four = [](UINT value) {
									constexpr UINT base{ 4 };
									auto result = value / base * 4;

									return result == value ? result : result + 4;
								};
								auto& metaData = trueColorImage.GetMetadata();
								auto width = multiply_four(metaData.width);
								auto height = multiply_four(metaData.height);

								if (FAILED(D3DXCreateTextureFromFileInMemoryEx(pDevice, blob.GetBufferPointer(), blob.GetBufferSize(), width, height, 0, surface_desc.Usage, D3DFMT_DXT5, surface_desc.Pool, D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, NULL, NULL, &pOutTexture))) {
									throw std::runtime_error("DXT texture failed to create");
								}
								
								task->_callback(pOutTexture);

								if (_log_callback) {
									auto elasped_time = std::chrono::system_clock::now() - task->_reserved_time;
									auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elasped_time);
									std::string log = "[" + std::to_string(width) + "x" + std::to_string(height) + "]" + "waifu2x done (" + std::to_string(elapsed_ms.count()) + "ms)";

									_log_callback(log);
								}
							}
						}
					}
				}

				_task_indices.pop();
				_tasks.erase(task_index);
			}
		}
		// if there is no running task, start reserved task at once
		else {
			assert(false == task->_cancelled);

			D3DSURFACE_DESC surface_desc{};

			if (SUCCEEDED(task->_pTexture->GetLevelDesc(0, &surface_desc))) {
				D3DLOCKED_RECT locked_rect{};

				if (SUCCEEDED(task->_pTexture->LockRect(0, &locked_rect, NULL, 0))) {

					DirectX::ScratchImage highColorImage;
					{
						auto imageFormat = DXGI_FORMAT_UNKNOWN;
						auto bitPerPixel = 0u;
						switch (surface_desc.Format)
						{
						case D3DFMT_A4R4G4B4:
						case D3DFMT_X4R4G4B4:
							imageFormat = DXGI_FORMAT_B4G4R4A4_UNORM;
							bitPerPixel = 16;
							break;
						case D3DFMT_A8B8G8R8:
						case D3DFMT_A8R8G8B8:
						case D3DFMT_X8B8G8R8:
						case D3DFMT_X8R8G8B8:
							imageFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
							bitPerPixel = 32;
							break;
						default:
							throw std::exception();
							break;
						}

						highColorImage.Initialize2D(imageFormat, surface_desc.Width, surface_desc.Height, 1, 1);
						__copy_from_surface_memory(highColorImage.GetPixels(), locked_rect.pBits, surface_desc.Width, surface_desc.Height, locked_rect.Pitch, bitPerPixel);
					}

					task->start(std::move(highColorImage));
					task->_pTexture->UnlockRect(0);
					task->_reserved_time = std::chrono::system_clock::now();
					
					if(_log_callback) {
						auto elapsed_time = std::chrono::system_clock::now() - task->_reserved_time;
						auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time);
						
						std::string log = "[" + std::to_string(surface_desc.Width) + "x" + std::to_string(surface_desc.Height) + "]" + "waifu2x started (" + std::to_string(elapsed_ms.count()) + "ms)";
						_log_callback(log);
					}
				}
			}
		}
	}

	void _ImageFilter::_remove_task(Token_index index)
	{
		auto task_iter = _tasks.find(index);

		if (std::end(_tasks) != task_iter) {
			auto& task = task_iter->second;

			// during async working if you elimitate future then will be block process. moreover waiting result should be in the thread that created DirectX device. In conclusion such action will be called freezing. To avoid it started task don't touch until finish.
			if (task->_async_started) {
				task_iter->second->_cancelled = true;
			}
			else {
				_tasks.erase(task_iter);
			}
		}
	}

	DirectX::ScratchImage _ImageFilter::__apply_waifu2x_async(bool has_alpha, int denoise_level, float scale, DirectX::ScratchImage&& highColorImage)
	{
		auto format = highColorImage.GetMetadata().format;

		if (format != DXGI_FORMAT_B8G8R8A8_UNORM && format != DXGI_FORMAT_B8G8R8X8_UNORM)
		{
			auto changingFormat = (has_alpha ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_B8G8R8X8_UNORM);
			DirectX::ScratchImage trueColorImage;

			// change to true color image
			if (FAILED(DirectX::Convert(highColorImage.GetImages(), highColorImage.GetImageCount(), highColorImage.GetMetadata(), changingFormat, DirectX::TEX_FILTER_FLAGS::TEX_FILTER_POINT, DirectX::TEX_THRESHOLD_DEFAULT, trueColorImage))) {
				throw std::runtime_error("image conversion is failed");
			}

			highColorImage = std::move(trueColorImage);
		}

		return _impl->filter(std::move(highColorImage), has_alpha, denoise_level, scale);
	}

	void _ImageFilter::__copy_from_surface_memory(LPVOID pDst, LPVOID pSrc, size_t width, size_t height, UINT pitch, UINT bitPerPixel) const
	{
		auto byteSize = bitPerPixel / 8;
		auto recordSize = byteSize * width;

		assert(pitch >= recordSize);

		for (size_t i{}; i < height; ++i) {
			LPVOID pData = static_cast<LPBYTE>(pSrc) + pitch * i;

			memcpy(reinterpret_cast<LPBYTE>(pDst) + recordSize * i, pData, recordSize);
		}
	}

	void _ImageFilter::__copy_to_surface_memory(LPVOID pDst, LPVOID pSrc, size_t width, size_t height, UINT pitch, UINT bitPerPixel) const
	{
		auto byteSize = bitPerPixel / 8;
		auto recordSize = byteSize * width;

		assert(pitch >= recordSize);

		for (size_t i{}; i < height; ++i) {
			LPVOID pData = static_cast<LPBYTE>(pDst) + recordSize * i;

			memcpy(pData, static_cast<LPBYTE>(pSrc) + recordSize * i, recordSize);
		}
	}

	std::shared_ptr<IImageFilter> ImageFilterFactory::createInstance()
	{
		return std::make_shared<_ImageFilter>();
	}
}