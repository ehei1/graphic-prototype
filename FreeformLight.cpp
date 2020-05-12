#include "StdAfx.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_set>
#include "FreeformLight.h"

//#define DEBUG_FREEFORM


void CFreeformLight::InvalidateDeviceObjects()
{
	SAFE_RELEASE( m_pLightTexture );
	SAFE_RELEASE( m_pLightIndexBuffer );
	SAFE_RELEASE( m_pLightVertexBuffer );
}

HRESULT CFreeformLight::RestoreDevice( const D3DDISPLAYMODE& displayMode )
{
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

		ASSERT( m_pLightTexture );
	}

	D3DXVECTOR3 centerPoint{ static_cast<float>( x ), static_cast<float>( y ),{} };
	using Points = std::vector< D3DXVECTOR3 >;
	Points points{ centerPoint };
	// 화면의 절반만 차지하도록 한다
	auto scaledWidth = m_displayMode.Width / 4;
	auto scaledHeight = m_displayMode.Height / 4;

	// 시계 방향으로 면을 살피면서 점을 추가한다
	for ( auto vertices : { m_leftTopSideVertices, m_rightTopSideVertices, m_rightBottomVertices, m_leftBottomVertices } ) {
		points.insert( points.end(), vertices.begin(), vertices.end() );
	}

	auto falloff = m_setting.fallOff;

	Vertices vertices( points.size(), { centerPoint,{ falloff, falloff } } );

	// 프리폼 조명의 위치를 화면 중앙에 놓는다
	auto updateVertex = [it = next( points.cbegin() ), i = -1, x, y, scaledWidth, scaledHeight, falloff]( CUSTOM_VERTEX& vertex ) mutable {
		auto position = *it++;
		auto newPosition = D3DXVECTOR3{ position.x * scaledWidth, position.y * scaledHeight, 0 };
		newPosition += D3DXVECTOR3{ static_cast<float>( x ), static_cast<float>( y ), 0 };

		vertex.position = newPosition;
		vertex.uv = ( ++i % 2 ? D3DXVECTOR2{ falloff, 0 } : D3DXVECTOR2{ 0, 0 } );
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

	auto verticesSize = sizeof( Vertices::value_type ) * vertices.size();

	// 버텍스 버퍼 갱신
	{
		if ( FAILED( pDevice->CreateVertexBuffer( verticesSize, 0, m_lightVertexFvf, D3DPOOL_DEFAULT, &m_pLightVertexBuffer, NULL ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		LPVOID pVertices{};

		if ( FAILED( m_pLightVertexBuffer->Lock( 0, verticesSize, &pVertices, 0 ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		memcpy( pVertices, vertices.data(), verticesSize );
		m_pLightVertexBuffer->Unlock();
	}

	// 인덱스를 만든다
	// https://en.cppreference.com/w/cpp/algorithm/generate
	auto increaseNumber = [n = 0]() mutable { return n++; };

	Indices indices( points.size() );
	std::generate( indices.begin(), indices.end(), increaseNumber );
	// 항상 LT 위치에서 끝나야 한다
	indices.emplace_back( 1 );
	auto indicesSize = sizeof( Indices::value_type ) * indices.size();

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

		memcpy( pIndices, indices.data(), indicesSize );
		m_pLightIndexBuffer->Unlock();
	}

	m_lightVertices = vertices;
	m_lightIndices = indices;

	m_vertexEditingStates.clear();
	m_vertexEditingStates.resize( vertices.size() );

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

	return S_OK;
}

HRESULT CFreeformLight::Draw( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DSURFACE9 pSurface, float x, float y )
{
	if ( !m_pLightVertexBuffer )
	{
		return S_OK;
	}

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

		auto primitiveCount = m_lightIndices.size() - 2;

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
		pDevice->SetStreamSource( 0, m_pLightVertexBuffer, 0, sizeof( Vertices::value_type ) );
		pDevice->SetIndices( m_pLightIndexBuffer );
		pDevice->DrawIndexedPrimitive( D3DPT_TRIANGLEFAN, 0, 0, m_lightVertices.size(), 0, primitiveCount );
		pDevice->EndScene();

		pDevice->SetFVF( curFVF );

		pDevice->SetRenderState( D3DRS_BLENDOP, curBlendOp );
		pDevice->SetRenderState( D3DRS_DESTBLEND, curDestBlend );
		pDevice->SetRenderState( D3DRS_SRCBLEND, curSrcBlend );

#ifdef DEBUG_FREEFORM
		D3DSURFACE_DESC surfaceDesc{};

		if ( SUCCEEDED( pSurface->GetDesc( &surfaceDesc ) ) ) {
			LPDIRECT3DTEXTURE9 pScreenshotTexture{};

			if ( SUCCEEDED( pDevice->CreateTexture( surfaceDesc.Width, surfaceDesc.Height, 1, surfaceDesc.Usage, surfaceDesc.Format, surfaceDesc.Pool, &pScreenshotTexture, NULL ) ) ) {
				LPDIRECT3DSURFACE9 pScreenshotSurface{};

				if ( SUCCEEDED( pScreenshotTexture->GetSurfaceLevel( 0, &pScreenshotSurface ) ) ) {
					if ( SUCCEEDED( pDevice->StretchRect( pSurface, NULL, pScreenshotSurface, NULL, D3DTEXF_NONE ) ) ) {
						D3DXSaveTextureToFile( TEXT( "D:\\mask.png" ), D3DXIFF_PNG, pScreenshotTexture, NULL );
					}
				}

				SAFE_RELEASE( pScreenshotTexture );
			}
		}
#endif
	}

	// 뷰 행렬 복원
	pDevice->SetTransform( D3DTS_VIEW, &curVm );

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

HRESULT CFreeformLight::CreateTexture( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DTEXTURE9* pOutTexture, UINT width, UINT height )
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

HRESULT CFreeformLight::CreateMesh( LPDIRECT3DDEVICE9 pDevice, LPD3DXMESH* pOutMesh, UINT width, UINT height )
{
	ASSERT( !*pOutMesh );

	LPD3DXMESH pMesh = {};
	constexpr auto vertexCount = 4;
	constexpr auto faceCount = 2;
	constexpr auto fvf = D3DFVF_XYZ | D3DFVF_TEX1;

	if ( FAILED( D3DXCreateMeshFVF( faceCount, vertexCount, D3DXMESH_MANAGED, fvf, pDevice, &pMesh ) ) ) {
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

void CFreeformLight::CreateImgui( LPDIRECT3DDEVICE9 pDevice, LONG xCenter, LONG yCenter, bool isVisible )
{
	if ( isVisible ) {
		ImGui::Begin( u8"프리폼", &isVisible );

		auto freeformLightVisible = IsVisible();

		if ( ImGui::Button( freeformLightVisible ? "Remove" : "Add" ) ) {
			if ( freeformLightVisible ) {
				RemoveLight();
			}
			else {
				AddLight( pDevice, xCenter, yCenter );
			}
		}

		if ( freeformLightVisible ) {
			auto newSetting = GetSetting();

			ImGui::ColorEdit3( "Shadow", reinterpret_cast<float*>( &newSetting.shadowColor ) );
			ImGui::ColorEdit3( "Light", reinterpret_cast<float*>( &newSetting.lightColor ) );
			ImGui::SliderFloat( "Intensity", &newSetting.intensity, 0.f, 1.f );
			ImGui::SliderFloat( "Falloff", &newSetting.fallOff, 0, 10 );
			ImGui::Checkbox( "mask only", &newSetting.maskOnly );

			SetSetting( pDevice, newSetting );
		}

		ImGui::End();

		if ( freeformLightVisible ) {
			D3DXMATRIX world{};
			pDevice->GetTransform( D3DTS_WORLD, &world );
			D3DXMATRIX view{};
			pDevice->GetTransform( D3DTS_VIEW, &view );
			D3DXMATRIX projection{};
			pDevice->GetTransform( D3DTS_PROJECTION, &projection );
			D3DVIEWPORT9 viewport{};
			pDevice->GetViewport( &viewport );

			using Coords = std::vector<D3DXVECTOR3>;
			Coords coords{ m_lightVertices.size() };

			auto projectToScreen = [it = m_lightVertices.cbegin(), &world, &view, &projection, &viewport]( auto& position ) mutable{
				D3DXVec3Project( &position, &( it->position ), &viewport, &projection, &view, &world );
				++it;
			};
			std::for_each( std::begin( coords ), std::end( coords ), projectToScreen );

			// 이동 가능한 단추를 그린다. 사실은 부유 창. 
			// 첫째 점은 원점이며, 마지막 점은 첫번째 정점으로 돌아오는 점이므로 그리지 않는다
			for ( auto i = 0; i < coords.size(); ++i ) {
				auto index = m_lightIndices[i];
				auto& coord = coords[index];
				auto name = std::to_string( i );

				// 단추 그림
				{
					auto&& isEditing = m_vertexEditingStates[i];

					ImGui::SetNextWindowPos( { coord.x, coord.y }, isEditing ? ImGuiCond_Once : ImGuiCond_Always );
					ImGui::SetNextWindowSize( { 5, 5 }, ImGuiCond_Always );

					if ( ImGui::Begin( name.c_str(), 0, ImGuiWindowFlags_NoDecoration ) ) {
						isEditing = ImGui::IsItemActive();
						auto nextPos = ImGui::GetWindowPos();

						using Position = std::pair<int, int>;
						Position p0{ static_cast<int>( coord.x ), static_cast<int>( coord.y ) };
						Position p1{ static_cast<int>( nextPos.x ), static_cast<int>( nextPos.y ) };
						
						if ( p0 != p1 ) {
							D3DXVECTOR3 out{};
							D3DXVec3Unproject( &out, &D3DXVECTOR3{ nextPos.x, nextPos.y, 0.f }, &viewport, &projection, &view, &world );
							out.z = {};

							auto x = nextPos.x;
							auto y = nextPos.y;
							UpdateLightVertex( index, out );
						}

						ImGui::LabelText( ".", name.c_str() );
						ImGui::End();
					}
				}
			}

			// 선을 그린다
			{
				auto drawList = ImGui::GetBackgroundDrawList();

				for ( auto i = 2; i < m_lightIndices.size(); ++i ) {
					auto index0 = m_lightIndices[i - 1];
					auto index1 = m_lightIndices[i];
					auto& coord0 = coords[index0];
					auto& coord1 = coords[index1];

					ImVec2 from{ coord0.x, coord0.y };
					ImVec2 to{ coord1.x, coord1.y };

					drawList->AddLine( from, to, ImColor{ 255,0,0 } );
				}
			}
		}
	}
}

HRESULT CFreeformLight::UpdateLightVertex( WORD updatingIndex, const D3DXVECTOR3& position )
{
	if ( m_pLightVertexBuffer ) {
		for ( auto index : m_lightIndices ) {
			if ( updatingIndex == index ) {
				m_lightVertices[index].position = position;

				//// 중점 수정
				//{
				//	auto area = 0.f;
				//	auto centerX = 0.f;
				//	auto centerY = 0.f;
				//	Vertices vertices{ std::next( std::cbegin( m_lightVertices ) ), std::cend( m_lightVertices ) };

				//	for ( auto i = 0; i < vertices.size(); ++i ) {
				//		auto nextIndex = ( i + 1 ) % vertices.size();
				//		auto& p0 = vertices[i].position;
				//		auto& p1 = vertices[nextIndex].position;
				//		auto f = p0.x * p1.y - p1.x * p0.y;

				//		area += f;
				//		centerX += ( p0.x + p1.x ) * f;
				//		centerY += ( p0.y + p1.y ) * f;
				//	}

				//	area *= 0.5 * vertices.size();

				//	centerX /= area;
				//	centerY /= area;

				//	m_lightVertices[0].position = { centerX, centerY, 0 };
				//}

				// 비디오 메모리에 복사
				{
					LPVOID pVertices{};
					auto size = m_lightVertices.size() * sizeof( Vertices::value_type );

					if ( FAILED( m_pLightVertexBuffer->Lock( 0, size, &pVertices, 0 ) ) ) {
						ASSERT( FALSE );
						return E_FAIL;
					}

					memcpy( pVertices, m_lightVertices.data(), size );
					m_pLightVertexBuffer->Unlock();
				}

				return S_OK;
			}
		}
	}

	return E_FAIL;
}