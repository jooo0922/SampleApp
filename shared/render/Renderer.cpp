#include "Renderer.h"
#include <android/log.h>  // 로그 출력을 위한 헤더
#include <chrono>

void Renderer::start(ANativeWindow* pWindow) {
  m_pNativeWindow = pWindow;

  if (m_pNativeWindow) {
    // 렌더링 루프 시작
    m_bIsRendering = true;
    // 렌더링 루프는 별도의 렌더링 스레드 생성하여 돌린다. (ui 스레드와 별개로 돌아가도록 하여 오버헤드를 줄이려는 목적)
    m_renderThread = std::thread([this]() { process(); });
  };
};

void Renderer::resize(int width, int height) {
  // skia 내부 framebuffer 에 적용할 이미지 크기 업데이트
  m_width = width;
  m_height = height;

  if (m_pNativeWindow) {
    // android surface 설정(해상도, pixel format)
    ANativeWindow_setBuffersGeometry(m_pNativeWindow, m_width, m_height, WINDOW_FORMAT_RGBA_8888);

    // android surface 크기 변경 시 렌더링 루프에서 감지하여 SkSurface 재생성하도록 리사이징 요청
    m_resizeRequested = true;
  }
};

void Renderer::stop() {
  if (m_renderThread.joinable()) {
    m_renderThread.join();
  }

  if (m_pNativeWindow) {
    ANativeWindow_release(m_pNativeWindow);
    m_pNativeWindow = nullptr;
  }
};

void Renderer::addDrawable(std::shared_ptr<IDrawable> drawable) {
  std::lock_guard<std::mutex> lock(m_drawablesMtx);
  m_drawables.push_back(std::move(drawable));
};

void Renderer::clearDrawables() {
  std::lock_guard<std::mutex> lock(m_drawablesMtx);
  m_drawables.clear();
};

void Renderer::process() {
  // 렌더링 루프 진입 직전 EGL 초기화 수행
  if (!m_egl.init(m_pNativeWindow)) {
    __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "EGL initialization failed");
    return;
  }

  // ganesh gpu 백엔드 기반 SkSurface 생성
  const int width = m_width.load();
  const int height = m_height.load();
  if (!m_skia.setupSkiaSurface(width, height)) {
    __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Failed to setup Skia surface");
    return;
  }

  auto prev = std::chrono::steady_clock::now();

  while (m_bIsRendering)
  {
    // 렌더링 루프 시작 시 매 프레임마다 SkSurface 재생성이 필요한지 체크
    if (m_resizeRequested.exchange(false)) {
      // SkSurface 재생성 요청 소비
      const int width = m_width.load();
      const int height = m_height.load();
      if (!m_skia.setupSkiaSurface(width, height)) {
        __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Failed to recreate Skia surface in rendering loop");
        continue;  // 혹은 break; 로 종료
      }
    }

    // 현재 프레임의 delta time 계산
    auto curr = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(curr - prev).count();
    prev = curr;

    if (auto* canvas = m_skia.canvas()) {
      // 캔버스 초기화
      canvas->clear(SK_ColorLTGRAY);
      
      // 렌더링 스레드에서 컨테이너에 추가된 렌더링 객체 포인터들(공유 자원) 접근 시 스냅샷 캡쳐하여 얕은 복사 수행
      std::vector<std::shared_ptr<IDrawable>> drawables;
      {
        std::lock_guard<std::mutex> lock(m_drawablesMtx);
        drawables = m_drawables;
      }

      // mutex 락 해제 후 캡쳐 뜬 포인터로 렌더링 객체들에 접근하여 draw call 수행
      for (auto& drawable : drawables) {
        if (drawable) {
          drawable->update(dt);
          drawable->draw(canvas);
        }
      }

      // Skia 내부 command queue 에 쌓인 현재 프레임까지 요청된 모든 draw operation 들을 GPU 로 전송하여 실행 요청
      m_skia.flush();
    }

    // 디스플레이 vsync 시점에 맞춰 back buffer 와 front buffer 를 교체하여 화면 업데이트
    m_egl.swapBuffer();

    // 16ms 대기 (약 60FPS)
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  // 렌더링 루프 종료 후 각종 자원 해제
  m_egl.destroy();
  m_skia.destroy();
};
