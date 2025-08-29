#pragma once

#ifdef __ANDROID__

#include "IVideoPlayer.h"
#include <jni.h>
#include <atomic>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <gpu/ganesh/GrDirectContext.h>

/**
 * VideoPlayerAndroid 클래스
 *
 * - Android MediaCodec + SurfaceTexture 파이프라인을 처리하는 플레이어 구현체
 * - MediaCodec이 쓰는 외부 OES 텍스처(GL_TEXTURE_EXTERNAL_OES)를 FBO의 RGBA 텍스처로 그려서
 *   Skia가 사용할 수 있는 SkImage로 래핑, 이후 Drawable이 그 SkImage를 캔버스에 그리게 함.
 */
class VideoPlayerAndroid : public IVideoPlayer
{
public:
  /**
   * 생성자 매개변수 설명
   *
   * - gr: SkiaGanesh 클래스의 GrDirectContext (SkImage로 텍스처를 래핑할 때 사용)
   * - env: JNIEnv (SurfaceTexture를 GlobalRef로 승격하는 데 사용)
   * - surfaceTextureGlobal: Android의 android.graphics.SurfaceTexture 객체 (이미 생성되어 전달됨)
   * - oesTex: MediaCodec 출력을 수신할 GL External OES 텍스처 핸들
   */
  VideoPlayerAndroid(GrDirectContext* gr, JNIEnv* env, jobject surfaceTextureGlobal, GLuint oesTex);

  // 자원 정리: SurfaceTexture GlobalRef 해제, GL 리소스(FBO/텍스처/프로그램) 파괴
  ~VideoPlayerAndroid() override;

public:
  // 단순 미디어 파일 경로 보관(옵션) – 실제 디코딩은 Kotlin/Java 쪽에서 수행된다고 가정
  bool open(const std::string& path) override;

  // 렌더링 가능 상태로 전환 (VideoPlayerAndroid::update() 내에서 프레임을 처리하도록 플래그 설정)
  bool start() override;

  // 비디오 렌더링 중단 및 현재 SkImage 해제(다음 프레임부터 그리지 않음)
  void stop() override;

  // android.graphics.SurfaceTexture.onFrameAvailable() 콜백 호출 시 video frame 갱신을 VideoPlayer 클래스에 notify
  // -> 다음 VideoPlayerAndroid::update() 루프에서 새 프레임이 있음을 알림(락/스레드 안전을 위해 atomic flag 사용)
  void notifyFrame() override { m_frameReady = true; }

  /**
   * 렌더링 스레드에서 VideoRect::update() 함수 내에서 호출할 함수.
   * - android.graphics.SurfaceTexture.updateTexImage() & 변환행렬 취득
   * - 신규 video frame 으로 갱신된 OES 텍스처를 RGBA 로 변환하여 offscreen 렌더 타겟으로 blit
   * - RGBA 로 변환된 프레임이 기록된 GL 텍스처를 SkImage로 래핑
   */
  void update() override;

  // 현재 프레임(SkImage) 반환 – VideoRect::draw() 단계에서 decoding 된 비디오 프레임 렌더링을 위해 사용
  sk_sp<SkImage> currentFrame() const override { return m_image; };

  // 비디오 소스의 크기 getter (초기에는 타깃 크기를 사용, 실제 해상도 전달되면 갱신)
  int width() const override { return m_w; };
  int height() const override { return m_h; };

private:
  // 매개변수로 전달된 크기에 맞는 RGBA 로 변환된 프레임을 담을 offscreen 렌더 타깃(FBO/텍스처) 생성
  bool createRenderTarget(int w, int h);

  // 원시 프레임(YUV) -> RGBA 로 변환 시 사용할 OES 샘플링 셰이더 프로그램 생성
  bool createShaderProgram();

  // 원시 프레임(YUV)이 갱신된 OES 텍스처를 RGBA 로 변환된 프레임을 담는 offscreen 렌더 타깃에 렌더링(복사)
  void blitOesToRgba();

  // RGBA 로 변환된 프레임이 기록된 GL 텍스처를 SkImage로 래핑 (Skia가 그릴 수 있도록 변환)
  void wrapSkImage();

  // 생성한 모든 GL 리소스 파괴(FBO/텍스처/프로그램)
  void destroyGL();

  // 현재 스레드에서 자바 가상머신에 접근할 수 있는 JNIEnv 얻기 JNIEnv 얻기 (없으면 AttachCurrentThread)
  JNIEnv* getEnv();

private:
  // SkiaGanesh 클래스의 GrDirectContext (텍스처 → SkImage 래핑 시 필요)
  GrDirectContext* m_gr = nullptr;

  // Java의 SurfaceTexture 전역 참조(GlobalRef). JNI 스레드 경계에서도 유효하게 유지.
  jobject m_surfaceTexture = nullptr;

  // MediaCodec 으로 decoding 한 원시프레임(YUV 포맷)이 출력되는 SurfaceTexture 에 바인딩되는 외부 OES 텍스처 핸들
  GLuint m_oesTex = 0;

  // GL 렌더 타깃: 원시 프레임(YUV) -> RGBA 로 변환된 프레임을 담는 offscreen 텍스처/프레임버퍼
  GLuint m_fbo = 0;
  GLuint m_colorTex = 0;

  // 원시 프레임(YUV) -> RGBA 로 변환 시 사용할 OES 샘플링 셰이더 프로그램과 attribute & uniform 변수들의 location
  GLuint m_prog = 0;
  GLint m_locPos = -1;
  GLint m_locUv = -1;
  GLint m_locMat = -1;

  // 비디오 프레임의 논리적 크기 (실제 포맷 수신 전까지는 타깃 크기로 대체)
  int m_w = 0;
  int m_h = 0;

  // 현재 할당된 렌더 타깃의 실제 크기 (변경 시 재할당)
  int m_allocW = 0;
  int m_allocH = 0;

  // android.graphics.SurfaceTexture.getTransformMatrix() 호출 시 제공되는 비디오 텍스처 좌표 변환 행렬 (회전/크롭 등 반영)
  float m_texMatrix[16]{};

  // android.graphics.SurfaceTexture 에 새로운 video frame 이 enqueue 되었음을 notify 하는 atomic flag (Java 콜백에서 설정)
  // (VideoPlayerAndroid::update() 루프에서 이 플래그를 보고 android.graphics.SurfaceTexture.updateTexImage() 를 호출하여 OES 텍스처에 최신 프레임으로 갱신)
  std::atomic<bool> m_frameReady{false};

  // start()/stop() 상태 관리
  bool m_running = false;

  // 선택적: open()으로 받은 미디어 파일 경로 저장
  std::string m_path;

  // 최신 RGBA 텍스처를 SkImage 로 래핑한 객체 (VideoRect::draw() 함수에서 decoding 된 새로운 video frame 렌더링 시 사용)
  sk_sp<SkImage> m_image;
};

#endif