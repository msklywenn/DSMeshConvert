#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WindowsX.h>
#include "types.h"
#include <gl/GL.h>
#include <gl/GLU.h>
#include <math.h>
#include <stdlib.h>

float minf(float a, float b)
{
	return a < b ? a : b;
}

float maxf(float a, float b)
{
	return a > b ? a : b;
}

HGLRC glContext;
HDC dcContext;
HWND window;
s32 mouseX = 0, mouseY = 0;
bool mousedown = false;
float rotatePhi = 0.f;
float rotateTheta = 0.f;
float dist = 4.f;
float h = .5f;

static LRESULT CALLBACK WinProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch ( message )
	{
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
			return 0;
		}

		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}

		default:
		{
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
}

void InitOpenGL()
{
	HINSTANCE instance = GetModuleHandle(0);
	WNDCLASSEX winClass =
	{
		sizeof(WNDCLASSEX),					// cbSize
		CS_HREDRAW | CS_VREDRAW | CS_OWNDC,	// style
		WinProc,						// lpfnWndProc
		0,									// cbClsExtra
		0,									// cbWndExtra
		instance,							// hInstance
		0,									// hIcon
		0,									// hCursor
		(HBRUSH)(COLOR_WINDOW + 1),			// hbrBackground
		0,									// lpszMenuName
		"OpenGL",							// lpszClassName
		0									// hIconSm
	};
	RegisterClassEx(&winClass);
	RECT rect = {0, 0, 640, 480};
	u32 style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	u32 styleEx = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
	AdjustWindowRectEx(&rect, style, FALSE, styleEx);
	window = CreateWindow("OpenGL", "OpenGL", style, CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top, 0, 0, instance, 0);
	PIXELFORMATDESCRIPTOR pfd;
	ZeroMemory(&pfd, sizeof(pfd));
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;

	dcContext = GetDC(window);
	int pixel = ChoosePixelFormat(dcContext, &pfd);

	SetPixelFormat(dcContext, pixel, &pfd);

	glContext = wglCreateContext(dcContext);
	wglMakeCurrent(dcContext, glContext);

	ShowWindow(window, 1);
	SetForegroundWindow(window);

	glClearColor(208.f/255.f, 192.f/255.f, 122.f/255.f, 0.f);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.f, 4./3., .1f, 1000.f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	//glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void DestroyOpenGL()
{
	// don't care o//
}

void BeginSceneOpenGL()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();
	gluLookAt(cosf(rotatePhi) * cosf(rotateTheta) * dist, 
		sinf(rotateTheta) * dist + h,
		sinf(rotatePhi) * cosf(rotateTheta) * dist,
		0.f, h, 0.f,
		0., 1., 0.);
}

bool EndSceneOpenGL()
{
	MSG msg;
	while ( PeekMessage(&msg, 0, 0, 0, PM_REMOVE) )
	{
		switch ( msg.message )
		{
			case WM_QUIT:
				exit(0);

			case WM_LBUTTONDOWN:
				mousedown = true;
				break;

			case WM_LBUTTONUP:
				mousedown = false;
				break;

			case WM_KEYDOWN:
				if ( msg.wParam == 'S' )
					return false;
				if ( msg.wParam == VK_ESCAPE )
					exit(0);
				if ( msg.wParam == VK_DOWN )
					dist = maxf(dist + 0.2f, 1.f);
				if ( msg.wParam == VK_UP )
					dist = minf(dist - 0.2f, 10.f);
				if ( msg.wParam == 'Y' )
					h = maxf(h + 0.02f, -5.f);
				if ( msg.wParam == 'H' )
					h = minf(h - 0.02f, 5.f);
				break;

			case WM_MOUSEMOVE:
			{
				int x = GET_X_LPARAM(msg.lParam);
				int y = GET_Y_LPARAM(msg.lParam);
				if ( mousedown )
				{
					rotatePhi += (x - mouseX) * .005f;
					rotateTheta += (y - mouseY) * .005f;
				}
				mouseX = x;
				mouseY = y;
				break;
			}
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	SwapBuffers(dcContext);

	return true;
}

void ColorOpenGL(u32 idx, float alpha)
{
	// C64 color palette \o/
	// from: http://www.pepto.de/projects/colorvic/
	static const float palette[][3] =
	{
		{   0.000f,   0.000f,   0.000f }, // black
		{ 254.999f, 254.999f, 254.999f }, // white
		{ 103.681f,  55.445f,  43.038f }, // red
		{ 111.932f, 163.520f, 177.928f }, // cyan
		{ 111.399f,  60.720f, 133.643f }, // purple
		{  88.102f, 140.581f,  67.050f }, // green
		{  52.769f,  40.296f, 121.446f }, // blue
		{ 183.892f, 198.676f, 110.585f }, // yellow
		{ 111.399f,  79.245f,  37.169f }, // orange
		{  66.932f,  57.383f,   0.000f }, // brown
		{ 153.690f, 102.553f,  89.111f }, // light red
		{  67.999f,  67.999f,  67.999f }, // dark grey
		{ 107.797f, 107.797f, 107.797f }, // grey
		{ 154.244f, 209.771f, 131.584f }, // light green
		{ 107.797f,  94.106f, 180.927f }, // light blue
		{ 149.480f, 149.480f, 149.480f }, // light grey
	};

	idx %= _countof(palette);
	glColor4f(palette[idx][0]/255.f, palette[idx][1]/255.f, palette[idx][2]/255.f, alpha);
}