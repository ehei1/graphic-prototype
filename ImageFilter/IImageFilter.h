#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct IDirect3DTexture9;
struct IDirect3DDevice9;


namespace Flat
{
	struct IToken
	{};

	struct IImageFilter
	{
		// �̹��� ���� �۾��� ��û�Ѵ�. �۾��� �Ϸ�Ǹ� Callback_type���� ���޵� �Լ��� ����ȴ�.
		// ��ȯ�� IToken�� �Ҹ�Ǹ� callback�� ȣ����� �ʴ´�. �̹� ���۵� �۾��� ��� �̷����Ƿ� �ٸ� ���� ����� �� ���� �ްų� ���� �ʴ´�. �̴� �����带 �۾� �߰��� ������ ��� �ڿ� ������ ���� �� �ֱ� ������ ������ �����ϱ� �����̴�.
		//
		// pTexture: ���͸��� �ؽ�ó. D3DFMT_A4R4G4B4, D3DFMT_A4R4G4B4 �� �� �ϳ����� �Ѵ�
		// denoise_level: ������ ��� ������ ������ ���Ѵ�. ���� 1, 2, 3�� ���ȴ�. ��ȯ�� �ʿ��� �н� ������ �ű⿡ ���ѵǾ� �ֱ� �����̴�.
		// scale: 0���� Ŀ�� �Ѵ�
		// callback: �۾� �Ϸ� �� ȣ��ȴ�. ���͸��� �Ϸ�� IDirect3DTexture9*�� ���ڷ� ���޵ȴ�. ������ ���� ä���� ������ D3DFMT_A8R8G8B8, �ƴϸ� D3DFMT_X8R8G8B8 �����̴�.
		using Filter_callback_type = std::function<void(IDirect3DTexture9*)>;
		virtual std::shared_ptr<IToken> filter_async(IDirect3DTexture9* pTexture, int denoise_level, float scale, Filter_callback_type callback) = 0;

		// �� ������ ȣ��Ǿ�� �Ѵ�. �׷��� ������ filter_async()���� ���޵� �ݹ� �Լ��� ���� ������� �ʴ´�
		virtual void update(IDirect3DDevice9*) = 0;

		// �۾� ������ �˷��ش�
		virtual size_t task_size() const = 0;

		// �α� �߻� �� ȣ��� �Լ��� �����Ѵ�
		using Log_callback_type = std::function<void(std::string const&)>;
		virtual void bind_log_callback(Log_callback_type) = 0;
	};

	struct ImageFilterFactory
	{
		static std::shared_ptr<IImageFilter> createInstance();
	};
}