#include "StdAfx.h"
#include <algorithm>
#include <iostream>
#include "FreeformLight.h"

//#define DEBUG_FREEFORM


void CFreeformLight::InvalidateDeviceObjects()
{
	SAFE_RELEASE( m_pLightTexture );
	SAFE_RELEASE( m_pLightIndexBuffer );
	SAFE_RELEASE( m_pLightVertexBuffer );
	SAFE_RELEASE( m_pScreenTexture );
	SAFE_RELEASE( m_pScreenMesh );
}

HRESULT CFreeformLight::RestoreDevice( LPDIRECT3DDEVICE9 pDevice, const D3DDISPLAYMODE& displayMode )
{
	if ( m_pLightVertexBuffer ) {
		SAFE_RELEASE( m_pScreenMesh );

		if ( FAILED( CreateMaskMesh( pDevice, &m_pScreenMesh ) ) ) {
			return E_FAIL;
		}

		SAFE_RELEASE( m_pScreenTexture );

		if ( FAILED( CreateMaskTexture( pDevice, &m_pScreenTexture ) ) ) {
			return E_FAIL;
		}
	}

	m_displayMode = displayMode;
	return S_OK;
}

HRESULT CFreeformLight::AddLight( LPDIRECT3DDEVICE9 pDevice, LONG x, LONG y )
{
	// TODO 메시로 바꾸기
	ASSERT( pDevice );

	if ( m_pLightVertexBuffer ) {
		return E_FAIL;
	}

	m_position = { x, y };

	// 초기화
	if ( !m_pLightTexture ) {
		if ( FAILED( CreateLightTextureByLockRect( pDevice, &m_pLightTexture, m_setting ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}
		else if ( FAILED( CreateMaskMesh( pDevice, &m_pScreenMesh ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}
		else if ( FAILED( CreateMaskTexture( pDevice, &m_pScreenTexture ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		ASSERT( m_pLightTexture && m_pScreenTexture && m_pScreenMesh );
	}

	D3DXVECTOR3 centerPoint{ static_cast<float>( x ), static_cast<float>( y ),{} };
	using Points = std::vector< D3DXVECTOR3 >;
	Points points{ centerPoint };
	// 화면의 절반만 차지하도록 한다
	auto scaledWidth = m_displayMode.Width / 4;
	auto scaledHeight = m_displayMode.Height / 4;

	auto addPoints = [&points, scaledWidth, scaledHeight]( const std::vector<D3DXVECTOR3 >& sideVertices ) {
		points.insert( points.end(), sideVertices.begin(), sideVertices.end() );

		return *sideVertices.begin();
	};

	// 시계 방향으로 면을 살피면서 점을 추가한다
	auto leftTopPoint = addPoints( m_topSideVertices );
	auto rightTopPoint = addPoints( m_rightSideVectices );
	auto rightBottomPoint = addPoints( m_bottomSideVertices );
	auto leftBottomPoint = addPoints( m_leftSideVertices );
	// 닫는다
	points.push_back( leftTopPoint );

	auto falloff = m_setting.fallOff;

	using Vertices = std::vector< CUSTOM_VERTEX >;
	Vertices vertices( points.size(), { centerPoint,{ 1, 1 } } );
	ASSERT( vertices.size() == points.size() );

	// 프리폼 조명의 위치를 화면 중앙에 놓는다
	auto updateVertex = [it = next( points.cbegin() ), i = -1, x, y, scaledWidth, scaledHeight, falloff]( CUSTOM_VERTEX& vertex ) mutable {
		auto position = *it++;
		auto newPosition = D3DXVECTOR3{ position.x * scaledWidth, position.y * scaledHeight, 0 };
		newPosition += D3DXVECTOR3{ static_cast<float>( x ), static_cast<float>( y ), 0 };

		vertex.position = newPosition;
		vertex.uv = ( ++i % 2 ? D3DXVECTOR2{ 1, falloff } : D3DXVECTOR2{ 0, falloff } );
	};
	std::for_each( std::next( vertices.begin() ), vertices.end(), updateVertex );

#ifdef DEBUG_FREEFORM
	auto debugPosition = []( const CUSTOM_VERTEX& vertex ) {
		auto& position = vertex.position;
		TCHAR debugText[MAX_PATH] = {};
		_stprintf_s( debugText, _countof( debugText ), TEXT( "%f,%f,%f\n" ), position.x, position.y, position.z );
		OutputDebugString( debugText );
	};
	std::for_each( vertices.begin(), vertices.end(), debugPosition );
#endif

	auto verticesSize = static_cast<UINT>( sizeof( Vertices::value_type ) * vertices.size() );

	// 버텍스 버퍼 갱신
	{
		if ( FAILED( pDevice->CreateVertexBuffer( verticesSize, 0, m_fvf, D3DPOOL_DEFAULT, &m_pLightVertexBuffer, NULL ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		LPVOID pVertices{};

		if ( FAILED( m_pLightVertexBuffer->Lock( 0, verticesSize, &pVertices, 0 ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		memcpy( pVertices, &( *vertices.cbegin() ), verticesSize );
		m_pLightVertexBuffer->Unlock();
	}

	// 인덱스를 만든다
	// https://en.cppreference.com/w/cpp/algorithm/generate
	auto increaseNumber = [n = 0]() mutable { return n++; };
	using Indices = std::vector<WORD>;

	// 항상 LT 위치로 끝나야 한다
	Indices indices( points.size(), 1 );
	// 마지막 위치를 제외하고는 차례대로 번호를 채운다
	std::generate( indices.begin(), std::prev( indices.end() ), increaseNumber );
	auto indicesSize = static_cast<UINT>( sizeof( Indices::value_type ) * indices.size() );

	// 인덱스 버퍼 갱신
	{
		if ( FAILED( pDevice->CreateIndexBuffer( indicesSize, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &m_pLightIndexBuffer, NULL ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		LPVOID pIndices = {};

		if ( m_pLightIndexBuffer->Lock( 0, indicesSize, &pIndices, 0 ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		memcpy( pIndices, &( *indices.cbegin() ), indicesSize );
		m_pLightIndexBuffer->Unlock();
	}

	m_lightVertexCount = static_cast<UINT>( points.size() );
	m_lightPrimitiveCount = static_cast<UINT>( indices.size() - 2 );

	return S_OK;
}

HRESULT CFreeformLight::RemoveLight()
{
	if ( !m_pLightVertexBuffer )
	{
		return S_OK;
	}

	SAFE_RELEASE( m_pLightIndexBuffer );
	SAFE_RELEASE( m_pLightVertexBuffer );

	m_lightVertexCount = 0;
	m_lightPrimitiveCount = 0;

	return S_OK;
}

HRESULT CFreeformLight::Draw( LPDIRECT3DDEVICE9 pDevice, float x, float y )
{
	if ( !m_pLightVertexBuffer )
	{
		return S_OK;
	}

	LPDIRECT3DSURFACE9 pScreenSurface = {};
	m_pScreenTexture->GetSurfaceLevel( 0, &pScreenSurface );

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

	auto& shadowColor = m_setting.shadowColor;

	LPDIRECT3DSURFACE9 curRT = {};
	pDevice->GetRenderTarget( 0, &curRT );
	pDevice->SetRenderTarget( 0, pScreenSurface );
	auto clearColor = D3DXCOLOR{ shadowColor.r, shadowColor.g, shadowColor.b, 1 };

	pDevice->Clear( 0, 0, D3DCLEAR_TARGET, clearColor, 0.0f, 0 );

	// 마스크에 조명을 그린다
	if ( SUCCEEDED( pDevice->BeginScene() ) )
	{
		DWORD curBlendOp = {};
		DWORD curSrcBlend = {};
		DWORD curDestBlend = {};
		pDevice->GetRenderState( D3DRS_BLENDOP, &curBlendOp );
		pDevice->GetRenderState( D3DRS_DESTBLEND, &curDestBlend );
		pDevice->GetRenderState( D3DRS_SRCBLEND, &curSrcBlend );

		D3DVERTEXBUFFER_DESC vertexBufferDesc = {};
		m_pLightVertexBuffer->GetDesc( &vertexBufferDesc );

		DWORD curFVF = {};
		pDevice->GetFVF( &curFVF );
		pDevice->SetFVF( vertexBufferDesc.FVF );
		pDevice->SetTexture( 0, m_pLightTexture );
		pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
		pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
		// 마스크를 덮어쓴다
		// result = src * srcAlpha + dest * ( 1 - srcAlpha )
		pDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
		pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		pDevice->SetStreamSource( 0, m_pLightVertexBuffer, 0, vertexBufferDesc.Size / m_lightVertexCount );
		pDevice->SetIndices( m_pLightIndexBuffer );
		pDevice->DrawIndexedPrimitive( D3DPT_TRIANGLEFAN, 0, 0, m_lightVertexCount, 0, m_lightPrimitiveCount );
		pDevice->EndScene();

		pDevice->SetFVF( curFVF );
		pDevice->SetRenderTarget( 0, curRT );
		SAFE_RELEASE( curRT );

		pDevice->SetRenderState( D3DRS_BLENDOP, curBlendOp );
		pDevice->SetRenderState( D3DRS_DESTBLEND, curDestBlend );
		pDevice->SetRenderState( D3DRS_SRCBLEND, curSrcBlend );

#ifdef DEBUG_FREEFORM
		D3DXSaveTextureToFile( TEXT( "D:\\mask.png" ), D3DXIFF_PNG, m_pScreenTexture, NULL );
#endif

		SAFE_RELEASE( pScreenSurface );
	}

	// 뷰 행렬 복원
	pDevice->SetTransform( D3DTS_VIEW, &curVm );

	// 마스크를 게임 화면에 씌운다
	if ( SUCCEEDED( pDevice->BeginScene() ) )
	{
		DWORD curBlendOp = {};
		DWORD curSrcBlend = {};
		DWORD curDestBlend = {};
		pDevice->GetRenderState( D3DRS_BLENDOP, &curBlendOp );
		pDevice->GetRenderState( D3DRS_SRCBLEND, &curSrcBlend );
		pDevice->GetRenderState( D3DRS_DESTBLEND, &curDestBlend );

		DWORD curFVF = {};
		pDevice->GetFVF( &curFVF );
		pDevice->SetFVF( m_fvf );
		pDevice->SetTexture( 0, m_pScreenTexture );

		// 메시 그리기
		{
			D3DXMATRIX curWm = {};
			pDevice->GetTransform( D3DTS_WORLD, &curWm );

			// 이동
			D3DXMATRIX tm{};
			D3DXMatrixTranslation( &tm, x, y, 0 );
			pDevice->SetTransform( D3DTS_WORLD, &tm );

			// 마스크를 반전해서 게임 화면이 그려진 렌더타겟의 색깔과 곱한다
			// result = src * 0 + dest * ( 1 - srcAlpha )
			pDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
			pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
			pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR );

			m_pScreenMesh->DrawSubset( 0 );

			pDevice->SetTransform( D3DTS_WORLD, &curWm );
		}

		pDevice->EndScene();
		pDevice->SetFVF( curFVF );

		pDevice->SetRenderState( D3DRS_BLENDOP, curBlendOp );
		pDevice->SetRenderState( D3DRS_SRCBLEND, curSrcBlend );
		pDevice->SetRenderState( D3DRS_DESTBLEND, curDestBlend );
	}

	return S_OK;
}

HRESULT CFreeformLight::CreateLightTextureByLockRect( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DTEXTURE9* pOutTexture, const Setting& setting ) const
{
	ASSERT( !*pOutTexture );

	constexpr auto size = 256;
	ASSERT( ceil( log2( size ) ) == floor( log2( size ) ) );
	LPDIRECT3DTEXTURE9 pTexture = {};

	if ( FAILED( D3DXCreateTexture( pDevice, size, size, 0, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pTexture ) ) ) {
		ASSERT( FALSE );
		return E_FAIL;
	}

	// 그라데이션을 그린다
	{
		D3DLOCKED_RECT lockedRect = {};
		pTexture->LockRect( 0, &lockedRect, NULL, D3DLOCK_READONLY );
		auto* const colors = static_cast<LPDWORD>( lockedRect.pBits );
		auto intensity = setting.intensity * 255.f;
		auto& color = setting.lightColor;
		auto r = static_cast<int>( color.r * intensity );
		auto g = static_cast<int>( color.g * intensity );
		auto b = static_cast<int>( color.b * intensity );

		for ( auto y = 0; y < size; ++y ) {
			auto a = y / static_cast<float>( size ) * 255;

			for ( auto x = 0; x < size; ++x ) {
				auto index = y * size + x;

				colors[index] = D3DCOLOR_ARGB( static_cast<int>( a ), static_cast<int>( r ), static_cast<int>( g ), static_cast<int>( b ) );
			}
		}

		pTexture->UnlockRect( 0 );
	}


	// 메모리에 쓴 것을 다시 읽어들인다. 이러면 렌더링 가능하게 된다
	{
		ID3DXBuffer* buffer = {};
		if ( FAILED( D3DXSaveTextureToFileInMemory( &buffer, D3DXIFF_PNG, pTexture, NULL ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		SAFE_RELEASE( pTexture );

		if ( FAILED( D3DXCreateTextureFromFileInMemory( pDevice, buffer->GetBufferPointer(), buffer->GetBufferSize(), &pTexture ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}
	}

#ifdef DEBUG_FREEFORM
	D3DXSaveTextureToFile( TEXT( "D:\\lightTex.png" ), D3DXIFF_PNG, pTexture, NULL );
#endif

	*pOutTexture = pTexture;
	return S_OK;
}

HRESULT CFreeformLight::CreateLightTextureByRenderer( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DTEXTURE9* pOutTexture ) const
{
	ASSERT( !*pOutTexture );

	constexpr auto resolution = 256;
	constexpr auto x = static_cast<float>( resolution );
	constexpr auto y = static_cast<float>( resolution );

	struct CUSTOMVERTEX_TEXTURE {
		D3DXVECTOR4 position; // x, y, z, rhw
		D3DCOLOR color;
	} customVertices[] = {
		{ { 0.f, 0.f, 0.f, 1.f }, 0xffffffff, },
		{ { 0.f,   y, 0.f, 1.f }, 0xff000000, },
		{ { x,   y, 0.f, 1.f }, 0xff000000, },
		{ { x, 0.f, 0.f, 1.f }, 0xffffffff, },
	};
	constexpr auto textureFVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;
	LPDIRECT3DVERTEXBUFFER9 pVertexBuffer = {};

	// 버텍스 버퍼 채우기
	{
		if ( FAILED( pDevice->CreateVertexBuffer( sizeof( customVertices ), 0, textureFVF, D3DPOOL_DEFAULT, &pVertexBuffer, NULL ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		LPVOID pVertices = {};

		if ( FAILED( pVertexBuffer->Lock( 0, sizeof( customVertices ), &static_cast<LPVOID>( pVertices ), 0 ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		memcpy( pVertices, customVertices, sizeof( customVertices ) );
		pVertexBuffer->Unlock();
	}

	LPDIRECT3DTEXTURE9 pTexture = {};

	// 그리기
	{
		if ( FAILED( D3DXCreateTexture( pDevice, resolution, resolution, 0, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pTexture ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		LPDIRECT3DSURFACE9 currentRenderTarget = {};
		pDevice->GetRenderTarget( 0, &currentRenderTarget );

		LPDIRECT3DSURFACE9 pTextureSurface = {};
		pTexture->GetSurfaceLevel( 0, &pTextureSurface );

		// 렌더링 대상 바꿈
		pDevice->SetRenderTarget( 0, pTextureSurface );
		pDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB( 0, 0, 255 ), 1.0f, 0 );

		if ( SUCCEEDED( pDevice->BeginScene() ) ) {
			pDevice->SetStreamSource( 0, pVertexBuffer, 0, sizeof( *customVertices ) );
			pDevice->SetFVF( textureFVF );
			pDevice->DrawPrimitive( D3DPT_TRIANGLEFAN, 0, 2 );
			pDevice->EndScene();
		}

		// 복구
		pDevice->SetRenderTarget( 0, currentRenderTarget );

		SAFE_RELEASE( pTextureSurface );
		SAFE_RELEASE( currentRenderTarget );

#ifdef DEBUG_FREEFORM
		if ( FAILED( D3DXSaveTextureToFile( TEXT( "D:\\lightTex.png" ), D3DXIFF_PNG, pTexture, NULL ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}
#endif
	}

	pVertexBuffer->Release();
	pVertexBuffer = {};

	*pOutTexture = pTexture;
	return S_OK;
}

HRESULT CFreeformLight::CreateMaskTexture( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DTEXTURE9* pOutTexture ) const
{
	ASSERT( !*pOutTexture );

	LPDIRECT3DTEXTURE9 pTexture = {};

	if ( FAILED( pDevice->CreateTexture( m_displayMode.Width, m_displayMode.Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pTexture, NULL ) ) ) {
		ASSERT( FALSE );
		return E_FAIL;
	}

	*pOutTexture = pTexture;

	return S_OK;
}

HRESULT CFreeformLight::CreateMaskMesh( LPDIRECT3DDEVICE9 pDevice, LPD3DXMESH* pOutMesh ) const
{
	ASSERT( !*pOutMesh );

	LPD3DXMESH pMesh = {};
	constexpr auto vertexCount = 4;
	constexpr auto faceCount = 2;

	if ( FAILED( D3DXCreateMeshFVF( faceCount, vertexCount, D3DXMESH_MANAGED, m_fvf, pDevice, &pMesh ) ) ) {
		return E_FAIL;
	}

	// 버텍스 버퍼 채우기
	{
		auto w = m_displayMode.Width / 2.f;
		auto h = m_displayMode.Height / 2.f;

		const CUSTOM_VERTEX vertices[] = {
			{ { -w, -h, 0 },{ 0, 0 } },//0
			{ { -w, +h, 0 },{ 0, 1 } },//1
			{ { +w, +h, 0 },{ 1, 1 } },//2
			{ { +w, -h, 0 },{ 1, 0 } },//3
		};
		static_assert( _countof( vertices ) == vertexCount, "invalid size" );

		LPVOID pMeshVertices = {};
		pMesh->LockVertexBuffer( 0, &pMeshVertices );
		memcpy( pMeshVertices, vertices, sizeof( vertices ) );
		pMesh->UnlockIndexBuffer();
	}

	// 인덱스 버퍼 채우기
	{
		const WORD indices[] = {
			0, 1, 2,
			0, 2, 3,
		};
		static_assert( _countof( indices ) == faceCount * 3, "invalid size" );

		LPVOID pMeshIndices = {};
		pMesh->LockIndexBuffer( 0, &pMeshIndices );
		memcpy( pMeshIndices, indices, sizeof( indices ) );
		pMesh->UnlockIndexBuffer();
	}

	*pOutMesh = pMesh;

	return S_OK;
}

HRESULT CFreeformLight::SetSetting( LPDIRECT3DDEVICE9 pDevice, const Setting& setting )
{
	if ( m_setting != setting ) {
		m_setting = setting;

		if ( IsVisible() ) {
			SAFE_RELEASE( m_pLightTexture );
			CreateLightTextureByLockRect( pDevice, &m_pLightTexture, setting );
			RemoveLight();

			AddLight( pDevice, m_position.x, m_position.y );
		}
	}

	return S_OK;
}