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

		// �̹��� ���� �۾��� ��û�Ѵ�. �۾��� �Ϸ�Ǹ� Callback_type���� ���޵� �Լ��� ����ȴ�.
		// ��ȯ�� IToken�� �Ҹ�Ǹ� callback�� ȣ����� �ʴ´�. �׷��� �۾��� ��� �̷����Ƿ� �ٸ� ���� ����� �� ���� �ްų� ���� �ʴ´�. �̴� �����带 �۾� �߰��� ������ ��� �ڿ� ������ ���� �� �ֱ� ������ ������ �����ϱ� �����̴�.
		//
		// pSrcTexture: ���͸��� ���� �ؽ�ó. D3DFMT_A4R4G4B4, D3DFMT_A4R4G4B4 �� �� �ϳ����� �Ѵ�
		// pDstTexture: ��ȯ ����� ��� �ؽ�ó. D3DFMT_A8R8G8B8,  D3DFMT_X8R8G8B8 �� �� �ϳ����� �Ѵ�
		// denoise_level: ������ ��� ������ ������ ���Ѵ�. ���� 1, 2, 3�� ���ȴ�. ��ȯ�� �ʿ��� �н� ������ �ű⿡ ���ѵǾ� �ֱ� �����̴�.
		// scale: 0���� Ŀ�� �Ѵ�
		// callback: �۾� �Ϸ� �� ȣ��ȴ�
		virtual std::shared_ptr<IToken> filter_async(IDirect3DTexture9* pSrcTexture, IDirect3DTexture9* pDstTexture, int denoise_level, float scale, Callback_type callback) = 0;

		// �� ������ ȣ��Ǿ�� �Ѵ�. �׷��� ������ filter_async()���� ���޵� �ݹ� �Լ��� ���� ������� �ʴ´�
		virtual void update() = 0;

		// �۾� ������ �˷��ش�
		virtual size_t task_size() const = 0;
	};

	struct ImageFilterFactory
	{
		static std::unique_ptr<IImageFilter> createInstance();
	};
}