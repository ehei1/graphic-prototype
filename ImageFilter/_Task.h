#pragma once

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <d3d9.h>
#include "DirectXTex\DirectXTex.h"
#include "IImageFilter.h"


namespace Flat
{
	class _ImageFilter;

	struct _Task
	{
		using Function_type = std::function<DirectX::ScratchImage(DirectX::ScratchImage&&)>;
		Function_type _function;
		IImageFilter::Filter_callback_type _callback;

		LPDIRECT3DTEXTURE9 _pTexture{};
		size_t const _index;
		static size_t _unique_index;

		std::future<DirectX::ScratchImage> _future;
		std::weak_ptr<_Token> _weak_token_ptr;
		bool _cancelled{};
		bool _token_issued{};
		bool _async_started{};

		std::chrono::system_clock::time_point _reserved_time;
		std::chrono::system_clock::time_point _started_time;

	public:
		_Task(LPDIRECT3DTEXTURE9 pTexutre, Function_type function, IImageFilter::Filter_callback_type callback);
		~_Task();
		_Task(const _Task&) = delete;
		_Task(_Task&&) = delete;
		_Task& operator=(const _Task&) = delete;
		_Task& operator=(_Task&&) = delete;

		std::shared_ptr<IToken> issue_token(_ImageFilter& imageFilter);
		void start(DirectX::ScratchImage&&);
	};
}