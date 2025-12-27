// ==WindhawkMod==
// @id              taskbar-thumbnail-size-absolute
// @name            Taskbar Thumbnail Size - Absolute Values
// @description     Set minimum and maximum absolute sizes for taskbar thumbnails in Windows 11
// @version         1.1
// @author          m417z (modified)
// @github          https://github.com/m417z
// @twitter         https://twitter.com/m417z
// @homepage        https://m417z.com/
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lole32 -loleaut32 -lruntimeobject
// ==/WindhawkMod==

// Source code is published under The GNU General Public License v3.0.
//
// For bug reports and feature requests, please open an issue here:
// https://github.com/ramensoftware/windhawk-mods/issues
//
// For pull requests, development takes place here:
// https://github.com/m417z/my-windhawk-mods

// ==WindhawkModReadme==
/*
# Taskbar Thumbnail Size - Absolute Values

Set minimum and maximum absolute sizes (in pixels) for taskbar thumbnails in Windows 11.

This modified version allows you to specify exact pixel dimensions for the thumbnail width,
rather than using percentage-based scaling. You can set both minimum and maximum widths,
and the aspect ratio will be preserved.

For older Windows versions, the size of taskbar thumbnails can be changed via the registry:

* [Change Size of Taskbar Thumbnail Previews in Windows
  11](https://www.elevenforum.com/t/change-size-of-taskbar-thumbnail-previews-in-windows-11.6340/)
  (before Windows 11 version 24H2)
* [How to Change the Size of Taskbar Thumbnails in Windows
  10](https://www.tenforums.com/tutorials/26105-change-size-taskbar-thumbnails-windows-10-a.html)

![Demonstration](https://i.imgur.com/nGxrBYG.png)
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- minWidth: 200
  $name: Minimum thumbnail width (pixels)
  $description: Minimum width for thumbnails in pixels. Set to 0 to disable minimum.
- maxWidth: 400
  $name: Maximum thumbnail width (pixels)
  $description: Maximum width for thumbnails in pixels. Set to 0 to disable maximum.
- minHeight: 150
  $name: Minimum thumbnail height (pixels)
  $description: Minimum height for thumbnails in pixels. Set to 0 to disable minimum.
- maxHeight: 300
  $name: Maximum thumbnail height (pixels)
  $description: Maximum height for thumbnails in pixels. Set to 0 to disable maximum.
- preserveAspectRatio: false
  $name: Preserve aspect ratio
  $description: >-
    When enabled, maintains the original aspect ratio of thumbnails.
    When disabled, applies width and height constraints independently,
    which may stretch or squash the thumbnail.
- useAbsoluteSize: true
  $name: Use absolute sizing
  $description: >-
    When enabled, uses min/max pixel values. When disabled, falls back to
    percentage-based scaling (legacy mode).
- percentageSize: 150
  $name: Percentage size (legacy mode)
  $description: >-
    Percentage of the original size. Only used when "Use absolute sizing" is
    disabled.
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>

#include <atomic>

#undef GetCurrentTime

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.h>

using namespace winrt::Windows::UI::Xaml;

struct {
    int minWidth;
    int maxWidth;
    int minHeight;
    int maxHeight;
    bool preserveAspectRatio;
    bool useAbsoluteSize;
    int percentageSize;
} g_settings;

std::atomic<bool> g_taskbarViewDllLoaded;

using ThumbnailHelpers_GetScaledThumbnailSize_t =
    winrt::Windows::Foundation::Size*(
        WINAPI*)(winrt::Windows::Foundation::Size* result,
                 winrt::Windows::Foundation::Size size,
                 float scale);
ThumbnailHelpers_GetScaledThumbnailSize_t
    ThumbnailHelpers_GetScaledThumbnailSize_Original;
winrt::Windows::Foundation::Size* WINAPI
ThumbnailHelpers_GetScaledThumbnailSize_Hook(
    winrt::Windows::Foundation::Size* result,
    winrt::Windows::Foundation::Size size,
    float scale) {
    Wh_Log(L"> Input: %fx%f scale=%f", size.Width, size.Height, scale);

    if (g_settings.useAbsoluteSize) {
        // Calculate the scaled size using original function first
        winrt::Windows::Foundation::Size* originalResult =
            ThumbnailHelpers_GetScaledThumbnailSize_Original(
                result, size, scale);

        float currentWidth = originalResult->Width;
        float currentHeight = originalResult->Height;
        float aspectRatio = currentHeight / currentWidth;

        Wh_Log(L"  Original result: %fx%f, aspect ratio: %f", 
               currentWidth, currentHeight, aspectRatio);

        float targetWidth = currentWidth;
        float targetHeight = currentHeight;

        if (g_settings.preserveAspectRatio) {
            // Apply width constraints first
            if (g_settings.minWidth > 0 && targetWidth < g_settings.minWidth) {
                targetWidth = static_cast<float>(g_settings.minWidth);
                Wh_Log(L"  Applying minimum width: %d", g_settings.minWidth);
            }
            
            if (g_settings.maxWidth > 0 && targetWidth > g_settings.maxWidth) {
                targetWidth = static_cast<float>(g_settings.maxWidth);
                Wh_Log(L"  Applying maximum width: %d", g_settings.maxWidth);
            }

            // Calculate height based on width (preserving aspect ratio)
            targetHeight = targetWidth * aspectRatio;

            // Check if height constraints would be violated
            bool heightAdjusted = false;
            if (g_settings.minHeight > 0 && targetHeight < g_settings.minHeight) {
                targetHeight = static_cast<float>(g_settings.minHeight);
                Wh_Log(L"  Applying minimum height: %d", g_settings.minHeight);
                heightAdjusted = true;
            }
            
            if (g_settings.maxHeight > 0 && targetHeight > g_settings.maxHeight) {
                targetHeight = static_cast<float>(g_settings.maxHeight);
                Wh_Log(L"  Applying maximum height: %d", g_settings.maxHeight);
                heightAdjusted = true;
            }

            // If height was adjusted, recalculate width to maintain aspect ratio
            if (heightAdjusted) {
                targetWidth = targetHeight / aspectRatio;
                Wh_Log(L"  Recalculated width from height: %f", targetWidth);
                
                // Recheck width constraints after recalculation
                if (g_settings.minWidth > 0 && targetWidth < g_settings.minWidth) {
                    targetWidth = static_cast<float>(g_settings.minWidth);
                    targetHeight = targetWidth * aspectRatio;
                    Wh_Log(L"  Re-applying minimum width: %d, height: %f", g_settings.minWidth, targetHeight);
                }
                
                if (g_settings.maxWidth > 0 && targetWidth > g_settings.maxWidth) {
                    targetWidth = static_cast<float>(g_settings.maxWidth);
                    targetHeight = targetWidth * aspectRatio;
                    Wh_Log(L"  Re-applying maximum width: %d, height: %f", g_settings.maxWidth, targetHeight);
                }
            }
        } else {
            // Apply width and height constraints independently (ignore aspect ratio)
            Wh_Log(L"  Ignoring aspect ratio");
            
            // Apply width constraints
            if (g_settings.minWidth > 0 && targetWidth < g_settings.minWidth) {
                targetWidth = static_cast<float>(g_settings.minWidth);
                Wh_Log(L"  Applying minimum width: %d", g_settings.minWidth);
            }
            
            if (g_settings.maxWidth > 0 && targetWidth > g_settings.maxWidth) {
                targetWidth = static_cast<float>(g_settings.maxWidth);
                Wh_Log(L"  Applying maximum width: %d", g_settings.maxWidth);
            }

            // Apply height constraints independently
            if (g_settings.minHeight > 0 && targetHeight < g_settings.minHeight) {
                targetHeight = static_cast<float>(g_settings.minHeight);
                Wh_Log(L"  Applying minimum height: %d", g_settings.minHeight);
            }
            
            if (g_settings.maxHeight > 0 && targetHeight > g_settings.maxHeight) {
                targetHeight = static_cast<float>(g_settings.maxHeight);
                Wh_Log(L"  Applying maximum height: %d", g_settings.maxHeight);
            }
        }

        result->Width = targetWidth;
        result->Height = targetHeight;

        Wh_Log(L"  Final result: %fx%f", result->Width, result->Height);
    } else {
        // Legacy percentage-based mode
        ThumbnailHelpers_GetScaledThumbnailSize_Original(
            result, size, scale * g_settings.percentageSize / 100.0);
        
        Wh_Log(L"  Percentage mode result: %fx%f", result->Width, result->Height);
    }

    return result;
}

using TaskItemThumbnailView_OnApplyTemplate_t = void(WINAPI*)(void* pThis);
TaskItemThumbnailView_OnApplyTemplate_t
    TaskItemThumbnailView_OnApplyTemplate_Original;
void WINAPI TaskItemThumbnailView_OnApplyTemplate_Hook(void* pThis) {
    Wh_Log(L">");

    TaskItemThumbnailView_OnApplyTemplate_Original(pThis);

    IUnknown* unknownPtr = *((IUnknown**)pThis + 1);
    if (!unknownPtr) {
        return;
    }

    FrameworkElement element = nullptr;
    unknownPtr->QueryInterface(winrt::guid_of<FrameworkElement>(),
                               winrt::put_abi(element));
    if (!element) {
        return;
    }

    try {
        Wh_Log(L"maxWidth=%f", element.MaxWidth());
        element.MaxWidth(std::numeric_limits<double>::infinity());
    } catch (...) {
        HRESULT hr = winrt::to_hresult();
        Wh_Log(L"Error %08X", hr);
    }
}

bool HookTaskbarViewDllSymbols(HMODULE module) {
    // Taskbar.View.dll
    WindhawkUtils::SYMBOL_HOOK symbolHooks[] = {
        {
            {LR"(struct winrt::Windows::Foundation::Size __cdecl winrt::Taskbar::implementation::ThumbnailHelpers::GetScaledThumbnailSize(struct winrt::Windows::Foundation::Size,float))"},
            &ThumbnailHelpers_GetScaledThumbnailSize_Original,
            ThumbnailHelpers_GetScaledThumbnailSize_Hook,
        },
        {
            {LR"(public: void __cdecl winrt::Taskbar::implementation::TaskItemThumbnailView::OnApplyTemplate(void))"},
            &TaskItemThumbnailView_OnApplyTemplate_Original,
            TaskItemThumbnailView_OnApplyTemplate_Hook,
        },
    };

    if (!HookSymbols(module, symbolHooks, ARRAYSIZE(symbolHooks))) {
        Wh_Log(L"HookSymbols failed");
        return false;
    }

    return true;
}

HMODULE GetTaskbarViewModuleHandle() {
    HMODULE module = GetModuleHandle(L"Taskbar.View.dll");
    if (!module) {
        module = GetModuleHandle(L"ExplorerExtensions.dll");
    }

    return module;
}

void HandleLoadedModuleIfTaskbarView(HMODULE module, LPCWSTR lpLibFileName) {
    if (!g_taskbarViewDllLoaded && GetTaskbarViewModuleHandle() == module &&
        !g_taskbarViewDllLoaded.exchange(true)) {
        Wh_Log(L"Loaded %s", lpLibFileName);

        if (HookTaskbarViewDllSymbols(module)) {
            Wh_ApplyHookOperations();
        }
    }
}

using LoadLibraryExW_t = decltype(&LoadLibraryExW);
LoadLibraryExW_t LoadLibraryExW_Original;
HMODULE WINAPI LoadLibraryExW_Hook(LPCWSTR lpLibFileName,
                                   HANDLE hFile,
                                   DWORD dwFlags) {
    HMODULE module = LoadLibraryExW_Original(lpLibFileName, hFile, dwFlags);
    if (module) {
        HandleLoadedModuleIfTaskbarView(module, lpLibFileName);
    }

    return module;
}

void LoadSettings() {
    g_settings.minWidth = Wh_GetIntSetting(L"minWidth");
    g_settings.maxWidth = Wh_GetIntSetting(L"maxWidth");
    g_settings.minHeight = Wh_GetIntSetting(L"minHeight");
    g_settings.maxHeight = Wh_GetIntSetting(L"maxHeight");
    g_settings.preserveAspectRatio = Wh_GetIntSetting(L"preserveAspectRatio");
    g_settings.useAbsoluteSize = Wh_GetIntSetting(L"useAbsoluteSize");
    g_settings.percentageSize = Wh_GetIntSetting(L"percentageSize");
    
    Wh_Log(L"Settings loaded: minWidth=%d, maxWidth=%d, minHeight=%d, maxHeight=%d, preserveAspect=%d, useAbsolute=%d, percentage=%d",
           g_settings.minWidth, g_settings.maxWidth, 
           g_settings.minHeight, g_settings.maxHeight,
           g_settings.preserveAspectRatio,
           g_settings.useAbsoluteSize, g_settings.percentageSize);
}

BOOL Wh_ModInit() {
    Wh_Log(L">");

    LoadSettings();

    if (HMODULE taskbarViewModule = GetTaskbarViewModuleHandle()) {
        g_taskbarViewDllLoaded = true;
        if (!HookTaskbarViewDllSymbols(taskbarViewModule)) {
            return FALSE;
        }
    } else {
        Wh_Log(L"Taskbar view module not loaded yet");
    }

    HMODULE kernelBaseModule = GetModuleHandle(L"kernelbase.dll");
    auto pKernelBaseLoadLibraryExW = (decltype(&LoadLibraryExW))GetProcAddress(
        kernelBaseModule, "LoadLibraryExW");
    WindhawkUtils::Wh_SetFunctionHookT(pKernelBaseLoadLibraryExW,
                                       LoadLibraryExW_Hook,
                                       &LoadLibraryExW_Original);

    return TRUE;
}

void Wh_ModAfterInit() {
    Wh_Log(L">");

    if (!g_taskbarViewDllLoaded) {
        if (HMODULE taskbarViewModule = GetTaskbarViewModuleHandle()) {
            if (!g_taskbarViewDllLoaded.exchange(true)) {
                Wh_Log(L"Got Taskbar.View.dll");

                if (HookTaskbarViewDllSymbols(taskbarViewModule)) {
                    Wh_ApplyHookOperations();
                }
            }
        }
    }
}

void Wh_ModUninit() {
    Wh_Log(L">");
}

void Wh_ModSettingsChanged() {
    Wh_Log(L">");

    LoadSettings();
}