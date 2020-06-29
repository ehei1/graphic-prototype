//-----------------------------------------------------------------------------
// File: Textures.cpp
//
// Desc: Better than just lights and materials, 3D objects look much more
//       convincing when texture-mapped. Textures can be thought of as a sort
//       of wallpaper, that is shrinkwrapped to fit a texture. Textures are
//       typically loaded from image files, and D3DX provides a utility to
//       function to do this for us. Like a vertex buffer, Textures have
//       Lock() and Unlock() functions to access (read or write) the image
//       data. Textures have a width, height, miplevel, and pixel format. The
//       miplevel is for "mipmapped" Textures, an advanced performance-
//       enhancing feature which uses lower resolutions of the texture for
//       objects in the distance where detail is less noticeable. The pixel
//       format determines how the colors are stored in a texel. The most
//       common formats are the 16-bit R5G6B5 format (5 bits of red, 6-bits of
//       green and 5 bits of blue) and the 32-bit A8R8G8B8 format (8 bits each
//       of alpha, red, green, and blue).
//
//       Textures are associated with geometry through texture coordinates.
//       Each vertex has one or more sets of texture coordinates, which are
//       named tu and tv and range from 0.0 to 1.0. Texture coordinates can be
//       supplied by the geometry, or can be automatically generated using
//       Direct3D texture coordinate generation (which is an advanced feature).
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "StdAfx.h"

#include <chrono>
#include <future>
#include <thread>

#pragma warning( disable : 4996 ) // disable deprecated warning 
#include <strsafe.h>
#pragma warning( default : 4996 )


#include "waifu2x\src\w2xconv.h"

#define HAVE_OPENCV
#include <opencv2\opencv.hpp>
#include <d3dx9tex.h>

#include <DirectXTex\DirectXTex.h>

#ifdef _DEBUG
#include "Debug\ps_gaussianblur.h"
#else
#include "Release\ps_gaussianblur.h"
#endif
#include "FreeformLight\light.h"

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
LPDIRECT3D9             g_pD3D = NULL; // Used to create the D3DDevice
LPDIRECT3DDEVICE9       g_pDevice = NULL; // Our rendering device
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL; // Buffer to hold vertices
LPDIRECT3DINDEXBUFFER9  g_pIB = NULL;
LPDIRECT3DTEXTURE9      g_pBackgroundTexture = NULL; // Our texture
LPDIRECT3DTEXTURE9		g_pMainScreenTexture = NULL;
LPD3DXMESH				g_pScreenMesh = NULL;
LPDIRECT3DTEXTURE9		g_pScreenTexture = NULL;
std::unique_ptr<CMutableFreeformLight> g_pFreemformLight = NULL;
const D3DDISPLAYMODE	gDisplayMode{ 1024, 768, 0, D3DFMT_A8R8G8B8 };
float					gScale = 100;
D3DXVECTOR2				gTranslation{};
LPDIRECT3DPIXELSHADER9  g_pBlurPixelShader{};

// A structure for our custom vertex type. We added texture coordinates
struct CUSTOM_VERTEX
{
	D3DXVECTOR3 position;
	D3DXVECTOR2 uv;
};

// Our custom FVF, which describes our custom vertex structure
#define D3DFVF_CUSTOM (D3DFVF_XYZ|D3DFVF_TEX1)

//#define DEBUG_SAMPLE

D3DXCOLOR ImVec4ToD3DXCOLOR( const ImVec4& src )
{
	return{ src.x, src.y, src.z, src.w };
}

ImVec4 D3DXCOLORToImVec4( const D3DXCOLOR& src )
{
	return{ src.r, src.g, src.b, src.a };
}

void check_for_errors( W2XConv* converter, int error )
{
	if ( error )
	{
		char *err = w2xconv_strerror( &converter->last_error );
		std::string errorMessage( err );
		w2xconv_free( err );
		throw std::runtime_error( errorMessage );
	}
}

