#include "pch.h"
#include "WorkAreaConfiguration.h"

#include <FancyZonesLib/FancyZonesData/AppZoneHistory.h>
#include <FancyZonesLib/WindowUtils.h>
#include <FancyZonesLib/WorkArea.h>

WorkArea* const WorkAreaConfiguration::GetWorkArea(HMONITOR monitor) const
{
    auto iter = m_workAreaMap.find(monitor);
    if (iter != m_workAreaMap.end())
    {
        return iter->second.get();
    }

    return nullptr;
}

WorkArea* const WorkAreaConfiguration::GetWorkAreaFromCursor() const
{
    const auto allMonitorsWorkArea = GetWorkArea(nullptr);
    if (allMonitorsWorkArea)
    {
        // First, check if there's a work area spanning all monitors (signalled by the NULL monitor handle)
        return allMonitorsWorkArea;
    }
    else
    {
        // Otherwise, look for the work area based on cursor position
        POINT cursorPoint;
        if (!GetCursorPos(&cursorPoint))
        {
            return nullptr;
        }

        return GetWorkArea(MonitorFromPoint(cursorPoint, MONITOR_DEFAULTTONULL));
    }
}

WorkArea* const WorkAreaConfiguration::GetWorkAreaFromWindow(HWND window) const
{
    const auto allMonitorsWorkArea = GetWorkArea(nullptr);
    if (allMonitorsWorkArea)
    {
        // First, check if there's a work area spanning all monitors (signalled by the NULL monitor handle)
        return allMonitorsWorkArea;
    }
    else
    {
        // Otherwise, look for the work area based on the window's position
        HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONULL);
        return GetWorkArea(monitor);
    }
}

const std::unordered_map<HMONITOR, std::unique_ptr<WorkArea>>& WorkAreaConfiguration::GetAllWorkAreas() const noexcept
{
    return m_workAreaMap;
}

void WorkAreaConfiguration::AddWorkArea(HMONITOR monitor, std::unique_ptr<WorkArea> workArea)
{
    m_workAreaMap.insert({ monitor, std::move(workArea) });
}

void WorkAreaConfiguration::UpdateStampedWindows(const std::vector<StampedWindow>& windows, bool checkOtherWorkAreas, bool updatePositions)
{
    AssignStampedWindows(windows, checkOtherWorkAreas);

    if (updatePositions)
    {
        for (const auto& [_, workArea] : m_workAreaMap)
        {
            if (workArea)
            {
                workArea->UpdateWindowPositions();
            }
        }
    }
}

void WorkAreaConfiguration::AssignStampedWindows(const std::vector<StampedWindow>& windows, bool checkOtherWorkAreas)
{
    const auto allMonitorsWorkArea = GetWorkArea(nullptr);
    if (allMonitorsWorkArea) // one work area across monitors
    {
        for (const auto& stampedWindow : windows)
        {
            allMonitorsWorkArea->Snap(stampedWindow.window, stampedWindow.zones, false);
        }

        return;
    }

    // a window that is placed on a monitor isn't kept by its zone index property on the work area of another monitor
    for (const auto& stampedWindow : windows)
    {
        if (!GetWorkArea(stampedWindow.monitor))
        {
            continue;
        }

        for (const auto& [monitor, workArea] : m_workAreaMap)
        {
            if (workArea && monitor != stampedWindow.monitor)
            {
                workArea->DismissStampedWindow(stampedWindow.window);
            }
        }
    }

    // first, snap windows to the monitor where they're placed
    std::vector<StampedWindow> windowsToSnap{};
    for (const auto& stampedWindow : windows)
    {
        if (IsWindowAssigned(stampedWindow.window))
        {
            continue;
        }

        const auto workAreaForMonitor = GetWorkArea(stampedWindow.monitor);
        if (workAreaForMonitor && AppZoneHistory::instance().GetAppLastZoneIndexSet(stampedWindow.window, workAreaForMonitor->UniqueId(), workAreaForMonitor->GetLayoutId()) == stampedWindow.zones)
        {
            workAreaForMonitor->Snap(stampedWindow.window, stampedWindow.zones, false);
        }
        else
        {
            windowsToSnap.push_back(stampedWindow);
        }
    }

    if (checkOtherWorkAreas)
    {
        // snap rest of the windows to other work areas (in case they were moved after the monitor unplug)
        for (const auto& stampedWindow : windowsToSnap)
        {
            for (const auto& [monitor, workArea] : m_workAreaMap)
            {
                if (monitor == stampedWindow.monitor)
                {
                    // the loop above already checked this work area
                    continue;
                }

                const auto savedIndexes = AppZoneHistory::instance().GetAppLastZoneIndexSet(stampedWindow.window, workArea->UniqueId(), workArea->GetLayoutId());
                if (savedIndexes == stampedWindow.zones)
                {
                    workArea->Snap(stampedWindow.window, stampedWindow.zones, false);
                    if (IsWindowAssigned(stampedWindow.window))
                    {
                        // one work area only, so that the window isn't in the lists of several monitors
                        break;
                    }
                }
            }
        }
    }

    // the windows that weren't snapped stay assigned on the monitor where they're placed,
    // by their zone index property and without app zone history
    for (const auto& stampedWindow : windows)
    {
        if (IsWindowAssigned(stampedWindow.window))
        {
            continue;
        }

        const auto workAreaForMonitor = GetWorkArea(stampedWindow.monitor ? stampedWindow.monitor : stampedWindow.nearestMonitor);
        if (workAreaForMonitor && !FancyZonesWindowUtils::IsExcluded(stampedWindow.window))
        {
            workAreaForMonitor->AssignStampedWindow(stampedWindow.window, stampedWindow.zones);
        }
    }
}

void WorkAreaConfiguration::Clear() noexcept
{
    m_workAreaMap.clear();
}

bool WorkAreaConfiguration::IsWindowAssigned(HWND window) const
{
    for (const auto& [_, workArea] : m_workAreaMap)
    {
        if (workArea && !workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window).empty())
        {
            return true;
        }
    }

    return false;
}
