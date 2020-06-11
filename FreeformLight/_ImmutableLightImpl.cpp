#include "stdafx.h"
#include "_ImmutableLightImpl.h"

//#define DEBUG_SURFACE


namespace FreeformLight
{
	_ImmutableLightImpl::_ImmutableLightImpl( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DPIXELSHADER9 pBlurShader, Points const& points, Setting const& setting ) :
		m_points{ points }, m_setting{ setting }, m_pBlurPixelShader( pBlurShader )
	{
		ASSERT( pDevice );

		if ( FAILED( ReadyToRender( pDevice ) ) ) {
			throw std::exception( "ReadyToRender() failed" );
		}
	}

	_ImmutableLightImpl::~_ImmutableLightImpl()
	{
		Invalidate();
	}

	HRESULT _ImmutableLightImpl::CreateLightTextureByRenderer( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DTEXTURE9* pOutTexture ) const
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
			{ { x,     y, 0.f, 1.f }, 0xff000000, },
			{ { x,   0.f, 0.f, 1.f }, 0xffffffff, },
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

			if ( FAILED( pVertexBuffer->Lock( 0, sizeof( customVertices ), &pVertices, 0 ) ) ) {
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

#ifdef DEBUG_SURFACE
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
	// 매우 느리지만 위의 함수를 고칠 때까지 사용한다. 리소스를 가능한 소스 폴더에 넣지 않으려는 시도
	HRESULT _ImmutableLightImpl::CreateLightTextureByLockRect( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DTEXTURE9* pOutTexture, const Setting& setting ) const
	{
		ASSERT( !*pOutTexture );

		constexpr auto size = 256;
		ASSERT( ceil( log2( size ) ) == floor( log2( size ) ) );
		LPDIRECT3DTEXTURE9 pTexture = {};

		if ( FAILED( D3DXCreateTexture( pDevice, size, size, 0, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pTexture ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		// 그라데이션을 그린다
		{
			D3DLOCKED_RECT lockedRect = {};
			pTexture->LockRect( 0, &lockedRect, NULL, D3DLOCK_NO_DIRTY_UPDATE );
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
					colors[index] = D3DCOLOR_ARGB( static_cast<int>( a ), r, g, b );
				}
			}

			if ( FAILED( pTexture->AddDirtyRect( NULL ) ) ) {
				return E_FAIL;
			}

			if ( FAILED( pTexture->UnlockRect( 0 ) ) ) {
				return E_FAIL;
			}
		}

#ifdef DEBUG_SURFACE
		D3DXSaveTextureToFile( TEXT( "D:\\lightTex.png" ), D3DXIFF_PNG, pTexture, NULL );
#endif

		*pOutTexture = pTexture;
		return S_OK;
	}

	HRESULT _ImmutableLightImpl::CopyToMemory( LPDIRECT3DVERTEXBUFFER9 dest, LPVOID src, UINT size ) const
	{
		ASSERT( size );

		LPVOID pVertices{};

		if ( FAILED( dest->Lock( 0, size, &pVertices, 0 ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		memcpy( pVertices, src, size );
		dest->Unlock();

		return S_OK;
	}

	HRESULT _ImmutableLightImpl::UpdateLightIndexBuffer( LPDIRECT3DINDEXBUFFER9* pOut, Indices& indices, LPDIRECT3DDEVICE9 pDevice, size_t vertexSize ) const
	{
		// 인덱스를 만든다
		// https://en.cppreference.com/w/cpp/algorithm/generate
		auto increaseNumber = [n = 0]() mutable { return n++; };

		indices.resize( vertexSize );
		std::generate( indices.begin(), indices.end(), increaseNumber );

		// 항상 LT 위치에서 끝나야 한다
		indices.push_back( 1 );
		auto indicesSize = sizeof( Indices::value_type ) * indices.size();

		LPDIRECT3DINDEXBUFFER9 pIndexBuffer{};

		// 인덱스 버퍼 갱신
		{
			if ( FAILED( pDevice->CreateIndexBuffer( static_cast<UINT>( indicesSize ), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &pIndexBuffer, NULL ) ) ) {
				ASSERT( FALSE );
				return E_FAIL;
			}

			LPVOID pIndices{};

			if ( pIndexBuffer->Lock( 0, static_cast<UINT>( indicesSize ), &pIndices, 0 ) ) {
				ASSERT( FALSE );
				return E_FAIL;
			}

			memcpy( pIndices, indices.data(), indicesSize );
			pIndexBuffer->Unlock();
		}


		SAFE_RELEASE( *pOut );
		*pOut = pIndexBuffer;

		return S_OK;
	}

	HRESULT _ImmutableLightImpl::UpdateLightVertexBuffer( LPDIRECT3DVERTEXBUFFER9* pOut, Vertices& vertices, LPDIRECT3DDEVICE9 pDevice, const Points& points, float falloff )
	{
		ASSERT( !points.empty() );

		vertices.clear();

		// 프리폼 조명의 위치를 화면 중앙에 놓는다
		auto updateVertex = [i = -1, falloff]( const D3DXVECTOR3& position ) mutable {
			auto uv = ( ++i % 2 ? D3DXVECTOR2{ falloff, 0 } : D3DXVECTOR2{ 0, 0 } );

			return CUSTOM_VERTEX{ position, uv };
		};
		std::transform( std::cbegin( points ), std::cend( points ), std::back_inserter( vertices ), updateVertex );
		vertices[0].uv = { falloff, falloff };

		auto verticesSize = sizeof( Vertices::value_type ) * vertices.size();
		LPDIRECT3DVERTEXBUFFER9 pVertexBuffer{};

		// 버텍스 버퍼 갱신
		{
			if ( FAILED( pDevice->CreateVertexBuffer( static_cast<UINT>( verticesSize ), 0, m_lightVertexFvf, D3DPOOL_DEFAULT, &pVertexBuffer, NULL ) ) ) {
				ASSERT( FALSE );
				return E_FAIL;
			}

			CopyToMemory( pVertexBuffer, vertices.data(), static_cast<UINT>( verticesSize ) );
		}


		SAFE_RELEASE( *pOut );
		*pOut = pVertexBuffer;

		return S_OK;
	}

	// TODO: use async
	HRESULT _ImmutableLightImpl::UpdateBlurMask( LPDIRECT3DDEVICE9 pDevice, const Vertices& vertices )
	{
		if ( !m_blurMask.m_pMesh ) {
			if ( FAILED( CreateMesh( pDevice, &m_blurMask.m_pMesh, 1, 1 ) ) ) {
				return E_FAIL;
			}
		}

		float width{};
		float height{};
		float cx{};
		float cy{};
		// 외곽에 블러 처리를 먹이기 위해 크기를 키운다
		constexpr auto meshScaling = 1.5f;

		// 마스크 메시의 위치 설정
		{
			float left{ FLT_MAX };
			float right{ FLT_MIN };
			float top{ FLT_MAX };
			float bottom{ FLT_MIN };

			// 정점의 LT, RT, LB, RB를 알아낸다
			for ( auto& vertice : vertices ) {
				auto& p = vertice.position;
				left = min( left, p.x );
				top = min( top, p.y );
				right = max( right, p.x );
				bottom = max( bottom, p.y );
			}

			// w, h를 구한다
			width = right - left;
			height = bottom - top;
			cx = left + width / 2;
			cy = top + height / 2;

			// 크기 행렬
			D3DXMATRIX sm{};
			D3DXMatrixScaling( &sm, width * meshScaling, height * meshScaling, 1 );
			// 이동 행렬
			D3DXMATRIX tm{};
			D3DXMatrixTranslation( &tm, cx, cy, 0 );

			m_blurMask.m_worldTransform = sm * tm;
		}

		// 블러를 강화시키도록 텍스처를 줄인다. 값이 너무 작으면 그림에 계단 현상이 생긴다
		constexpr float textureScaling = 0.5f;

		// 마스크 복사
		{
			auto& pTexture = m_blurMask.m_pTexture;
			auto maskWidth = static_cast<UINT>( width * textureScaling );
			auto maskHeight = static_cast<UINT>( height * textureScaling );

			// 크기가 다르면 다시 만든다
			if ( pTexture ) {
				LPDIRECT3DSURFACE9 pSurface{};
				pTexture->GetSurfaceLevel( 0, &pSurface );

				D3DSURFACE_DESC desc{};
				pSurface->GetDesc( &desc );

				SAFE_RELEASE( pSurface );

				if ( desc.Width != maskWidth || desc.Height != maskHeight ) {
					SAFE_RELEASE( pTexture );
				}
			}

			if ( !pTexture ) {
				if ( FAILED( CreateTexture( pDevice, &pTexture, maskWidth, maskHeight ) ) ) {
					return E_FAIL;
				}
			}

			// 마스크를 중앙에 복사한다
			if ( SUCCEEDED( pDevice->BeginScene() ) ) {
				LPDIRECT3DSURFACE9 pCurrrentSurface{};
				pDevice->GetRenderTarget( 0, &pCurrrentSurface );

				LPDIRECT3DSURFACE9 pMaskSurface{};
				m_blurMask.m_pTexture->GetSurfaceLevel( 0, &pMaskSurface );
				pDevice->SetRenderTarget( 0, pMaskSurface );
				pDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB( 0, 0, 0, 0 ), 1.0f, 0 );

				D3DXMATRIX oldVm{};
				pDevice->GetTransform( D3DTS_VIEW, &oldVm );

				D3DXMATRIX oldWm{};
				pDevice->GetTransform( D3DTS_WORLD, &oldWm );

				D3DXMATRIX oldPm{};
				pDevice->GetTransform( D3DTS_PROJECTION, &oldPm );

				// 새 행렬 설정
				{
					D3DXMATRIX pm{};
					D3DXMatrixOrthoLH( &pm, width, height, -1, 1 );
					pDevice->SetTransform( D3DTS_PROJECTION, &pm );

					auto x = width / 2.f;
					auto y = height / 2.f;
					D3DXVECTOR3 eye{ x, y, 1 };
					D3DXVECTOR3 at{ x, y, -1 };
					D3DXVECTOR3 up{ 0, -1, 0 };

					D3DXMATRIX vm{};
					D3DXMatrixLookAtLH( &vm, &eye, &at, &up );

					D3DSURFACE_DESC desc{};
					pMaskSurface->GetDesc( &desc );

					D3DXMATRIX sm{};
					D3DXMatrixScaling( &sm, width / desc.Width / meshScaling * textureScaling, height / desc.Height / meshScaling * textureScaling, 1.f );
					vm *= sm;
					pDevice->SetTransform( D3DTS_VIEW, &vm );

					D3DXMATRIX tm{};
					D3DXMatrixTranslation( &tm, x - cx, y - cy, 0 );
					pDevice->SetTransform( D3DTS_WORLD, &tm );
				}

				D3DVERTEXBUFFER_DESC vertexBufferDesc = {};
				m_pLightVertexBuffer->GetDesc( &vertexBufferDesc );

				auto primitiveCount = m_lightIndices.size() - 2;

				DWORD oldFVF = {};
				pDevice->GetFVF( &oldFVF );
				pDevice->SetFVF( vertexBufferDesc.FVF );
				pDevice->SetTexture( 0, m_pLightTexture );
				pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
				pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
				pDevice->SetStreamSource( 0, m_pLightVertexBuffer, 0, sizeof( Vertices::value_type ) );
				pDevice->SetIndices( m_pLightIndexBuffer );
				pDevice->SetPixelShader( m_pBlurPixelShader );

				const D3DXVECTOR4 blurDatas[] = {
					{ 0.f, width, 0.f, 0.f },
					{ 1.f, width, 0.f, 0.f },
				};

				for ( auto& blurData : blurDatas ) {
					pDevice->SetPixelShaderConstantF( 0, blurData, 1 );
					pDevice->DrawIndexedPrimitive( D3DPT_TRIANGLEFAN, 0, 0, static_cast<UINT>( m_lightVertices.size() ), 0, static_cast<UINT>( primitiveCount ) );
				}

				pDevice->SetPixelShader( nullptr );
				pDevice->SetFVF( oldFVF );

				pDevice->EndScene();
				pDevice->SetRenderTarget( 0, pCurrrentSurface );
				pDevice->SetTransform( D3DTS_VIEW, &oldVm );
				pDevice->SetTransform( D3DTS_PROJECTION, &oldPm );
				pDevice->SetTransform( D3DTS_WORLD, &oldWm );

				SAFE_RELEASE( pCurrrentSurface );
				SAFE_RELEASE( pMaskSurface );

#ifdef DEBUG_SURFACE
				D3DXSaveTextureToFile( TEXT( "D:\\lightTex.png" ), D3DXIFF_PNG, m_blurMask.m_pTexture, NULL );
#endif
			}
		}

		return S_OK;
	}

	HRESULT _ImmutableLightImpl::CreateTexture( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DTEXTURE9* pOutTexture, UINT width, UINT height )
	{
		ASSERT( !*pOutTexture );

		LPDIRECT3DTEXTURE9 pTexture = {};

		if ( FAILED( pDevice->CreateTexture( width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pTexture, NULL ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		*pOutTexture = pTexture;

		return S_OK;
	}

	HRESULT _ImmutableLightImpl::CreateMesh( LPDIRECT3DDEVICE9 pDevice, LPD3DXMESH* pOutMesh, UINT width, UINT height )
	{
		ASSERT( !*pOutMesh );

		LPD3DXMESH pMesh = {};
		constexpr auto vertexCount = 4;
		constexpr auto faceCount = 2;
		constexpr auto fvf = D3DFVF_XYZ | D3DFVF_TEX1;

		if ( FAILED( D3DXCreateMeshFVF( faceCount, vertexCount, D3DXMESH_WRITEONLY, fvf, pDevice, &pMesh ) ) ) {
			return E_FAIL;
		}

		// 버텍스 버퍼 채우기
		{
			auto w = width / 2.f;
			auto h = height / 2.f;

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
			pMesh->UnlockVertexBuffer();
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

	HRESULT _ImmutableLightImpl::Draw( LPDIRECT3DDEVICE9 pDevice )
	{
		// 마스크 메시를 그린다
		if ( SUCCEEDED( pDevice->BeginScene() ) ) {
			D3DMATRIX wm{};
			pDevice->GetTransform( D3DTS_WORLD, &wm );
			pDevice->SetTransform( D3DTS_WORLD, &m_blurMask.m_worldTransform );
			pDevice->SetTexture( 0, m_blurMask.m_pTexture );

			pDevice->SetTexture( 0, m_blurMask.m_pTexture );
			pDevice->SetFVF( m_blurMask.m_pMesh->GetFVF() );
			m_blurMask.m_pMesh->DrawSubset( 0 );

			pDevice->SetTransform( D3DTS_WORLD, &wm );
			pDevice->EndScene();
		}

		return S_OK;
	}

	HRESULT _ImmutableLightImpl::RestoreDevice( LPDIRECT3DDEVICE9 pDevice )
	{
		ASSERT( !m_pLightTexture );
		ASSERT( !m_pLightIndexBuffer );
		ASSERT( !m_pLightVertexBuffer );
		ASSERT( !m_blurMask.m_pTexture );
		ASSERT( !m_blurMask.m_pMesh );

		return ReadyToRender( pDevice );
	}

	HRESULT _ImmutableLightImpl::ReadyToRender( LPDIRECT3DDEVICE9 pDevice )
	{
		// 초기화
		if ( FAILED( CreateLightTextureByLockRect( pDevice, &m_pLightTexture, m_setting ) ) ) {
			ASSERT( FALSE );

			return E_FAIL;
		}
		else if ( FAILED( UpdateLightVertexBuffer( &m_pLightVertexBuffer, m_lightVertices, pDevice, m_points, m_setting.falloff ) ) ) {
			ASSERT( FALSE );

			return E_FAIL;
		}
		else if ( FAILED( UpdateLightIndexBuffer( &m_pLightIndexBuffer, m_lightIndices, pDevice, m_points.size() ) ) ) {
			ASSERT( FALSE );

			return E_FAIL;
		}
		// 현재 메시를 마스크로 찍어낸다. 정점이 업데이트된 경우에도 그러함
		else if ( FAILED( UpdateBlurMask( pDevice, m_lightVertices ) ) ) {
			ASSERT( FALSE );

			return E_FAIL;
		}

		return S_OK;
	}

	void _ImmutableLightImpl::Invalidate()
	{
		SAFE_RELEASE( m_pLightTexture );
		SAFE_RELEASE( m_pLightIndexBuffer );
		SAFE_RELEASE( m_pLightVertexBuffer );

		SAFE_RELEASE( m_blurMask.m_pTexture );
		SAFE_RELEASE( m_blurMask.m_pMesh );
	}
}