#pragma once

#include <functional>
#include <future>
#include <queue>
#include <memory>
#include <unordered_map>
#include <winnt.h>
#include <d3d9types.h>
#include <d3d9.h>


namespace DirectX {
	class ScratchImage;
}


namespace DotEngine
{
	class Waifu2xFilterImpl;
	class ImageFilter;

	struct Token
	{
		using Index = size_t;
		ImageFilter& _imageFilter;
		Index const _index{};
		bool _valid{ true };

		Token(ImageFilter& imageFilter, Index index) : _imageFilter{ imageFilter }, _index{ index }
		{}
		~Token();

		inline void invalidate() { _valid = false; }
	};

	class ImageFilter
	{
		friend Token;
		using Callback_type = std::function<void(LPDIRECT3DTEXTURE9)>;

		struct Task
		{
			using Function_type = std::function<DirectX::ScratchImage(DirectX::ScratchImage&&)>;
			Function_type _function;
			Callback_type _callback;

			LPDIRECT3DTEXTURE9 _pSrcTexture{};
			LPDIRECT3DTEXTURE9 _pDstTexture{};
			Token::Index const _index;
			static Token::Index _unique_index;

			std::future<DirectX::ScratchImage> _future;
			std::weak_ptr<Token> _weak_token_ptr;
			bool _cancelled{};
			bool _token_issued{};
			bool _async_started{};

			Task(LPDIRECT3DTEXTURE9 pSrcTexture, LPDIRECT3DTEXTURE9 pDstTexture, Function_type function, Callback_type callback) : _pSrcTexture{ pSrcTexture }, _pDstTexture{ pDstTexture }, _function{ function }, _callback{ callback }, _index { ++_unique_index }
			{}

			~Task() 
			{
				if (_token_issued) {
					if (auto token_ptr = _weak_token_ptr.lock()) {
						token_ptr->invalidate();
					}
				}
			}

			std::shared_ptr<Token> issue_token(ImageFilter& imageFilter) 
			{
				if (_token_issued) {
					throw std::runtime_error("token issued already");
				}

				auto shared_ptr = std::make_shared<Token>(imageFilter, _index);
				_weak_token_ptr = shared_ptr;
				_token_issued = true;

				return shared_ptr;
			}

			void start(DirectX::ScratchImage&&);
		};

		std::unique_ptr< Waifu2xFilterImpl > _impl;
		std::queue<Token::Index> _task_indices;
		std::unordered_map<Token::Index, std::shared_ptr<Task>> _tasks;

	public:
		ImageFilter();
		~ImageFilter();

		HRESULT applyWaifu2x(D3DLOCKED_RECT& lockedRect, size_t width, size_t height, D3DFORMAT) const;		
		HRESULT applyWaifu2x(const D3DLOCKED_RECT& srcLockedRect, D3DLOCKED_RECT& dstLockedRect, size_t width, size_t height, D3DFORMAT) const;

		// 필터링을 시작한다
		std::shared_ptr<Token> filter_async(LPDIRECT3DTEXTURE9 pSrcTexture, LPDIRECT3DTEXTURE9 pDstTexture, int denoise_level, float scale, Callback_type);

		// 비동기 처리가 완료되었는지 엿보기 위해 매 프레임 호출되어야 한다
		void update();

	protected:
		void _remove_task(Token::Index index);

	private:
		void __copy_from_surface_memory(LPVOID pDst, LPVOID pSrc, size_t width, size_t height, UINT pitch, UINT bitPerPixel) const;
		void __copy_to_surface_memory(LPVOID pDst, LPVOID pSrc, size_t width, size_t height, UINT pitch, UINT bitPerPixel) const;

		DirectX::ScratchImage __apply_waifu2x_async(size_t width, size_t height, bool has_alpha, int denoise_level, float scale, DirectX::ScratchImage&&);
	};
}

// TODO: remove token-task frame