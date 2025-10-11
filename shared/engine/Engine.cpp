#include "Engine.h"
#include "../drawables/RotatingRect.h"
#include "../logger/Logger.h"
#include <android/native_window_jni.h> // ANativeWindow_fromSurface, ANativeWindow_release
#include <algorithm> // std::clamp
#if defined (__ANDROID__)
  #include "../encoder/android/AndroidEncoder.h"
#elif defined (__APPLE__)
  #include <TargetConditionals.h>
  #if TARGET_OS_IOS
    // TODO : iOS Encoder 헤더 포함
  #endif
#endif

Engine& Engine::instance() {
  static Engine instance;
  return instance;
}

void Engine::initSurface(ANativeWindow *window)
{
  if (!window) return;

  std::lock_guard<std::mutex> lock(m_mtx);

  // 이미 Native Surface 가 초기화되어 있으면 중복 init 방지
  if (m_rendererStarted) {
    ANativeWindow_release(window);
    return;
  }

  // renderer nullptr check
  if (!m_renderer) {
    m_renderer = std::make_shared<Renderer>();
  }

  // preview controller nullptr check
  if (!m_previewController) {
    m_previewController = std::make_shared<PreviewController>(m_renderer);
  }

  /** drawable 객체 생성 및 추가 */
  // 회전 사각형 생성
  auto pRect = std::make_shared<RotatingRect>();
  pRect->setSize(100.0f, 100.0f);
  pRect->setSpeed(120.0f);
  pRect->setColor(SK_ColorRED);
  m_renderer->addDrawable(pRect);

  // renderer 초기화
  m_renderer->start(window);
  m_rendererStarted = true;

  ANativeWindow_release(window);
};

void Engine::changeSurface(int width, int height)
{
  std::lock_guard<std::mutex> lock(m_mtx);
  if (m_rendererStarted && m_renderer) {
    m_renderer->resize(width, height);
  }
};

void Engine::destroySurface()
{
  std::lock_guard<std::mutex> lock(m_mtx);
  if (!m_rendererStarted) return; // 초기화도 안된 상태에서 메모리 해제 시도 방지
  if (m_renderer) {
    m_renderer->clearDrawables();
    m_renderer->stop();
  }
  m_rendererStarted = false;
};

void Engine::setImageSequence(const std::vector<std::string>& paths, double clipDurSec, double xfadeSec)
{
  std::lock_guard<std::mutex> lock(m_mtx);
  if (!m_renderer) {
    m_renderer = std::make_shared<Renderer>();
  }

  // preview controller nullptr check
  if (!m_previewController) {
    m_previewController = std::make_shared<PreviewController>(m_renderer);
  }

  if (m_previewController) {
    if (m_previewController->setImageSequence(paths, clipDurSec, xfadeSec)) {
      m_lastTimelineDurationSec = m_previewController->durationSec();
    }
  }
};

void Engine::previewPlay()
{
  std::lock_guard<std::mutex> lock(m_mtx);
  if (m_previewController)
  {
    m_previewController->previewPlay();
  }
};

void Engine::previewPause()
{
  std::lock_guard<std::mutex> lock(m_mtx);
  if (m_previewController)
  {
    m_previewController->previewPause();
  }
};

void Engine::previewStop()
{
  std::lock_guard<std::mutex> lock(m_mtx);
  if (m_previewController)
  {
    m_previewController->previewStop();
  }
};

void Engine::startEncoding(const EncoderConfig& config) {

};

void Engine::cancelEncoding() {

};

void Engine::joinEncodeThread() {
  // 기존에 실행 중인 인코딩 스레드가 있다면 작업이 끝날 때까지 대기 후 스레드 자원 정리.
  // -> 인코딩 스레드 정리 및 중복 생성 방지 목적
  if (m_encodeThread.joinable()) {
    m_encodeThread.join();
  }
};