HRESULT filterWaifu2x(LPVOID pBits, size_t width, size_t height, bool has_alpha)
{
	auto denoise_level = 3;
	auto scale = 1;
	auto block_size = 0;
	/*
	TTA 모드로 하면 품질은 올라가지만 소요 시간은 8배 증가한다고 함
	https://comocloud.tistory.com/129
	*/
	auto converter = w2xconv_init(W2XConvGPUMode::W2XCONV_GPU_AUTO, 1, 0);

	if (auto error = w2xconv_load_model(denoise_level, converter, TEXT("D:\\dot\\Freeform-light\\Debug\\models_rgb"))) {
		ASSERT(FALSE);

		check_for_errors(converter, error);
		return E_FAIL;
	}
	else if (error = w2xconv_convert_memory(converter, width, height, pBits, denoise_level, scale, block_size, has_alpha, CV_8UC4)) {
		ASSERT(FALSE);

		check_for_errors(converter, error);
		return E_FAIL;
	}

	// TODO: release once
	w2xconv_fini(converter);

	return S_OK;
}

HRESULT ApplyWaifu2x( LPDIRECT3DTEXTURE9 pTexture )
{
	LPDIRECT3DSURFACE9 pSurface{};

	if ( FAILED( g_pBackgroundTexture->GetSurfaceLevel( 0, &pSurface ) ) ) {
		return E_FAIL;
	}

	struct AutoRelease
	{
		IUnknown *_pUnknown;

		AutoRelease(IUnknown* pUnknown) : _pUnknown{ pUnknown }
		{
			ASSERT(pUnknown);
		}

		~AutoRelease()
		{
			_pUnknown->Release();
		}
	};

	AutoRelease autoRelease{ pSurface };

	D3DSURFACE_DESC surfaceDesc{};
	pSurface->GetDesc( &surfaceDesc );

	auto has_alpha = false;
	auto width = surfaceDesc.Width;
	auto height = surfaceDesc.Height;
	
	switch (surfaceDesc.Format) {
	case D3DFMT_A4R4G4B4:
		has_alpha = true;
	case D3DFMT_X4R4G4B4:
		{
			DirectX::ScratchImage highColorImage;
			DirectX::ScratchImage trueColorImage;

			D3DLOCKED_RECT lockedRect{};

			if (FAILED(pSurface->LockRect(&lockedRect, NULL, D3DLOCK_READONLY))) {
				return E_FAIL;
			}

			// one mipmap apply
			highColorImage.Initialize2D(DXGI_FORMAT_B4G4R4A4_UNORM, width, height, 1, 1);
			memcpy(highColorImage.GetPixels(), lockedRect.pBits, highColorImage.GetPixelsSize());
			//DirectX::SaveToDDSFile(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::DDS_FLAGS_NONE, L"D:\\study\\original.dds");

			pSurface->UnlockRect();

			// convert high color image to true color
			if (FAILED(DirectX::Convert(highColorImage.GetImages(), highColorImage.GetImageCount(), highColorImage.GetMetadata(), DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_FLAGS::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, trueColorImage))) {
				ASSERT(FALSE);
				return E_FAIL;
			}

			//DirectX::SaveToDDSFile(tImage.GetImages(), tImage.GetImageCount(), tImage.GetMetadata(), DirectX::DDS_FLAGS_NONE, L"D:\\study\\32bit.dds");

			if (FAILED(filterWaifu2x(trueColorImage.GetPixels(), surfaceDesc.Width, surfaceDesc.Height, has_alpha))) {
				return E_FAIL;
			}

			// convert true color to high color
			if (FAILED(DirectX::Convert(trueColorImage.GetImages(), trueColorImage.GetImageCount(), trueColorImage.GetMetadata(), DXGI_FORMAT_B4G4R4A4_UNORM, DirectX::TEX_FILTER_FLAGS::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, highColorImage))) {
				return E_FAIL;
			}

			if (FAILED(pSurface->LockRect(&lockedRect, NULL, D3DLOCK_NO_DIRTY_UPDATE))) {
				return E_FAIL;
			}

			memcpy(lockedRect.pBits, highColorImage.GetPixels(), highColorImage.GetPixelsSize());

			pSurface->UnlockRect();
			pTexture->AddDirtyRect(NULL);

			break;
		}
	case D3DFMT_A8R8G8B8:
		has_alpha = true;
	case D3DFMT_X8R8G8B8:
		{
			D3DLOCKED_RECT lockedRect{};

			if (FAILED(pSurface->LockRect(&lockedRect, NULL, D3DLOCK_NO_DIRTY_UPDATE))) {
				return E_FAIL;
			}
			// make async....
			else if (FAILED(filterWaifu2x(lockedRect.pBits, surfaceDesc.Width, surfaceDesc.Height, has_alpha))) {
				return E_FAIL;
			}

			pSurface->UnlockRect();
			pTexture->AddDirtyRect(NULL);

			if (FAILED(D3DXSaveSurfaceToFile("D:\\study\\b01s.waifu.png", D3DXIFF_PNG, pSurface, NULL, NULL))) {
				return E_FAIL;
			}

			break;
		}

		
	default:
		ASSERT(FALSE);
		return E_FAIL;
	}

	return S_OK;
}


