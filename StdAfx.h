#include <algorithm>
#include <array>
#include <vector>
#include <unordered_map>
#include <memory>

#include <assert.h>
#include <d3dx9.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <mmsystem.h>
#include <tchar.h>
#include <Windows.h>

#include "imgui\imgui.h"
#include "imgui_internal.h"
#include "imgui\backends\imgui_impl_dx9.h"
#include "imgui\backends\imgui_impl_win32.h"

#define SAFE_RELEASE(p) if(p) { (p)->Release( ); (p)=NULL;}
#define SAFE_DELETE(p)  if(p) { delete (p); (p)=NULL; }
#define ASSERT( x ) assert( x )