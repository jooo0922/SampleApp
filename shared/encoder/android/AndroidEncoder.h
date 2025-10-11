#pragma once

#include <android/native_window.h>         // Surface를 NDK에서 쓰기 위한 타입
#include <media/NdkMediaCodec.h>           // AMediaCodec (코덱)
#include <media/NdkMediaFormat.h>          // AMediaFormat (포맷)
#include <media/NdkMediaMuxer.h>           // AMediaMuxer (mp4 저장)
#include "../IEncoder.h"                   // 공용 인터페이스
#include "../EncoderConfig.h"              // 공용 config
#include "../../render/EglContext.h"       // Encoder 전용 EGL 컨텍스트
#include "../../render/SkiaGanesh.h"       // Encoder 전용 Skia wrapper
#include "../../video/Timeline.h"          // 인코딩할 타임라인

class AndroidEncoder : public IEncoder {
public:
    AndroidEncoder();
    ~AndroidEncoder() override;

public:
  // 인코딩에 사용할 타임라인 설정(프리뷰와 동일한 렌더 경로 재사용)
  void setTimeline(std::shared_ptr<Timeline> tl) override;
  // 인코더 준비(Codec/Surface/EGL/Skia 준비)
  bool prepare(const EncoderConfig& cfg) override;
  // 실제 인코딩(모든 프레임 처리). 호출한 스레드는 이 함수가 끝날 때까지 기다린다.
  bool encodeBlocking(std::atomic<bool>& cancelFlag, std::function<void(double)> onProgress) override;
  // 내부 자원 정리(Codec/Surface/EGL/Skia/Muxer 등)
  void release() override;
  // 최종 출력 파일 경로 반환
  std::string outputPath() const override;

private:
  // 1) Codec/native 입력 Surface(ANativeWindow -> offscreen 전용 native surface) 준비
  bool createCodecAndSurface();
  bool startCodec();

  // 2) EGL/Skia 준비 및 해제 (AMediaCodec native surface에 바인딩해서 GL/Skia로 그림을 그리기 위함)
  bool initEGL();         // EglContext로 AMediaCodec native surface 와 EGLSurface 연결
  bool initSkia();        // SkiaGanesh 로 encoding 결과물을 그려낼 캔버스 생성
  void destroyEGL();
  void destroySkia();

  // 3) 주어진 시간값(tSec)에 맞는 타임라인 프레임을 SkCanvas 에 렌더링
  bool renderOneFrame(double tSec);

  // 4) Codec 에서 인코딩된 출력 패킷을 뽑아서 Muxer 를 통해 .mp4 컨테이너에 기록
  bool drainEncoderAndMux(bool endOfStream);

  // 5) Muxer 열고 닫기
  bool openMuxer();
  void closeMuxer();

  // 6) 프레임 표시 시간(PTS) 지정: "이 프레임을 언제 보여줄지"를 ns 단위로 각 프레임마다 붙여주는 시간 스티커
  /**
   * PTS (Presentation Time Stamp) 계산 함수
   *
   * - 왜 필요한가?
   *   - 동영상 재생 시 “언제 보여줄지”가 있어야 속도가 일정해지고(30fps면 매 33.3ms), 프레임 순서가 꼬이지 않으며, 오디오와도 맞출 수 있다.
   *
   * - 어떻게 쓰이나
   *   - 한 프레임을 그린 뒤, 그 프레임에 시간 스티커(pts)를 붙인다.
   *   - 여기서는 eglPresentationTimeANDROID로 나노초 단위 시간을 붙인다.
   *   - 그 다음 swapBuffers로 코덱에 보낸다.
   *   - 코덱은 이 시간을 기반으로 출력 패킷의 presentationTimeUs를 채운다.
   *   - Muxer가 이 시간을 .mp4에 기록하고, 플레이어는 그 시간에 맞춰 프레임을 보여준다.
   *   - 영상 재생 시 오디오/비디오 동기화에 쓰인다
   *     - video decoding 시 마스터 타임인 오디오를 기준으로
   *       현재 비디오 프레임에 붙여진 PTS 가 오디오보다 빠르면 그 시간까지 대기,
   *       오디오보다 느리면 해당 비디오 프레임을 드롭한 뒤 따라잡는 원리.
   *
   * - 간단 예
   *   - fps = 30이면 프레임 i의 시간 = i / 30 초
   *   - i=0 → 0.0s, i=1 → 0.033s, i=2 → 0.066s …
   *   - setPresentationTimeNs(tSec * 1e9)로 붙이고, swapBuffers 호출.
   *
   * - 지켜야 할 규칙
   *   - 단위: 나노초(ns)
   *   - 호출 시점: 매 프레임 swapBuffers 하기 “직전”
   *   - 값: 0부터 시작해 프레임마다 “증가(또는 같게)”해야 함(되돌아가면 안 됨)
   *     (정확히 맞출 필요는 없고, 일정하게 증가하기만 하면 됨(30fps면 33.3ms 간격으로 증가))
   */
  void setPresentationTimeNs(int64_t ptsNs);

private:
  std::shared_ptr<Timeline> m_pTimeline;        // 인코딩에 사용할 타임라인(프리뷰와 동일한 그림을 그리기 위함)

  EncoderConfig m_encoderConfig;                // 인코딩 설정(해상도/FPS/비트레이트/코덱/출력 경로)

  AMediaCodec* m_pCodec = nullptr;              // MediaCodec(비디오 인코더)
  ANativeWindow* m_pInputWindow = nullptr;      // MediaCodec의 입력 Surface(NDK 윈도우)

  // EGL + Skia (Renderer 에서 쓰는 것과 공유하지 못하도록 Encoder 전용으로 사용)
  EglContext m_egl;                             // encoder 전용 EGL 컨텍스트(native 입력 Surface에 바인딩해서 GL/Skia로 그림을 그리기 위함)
  SkiaGanesh m_skia;                            // encoder 전용 Skia wrapper

  AMediaMuxer* m_pMuxer = nullptr;              // Muxer(.mp4 컨테이너에 인코딩 결과 패키징)
  int m_trackIndex = -1;                        // 트랙 인덱스 (포맷 확정 후 설정)
  bool m_muxerStarted = false;                  // Muxer 시작 여부
  int m_outputFd = -1;                          // .mp4 출력 파일 디스크립터

  double m_durationSec = 0.0;                   // 타임라인 총 길이(초) 캐시(프레임 수 계산용)

private:
  static constexpr const char* k_logTag = "AndroidEncoder";
};
