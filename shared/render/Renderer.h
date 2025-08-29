#pragma once
#include <android/native_window_jni.h> // ANativeWindow_fromSurface, ANativeWindow_release
#include <memory>
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include "EglContext.h"
#include "SkiaGanesh.h"
#include "drawables/IDrawable.h"

class Renderer
{
public:
  ~Renderer() { stop(); };

public:
  void start(ANativeWindow* pWindow);
  void resize(int width, int height);
  void stop();

public:
  // skia 렌더링 객체 추가
  void addDrawable(std::shared_ptr<IDrawable> drawable);
  // skia 렌더링 객체 전부 제거
  void clearDrawables();

public:
  // 현재 스레드에 EGLContext 바인딩 보장용 Assertion
  void assertEGLContextCurrent() const;

private:
  void process();

private:
  EglContext m_egl;
  SkiaGanesh m_skia;
  ANativeWindow* m_pNativeWindow = nullptr;             // Android Surface를 저장할 전역 변수
  std::thread m_renderThread;                           // 렌더링 루프를 실행할 스레드
  std::atomic<bool> m_bIsRendering = false;             // 렌더링 루프 상태
  std::atomic<int> m_width = 0;                         // skia 내부에서 렌더링할 이미지(framebuffer) width
  std::atomic<int> m_height = 0;                        // skia 내부에서 렌더링할 이미지(framebuffer) height
  std::atomic<bool> m_resizeRequested = false;          // skia surface 리사이징 요청
  std::vector<std::shared_ptr<IDrawable>> m_drawables;  // skia 내부에서 렌더링할 객체들
  std::mutex m_drawablesMtx;                            // 렌더링 객체 컨테이너 mutex
};
