#pragma once

#include <FancyZonesLib/FancyZonesDataTypes.h>
#include <FancyZonesLib/util.h>

namespace MonitorUtils
{
    namespace Display
    {
        // A monitor that FancyZones identified before: the id and the rectangle of its work area.
        struct KnownMonitor
        {
            FancyZonesDataTypes::MonitorId monitorId;
            FancyZonesUtils::Rect workAreaRect;
        };

        std::pair<bool, std::vector<FancyZonesDataTypes::MonitorId>> GetDisplays(bool logDisplayDevices = true);

        // GetDisplays gives a display without an active display device its display name (\\.\DISPLAY1) as id.
        bool IsDisplayName(const FancyZonesDataTypes::DeviceId& deviceId) noexcept;
        std::vector<std::wstring> GetDisplayNames(const std::vector<FancyZonesDataTypes::MonitorId>& monitors);

        // The display names in monitors that no known monitor has as id, when a known monitor with a device id
        // is missing: such a display can be that monitor.
        std::vector<std::wstring> GetDisplayNamesWithLostDeviceId(const std::vector<FancyZonesDataTypes::MonitorId>& monitors, const std::vector<KnownMonitor>& knownMonitors);

        // The display names to check for a display device: those of GetDisplayNamesWithLostDeviceId, and the
        // displays that were checked before and still have no display device.
        std::vector<std::wstring> GetDisplayNamesToCheck(const std::vector<FancyZonesDataTypes::MonitorId>& monitors, const std::vector<KnownMonitor>& knownMonitors, const std::vector<std::wstring>& checkedDisplayNames);

        // Gives a display of displayNames the device id and the serial number of the known monitor that has the
        // same monitor handle and the same work area rectangle, when the number of monitors didn't change.
        bool KeepDeviceIds(std::vector<FancyZonesDataTypes::MonitorId>& monitors, const std::vector<std::wstring>& displayNames, const std::vector<KnownMonitor>& knownMonitors, const std::function<FancyZonesUtils::Rect(HMONITOR)>& getWorkAreaRect);

        FancyZonesDataTypes::DeviceId SplitDisplayDeviceId(const std::wstring& str) noexcept;
        FancyZonesDataTypes::DeviceId ConvertObsoleteDeviceId(const std::wstring& str) noexcept;
    }

    namespace WMI
    {
        std::vector<FancyZonesDataTypes::MonitorId> GetHardwareMonitorIds();
        FancyZonesDataTypes::DeviceId SplitWMIDeviceId(const std::wstring& str) noexcept;
    }

    std::vector<FancyZonesDataTypes::MonitorId> IdentifyMonitors() noexcept;
    void OpenWindowOnActiveMonitor(HWND window, HMONITOR monitor) noexcept;

    FancyZonesUtils::Rect GetWorkAreaRect(HMONITOR monitor);
};
