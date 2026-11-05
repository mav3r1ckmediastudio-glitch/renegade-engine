# Native window surface recovery - November 2, 2026

The e12bc01 candidate passed all four CI jobs and all 305 local Release tests, then failed actual Studio use after a minimize/restore transition. Passing automation did not close the package gate.

## Evidence and cause

The original RenegadeStudio.exe (PID33536, Windows Application Verifier settings unchanged) reached Project Hub and later raised an access violation at RVA0xB3748B. The dump shows a null allocation dereference inside GraphicsDevice_DX12::CreateTexture, called by RenderPath3D::ResizeBuffers and StudioRenderPath::ResizeBuffers. Its D3D12 resource descriptor had width0 and height0, and the preceding allocation returned E_INVALIDARG. This is a native window/graphics lifecycle fault; drive placement or CPU degradation is not established as its cause.

A diagnostic relink reused the original 68 Studio objects and existing libraries. It did not change the original package. Although executable text hashes differ, all 37,804 runtime function address ranges match, and the exact fault instruction bytes match. That correspondence was checked before attributing the stack. The dump, WER report, debugger output and address-map receipt are preserved with PR189's linked recovery evidence.

## Repair

NativeWindowSurface.h provides TryBindDrawableNativeWindow. It refuses invalid, minimized and nonpositive client areas before calling the graphics application's SetWindow. The last valid canvas and buffers remain intact. A subsequent valid window event binds the restored extent normally.

Every native binding in Studio's Windows entry point (8 sites) and Runtime's Windows entry point (3 sites), including size, DPI, focus and startup handling, uses this guard. Existing inactive-frame and message-pump behavior is retained. The pinned Wicked source/commit, renderer assertions, graphics quality and resource budgets are unchanged.

## Targeted verification before commit

- Serial Release Studio and Runtime build: PASS.
- RenegadeNativeWindowSurfaceTests: PASS. A real Win32 window exercises null/destroyed handles, zero width, zero height, both zero, preservation of previous dimensions, restored dimensions and native minimization/restoration.
- All 59 registered SourceContract tests: PASS.
- Actual Studio original basename, with existing verifier settings retained: five minimize/restore cycles passed, each minimized client measuring0x0; restored client1920x1009.
- Actual overlapped Studio window also survived client areas624x0 and120x0, then restored its normal rendered Project Hub. The requested zero-width-only case was constrained by Windows to120x441; the hidden popup-window regression covers genuinely zero width.
- The restored UI screenshot visibly shows Project Hub and the saved developer identity. This is a precommit verification of the window repair, not final alpha acceptance.

Commands:

    cmake --build <build> --config Release --target RenegadeStudio RenegadeRuntime RenegadeNativeWindowSurfaceTests --parallel 1
    ctest --test-dir <build> -C Release -R ^RenegadeNativeWindowSurfaceTests$ --output-on-failure
    ctest --test-dir <build> -C Release -R SourceContract --output-on-failure

The final head still needs its full 306-test local suite, all four CI jobs, packaged Studio workflows, final archive verification and fresh normal launch. Independent exact-commit review remains required before a release gate is closed.
