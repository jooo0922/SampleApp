package com.sampleapp

import android.content.Context
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView

class SkiaView(context: Context) : SurfaceView(context), SurfaceHolder.Callback {
    init {
        holder.addCallback(this) // Surface 상태 변경 콜백 등록
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        // Surface가 생성되었을 때 C++로 전달
        nativeInitSurface(holder.surface)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        // Surface 크기 변경 처리 (필요 시)
        nativeChangeSurface(holder.surface, width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        // Surface가 파괴되었을 때 C++로 알림
        nativeDestroySurface()
    }

    // JNI를 통해 C++ 함수 호출
    private external fun nativeInitSurface(surface: Surface)
    private external fun nativeChangeSurface(surface: Surface, width: Int, height: Int)
    private external fun nativeDestroySurface()
}
