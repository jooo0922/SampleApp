#pragma once
#include <android/native_window_jni.h> // ANativeWindow_fromSurface, ANativeWindow_release
#include <memory>
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include "./EglContext.h"
#include "./SkiaGanesh.h"
#include "../drawables/IDrawable.h"
#include "../video/Timeline.h"

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
  int surfaceWidth() const { return m_width.load(); };
  int surfaceHeight() const { return m_height.load(); };

public:
  // 타임라인 연결/제어(Preview) 관련 메서드
  void setTimeline(std::shared_ptr<Timeline> tl);
  std::shared_ptr<Timeline> timelineSnapshot();
  void previewPlay();
  void previewPause();
  void previewStop();

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

private:
  std::shared_ptr<Timeline> m_timeline = nullptr;       // m_timeline 초기화 여부에 따라 m_drawables 를 렌더링할 지 타임라인을 렌더링할 지 결정
  std::mutex m_timelineMtx;                             // 각 스레드에서 m_timeline 접근 시 race condition 방지를 위한 동기화 도구
  std::atomic<bool> m_previewPlaying = false;           // 타임라인 재생 여부
  double m_previewTimeSec = 0.0;                        // 현재 타임라인 재생 시간(초) -> 렌더링 스레드에서 갱신
  double m_previewDurationSec = 0.0;                    // 타임라인 전체 길이(초) -> Renderer::setTimeline() 내에서 재계산
};
