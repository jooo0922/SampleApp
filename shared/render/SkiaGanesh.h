#pragma once
#include <core/SkSurface.h>
#include <core/SkColorSpace.h>
#include <gpu/ganesh/SkSurfaceGanesh.h>
#include <gpu/ganesh/GrDirectContext.h>
#include <gpu/ganesh/GrBackendSurface.h>
#include <gpu/ganesh/gl/GrGLBackendSurface.h>
#include <gpu/ganesh/gl/GrGLDirectContext.h>
#include <gpu/ganesh/gl/GrGLInterface.h>
#include <GLES3/gl3.h> // OpenGL ES 3.0 API 사용

class SkiaGanesh
{
public:
  // ganesh gpu 백엔드 기반 SkSurface 생성 함수
  bool setupSkiaSurface(int width, int height);
  void flush();
  void destroy();

public:
  SkCanvas* canvas() const { return m_pSkiaSurface ? m_pSkiaSurface->getCanvas() : nullptr; };
  sk_sp<SkSurface> surface() const { return m_pSkiaSurface; };

private:
  // ganesh gpu 백엔드 관련 전역 객체 (skia 버전 스마트 포인터(std::shared_ptr 과 유사)로 관리)
  sk_sp<GrDirectContext> m_pGrContext;
  sk_sp<SkSurface> m_pSkiaSurface;

private:
  static constexpr const char* k_logTag = "SkiaGanesh";
};
