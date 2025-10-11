#include "SkiaGanesh.h"
#include "../logger/Logger.h"

/**
 * GrDirectContext 및 SkSurface 생성 함수.
 * -> imageInfo (즉, render target 크기)가 유효할 때 한번 생성하거나, 크기가 변경되었으면 재생성함.
 */
bool SkiaGanesh::setupSkiaSurface(int width, int height) {
  if (width <= 0 || height <= 0) {
    Logger::error(k_logTag, "Invalid image size");
    return false;
  }

  // GrDirectContext 생성 (내부적으로 현재 스레드에 바인딩된 egl context 사용)
  if (!m_pGrContext) {
    auto interface = GrGLMakeNativeInterface();
    m_pGrContext = GrDirectContexts::MakeGL(interface);
    if (!m_pGrContext) {
      Logger::error(k_logTag, "Failed to create GrDirectContext");
      return false;
    }
  }

  // 기존 SkSurface 크기가 현재 이미지 크기(메타데이터)와 맞지 않을 경우 재생성하도록 nullptr 초기화
  if (m_pSkiaSurface) {
    SkImageInfo currentInfo = m_pSkiaSurface->imageInfo();
    if (currentInfo.width() != width || currentInfo.height() != height) {
      m_pSkiaSurface = nullptr;
    }
  }

  // SkSurface (재)생성
  if (!m_pSkiaSurface) {
    // 1. OpenGL ES 컨텍스트에 바인딩된 default framebuffer(fbo id == 0) 설정을 GrGLFramebufferInfo 로 구성.
    GrGLFramebufferInfo fboInfo;
    fboInfo.fFBOID = (GrGLuint)0;
    fboInfo.fFormat = GL_RGBA8;

    // 2. OpenGL ES 컨텍스트에 바인딩된 default framebuffer 를 감싸는 skia 의 render target (interface) 생성.
    GrBackendRenderTarget backendRT = GrBackendRenderTargets::MakeGL(
      width,         // render target width
      height,        // render target height
      0,             // sampleCount (멀티샘플링 안 함)
      0,             // 스텐실 비트 수 (없다면 0)
      fboInfo        // OpenGL 기본 FBO 정보
    );

    // 3. WrapBackendRenderTarget 을 사용하여 default framebuffer 기반 SkSurface 를 생성.
    m_pSkiaSurface = SkSurfaces::WrapBackendRenderTarget(
      m_pGrContext.get(),                         // 이미 생성된 GrDirectContext
      backendRT,
      kBottomLeft_GrSurfaceOrigin,              // OpenGL은 좌하단 기준
      SkColorType::kRGBA_8888_SkColorType,      // SkImageInfo의 color type (예, kN32_SkColorType)
      nullptr,                                  // 컬러 공간 (필요에 따라 지정)
      nullptr                                   // SkSurfaceProps (필요에 따라)
    );

    if (!m_pSkiaSurface) {
      Logger::error(k_logTag, "Failed to create SkSurface from backend render target");
      return false;
    }
  }

  return true;
};

void SkiaGanesh::flush() {
  if (m_pGrContext) {
    // Skia 내부 command queue 에 쌓인 현재 프레임까지 요청된 모든 draw operation 들을 GPU 로 전송하여 실행 요청
    m_pGrContext->flush();
  }
};

void SkiaGanesh::destroy() {
  // ganesh gpu 백엔드 관련 전역 객체 자원 해제
  m_pGrContext = nullptr;
  m_pSkiaSurface = nullptr;
};
