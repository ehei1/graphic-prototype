#pragma once
#include "_FreeformImpl.h"
#include "_MutableLightImpl.h"


namespace FreeformLight
{
	class _MutableFreeform : public _FreeformImpl<_MutableLightImpl>
	{
	public:
		_MutableFreeform( LPDIRECT3DPIXELSHADER9 pBlurShader, D3DDISPLAYMODE const& displayMode ) : _FreeformImpl<_MutableLightImpl>{ pBlurShader }
		{
			m_displayMode = displayMode;
		}

		// 개발 용도의 imgui 창을 만든다
		HRESULT DrawImgui( LPDIRECT3DDEVICE9, LONG xCenter, LONG yCenter, bool isAmbientMode, bool* pIsVisible );

		inline bool IsMaskInvisible() const { return !m_setting.maskVisible; }
		inline D3DXCOLOR GetAmbientColor() const { return m_setting.ambient; }

		virtual HRESULT RestoreDevice( LPDIRECT3DDEVICE9 pDevice, D3DDISPLAYMODE const& displayMode ) override final;

	private:
		HRESULT AddLight( LPDIRECT3DDEVICE9, LONG x, LONG y );
		HRESULT RemoveLight( size_t index );
		_MutableLightImpl::Points GetDefaultPoints( D3DDISPLAYMODE const&, LONG x, LONG y ) const;

	private:
		struct Setting
		{
			bool maskVisible{};
			bool helper{};

			D3DXCOLOR ambient{ D3DCOLOR_XRGB( 125, 125, 125 ) };
		} m_setting;

		struct Tab
		{
			bool m_opened;
			std::string m_name;

			Tab( bool opened, std::string& name ) : m_opened{ opened }, m_name{ name }
			{}

			Tab( Tab&& tab ) {
				m_opened = tab.m_opened;
				m_name = std::move( tab.m_name );
			}

			Tab& operator=( const Tab& tab ) {
				m_opened = tab.m_opened;
				m_name = tab.m_name;

				return *this;
			}
		};

		std::vector<Tab> m_tabs;
		D3DDISPLAYMODE m_displayMode{};
	};
}