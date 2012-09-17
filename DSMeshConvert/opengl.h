#pragma once
#ifndef _OPENGL_H_
#define _OPENGL_H_

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <gl/GL.h>
#include <gl/GLU.h>

void InitOpenGL();
bool EndSceneOpenGL();
void BeginSceneOpenGL();
void ColorOpenGL(u32 idx, float alpha = 1.f);

#endif // _OPENGL_H_