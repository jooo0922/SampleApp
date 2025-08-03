package com.sampleapp

import android.view.View
import com.facebook.react.uimanager.SimpleViewManager
import com.facebook.react.uimanager.ThemedReactContext
import com.sampleapp.SkiaView

class SkiaViewManager : SimpleViewManager<View>() {
    override fun getName(): String = "SkiaView" // React Native에서 사용할 이름

    override fun createViewInstance(reactContext: ThemedReactContext): View {
        return SkiaView(reactContext) // SkiaView 생성
    }
}
