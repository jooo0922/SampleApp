#pragma once
#include <EGL/egl.h>
#include <android/native_window.h> // ANativeWindow

class EglContext
{
public:
  // egl 초기화
  bool init(ANativeWindow* window);
  bool makeCurrent();
  bool swapBuffer();
  // egl 리소스 해제
  void destroy();

public:
  EGLDisplay display() const { return m_display; };
  EGLContext context() const { return m_context; };
  EGLSurface surface() const { return m_surface; };

private:
  // egl 설정 관련 전역 변수
  EGLDisplay m_display = EGL_NO_DISPLAY;
  EGLContext m_context = EGL_NO_CONTEXT;
  EGLSurface m_surface = EGL_NO_SURFACE;
};
