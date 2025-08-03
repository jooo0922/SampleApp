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

  // Surface 초기화 및 파괴 함수
  void initSurface(ANativeWindow *window);
  void destroySurface();

protected:
  void renderLoop();
};

} // namespace facebook::react
