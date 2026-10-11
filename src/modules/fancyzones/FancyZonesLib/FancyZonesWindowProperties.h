#pragma once

#include <optional>

#include <FancyZonesLib/Zone.h>

namespace FancyZonesDataTypes
{
    struct MonitorId;
}

// Zoned window properties are not localized.
namespace ZonedWindowProperties
{
    const wchar_t PropertyRestoreSizeID[] = L"FancyZones_RestoreSize";
    const wchar_t PropertyRestoreOriginID[] = L"FancyZones_RestoreOrigin";
    const wchar_t PropertyCornerPreference[] = L"FancyZones_CornerPreference";
    const wchar_t PropertyMovedOnOpening[] = L"FancyZones_MovedOnOpening";

    const wchar_t MultiMonitorName[] = L"FancyZones";
    const wchar_t MultiMonitorInstance[] = L"MultiMonitorDevice";
}

namespace FancyZonesWindowProperties
{
    bool StampZoneIndexProperty(HWND window, const ZoneIndexSet& zoneSet);
    void RemoveZoneIndexProperty(HWND window);
    ZoneIndexSet RetrieveZoneIndexProperty(HWND window);

    // The monitor where the zone index property was stamped. It's removed together with the zone index property.
    void StampZoneIndexMonitorProperty(HWND window, const FancyZonesDataTypes::MonitorId& monitorId);
    bool IsZoneIndexPropertyFromAnotherMonitor(HWND window, const FancyZonesDataTypes::MonitorId& monitorId);

    void StampMovedOnOpeningProperty(HWND window);
    bool RetrieveMovedOnOpeningProperty(HWND window);

    std::optional<size_t> GetTabSortKeyWithinZone(HWND window);
    void SetTabSortKeyWithinZone(HWND window, std::optional<size_t> tabSortKeyWithinZone);

    bool IsLaunchedByWorkspaces(HWND window);
}

