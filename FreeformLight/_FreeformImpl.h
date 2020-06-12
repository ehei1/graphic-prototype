#pragma once
#include <d3dx9.h>


/*
조명 컨테이너의 공통 구현

조명을 컨테이너 내에 생성하고 그린다
*/
namespace FreeformLight
{
	template<typename LIGHT_IMPL>
	class _FreeformImpl
	{
	public:
		_FreeformImpl( const _FreeformImpl& ) = delete;

		// 컨테이너 내에 조명이 있는지 알려준다
		inline bool HasLight() const { return !m_lightImpls.empty(); }

		// 모든 조명을 그린다. 현재 설정된 LPDIRECT3DSURFACE9에 그린다
		HRESULT Draw( LPDIRECT3DDEVICE9 pDevice, float x, float y )
		{
			if ( m_lightImpls.size() ) {
				D3DMATRIX curVm{};
				pDevice->GetTransform( D3DTS_VIEW, &curVm );

				// 뷰 행렬을 기본으로 바꾼다. 게임 화면은 확대를 하는 경우가 있기 때문
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

				// 뷰 행렬 복원
				pDevice->SetTransform( D3DTS_VIEW, &curVm );
			}

			return S_OK;
		}

		// 장치가 복구될 때 호출된다
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

		// 장치가 무효화될 때 호출된다
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