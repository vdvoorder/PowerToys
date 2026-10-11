#pragma once

#include <FancyZonesLib/FancyZonesDataTypes.h>

class WorkArea;

// Window with the zone index property.
struct StampedWindow
{
    HWND window{};
    ZoneIndexSet zones{};
    HMONITOR monitor{}; // Monitor where the window is placed, null if the window isn't placed on any monitor.
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
     * Snap the windows with the zone index property on the work areas where the app zone history
     * has the same zones for them, and update the positions of the snapped windows.
     *
     * @param[in]  windows           Windows with the zone index property.
     * @param[in]  updatePositions   Whether the windows are moved to their zones.
     */
    void UpdateStampedWindows(const std::vector<StampedWindow>& windows, bool updatePositions);

    /**
     * Clear all persisted work area related data.
     */
    void Clear() noexcept;
    
private:
    bool IsWindowAssigned(HWND window) const;

    std::unordered_map<HMONITOR, std::unique_ptr<WorkArea>> m_workAreaMap;
};
