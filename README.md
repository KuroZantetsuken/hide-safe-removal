# Hide Safely Remove Hardware Icon

A [Windhawk](https://windhawk.net/) mod that permanently hides the **Safely Remove Hardware and Eject Media** notification-area icon. It prevents the icon from being added and removes an existing instance when the mod loads.

You can still eject removable devices through File Explorer, **Settings > Bluetooth & devices > Devices**, or `RunDll32.exe shell32.dll,Control_RunDLL hotplug.dll`.

## Installation

1. Install [Windhawk](https://windhawk.net/).
2. Create a new mod in Windhawk.
3. Replace the generated source with the contents of [`hide-safe-removal.wh.cpp`](hide-safe-removal.wh.cpp).
4. Compile and enable the mod.

The mod targets `explorer.exe`. After disabling or uninstalling it, the icon can reappear when a removable device is connected or Explorer is restarted.

## License

This project is licensed under the [GNU General Public License v3.0 or later](LICENSE).

## AI disclosure

This project was created with the help of LLM tools. I try to understand and review every change made by code generators, but I am an amateur and do this for fun and to create helpful tools for myself.