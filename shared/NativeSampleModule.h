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
  void changeSurface(ANativeWindow *window, int width, int height);
  // android surface 제거
  void destroySurface();

protected:
  // skia 렌더링 루프
  void renderLoop();
};

} // namespace facebook::react
