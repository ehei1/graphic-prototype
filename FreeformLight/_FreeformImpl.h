#pragma once
#include <d3dx9.h>


/*
 * common implementation for light container
 * 
 * make a light in the container and then draw it
*/
namespace FreeformLight
{
	template<typename LIGHT_IMPL>
	class _FreeformImpl
	{
	public:
		_FreeformImpl( const _FreeformImpl& ) = delete;

		inline bool HasLight() const { return !m_lightImpls.empty(); }

		// draw all lights
		HRESULT Draw( LPDIRECT3DDEVICE9 pDevice, float x, float y )
		{
			if ( m_lightImpls.size() ) {
				D3DMATRIX curVm{};
				pDevice->GetTransform( D3DTS_VIEW, &curVm );

				// change view matrix to default. because game screen is magnified by user
				{
					D3DXVECTOR3 eye{ x, y, 1 };
					D3DXVECTOR3 at{ x, y, -1 };
					D3DXVECTOR3 up{ 0, -1, 0 };

					D3DXMATRIX view{};
					D3DXMatrixLookAtLH( &view, &eye, &at, &up );

					pDevice->SetTransform( D3DTS_VIEW, &view );
				}

				DWORD curBlendOp = {};
				DWORD curSrcBlend = {};
				DWORD curDestBlend = {};
				pDevice->GetRenderState( D3DRS_BLENDOP, &curBlendOp );
				pDevice->GetRenderState( D3DRS_DESTBLEND, &curDestBlend );
				pDevice->GetRenderState( D3DRS_SRCBLEND, &curSrcBlend );
				DWORD fillMode{};
				pDevice->GetRenderState( D3DRS_FILLMODE, &fillMode );

				DWORD oldFVF{};
				pDevice->GetFVF( &oldFVF );

				{
					pDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
					pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
					pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_DESTALPHA );

					for ( auto&& lightImpl : m_lightImpls ) {
						if ( FAILED( lightImpl->Draw( pDevice ) ) ) {
							ASSERT( FALSE );

							return E_FAIL;
						}
					}
				}

				pDevice->SetRenderState( D3DRS_BLENDOP, curBlendOp );
				pDevice->SetRenderState( D3DRS_DESTBLEND, curDestBlend );
				pDevice->SetRenderState( D3DRS_SRCBLEND, curSrcBlend );
				pDevice->SetRenderState( D3DRS_FILLMODE, fillMode );

				pDevice->SetFVF( oldFVF );

				// restore view matrix
				pDevice->SetTransform( D3DTS_VIEW, &curVm );
			}

			return S_OK;
		}

		// it'll call when device is restored
		virtual HRESULT RestoreDevice( LPDIRECT3DDEVICE9 pDevice, D3DDISPLAYMODE const& )
		{
			for ( auto&& lightImpl : m_lightImpls ) {
				if ( FAILED( lightImpl->RestoreDevice( pDevice ) ) ) {
					ASSERT( FALSE );

					return E_FAIL;
				}
			}

			return S_OK;
		}

		// it'll call when device is invalidated
		void Invalidate()
		{
			for ( auto&& lightImpl : m_lightImpls ) {
				lightImpl->Invalidate();
			}
		}

	protected:
		explicit _FreeformImpl( LPDIRECT3DPIXELSHADER9 pBlurShader ) : m_pBlurPixelShader{ pBlurShader }
		{}

	protected:
		std::vector<std::shared_ptr<LIGHT_IMPL>> m_lightImpls;
		LPDIRECT3DPIXELSHADER9 m_pBlurPixelShader{};
	};
}