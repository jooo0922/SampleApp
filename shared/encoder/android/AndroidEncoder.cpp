#include "./AndroidEncoder.h"

#include <android/log.h>  // 로그 출력을 위한 헤더

// [cmath/ fcntl/ unistd 헤더 용도 설명]
// - <cmath>    : 프레임 수/시간 계산에 사용
//                예) std::ceil(dur * fps), std::llround(t * 1e9), std::max(...)
// - <fcntl.h>  : 출력 mp4 파일 "열기"에 사용 (POSIX open)
//                예) m_outputFd = ::open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
// - <unistd.h> : 파일 "닫기" 등 POSIX 함수에 사용
//                예) ::close(m_outputFd);
#include <utility>      // std::move 사용을 위해
#include <cmath>        // 수학 함수 (ceil, llround -> 반올림해서 long long 정수형으로 캐스팅, max)
#include <fcntl.h>      // POSIX open
#include <unistd.h>     // POSIX close

// 로그 매크로(간단히 보기 좋게)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "AndroidEncoder", __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "AndroidEncoder", __VA_ARGS__)

// [COLOR_FormatSurface 상수 설명]
// - 의미: "인코더 입력을 Surface로 받겠다"는 스위치(설정값)
// - 쓰는 곳: AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_COLOR_FORMAT, COLOR_FormatSurface);
// - 왜 필요: 이걸 켜야 AMediaCodec_createInputSurface(...)로 Codec 이 결과물을 그릴 수 있는 전용 Native Surface를 만들어 준다.
// - 왜 하드코딩: NDK 헤더에 이 상수가 심볼로 노출되지 않아 값(자바 쪽 상수값)을 직접 정의해 사용한다.
static const int32_t COLOR_FormatSurface = 0x7F000789;

// [eglPresentationTimeANDROID 함수 포인터 설명]
// - 의미: "이 프레임을 언제 보여줄지" 시간을 붙이는 함수의 주소(프레젠테이션 타임, PTS)
// - 로드: s_eglPresentationTimeANDROID = (PFN...) eglGetProcAddress("eglPresentationTimeANDROID");
// - 쓰는 곳: setPresentationTimeNs(...)에서 호출 (swapBuffers 직전에 PTS를 붙임)
// - 왜 '함수 포인터'인가: EGL 확장 함수라서 기기/드라이버가 지원할 때만 런타임에 얻어와 호출해야 한다.
typedef void (EGLAPIENTRY* PFNEGLPRESENTATIONTIMEANDROIDPROC)(EGLDisplay, EGLSurface, khronos_stime_nanoseconds_t);
static PFNEGLPRESENTATIONTIMEANDROIDPROC s_eglPresentationTimeANDROID = nullptr;

AndroidEncoder::AndroidEncoder() {};

AndroidEncoder::~AndroidEncoder() {
  release(); // 소멸자에서 안전하게 자원 해제
};

void AndroidEncoder::setTimeline(std::shared_ptr<Timeline> tl) {
  // 인코딩에 사용할 타임라인의 포인터를 보관(프리뷰와 동일한 그림을 얻기 위함)
  m_pTimeline = std::move(tl);
  m_durationSec = (m_pTimeline ? m_pTimeline->totalDuration() : 0.0); // 공유된 타임라인에 계산된 전체 영상 길이 읽어오기.
};

bool AndroidEncoder::prepare(const EncoderConfig& cfg) {
  // 인코딩 설정 옵션 보관
  m_encoderConfig = cfg;

  // 타임라인을 캡쳐해놨는지 확인 후 encoder 준비
  if (!m_pTimeline) {
    LOGE("Timeline not set");
    return false;
  }

  // 1) Codec/native 입력 Surface(ANativeWindow -> offscreen 전용 native surface) 준비
  if (!createCodecAndSurface()) return false;

  // 2) Muxer 준비(출력 파일 오픈)
  if (!openMuxer()) return false;

  // 3) 코덱 시작
  if (!startCodec()) return false;

  // 4) EGL/Skia 준비 (AMediaCodec native surface에 바인딩해서 GL/Skia로 그림을 그리기 위함)
  if (!initEGL()) return false;
  if (!initSkia()) return false;

  return true;
};

