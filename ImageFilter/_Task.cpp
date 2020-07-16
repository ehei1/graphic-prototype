#include "stdafx.h"
#include "_ImageFilter.h"
#include "_Token.h"
#include "_Task.h"


namespace DotEngine
{
	size_t _Task::_unique_index{};


	_Task::_Task(LPDIRECT3DTEXTURE9 pTexture, Function_type function, IImageFilter::Callback_type callback) : _pTexture{ pTexture }, _function{ function }, _callback{ callback }, _index{ ++_unique_index }, _started_time{ std::chrono::system_clock::now() }
	{
		_pTexture->AddRef();
	}

	_Task::~_Task()
	{
		_pTexture->Release();

		if (_token_issued) {
			if (auto token_ptr = _weak_token_ptr.lock()) {
				auto token = std::static_pointer_cast<_Token>(token_ptr);

				token->invalidate();
			}
		}
	}

	std::shared_ptr<IToken> _Task::issue_token(_ImageFilter& imageFilter)
	{
		if (_token_issued) {
			throw std::runtime_error("token issued already");
		}

		auto token_ptr = std::make_shared<_Token>(imageFilter, _index);
		_weak_token_ptr = token_ptr;
		_token_issued = true;

		return std::static_pointer_cast<IToken>(token_ptr);
	}

	void _Task::start(DirectX::ScratchImage&& image)
	{
		if (_async_started) {
			throw std::runtime_error("async job started already");
		}
		else {
			_future = std::async(std::launch::async, _function, std::move(image));
			_async_started = true;
		}
	}
}