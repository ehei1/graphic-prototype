#pragma once

#include <functional>
#include <future>
#include <memory>
#include <d3d9.h>
#include "DirectXTex\DirectXTex.h"
#include "IImageFilter.h"


namespace DotEngine
{
	class _ImageFilter;

	struct _Task
	{
		using Function_type = std::function<DirectX::ScratchImage(DirectX::ScratchImage&&)>;
		Function_type _function;
		IImageFilter::Callback_type _callback;

		LPDIRECT3DTEXTURE9 _pSrcTexture{};
		LPDIRECT3DTEXTURE9 _pDstTexture{};
		size_t const _index;
		static size_t _unique_index;

		std::future<DirectX::ScratchImage> _future;
		std::weak_ptr<_Token> _weak_token_ptr;
		bool _cancelled{};
		bool _token_issued{};
		bool _async_started{};

	public:
		_Task(LPDIRECT3DTEXTURE9 pSrcTexture, LPDIRECT3DTEXTURE9 pDstTexture, Function_type function, IImageFilter::Callback_type callback);
		~_Task();

		std::shared_ptr<IToken> issue_token(_ImageFilter& imageFilter);
		void start(DirectX::ScratchImage&&);
	};
}