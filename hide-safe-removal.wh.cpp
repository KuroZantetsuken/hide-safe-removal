// ==WindhawkMod==
// @id              hide-safe-removal
// @name            Hide Safely Remove Hardware Icon
// @description     Permanently hides the "Safely Remove Hardware and Eject Media" tray icon
// @version         1.1
// @author          Kuro
// @github          https://github.com/KuroZantetsuken
// @license         GPL-3.0-or-later
// @include         explorer.exe
// @compilerOptions -lshell32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Hide Safely Remove Hardware Icon

This mod completely hides the "Safely Remove Hardware and Eject Media" tray
icon from the notification area (system tray). Unlike simply moving the icon
to the overflow menu, this mod prevents the icon from appearing at all.

The mod works by:
1. Hooking `Shell_NotifyIconW` to intercept and suppress the icon based on its
   known GUID (`{7820AE78-23E3-4229-82C1-E41CB67D5B9C}`) or tooltip text.
2. Actively removing any already-existing icon when the mod loads.

## Notes

- The mod targets `explorer.exe`, which is the process that manages this icon.
- If you disable or uninstall the mod, the icon will reappear the next time a
  removable device is plugged in or after restarting Explorer.
- You can still safely eject devices via:
  - Right-clicking the drive in File Explorer and selecting "Eject"
  - Settings > Bluetooth & devices > Devices
  - Running `RunDll32.exe shell32.dll,Control_RunDLL hotplug.dll`
*/
// ==/WindhawkModReadme==

#include <shellapi.h>
#include <wchar.h>

// The known GUID for the "Safely Remove Hardware" tray icon.
// This follows the system tray icon GUID pattern:
//   7820AE72 = Clock, 7820AE73 = Volume, 7820AE74 = Network,
//   7820AE75 = Power, 7820AE76 = Action Center, 7820AE78 = Hotplug
static const GUID HOTPLUG_ICON_GUID = {
    0x7820AE78, 0x23E3, 0x4229,
    {0x82, 0xC1, 0xE4, 0x1C, 0xB6, 0x7D, 0x5B, 0x9C}
};

// Check whether the GUID matches the hotplug icon.
static bool IsHotplugGuid(const GUID* guid) {
    return memcmp(guid, &HOTPLUG_ICON_GUID, sizeof(GUID)) == 0;
}

// Check whether the tooltip matches the "Safely Remove Hardware" icon.
static bool IsSafeRemovalTooltip(PCWSTR szTip) {
    if (!szTip || !szTip[0]) {
        return false;
    }
    return wcsstr(szTip, L"Safely Remove Hardware") != nullptr ||
           wcsstr(szTip, L"Safely remove hardware") != nullptr;
}

// Check if the NOTIFYICONDATAW refers to the safe removal icon.
static bool IsSafeRemovalIcon(const NOTIFYICONDATAW* lpData) {
    if (!lpData) return false;

    // Check by GUID first (preferred on Windows 7+)
    if ((lpData->uFlags & NIF_GUID) && IsHotplugGuid(&lpData->guidItem)) {
        return true;
    }

    // Fallback: check by tooltip text
    if ((lpData->uFlags & NIF_TIP) && IsSafeRemovalTooltip(lpData->szTip)) {
        return true;
    }

    return false;
}

// Also track by hWnd+uID once we've seen the icon, for cases where
// subsequent calls don't include the GUID or tooltip.
static bool g_iconTracked = false;
static HWND g_trackedHwnd = nullptr;
static UINT g_trackedId = 0;

static bool IsTrackedIcon(const NOTIFYICONDATAW* lpData) {
    if (!g_iconTracked || !lpData) return false;
    return lpData->hWnd == g_trackedHwnd && lpData->uID == g_trackedId;
}

static void TrackIcon(const NOTIFYICONDATAW* lpData) {
    if (!lpData) return;
    g_trackedHwnd = lpData->hWnd;
    g_trackedId = lpData->uID;
    g_iconTracked = true;
    Wh_Log(L"Tracking safe removal icon (hWnd=%p, uID=%u)", g_trackedHwnd, g_trackedId);
}

static void UntrackIcon() {
    g_iconTracked = false;
    g_trackedHwnd = nullptr;
    g_trackedId = 0;
}

using Shell_NotifyIconW_t = decltype(&Shell_NotifyIconW);
Shell_NotifyIconW_t Shell_NotifyIconW_orig;

BOOL WINAPI Shell_NotifyIconW_hook(DWORD dwMessage, PNOTIFYICONDATAW lpData) {
    if (!lpData) {
        return Shell_NotifyIconW_orig(dwMessage, lpData);
    }

    bool isSafeRemoval = IsSafeRemovalIcon(lpData) || IsTrackedIcon(lpData);

    if (isSafeRemoval) {
        switch (dwMessage) {
            case NIM_ADD:
                TrackIcon(lpData);
                Wh_Log(L"Suppressed NIM_ADD for safe removal icon");
                return TRUE;

            case NIM_MODIFY:
                if (!g_iconTracked) TrackIcon(lpData);
                Wh_Log(L"Suppressed NIM_MODIFY for safe removal icon");
                return TRUE;

            case NIM_SETVERSION:
                Wh_Log(L"Suppressed NIM_SETVERSION for safe removal icon");
                return TRUE;

            case NIM_DELETE:
                Wh_Log(L"Allowing NIM_DELETE for safe removal icon (cleanup)");
                UntrackIcon();
                // Let the delete go through — the icon is being removed anyway
                return Shell_NotifyIconW_orig(dwMessage, lpData);
        }
    }

    return Shell_NotifyIconW_orig(dwMessage, lpData);
}

// Remove the existing hotplug icon using its known GUID.
static void RemoveExistingIcon() {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.uFlags = NIF_GUID;
    nid.guidItem = HOTPLUG_ICON_GUID;

    if (Shell_NotifyIconW_orig(NIM_DELETE, &nid)) {
        Wh_Log(L"Successfully removed existing safe removal icon via GUID");
    } else {
        Wh_Log(L"No existing safe removal icon found (or already removed)");
    }
}

BOOL Wh_ModInit(void) {
    Wh_Log(L"Initializing hide-safe-removal mod v1.1");

    // Resolve Shell_NotifyIconW from shell32.dll to ensure we hook the
    // actual function, not a forwarder or stub.
    HMODULE hShell32 = GetModuleHandleW(L"shell32.dll");
    if (!hShell32) {
        hShell32 = LoadLibraryW(L"shell32.dll");
    }

    void* pShellNotifyIconW = nullptr;
    if (hShell32) {
        pShellNotifyIconW = (void*)GetProcAddress(hShell32, "Shell_NotifyIconW");
    }

    if (!pShellNotifyIconW) {
        // Fallback to the linked import
        Wh_Log(L"GetProcAddress failed, using linked import");
        pShellNotifyIconW = (void*)Shell_NotifyIconW;
    }

    Wh_Log(L"Hooking Shell_NotifyIconW at %p", pShellNotifyIconW);

    if (!Wh_SetFunctionHook(pShellNotifyIconW,
                            (void*)Shell_NotifyIconW_hook,
                            (void**)&Shell_NotifyIconW_orig)) {
        Wh_Log(L"Failed to hook Shell_NotifyIconW");
        return FALSE;
    }

    return TRUE;
}

void Wh_ModAfterInit(void) {
    Wh_Log(L"After init - removing existing safe removal icon");
    RemoveExistingIcon();
}

void Wh_ModUninit(void) {
    Wh_Log(L"Uninitializing hide-safe-removal mod");
}
