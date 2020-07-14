#pragma once

#include <functional>
#include <memory>

struct IDirect3DTexture9;


namespace DotEngine
{
	struct IToken
	{};

	struct IImageFilter
	{
		using Callback_type = std::function<void(IDirect3DTexture9*)>;

		// 이미지 필터 작업을 요청한다. 작업이 완료되면 Callback_type으로 전달된 함수가 실행된다.
		// 반환된 IToken이 소멸되면 callback은 호출되지 않는다. 그러나 작업은 계속 이뤄지므로 다른 것이 결과를 더 빨리 받거나 하진 않는다. 이는 스레드를 작업 중간에 제거할 경우 자원 누수가 생길 수 있기 때문에 끝까지 진행하기 때문이다.
		//
		// pSrcTexture: 필터링할 원본 텍스처. D3DFMT_A4R4G4B4, D3DFMT_A4R4G4B4 둘 중 하나여야 한다
		// pDstTexture: 변환 결과가 담길 텍스처. D3DFMT_A8R8G8B8,  D3DFMT_X8R8G8B8 둘 중 하나여야 한다
		// denoise_level: 잡음을 어느 정도로 줄일지 정한다. 값은 1, 2, 3만 허용된다. 변환에 필요한 학습 정보가 거기에 국한되어 있기 때문이다.
		// scale: 0보다 커야 한다
		// callback: 작업 완료 시 호출된다
		virtual std::shared_ptr<IToken> filter_async(IDirect3DTexture9* pSrcTexture, IDirect3DTexture9* pDstTexture, int denoise_level, float scale, Callback_type callback) = 0;

		// 매 프레임 호출되어야 한다. 그렇지 않으면 filter_async()에서 전달된 콜백 함수는 절대 실행되지 않는다
		virtual void update() = 0;

		// 작업 개수를 알려준다
		virtual size_t task_size() const = 0;
	};

	struct ImageFilterFactory
	{
		static std::unique_ptr<IImageFilter> createInstance();
	};
}