//-----------------------------------------------------------------------------
// Name: InitD3D()
// Desc: Initializes Direct3D
//-----------------------------------------------------------------------------
HRESULT InitD3D( HWND hWnd )
{
    // Create the D3D object.
    if( NULL == ( g_pD3D = Direct3DCreate9( D3D_SDK_VERSION ) ) )
        return E_FAIL;

    // Set up the structure used to create the D3DDevice. Since we are now
    // using more complex geometry, we will create a device with a zbuffer.
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory( &d3dpp, sizeof( d3dpp ) );
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
	d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    // Create the D3DDevice
    if( FAILED( g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_MULTITHREADED | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &d3dpp, &g_pDevice ) ) )
    {
        return E_FAIL;
    }

    // Turn off culling
    g_pDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    // Turn off D3D lighting
    g_pDevice->SetRenderState( D3DRS_LIGHTING, FALSE );

    // Turn on the zbuffer
	g_pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    g_pDevice->SetRenderState( D3DRS_ZENABLE, TRUE);
	g_pDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );

	g_pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );    // set source factor
	g_pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_DESTALPHA );    // set dest factor
	g_pDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );    // set the operation

	// ���� ����Ʈ�� �д´�
	if ( !g_pBlurPixelShader )
	{
		auto function = const_cast<BYTE*>( g_ps21_gaussianblur );

		if ( FAILED( g_pDevice->CreatePixelShader( reinterpret_cast<LPDWORD>( function ), &g_pBlurPixelShader ) ) ) {
			SAFE_RELEASE( g_pBlurPixelShader );
			return E_FAIL;
		}
	}

	g_pFreemformLight.reset( new CMutableFreeformLight{ g_pBlurPixelShader, gDisplayMode } );

	if ( FAILED( g_pDevice->CreateTexture( gDisplayMode.Width, gDisplayMode.Height, 1, D3DUSAGE_RENDERTARGET, gDisplayMode.Format, D3DPOOL_DEFAULT, &g_pMainScreenTexture, NULL ) ) ) {
		return E_FAIL;
	}

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: InitGeometry()
// Desc: Create the Textures and vertex buffers
//-----------------------------------------------------------------------------
HRESULT InitGeometry()
{
	// �����
	{
		// ���ؽ�
		{
			auto w = static_cast<float>( gDisplayMode.Width );
			auto h = static_cast<float>( gDisplayMode.Height );

			CUSTOM_VERTEX positions[]{
				{ { 0, 0, 0.f }, { 0, 0 } },
				{ { 0, h, 0.f }, { 0, 1 } },
				{ { w, h, 0.f }, { 1, 1 } },
				{ { w, 0, 0.f }, { 1, 0 } },
			};

			if ( FAILED( g_pDevice->CreateVertexBuffer( sizeof( positions ), 0, D3DFVF_CUSTOM, D3DPOOL_DEFAULT, &g_pVB, NULL ) ) )
			{
				return E_FAIL;
			}

			LPVOID pVertices = {};

			if ( FAILED( g_pVB->Lock( 0, sizeof( positions ), &pVertices, 0 ) ) )
				return E_FAIL;

			memcpy( pVertices, positions, sizeof( positions ) );
			g_pVB->Unlock();
		}

		// �ε���
		{
			WORD indices[]{
				0, 1, 2, 3,
			};

			if ( FAILED( g_pDevice->CreateIndexBuffer( sizeof( indices ),
				D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
				&g_pIB, NULL ) ) ) {
				return E_FAIL;
			}

			LPVOID pIndices = {};
			g_pIB->Lock( 0, sizeof( indices ), &pIndices, 0 );
			memcpy( pIndices, indices, sizeof( indices ) );
			g_pIB->Unlock();
		}
	}

	FreeformLight::_ImmutableLightImpl::CreateMesh( g_pDevice, &g_pScreenMesh, gDisplayMode.Width, gDisplayMode.Height );
	FreeformLight::_ImmutableLightImpl::CreateTexture( g_pDevice, &g_pScreenTexture, gDisplayMode.Width, gDisplayMode.Height );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Cleanup()
// Desc: Releases all previously initialized objects
//-----------------------------------------------------------------------------
VOID Cleanup()
{
	SAFE_RELEASE( g_pScreenTexture );
	SAFE_RELEASE( g_pScreenMesh );

    if( g_pBackgroundTexture != NULL )
        g_pBackgroundTexture->Release();

	if ( g_pMainScreenTexture != NULL )
		g_pMainScreenTexture->Release();

    if( g_pVB != NULL )
        g_pVB->Release();

	g_pFreemformLight.reset();

    if( g_pDevice != NULL )
        g_pDevice->Release();

    if( g_pD3D != NULL )
        g_pD3D->Release();
}



//-----------------------------------------------------------------------------
// Name: SetupMatrices()
// Desc: Sets up the world, view, and projection transform matrices.
//-----------------------------------------------------------------------------
VOID SetupMatrices()
{
    // Set up world matrix
    D3DXMATRIXA16 matWorld;
    D3DXMatrixIdentity( &matWorld );
    D3DXMatrixRotationX( &matWorld, timeGetTime() / 1000.0f );
    g_pDevice->SetTransform( D3DTS_WORLD, &matWorld );

    // Set up our view matrix. A view matrix can be defined given an eye point,
    // a point to lookat, and a direction for which way is up. Here, we set the
    // eye five units back along the z-axis and up three units, look at the
    // origin, and define "up" to be in the y-direction.
    D3DXVECTOR3 vEyePt( 0.0f, 3.0f,-8.0f );
    D3DXVECTOR3 vLookatPt( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUpVec( 0.0f, 1.0f, 0.0f );
    D3DXMATRIXA16 matView;
    D3DXMatrixLookAtLH( &matView, &vEyePt, &vLookatPt, &vUpVec );
    g_pDevice->SetTransform( D3DTS_VIEW, &matView );

    // For the projection matrix, we set up a perspective transform (which
    // transforms geometry from 3D view space to 2D viewport space, with
    // a perspective divide making objects smaller in the distance). To build
    // a perpsective transform, we need the field of view (1/4 pi is common),
    // the aspect ratio, and the near and far clipping planes (which define at
    // what distances geometry should be no longer be rendered).
    D3DXMATRIXA16 matProj;
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI / 4, 1.0f, 1.0f, 100.0f );
    g_pDevice->SetTransform( D3DTS_PROJECTION, &matProj );
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the scene
//-----------------------------------------------------------------------------
VOID Render()
{	
	auto w = static_cast<float>( gDisplayMode.Width );
	auto h = static_cast<float>( gDisplayMode.Height );
	auto x = w / 2.f;
	auto y = h / 2.f;

	// camera
	{
		D3DXVECTOR3 eye{ x, y, 1 };
		D3DXVECTOR3 at{ x, y, -1 };
		D3DXVECTOR3 up{ 0, -1, 0 };

		D3DXMATRIX projection{};
		D3DXMatrixOrthoLH( &projection, w, h, -1, 1 );
		g_pDevice->SetTransform( D3DTS_PROJECTION, &projection );

		D3DXMATRIX view{};
		D3DXMatrixLookAtLH( &view, &eye, &at, &up );

		D3DXMATRIX scale{};
		D3DXMatrixScaling( &scale, gScale / 100.f, gScale / 100.f, 1 );
		view *= scale;

		D3DXMATRIX translation{};
		D3DXMatrixTranslation( &translation, gTranslation.x, gTranslation.y, 0 );
		view *= translation;

		g_pDevice->SetTransform( D3DTS_VIEW, &view );
	}

	LPDIRECT3DSURFACE9 pCurrentSurface{};
	g_pDevice->GetRenderTarget( 0, &pCurrentSurface );

	// Draw on main screen
	{
		LPDIRECT3DSURFACE9 pMainScreenSurface{};
		g_pMainScreenTexture->GetSurfaceLevel( 0, &pMainScreenSurface );
		g_pDevice->SetRenderTarget( 0, pMainScreenSurface );

		// Clear the backbuffer and the zbuffer
		g_pDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB( 255, 0, 0, 0 ), 0.0f, 0 );

		// Begin the scene
		if ( SUCCEEDED( g_pDevice->BeginScene() ) )
		{
			DWORD currentFVF = {};
			g_pDevice->GetFVF( &currentFVF );
			g_pDevice->SetStreamSource( 0, g_pVB, 0, sizeof( CUSTOM_VERTEX ) );
			g_pDevice->SetIndices( g_pIB );
			g_pDevice->SetFVF( D3DFVF_CUSTOM );

			DWORD curBlendOp = {};
			DWORD curSrcBlend = {};
			DWORD curDestBlend = {};
			g_pDevice->GetRenderState( D3DRS_BLENDOP, &curBlendOp );
			g_pDevice->GetRenderState( D3DRS_SRCBLEND, &curSrcBlend );
			g_pDevice->GetRenderState( D3DRS_DESTBLEND, &curDestBlend );

			g_pDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
			g_pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
			g_pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );

			g_pDevice->SetTexture( 0, g_pBackgroundTexture );
			g_pDevice->DrawIndexedPrimitive( D3DPT_TRIANGLEFAN, 0, 0, 4, 0, 2 );

			g_pDevice->SetRenderState( D3DRS_BLENDOP, curBlendOp );
			g_pDevice->SetRenderState( D3DRS_SRCBLEND, curSrcBlend );
			g_pDevice->SetRenderState( D3DRS_DESTBLEND, curDestBlend );

			// End the scene
			g_pDevice->EndScene();

			// restore previos setting
			g_pDevice->SetFVF( currentFVF );

#ifdef DEBUG_SAMPLE
			D3DXSaveTextureToFile( L"D:\\screenshot.png", D3DXIFF_PNG, g_pMainScreenTexture, NULL );
#endif
		}

		// ������ ����
		// ambient�� ���������� ������ �����ؼ� �ű�Ⱑ ����ϴ�
		{
			LPDIRECT3DSURFACE9 pScreenSurface{};
			g_pScreenTexture->GetSurfaceLevel( 0, &pScreenSurface );

			auto clearColor = g_pFreemformLight->GetAmbientColor();

			LPDIRECT3DSURFACE9 curRT = {};
			g_pDevice->GetRenderTarget( 0, &curRT );
			g_pDevice->SetRenderTarget( 0, pScreenSurface );
			g_pDevice->Clear( 0, 0, D3DCLEAR_TARGET, clearColor, 1.0f, 0 );

			g_pFreemformLight->Draw( g_pDevice, x + gTranslation.x, y + gTranslation.y );

			SAFE_RELEASE( pScreenSurface );
			SAFE_RELEASE( curRT );
		}
	}

	g_pDevice->SetRenderTarget( 0, pCurrentSurface );
	SAFE_RELEASE( pCurrentSurface );

	g_pDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB( 0, 0, 0, 0 ), 0.0f, 0 );

	// Overwrite to current render target
	if ( SUCCEEDED( g_pDevice->BeginScene() ) )
	{
		DWORD curBlendOp = {};
		DWORD curSrcBlend = {};
		DWORD curDestBlend = {};
		g_pDevice->GetRenderState( D3DRS_BLENDOP, &curBlendOp );
		g_pDevice->GetRenderState( D3DRS_SRCBLEND, &curSrcBlend );
		g_pDevice->GetRenderState( D3DRS_DESTBLEND, &curDestBlend );

		DWORD currentFVF = {};
		g_pDevice->GetFVF( &currentFVF );

		{
			g_pDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
			g_pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
			g_pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );

			g_pDevice->SetTexture( 0, g_pMainScreenTexture );
			g_pDevice->SetStreamSource( 0, g_pVB, 0, sizeof( CUSTOM_VERTEX ) );
			g_pDevice->SetIndices( g_pIB );
			g_pDevice->SetFVF( D3DFVF_CUSTOM );
			g_pDevice->DrawIndexedPrimitive( D3DPT_TRIANGLEFAN, 0, 0, 4, 0, 2 );
		}

		// ����ũ �׸���
		{
			D3DXMATRIX curWm = {};
			g_pDevice->GetTransform( D3DTS_WORLD, &curWm );

			// �̵�
			D3DXMATRIX tm{};
			D3DXMatrixTranslation( &tm, x, y, 0 );
			g_pDevice->SetTransform( D3DTS_WORLD, &tm );

			if ( g_pFreemformLight->IsMaskInvisible() ) {
				// ����ũ�� �����ؼ� ���� ȭ���� �׷��� ����Ÿ���� ����� ���Ѵ�
				// result = src * 0 + dest * ( 1 - srcAlpha )
				g_pDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
				g_pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
				g_pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR );
			}

			g_pDevice->SetTexture( 0, g_pScreenTexture );
			g_pScreenMesh->DrawSubset( 0 );

			g_pDevice->SetTransform( D3DTS_WORLD, &curWm );
		}

		g_pDevice->EndScene();
		g_pDevice->SetFVF( currentFVF );
		g_pDevice->SetRenderState( D3DRS_BLENDOP, curBlendOp );
		g_pDevice->SetRenderState( D3DRS_SRCBLEND, curSrcBlend );
		g_pDevice->SetRenderState( D3DRS_DESTBLEND, curDestBlend );
	}
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

