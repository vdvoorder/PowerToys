#pragma once

#include <FancyZonesLib/FancyZonesDataTypes.h>

class WorkArea;

// Window with the zone index property.
struct StampedWindow
{
    HWND window{};
    ZoneIndexSet zones{};
    HMONITOR monitor{}; // Monitor where the window is placed, null if the window isn't placed on any monitor.
    HMONITOR nearestMonitor{}; // Monitor that is nearest to the window, set only if the window isn't placed on any monitor.
};

class WorkAreaConfiguration
{
public:
    /**
     * Get work area based on monitor handle.
     *
     * @param[in]  monitor   Monitor handle.
     *
     * @returns    Object representing single work area, interface to all actions available on work area
     *             (e.g. moving windows through zone layout specified for that work area).
     */
    WorkArea* const GetWorkArea(HMONITOR monitor) const;

    /**
     * Get work area based on the current cursor position.
     *
     * @returns    Object representing single work area, interface to all actions available on work area
     *             (e.g. moving windows through zone layout specified for that work area).
     */
    WorkArea* const GetWorkAreaFromCursor() const;

    /**
     * Get work area on which specified window is located.
     *
     * @param[in]  window Window handle.
     * 
     * @returns    Object representing single work area, interface to all actions available on work area
     *             (e.g. moving windows through zone layout specified for that work area).
     */
    WorkArea* const GetWorkAreaFromWindow(HWND window) const;

    /**
     * @returns    All registered work areas.
     */
    const std::unordered_map<HMONITOR, std::unique_ptr<WorkArea>>& GetAllWorkAreas() const noexcept;

    /**
     * Register new work area.
     *
     * @param[in]  monitor   Monitor handle.
     * @param[in]  workAra   Object representing single work area.
     */
    void AddWorkArea(HMONITOR monitor, std::unique_ptr<WorkArea> workArea);

    /**
     * Update the windows with the zone index property after a display, work area, virtual desktop or layout change.
     * A window is snapped on the work area where the app zone history has the same zones for it, unless its zone
     * index property was saved on another monitor; the monitor where the window is placed is checked first.
     * Every other window is assigned on the monitor where it's placed, without saving app zone history
     * (WorkArea::AssignStampedWindow). A window whose zone index property was saved on another monitor
     * isn't assigned while that monitor has a work area.
     *
     * @param[in]  windows               Windows with the zone index property.
     * @param[in]  checkOtherWorkAreas   Whether the app zone history is also checked on the work areas of
     *                                   the monitors where the window isn't placed.
     * @param[in]  updatePositions       Whether the windows are moved to their zones.
     */
    void UpdateStampedWindows(const std::vector<StampedWindow>& windows, bool checkOtherWorkAreas, bool updatePositions);

    /**
     * Clear all persisted work area related data.
     */
    void Clear() noexcept;
    
private:
    void AssignStampedWindows(const std::vector<StampedWindow>& windows, bool checkOtherWorkAreas);
    bool IsWindowAssigned(HWND window) const;
    bool IsZoneIndexPropertyFromOtherWorkArea(HWND window, const WorkArea& workArea) const;

    std::unordered_map<HMONITOR, std::unique_ptr<WorkArea>> m_workAreaMap;
};
