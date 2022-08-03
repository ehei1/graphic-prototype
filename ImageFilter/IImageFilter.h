#pragma once

#include <functional>
#include <memory>

struct IDirect3DTexture9;
struct IDirect3DDevice9;


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
		// pTexture: ���͸��� �ؽ�ó. D3DFMT_A4R4G4B4, D3DFMT_A4R4G4B4 �� �� �ϳ����� �Ѵ�
		// denoise_level: ������ ��� ������ ������ ���Ѵ�. ���� 1, 2, 3�� ���ȴ�. ��ȯ�� �ʿ��� �н� ������ �ű⿡ ���ѵǾ� �ֱ� �����̴�.
		// scale: 0���� Ŀ�� �Ѵ�
		// callback: �۾� �Ϸ� �� ȣ��ȴ�. ���͸��� �Ϸ�� IDirect3DTexture9*�� ���ڷ� ���޵ȴ�. ������ ���� ä���� ������ D3DFMT_A8R8G8B8, �ƴϸ� D3DFMT_X8R8G8B8 �����̴�.
		virtual std::shared_ptr<IToken> filter_async(IDirect3DTexture9* pTexture, int denoise_level, float scale, Callback_type callback) = 0;

		// �� ������ ȣ��Ǿ�� �Ѵ�. �׷��� ������ filter_async()���� ���޵� �ݹ� �Լ��� ���� ������� �ʴ´�
		virtual void update(IDirect3DDevice9*) = 0;

		// �۾� ������ �˷��ش�
		virtual size_t task_size() const = 0;
	};

	struct ImageFilterFactory
	{
		static std::unique_ptr<IImageFilter> createInstance();
	};
}