bool AndroidEncoder::encodeBlocking(std::atomic<bool>& cancelFlag, std::function<void(double)> onProgress) {
  /** 실제 인코딩을 "끝날 때까지" 수행한다(이 함수를 부른 스레드는 기다린다). -> 이런 동기적 함수를 "blocking" 이라고 표현함. */

  // prepare() 함수에서 준비되지 못한 객체가 있으면 encoding 안함.
  if (!m_pCodec || !m_pInputWindow || !m_pTimeline) return false;

  // FPS, 한 프레임 길이, 전체 길이, 총 프레임 수 계산
  const int fps = std::max(1, m_encoderConfig.fps);                     // 최소 1fps 보장
  const double frameDur = 1.0 / (double)fps;                                 // 한 프레임이 차지하는 시간(초)
  const double dur = std::max(0.0, m_durationSec);                      // 전체 길이(음수 방지)
  const int totalFrames = std::max(1, (int)std::ceil(dur * fps));    // 총 프레임 수(올림)

  // 0번 프레임부터 마지막 프레임까지 루프를 돌며 encoding 및 packet 을 muxer 에 기록
  for (int i = 0; i < totalFrames; i++)
  {
    // 외부에서 atomic 플래그를 통해 encoding 취소 요청하면 중단
    if (cancelFlag.load()) {
      break;
    }

    const double t = std::min(dur, i * frameDur);                   // 현재 프레임의 시간값(초)
    const int64_t ptsNs = (int64_t)std::llround(t * 1'000'000'000.0); // 현재 프레임의 시간값을 나노초 단위로 변환
    setPresentationTimeNs(ptsNs);                                        // 현재 프레임을 "언제 보여줄지" 시간 스티커를 offscreen 전용 native surface 에 바인딩된 EGLSurface 에 붙임

    // 현재 프레임 시간(t)에 해당하는 그림을 바인딩된 EGLSurface 에 그린다.
    if (!renderOneFrame(t)) {
      LOGE("renderOneFrame failed");
      return false;
    }

    // 현재 프레임이 그려진 결과물을 Codec 이 packet 으로 압축하면 그걸 꺼내서 mp4 컨테이너에 쓴다
    if (!drainEncoderAndMux(true)) {
      LOGE("drain running failed");
      return false;
    }

    // 진행률 콜백 호출([0.0, 1.0] 사이)
    if (onProgress) {
      onProgress(double(i + 1) / double(totalFrames));
    }
  }

  // 프레임 루프를 탈출했으니 더 이상 인코딩할 프레임이 없음을 Codec 에 알리기 (EOS)
  media_status_t ms = AMediaCodec_signalEndOfInputStream(m_pCodec);
  if (ms != AMEDIA_OK) {
    LOGE("AMediaCodec_signalEndOfInputStream failed: %d", ms);
    return false;
  }

  // 아직 꺼내지 못한 압축 packet 이 남아있으면 모두 꺼내서 mp4 컨테이너에 쓴다.
  if (!drainEncoderAndMux(true)) {
    LOGE("drain final failed");
    return false;
  }

  return true;
};

void AndroidEncoder::release() {
  /** 인코딩에 사용한 모든 자원을 안전하게 해제한다(생성 역순으로). */

  // encoder 전용 skia 해제
  destroySkia();

  // encoder 전용 EGL 컨텍스트 해제
  destroyEGL();

  if (m_pInputWindow) {
    // offscreen 전용 AMediaCodec native surface 해제
    ANativeWindow_release(m_pInputWindow);
    m_pInputWindow = nullptr;
  }

  if (m_pCodec) {
    // AMediaCodec 정지 후 해제
    AMediaCodec_stop(m_pCodec);
    AMediaCodec_delete(m_pCodec);
    m_pCodec = nullptr;
  }

  // Muxer 닫기
  closeMuxer();

  // encoding 에 사용된 멤버변수들 초기화
  m_trackIndex = -1;
  m_muxerStarted = false;
  m_outputFd = -1;
  m_durationSec = 0.0;
};

std::string AndroidEncoder::outputPath() const {
  return m_encoderConfig.outputPath;
};

bool AndroidEncoder::createCodecAndSurface() {
  // Codec 이 각 프레임을 어떤 방식으로 압축할 지 codec format 설정
  AMediaFormat* fmt = AMediaFormat_new();
  AMediaFormat_setString(fmt, AMEDIAFORMAT_KEY_MIME, m_encoderConfig.mime.c_str());                   // 어떤 코덱 알고리즘으로 압축할 지?(ex> H.264, HEVC 등...)
  AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_WIDTH, m_encoderConfig.width);                          // 출력 영상의 가로 해상도
  AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT, m_encoderConfig.height);                        // 출력 영상의 세로 해상도
  AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_BIT_RATE, m_encoderConfig.bitrate);                     // 비트레이트(bps)
  AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_FRAME_RATE, m_encoderConfig.fps);                       // 프레임레이트(fps)
  AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, m_encoderConfig.iFrameIntervalSec);   // I-프레임 간격(초)
  AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_COLOR_FORMAT, COLOR_FormatSurface);                     // 입력을 Surface 로 받겠다는 설정

  // 특정 코덱 알고리즘에 해당하는 Codec 생성
  m_pCodec = AMediaCodec_createEncoderByType(m_encoderConfig.mime.c_str());
  if (!m_pCodec) {
    // Codec 생성 실패 시, codec format 메모리 해제 후 실패 리턴
    LOGE("createCodecAndSurface failed: %s", m_encoderConfig.mime.c_str());
    AMediaFormat_delete(fmt);
    return false;
  }

  // 생성한 Codec 을 "인코딩 모드(encoder)"로 설정한다.
  // (참고로 AMediaCodec 에서 "Codec" 이란 encoder/decoder 모드를 모두 포괄하는 추상화된 구현체이므로, 목적에 따라 Codec 모드를 설정해서 사용)
  media_status_t ms = AMediaCodec_configure(m_pCodec, fmt, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
  if (ms != AMEDIA_OK) {
    LOGE("AMediaCodec_configure failed: %d", ms);
    return false;
  }

  // 생성한 Codec(encoder) 에 공급할 '입력 프레임'을 그릴 offscreen 전용 native surface 를 AMediaCodec API 를 통해 생성한다.
  /**
   * 이 native surface는 AMediaCodec API 로 생성한 인코딩 전용 surface 이므로 화면에 보이지 않으며,
   * 우리가 EGL/Skia 를 여기에 바인딩하여 그림을 그리면, 해당 프레임이 encoder 입력으로 들어간다.
   */
  ms = AMediaCodec_createInputSurface(m_pCodec, &m_pInputWindow);
  if (ms != AMEDIA_OK || !m_pInputWindow) {
    LOGE("AMediaCodec_createInputSurface failed: %d", ms);
    return false;
  }

  return true;
};

