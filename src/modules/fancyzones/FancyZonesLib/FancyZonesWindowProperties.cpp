#include "pch.h"
#include "FancyZonesWindowProperties.h"

#include <FancyZonesLib/FancyZonesDataTypes.h>
#include <FancyZonesLib/ZoneIndexSetBitmask.h>

#include <common/logger/logger.h>
#include <common/utils/winapi_error.h>

#include <modules/Workspaces/WindowProperties/WorkspacesWindowPropertyUtils.h>

// Zoned window properties are not localized.
namespace ZonedWindowProperties
{
    const wchar_t PropertyMultipleZone64ID[] = L"FancyZones_zones"; // maximum possible zone count = 64
    const wchar_t PropertyMultipleZone128ID[] = L"FancyZones_zones_max128"; // additional property to allow maximum possible zone count = 128
    const wchar_t PropertyZoneIndexMonitorID[] = L"FancyZones_zones_monitor"; // monitor where the zone index property was stamped

    const wchar_t PropertySortKeyWithinZone[] = L"FancyZones_TabSortKeyWithinZone";
}

namespace
{
    // The value stays on the window while FancyZones is restarted or updated, so it can't come from std::hash.
    // Only the device id is used: the handle, the instance id, the number and the serial number of a monitor
    // can be different after it was connected again.
    HANDLE MonitorPropertyValue(const FancyZonesDataTypes::MonitorId& monitorId) noexcept
    {
        constexpr uint64_t fnvOffset = 14695981039346656037ull;
        constexpr uint64_t fnvPrime = 1099511628211ull;

        uint64_t hash = fnvOffset;
        for (const wchar_t ch : monitorId.deviceId.id)
        {
            hash ^= ch;
            hash *= fnvPrime;
        }

        if (hash == 0)
        {
            hash = 1; // GetProp returns null for a window without the property
        }

        static_assert(sizeof(HANDLE) == sizeof(uint64_t));
        return reinterpret_cast<HANDLE>(hash);
    }
}

bool FancyZonesWindowProperties::StampZoneIndexProperty(HWND window, const ZoneIndexSet& zoneSet)
{
    RemoveZoneIndexProperty(window);
    ZoneIndexSetBitmask bitmask = ZoneIndexSetBitmask::FromIndexSet(zoneSet);

    if (bitmask.part1 != 0)
    {
        std::array<int32_t, 2> data{
            static_cast<int>(bitmask.part1),
            static_cast<int>(bitmask.part1 >> 32)
        };

        HANDLE rawData;
        memcpy(&rawData, data.data(), sizeof data);

        if (!SetProp(window, ZonedWindowProperties::PropertyMultipleZone64ID, rawData))
        {
            Logger::error(L"Failed to stamp window {}", get_last_error_or_default(GetLastError()));
            return false;
        }
    }

    if (bitmask.part2 != 0)
    {
        std::array<int32_t, 2> data{
            static_cast<int>(bitmask.part2),
            static_cast<int>(bitmask.part2 >> 32)
        };

        HANDLE rawData;
        memcpy(&rawData, data.data(), sizeof data);

        if (!SetProp(window, ZonedWindowProperties::PropertyMultipleZone128ID, rawData))
        {
            Logger::error(L"Failed to stamp window {}", get_last_error_or_default(GetLastError()));
            return false;
        }
    }

    return true;
}

void FancyZonesWindowProperties::RemoveZoneIndexProperty(HWND window)
{
    ::RemoveProp(window, ZonedWindowProperties::PropertyMultipleZone64ID);
    ::RemoveProp(window, ZonedWindowProperties::PropertyMultipleZone128ID);
    ::RemoveProp(window, ZonedWindowProperties::PropertyZoneIndexMonitorID);
}

ZoneIndexSet FancyZonesWindowProperties::RetrieveZoneIndexProperty(HWND window)
{
    HANDLE handle64 = ::GetProp(window, ZonedWindowProperties::PropertyMultipleZone64ID);
    HANDLE handle128 = ::GetProp(window, ZonedWindowProperties::PropertyMultipleZone128ID);

    ZoneIndexSetBitmask bitmask{};

    if (handle64)
    {
        std::array<int32_t, 2> data;
        memcpy(data.data(), &handle64, sizeof data);
        bitmask.part1 = (static_cast<decltype(bitmask.part1)>(data[1]) << 32) + data[0];
    }

    if (handle128)
    {
        std::array<int32_t, 2> data;
        memcpy(data.data(), &handle128, sizeof data);
        bitmask.part2 = (static_cast<decltype(bitmask.part2)>(data[1]) << 32) + data[0];
    }

    return bitmask.ToIndexSet();
}

void FancyZonesWindowProperties::StampZoneIndexMonitorProperty(HWND window, const FancyZonesDataTypes::MonitorId& monitorId)
{
    if (!SetProp(window, ZonedWindowProperties::PropertyZoneIndexMonitorID, MonitorPropertyValue(monitorId)))
    {
        Logger::error(L"Failed to stamp window {}", get_last_error_or_default(GetLastError()));
    }
}

bool FancyZonesWindowProperties::IsZoneIndexPropertyFromAnotherMonitor(HWND window, const FancyZonesDataTypes::MonitorId& monitorId)
{
    const HANDLE value = ::GetProp(window, ZonedWindowProperties::PropertyZoneIndexMonitorID);
    return value && value != MonitorPropertyValue(monitorId);
}

void FancyZonesWindowProperties::StampMovedOnOpeningProperty(HWND window)
{
    ::SetPropW(window, ZonedWindowProperties::PropertyMovedOnOpening, reinterpret_cast<HANDLE>(1));
}

bool FancyZonesWindowProperties::RetrieveMovedOnOpeningProperty(HWND window)
{
    HANDLE handle = ::GetProp(window, ZonedWindowProperties::PropertyMovedOnOpening);
    return handle != nullptr;
}

std::optional<size_t> FancyZonesWindowProperties::GetTabSortKeyWithinZone(HWND window)
{
    auto rawTabSortKeyWithinZone = ::GetPropW(window, ZonedWindowProperties::PropertySortKeyWithinZone);
    if (rawTabSortKeyWithinZone == NULL)
    {
        return std::nullopt;
    }

    auto tabSortKeyWithinZone = reinterpret_cast<uint64_t>(rawTabSortKeyWithinZone) - 1;
    return tabSortKeyWithinZone;
}

void FancyZonesWindowProperties::SetTabSortKeyWithinZone(HWND window, std::optional<size_t> tabSortKeyWithinZone)
{
    if (!tabSortKeyWithinZone.has_value())
    {
        ::RemovePropW(window, ZonedWindowProperties::PropertySortKeyWithinZone);
    }
    else
    {
        auto rawTabSortKeyWithinZone = reinterpret_cast<HANDLE>(tabSortKeyWithinZone.value() + 1);
        ::SetPropW(window, ZonedWindowProperties::PropertySortKeyWithinZone, rawTabSortKeyWithinZone);
    }
}

bool FancyZonesWindowProperties::IsLaunchedByWorkspaces(HWND window)
{
    HANDLE handle = ::GetProp(window, WorkspacesWindowProperties::Properties::LaunchedByWorkspacesID);
    return handle != nullptr;
}