//-----------------------------------------------------------------------------
// Name: MsgProc()
// Desc: The window's message handler
//-----------------------------------------------------------------------------
LRESULT WINAPI MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	if ( ImGui_ImplWin32_WndProcHandler( hWnd, msg, wParam, lParam ) )
		return true;

    switch( msg )
    {
        case WM_DESTROY:
            Cleanup();
            PostQuitMessage( 0 );
            return 0;
		case WM_KEYDOWN:
			switch ( wParam ) {
			case VK_LEFT:
				--gTranslation.x;
				break;
			case VK_RIGHT:
				++gTranslation.x;
				break;
			case VK_UP:
				--gTranslation.y;
				break;
			case VK_DOWN:
				++gTranslation.y;
				break;
			case VK_HOME:
				gTranslation = {};
				gScale = 100;
				break;
			}

			return 0;
    }

    return DefWindowProc( hWnd, msg, wParam, lParam );
}




//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: The application's entry point
//-----------------------------------------------------------------------------
INT WINAPI wWinMain( HINSTANCE hInst, HINSTANCE, LPWSTR, INT )
{
    UNREFERENCED_PARAMETER( hInst );

    // Register the window class
    WNDCLASSEX wc =
    {
        sizeof( WNDCLASSEX ), CS_CLASSDC, MsgProc, 0L, 0L,
        GetModuleHandle( NULL ), NULL, NULL, NULL, NULL,
        TEXT( "D3D Tutorial" ), NULL
    };
    RegisterClassEx( &wc );

    // Create the application's window
    HWND hWnd = CreateWindow( TEXT( "D3D Tutorial" ), TEXT( "Freeform Light" ),
                              WS_OVERLAPPEDWINDOW, 100, 100, gDisplayMode.Width, gDisplayMode.Height,
                              NULL, NULL, wc.hInstance, NULL );

    // Initialize Direct3D
    if( SUCCEEDED( InitD3D( hWnd ) ) )
    {	
		//if ( FAILED( D3DXCreateTextureFromFile( g_pd3dDevice, TEXT( "resource\\maplestory-002.jpg" ), &g_pBackgroundTexture ) ) ) {
		if ( FAILED( D3DXCreateTextureFromFile( g_pDevice, TEXT( "D:\\b01.dds" ), &g_pBackgroundTexture ) ) ) {
			return E_FAIL; 
		}

        // Create the scene geometry
        if( SUCCEEDED( InitGeometry() ) )
        {
            // Show the window
            ShowWindow( hWnd, SW_SHOWDEFAULT );
            UpdateWindow( hWnd );

			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO(); (void)io;

			// Setup Dear ImGui style
			ImGui::StyleColorsDark();
			//ImGui::StyleColorsClassic();

			// Setup Platform/Renderer bindings
			ImGui_ImplWin32_Init( hWnd );
			ImGui_ImplDX9_Init( g_pDevice );

			// https://stackoverflow.com/questions/11387564/get-a-font-filepath-from-name-and-style-in-c-windows
			{
				char winDir[MAX_PATH]{};
				GetWindowsDirectoryA( winDir, _countof( winDir ) );

				std::string fontName = "\\fonts\\malgun.ttf";
				fontName.insert( fontName.cbegin(), winDir, winDir + strlen( winDir ) );

				io.Fonts->AddFontFromFileTTF( fontName.c_str(), 15, nullptr, io.Fonts->GetGlyphRangesKorean() );
			}

			auto showWindow = true;

			// asynchronous job
			std::future< HRESULT > waifu2x_future = std::async(std::launch::async, ApplyWaifu2x, g_pBackgroundTexture);
			auto start_clock = std::chrono::steady_clock::now();
			decltype(start_clock) end_clock;

			// synchronous job
			//if (FAILED(ApplyWaifu2x(g_pBackgroundTexture))) {
			//	return E_FAIL;
			//}

            // Enter the message loop
			MSG msg{};

            while( msg.message != WM_QUIT )
            {
				if ( msg.message == WM_MOUSEWHEEL ) {
					auto delta = GET_WHEEL_DELTA_WPARAM( msg.wParam ) / static_cast<float>( WHEEL_DELTA );
					gScale += delta;

					gScale = MAX( gScale, 50 );
					gScale = MIN( gScale, 200 );
				}

                if( PeekMessage( &msg, NULL, 0U, 0U, PM_REMOVE ) )
                {
					TranslateMessage( &msg );
					DispatchMessage( &msg );
					continue;
                }

				// Start the Dear ImGui frame
				ImGui_ImplDX9_NewFrame();
				ImGui_ImplWin32_NewFrame();
				ImGui::NewFrame();

				ImGui::SetNextWindowPos( { gDisplayMode.Width / 2.f, gDisplayMode.Height / 2.f }, ImGuiCond_Once );

				// Create ImGui widget
				auto xCenter = gDisplayMode.Width / 2;
				auto yCenter = gDisplayMode.Height / 2;
				g_pFreemformLight->DrawImgui( g_pDevice, xCenter, yCenter, true, &showWindow );

				using namespace std::chrono_literals;
				auto status = waifu2x_future.wait_for(0.01s);

				if (status == std::future_status::ready) {
					// write code to defend about deleted memory

					ImGui::Text("applied waifu2x.... ");
				}
				else if (status == std::future_status::timeout) {
					ImGui::Text("applying waifu2x....");
				}

				// Rendering
				ImGui::EndFrame();

				Render();

				if ( SUCCEEDED( g_pDevice->BeginScene() ) ) {
					ImGui::Render();
					ImGui_ImplDX9_RenderDrawData( ImGui::GetDrawData() );

					g_pDevice->EndScene();
				}

				// Present the backbuffer contents to the display
				g_pDevice->Present( NULL, NULL, NULL, NULL );
            }
        }
    }

	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	SAFE_RELEASE( g_pDevice );
	SAFE_RELEASE( g_pD3D );

    UnregisterClass( TEXT( "D3D Tutorial" ), wc.hInstance );
    return 0;
}