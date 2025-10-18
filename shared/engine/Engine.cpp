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
  // 이미 인코딩 중일때는 중복 시작 방지
  if (m_isEncoding.load()) {
    Logger::warn(k_logTag, "Encoding is already in progress.");
    return;
  }

  // Renderer 가 들고 있는 Timeline 스냅샷 획득
  // (Renderer 가 hosting 하고 있는 동일한 Timeline 을 공유하여 인코딩에 사용하기 위한 목적)
  std::shared_ptr<Timeline> timeline;
  {
    std::lock_guard<std::mutex> lock(m_mtx);
    if (!m_renderer) {
      Logger::error(k_logTag, "Renderer is not initialized. Cannot start encoding.");
      return;
    }
    timeline = m_renderer->timelineSnapshot();
  }

  // Timeline 스냅샷 획득 여부 검사
  if (!timeline) {
    Logger::error(k_logTag, "No timeline available for encoding.");
    return;
  }

  // 현재 플랫폼에 맞는 인코더 객체 생성
#if defined (__ANDROID__)
  auto encoder = std::make_shared<AndroidEncoder>();
#elif defined (__APPLE__)
  #if TARGET_OS_IOS
    // TODO : iOS Encoder 객체 생성
    std::shared_ptr<IEncoder> encoder; // 임시
  #endif
#endif

  // 인코더 객체 생성 여부 검사
  if (!encoder) {
    Logger::error(k_logTag, "Failed to create encoder for the current platform.");
  }

  // 인코더 객체에 Timeline 세팅과 준비 작업 수행
  encoder->setTimeline(timeline);
  if (!encoder->prepare(config)) {
    Logger::error(k_logTag, "Encoder preparation failed.");
    return;
  }

  /**
   * Encoder 객체 생성, Timeline 세팅, encoding 준비 등 무거운 작업들은 락 밖에서 미리 처리하고,
   * 공유 상태(m_encoder, m_lastEncodedPath)를 수정할 때만 잠금으로 m_encoder 를 보호한다.
   *
   * -> 멀티스레드 환경(encoding 스레드와 main 스레드가 경합 가능성 존재)에서
   * shared_ptr 레퍼런스 카운트 갱신이 안전하게 이루어지도록 하기 위함
   */
  {
    std::lock_guard<std::mutex> lock(m_mtx);
    m_encoder = encoder;
    m_lastEncodedPath.clear();
  }

  // 인코딩 수행 직전 관련 상태값 초기화
  m_isEncoding.store(true);
  m_cancelFlag.store(false);
  m_encodingProgress.store(0.0);

  // 인코딩 작업을 별도 스레드에서 수행
  m_encodeThread = std::thread([this, encoder]() {
    // 인코딩 진행률 콜백함수 정의
    auto progressCb = [this](double ratio) {
      m_encodingProgress.store(std::clamp(ratio, 0.0, 1.0));
    };

    // 모든 프레임을 돌려 인코딩 수행
    bool ok = encoder->encodeBlocking(m_cancelFlag, progressCb);
    std::string output = ok ? encoder->outputPath() : std::string();
    encoder->release();

    // 인코딩 성공 여부 및 중단 여부에 따라 인코딩 진행률을 최종 갱신
    if (ok && !m_cancelFlag.load()) {
      /** 인코딩 성공 시 진행률을 100% 로 최종 갱신 */
      m_encodingProgress.store(1.0);
    } else if (!ok && !m_cancelFlag.load()) {
      /** 인코딩 실패 시 진행률을 0% 로 최종 갱신 */
      m_encodingProgress.store(0.0);
      Logger::error(k_logTag, "Encoding failed.");
    }

    // 인코딩 결과물(.mp4)가 생성되어 있는 파일 경로 문자열을 멤버변수에 이동
    {
      std::lock_guard<std::mutex> lock(m_mtx);
      if (ok) {
        m_lastEncodedPath = std::move(output);
      }
      // 인코딩 작업이 완료되었으므로 멤버변수로 붙들고 있던 인코더 객체 해제
      m_encoder.reset();
    }

    // 인코딩 작업이 모두 끝났으므로 인코딩 상태 플래그를 false 로 갱신
    m_isEncoding.store(false);
  });
};

void Engine::cancelEncoding() {
  // 인코딩 중이 아닐 때에는 취소 요청 무시
  if (!m_isEncoding.load()) return;

  // 인코딩 취소가 요청되었음을 로그 출력
  Logger::info(k_logTag, "Encoding cancellation requested.");

  // 인코딩 취소 플래그 설정 -> IEncoder::encodeBlocking() 의 인코딩 루프에서 주기적으로 체크하여 인코딩 중단 처리
  m_cancelFlag.store(true);

  // 인코딩 루프 중단 후 인코딩 스레드가 안전하게 종료될 때까지 대기
  joinEncodeThread();
};

void Engine::joinEncodeThread() {
  // 기존에 실행 중인 인코딩 스레드가 있다면 작업이 끝날 때까지 대기 후 스레드 자원 정리.
  // -> 인코딩 스레드 정리 및 중복 생성 방지 목적
  if (m_encodeThread.joinable()) {
    m_encodeThread.join();
  }
};
