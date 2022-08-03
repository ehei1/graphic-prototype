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
		// ������ ������ ���� �ؽ�ó�� �����
		// TODO: �ؽ�ó�� ���� �� �������� ������ �����. DX9 ����� ����� �Ѽ� Ȯ���غ���. 
		//
		// pTexture: ������ �ؽ�ó�� ���� ������
		HRESULT CreateLightTextureByRenderer( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9* pTexture ) const;
		// �ſ� �������� ���� �Լ��� ��ĥ ������ ����Ѵ�. ���ҽ��� ������ �ҽ� ������ ���� �������� �õ�
		HRESULT CreateLightTextureByLockRect( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9* pTexture, const Setting& ) const;

		HRESULT CopyToMemory( LPDIRECT3DVERTEXBUFFER9 pDest, LPVOID pSrc, UINT size ) const;
		HRESULT UpdateLightVertexBuffer( LPDIRECT3DVERTEXBUFFER9* pOut, Vertices& vertices, LPDIRECT3DDEVICE9 pDevice, const Points& points, float falloff );
		// �ε��� ���۸� �����Ѵ�
		HRESULT UpdateLightIndexBuffer( LPDIRECT3DINDEXBUFFER9* pOut, Indices& indices, LPDIRECT3DDEVICE9, size_t vertexSize ) const;

		// �� ó���� ����ũ�� �����
		HRESULT UpdateBlurMask( LPDIRECT3DDEVICE9, const Vertices& );

		HRESULT ReadyToRender( LPDIRECT3DDEVICE9 );

	protected:
		// TODO: ���ؽ� �÷��� �׸� �� ������ ��û�� �ߺ� �߻�
		// �ؽ�ó
		LPDIRECT3DTEXTURE9 m_pLightTexture{};
		// ����
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