// 코덱을 시작하기 위해 호출하는 함수
bool AndroidEncoder::startCodec() {
  // AMediaCodec_start()를 호출해야 그때부터 offscreen 전용 native surface에 그린 프레임이 실제로 인코더로 흘러간다.
  // 즉, 이 API 가 호출 및 성공해야 AMediaCodec 내부 BufferQueue 를 통해 연결된 surface -> 인코더 입력 파이프라인이 가동되기 시작함.
  media_status_t ms = AMediaCodec_start(m_pCodec);
  if (ms != AMEDIA_OK) {
    // 시작 실패에 대한 예외 처리
    LOGE("AMediaCodec_start failed: %d", ms);
    return false;
  }
  return true;
};

bool AndroidEncoder::initEGL() {
  // AMediaCodec API 가 생성한 입력용 offscreen native surface 를 EGL 에서 사용할 수 있도록 초기화한다.
  if (!m_egl.init(m_pInputWindow)) {
    LOGE("EglContext::init failed");
    return false;
  }

  // EGL 초기화 완료 후, PTS 시간 스티커를 프레임마다 붙이는 데 사용되는 EGL 확장 함수(eglPresentationTimeANDROID) 포인터를 로드한다.
  if (!s_eglPresentationTimeANDROID) {
    s_eglPresentationTimeANDROID = (PFNEGLPRESENTATIONTIMEANDROIDPROC)eglGetProcAddress("eglPresentationTimeANDROID");
  }

  return true;
};

