#pragma once
#include <vector>
#include <d3dx9.h>


namespace FreeformLight
{
	template<typename T> class _FreeformImpl;

	class _ImmutableLightImpl
	{
		friend _FreeformImpl<_ImmutableLightImpl>;

	protected:
		struct CUSTOM_VERTEX {
			D3DXVECTOR3 position;
			D3DXVECTOR2 uv;
		};
		using Indices = std::vector<WORD>;
		using Points = std::vector<D3DXVECTOR3>;
		using Vertices = std::vector<CUSTOM_VERTEX>;

		struct Setting
		{
			D3DXCOLOR lightColor;
			float intensity;
			float falloff;
		};

	public:
		_ImmutableLightImpl( LPDIRECT3DDEVICE9, LPDIRECT3DPIXELSHADER9 pBlurShader, Points const&, Setting const& );
		virtual ~_ImmutableLightImpl();

		virtual HRESULT Draw( LPDIRECT3DDEVICE9 );

		static HRESULT CreateMesh( LPDIRECT3DDEVICE9, LPD3DXMESH*, UINT width, UINT height );
		static HRESULT CreateTexture( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9*, UINT width, UINT height );

		HRESULT RestoreDevice( LPDIRECT3DDEVICE9 );
		void Invalidate();

	protected:
		// 프리폼 조명을 위한 텍스처를 만든다
		// TODO: 텍스처를 만든 후 렌더러가 동작을 멈춘다. DX9 디버그 기능을 켜서 확인해보기. 
		//
		// pTexture: 생성할 텍스처를 담을 포인터
		HRESULT CreateLightTextureByRenderer( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9* pTexture ) const;
		// 매우 느리지만 위의 함수를 고칠 때까지 사용한다. 리소스를 가능한 소스 폴더에 넣지 않으려는 시도
		HRESULT CreateLightTextureByLockRect( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9* pTexture, const Setting& ) const;

		HRESULT CopyToMemory( LPDIRECT3DVERTEXBUFFER9 pDest, LPVOID pSrc, UINT size ) const;
		HRESULT UpdateLightVertexBuffer( LPDIRECT3DVERTEXBUFFER9* pOut, Vertices& vertices, LPDIRECT3DDEVICE9 pDevice, const Points& points, float falloff );
		// 인덱스 버퍼를 갱신한다
		HRESULT UpdateLightIndexBuffer( LPDIRECT3DINDEXBUFFER9* pOut, Indices& indices, LPDIRECT3DDEVICE9, size_t vertexSize ) const;

		// 블러 처리할 마스크를 만든다
		HRESULT UpdateBlurMask( LPDIRECT3DDEVICE9, const Vertices& );

		HRESULT ReadyToRender( LPDIRECT3DDEVICE9 );

	protected:
		// TODO: 버텍스 컬러로 그릴 수 없을까 엄청난 중복 발생
		// 텍스처
		LPDIRECT3DTEXTURE9 m_pLightTexture{};
		// 버퍼
		LPDIRECT3DINDEXBUFFER9 m_pLightIndexBuffer{};
		LPDIRECT3DVERTEXBUFFER9 m_pLightVertexBuffer{};

		const int m_lightVertexFvf = D3DFVF_XYZ | D3DFVF_TEX1;

		Indices m_lightIndices;
		Vertices m_lightVertices;
		Points const m_points;
		Setting m_setting;

		struct Mask
		{
			D3DXMATRIX m_worldTransform{};
			LPDIRECT3DTEXTURE9 m_pTexture{};
			LPD3DXMESH m_pMesh{};
		}
		m_blurMask;

		LPDIRECT3DPIXELSHADER9 m_pBlurPixelShader{};
	};
}