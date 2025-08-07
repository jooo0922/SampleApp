#pragma once

#include <AppSpecsJSI.h>
#include <android/native_window.h> // ANativeWindow
#include <memory>
#include <string>

namespace facebook::react {

class NativeSampleModule
    : public NativeSampleModuleCxxSpec<NativeSampleModule> {
public:
  NativeSampleModule(std::shared_ptr<CallInvoker> jsInvoker);

  std::string reverseString(jsi::Runtime &rt, std::string input);

  // android surface 초기화
  void initSurface(ANativeWindow *window);
  // android surface 크기 변경
  void changeSurface(int width, int height);
  // android surface 제거
  void destroySurface();

protected:
  // egl 초기화
  bool initEGL();
  // egl 리소스 해제
  void destroyEGL();

protected:
  // ganesh gpu 백엔드 기반 SkSurface 생성 함수
  bool setupSkiaSurface();

protected:
  // skia 렌더링 루프
  void renderLoop();
};

} // namespace facebook::react