bool AndroidEncoder::initSkia() {
  // encoder 전용 스레드(Engine::m_encodeThread)에 바인딩된 EGLContext 를 사용하는 ganesh gpu 백엔드 기반 skia surface 생성
  /**
   * encoder 전용 스레드에 바인딩된 EGLContext를 현재로 만들고(eglMakeCurrent),
   * AMediaCodec이 제공한 ANativeWindow(offscreen native surface)로 생성한 EGLSurface(윈도우 표면)의
   * default framebuffer(FBO 0)를 Skia Ganesh(GL) 백엔드로 래핑해 SkSurface를 만든다.
   *
   * 결과적으로, 이 SkSurface에 그리는 모든 내용은 해당 EGLSurface에 기록되고,
   * flush 후 eglSwapBuffers()를 호출하면 프레임이 BufferQueue를 통해
   * MediaCodec 인코더 입력으로 제출된다.
   */
  if (!m_skia.setupSkiaSurface(m_encoderConfig.width, m_encoderConfig.height)) {
    LOGE("SkiaGanesh::setupSkiaSurface failed");
    return false;
  }
  return true;
};

void AndroidEncoder::destroyEGL() {
  // encoder 전용 EGL 자원 해제
  m_egl.destroy();
};

void AndroidEncoder::destroySkia() {
  // encoder 전용 skia 자원 해제
  m_skia.destroy();
};

// TODO : AndroidEncdoer.cpp 구현 완료 후 ./tools/gen_compile_db.sh 스크립트 다시 돌려서 현재 소스를 compile db 대상에 포함시키기
bool AndroidEncoder::renderOneFrame(double tSec) {
  /**
   * 타임라인 기반 렌더링 수행
   *
   * Preview 렌더링과 동일하게 Renderer::process() 내부에서
   * Timeline::render() 함수를 호출하여 렌더링한다!
   *
   * 단, encoder 의 렌더링 함수는 Codec 에 입력하여 encoding 할 버퍼를 렌더링하는 목적이므로,
   * 실시간 루프 타이밍 제약이 없다!
   */

  // 현재 인코딩 스레드에 생성된 EGLContext 바인딩된 상태에서만 렌더링
  if (!m_egl.makeCurrent()) {
    LOGE("EglContext::makeCurrent failed");
    return false;
  }

  // SkCanvas 포인터를 얻어와서 렌더링
  SkCanvas* canvas = m_skia.canvas();
  if (!canvas) {
    LOGE("SkiaGanesh::canvas is null");
    return false;
  }

  // AndroidEncoder::encodeBlocking 함수 내 인코딩 루프에서 계산된 현재 프레임 시간값(초)를 기준으로 이미지 시퀀스 렌더링
  RenderContext ctx{ canvas, m_encoderConfig.width, m_encoderConfig.height, tSec };
  m_pTimeline->render(ctx);

  // Skia 내부 command queue 에 쌓인 현재 프레임까지 요청된 모든 draw operation 들을 GPU 로 전송하여 실행 요청
  m_skia.flush();

  /**
   * 디스플레이 출력용 native surface 의 경우,
   * vsync 시점에 맞춰 back buffer 와 front buffer 를 교체하면 출력되는 화면이 업데이트 됬었다.
   *
   * 이와 마찬가지로, encoding 용 offscreen native surface 에 바인딩된 EGLContext 에서
   * scanline 으로 렌더링이 완료된 back buffer 는 Codec 에 입력 buffer queue 에 전달되어 인코딩 대기 상태가 된다.
   */
  if (!m_egl.swapBuffer()) {
    LOGE("EglContext::swapBuffer failed");
    return false;
  }
  return true;
};

bool AndroidEncoder::drainEncoderAndMux(bool endOfStream) {
  return true;
};

bool AndroidEncoder::openMuxer() {
  return true;
};

void AndroidEncoder::closeMuxer() {

};

void AndroidEncoder::setPresentationTimeNs(int64_t ptsNs) {

};
