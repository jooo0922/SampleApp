package com.sampleapp

import com.facebook.react.ReactPackage
import com.facebook.react.bridge.NativeModule
import com.facebook.react.uimanager.ViewManager
import com.sampleapp.SkiaViewManager

class SkiaViewPackage : ReactPackage {
    override fun createNativeModules(reactContext: com.facebook.react.bridge.ReactApplicationContext): List<NativeModule> {
        return emptyList() // 네이티브 모듈이 없으므로 빈 리스트 반환
    }

    override fun createViewManagers(reactContext: com.facebook.react.bridge.ReactApplicationContext): List<ViewManager<*, *>> {
        return listOf(SkiaViewManager()) // SkiaViewManager 등록
    }
}
