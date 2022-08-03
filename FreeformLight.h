#pragma once
#include <vector>
#include <d3dx9.h>


// ������ ������ ���� ����
class CFreeformLight
{
public:
	// 
	HRESULT AddLight( LPDIRECT3DDEVICE9, LONG x, LONG y );
	HRESULT RemoveLight();
	HRESULT UpdateLightVertex( WORD index, const D3DXVECTOR3& position );

	// ������ �׸���
	//
	// pDevice: ������ �׸��µ� ���� ��ġ
	// pSurface: ������ �׸� ǥ��
	// x: ������ �׸� ��ġ
	// y: ������ �׸� ��ġ
	HRESULT Draw( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DSURFACE9 pSurface, float x, float y );
	HRESULT RestoreDevice( const D3DDISPLAYMODE& );
	void InvalidateDeviceObjects();
	inline void Uninitialize() { InvalidateDeviceObjects(); }
	inline bool IsVisible() const { return !!m_pLightVertexBuffer; }

	// ���� �뵵�� imgui â�� �����
	void CreateImgui( LPDIRECT3DDEVICE9, LONG xCenter, LONG yCenter, bool& isVisible );

	struct Setting
	{
		D3DXCOLOR lightColor = D3DCOLOR_XRGB( 255, 255, 255 );
		D3DXCOLOR shadowColor = D3DCOLOR_XRGB( 0, 0, 0 );
		float intensity = 1.f;
		float fallOff = 1.f;
		bool maskOnly{};

		inline bool operator==( const Setting& setting ) const { return !memcmp( &setting, this, sizeof( setting ) ); }
		inline bool operator!=( const Setting& setting ) const { return !( *this == setting ); }
	};
	inline const Setting& GetSetting() const { return m_setting; }
	HRESULT SetSetting( LPDIRECT3DDEVICE9, const Setting& );

	static HRESULT CreateMaskMesh( LPDIRECT3DDEVICE9, LPD3DXMESH*, UINT width, UINT height );
	static HRESULT CreateMaskTexture( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9*, UINT width, UINT height );

private:
	// ������ ������ ���� �ؽ�ó�� �����
	// TODO: �ؽ�ó�� ���� �� �������� ������ �����. DX9 ����� ����� �Ѽ� Ȯ���غ���. 
	//
	// pTexture: ������ �ؽ�ó�� ���� ������
	HRESULT CreateLightTextureByRenderer( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9* pTexture ) const;
	// �ſ� �������� ���� �Լ��� ��ĥ ������ ����Ѵ�. ���ҽ��� ������ �ҽ� ������ ���� �������� �õ�
	HRESULT CreateLightTextureByLockRect( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9* pTexture, const Setting& ) const;

private:
	struct CUSTOM_VERTEX {
		D3DXVECTOR3 position;
		D3DXVECTOR2 uv;
	};

	// ���� ������ ���� ����
	// �ؽ�ó
	LPDIRECT3DTEXTURE9 m_pLightTexture{};
	// ����
	LPDIRECT3DINDEXBUFFER9 m_pLightIndexBuffer{};
	LPDIRECT3DVERTEXBUFFER9 m_pLightVertexBuffer{};

	std::vector< D3DXVECTOR3 > m_topSideVertices{
		{ -1.f, +1.f, -0.f },
	};
	std::vector< D3DXVECTOR3 > m_rightSideVectices{
		{ +1.0f, +1.f, 0.f },
		//{ +1.5f,  0.f, 0.f },
	};
	std::vector< D3DXVECTOR3 > m_bottomSideVertices{
		{ +1.f, -1.f, 0.f },
	};
	std::vector< D3DXVECTOR3 > m_leftSideVertices{
		{ -1.0f, -1.f, 0.f },
		//{ -1.5f, -1.f, 0.f },
		//{ -1.0f,  0.f, 0.f },
	};
	D3DDISPLAYMODE m_displayMode{};

	Setting m_setting{};
	POINT m_position{};

	const int m_lightVertexFvf = D3DFVF_XYZ | D3DFVF_TEX1;

	using Indices = std::vector<WORD>;
	Indices m_lightIndices;

	using Vertices = std::vector<CUSTOM_VERTEX>;
	Vertices m_lightVertices;
};