#pragma once

#include <functional>
#include <future>
#include <queue>
#include <memory>
#include <unordered_map>

#include "IImageFilter.h"


namespace DirectX {
	class ScratchImage;
}

namespace DotEngine
{
	struct _Task;

	class _Waifu2xImpl;
	class _ImageFilter;
	class _Token;

	class _ImageFilter : public IImageFilter
	{
		friend _Token;

		using Token_index = size_t;

		std::unique_ptr< _Waifu2xImpl > _impl;
		std::queue<Token_index> _task_indices;
		std::unordered_map<Token_index, std::shared_ptr<_Task>> _tasks;

	public:
		_ImageFilter();
		virtual ~_ImageFilter();

		std::shared_ptr<IToken> filter_async(LPDIRECT3DTEXTURE9 pSrcTexture, LPDIRECT3DTEXTURE9 pDstTexture, int denoise_level, float scale, Callback_type) override final;

		void update() override final;

		inline size_t task_size() const override final { return _tasks.size(); }

	protected:
		void _remove_task(Token_index index);

	private:
		void __copy_from_surface_memory(LPVOID pDst, LPVOID pSrc, size_t width, size_t height, UINT pitch, UINT bitPerPixel) const;
		void __copy_to_surface_memory(LPVOID pDst, LPVOID pSrc, size_t width, size_t height, UINT pitch, UINT bitPerPixel) const;

		DirectX::ScratchImage __apply_waifu2x_async(size_t width, size_t height, bool has_alpha, int denoise_level, float scale, DirectX::ScratchImage&&);
	};
}