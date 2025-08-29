#include "EglContext.h"
#include <android/log.h>  // 로그 출력을 위한 헤더

bool EglContext::init(ANativeWindow* window) {
  // EGLDisplay 생성
  m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (m_display == EGL_NO_DISPLAY) {
    __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Unable to get EGLDisplay");
    return false;
  }

  // egl 초기화
  if (!eglInitialize(m_display, nullptr, nullptr)) {
    __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Failed to initialize EGL");
    return false;
  }

  // EGLConfig 설정
  EGLint configAttribs[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
    EGL_RED_SIZE,   8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE,  8,
    EGL_ALPHA_SIZE, 8,
    EGL_NONE
  };
  EGLConfig eglConfig;
  EGLint numConfigs;
  if (!eglChooseConfig(m_display, configAttribs, &eglConfig, 1, &numConfigs) || numConfigs < 1) {
    __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Failed to choose EGL config");
    return false;
  }

  // EGLContext 생성
  EGLint contextAttribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 3,
    EGL_NONE
  };
  m_context = eglCreateContext(m_display, eglConfig, EGL_NO_CONTEXT, contextAttribs);
  if (m_context == EGL_NO_CONTEXT) {
    __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Failed to create EGL context");
    return false;
  }

  // 생성된 EGLContext 기반으로 EGLSurface 생성 -> android native window 와 연결
  m_surface = eglCreateWindowSurface(m_display, eglConfig, window, nullptr);
  if (m_surface == EGL_NO_SURFACE) {
    __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Failed to create EGL surface");
    return false;
  }

  return makeCurrent();
};

bool EglContext::makeCurrent() {
  // 생성된 EGLContext 를 현재 스레드에 바인딩하여 skia ganesh 백엔드가 사용할 수 있도록 함.
  return eglMakeCurrent(m_display, m_surface, m_surface, m_context) == EGL_TRUE;
};

bool EglContext::swapBuffer() {
  // 디스플레이 vsync 시점에 맞춰 back buffer 와 front buffer 를 교체하여 화면 업데이트
  return eglSwapBuffers(m_display, m_surface) == EGL_TRUE;
};

void EglContext::destroy() {
  if (m_display != EGL_NO_DISPLAY) {
    eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    // EGLContext 메모리 해제
    if (m_context != EGL_NO_CONTEXT) {
      eglDestroyContext(m_display, m_context);
      m_context = EGL_NO_CONTEXT;
    }

    // EGLSurface 메모리 해제
    if(m_surface != EGL_NO_SURFACE) {
      eglDestroySurface(m_display, m_surface);
      m_surface = EGL_NO_SURFACE;
    }

    // EGLDisplay 메모리 해제
    eglTerminate(m_display);
    m_display = EGL_NO_DISPLAY;
  }
};

bool EglContext::isEGLContextCurrent() {
  EGLDisplay curDisplay = eglGetCurrentDisplay();
  EGLContext curContext = eglGetCurrentContext();

  if (curDisplay == EGL_NO_DISPLAY || curContext == EGL_NO_CONTEXT) {
    return false;
  }
  if (m_context != EGL_NO_CONTEXT && curContext != m_context) {
    return false;
  }

  return true;
};
