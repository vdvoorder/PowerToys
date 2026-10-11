#include "pch.h"
#include "WorkArea.h"

#include <common/logger/logger.h>

#include "FancyZonesData/AppliedLayouts.h"
#include "FancyZonesData/AppZoneHistory.h"
#include "FancyZonesData/CustomLayouts.h"
#include "ZonesOverlay.h"
#include "Settings.h"
#include <FancyZonesLib/FancyZonesWindowProperties.h>
#include <FancyZonesLib/VirtualDesktop.h>
#include <FancyZonesLib/WindowUtils.h>

// disabling warning 4458 - declaration of 'identifier' hides class member
// to avoid warnings from GDI files - can't add winRT directory to external code
// in the Cpp.Build.props
#pragma warning(push)
#pragma warning(disable : 4458)
#include <gdiplus.h>
#pragma warning(pop)

// Non-Localizable strings
namespace NonLocalizable
{
    const wchar_t ToolWindowClassName[] = L"FancyZones_ZonesOverlay";
    const wchar_t ToolWindowName[] = L"FancyZones_ZonesOverlay";
}

using namespace FancyZonesUtils;

namespace
{
    // The reason for using this class is the need to call ShowWindow(window, SW_SHOWNORMAL); on each
    // newly created window for it to be displayed properly. The call sometimes has side effects when
    // a fullscreen app is running, and happens when the resolution change event is triggered
    // (e.g. when running some games).
    // This class will serve as a pool of windows for which this call was already done.
    class WindowPool
    {
        std::vector<HWND> m_pool;
        std::mutex m_mutex;

        HWND ExtractWindow()
        {
            std::unique_lock lock(m_mutex);

            if (m_pool.empty())
            {
                return NULL;
            }

            HWND window = m_pool.back();
            m_pool.pop_back();
            return window;
        }

    public:
        HWND NewZonesOverlayWindow(Rect position, HINSTANCE hinstance, WorkArea* owner)
        {
            HWND windowFromPool = ExtractWindow();
            if (windowFromPool == NULL)
            {
                HWND window = CreateWindowExW(WS_EX_TOOLWINDOW, NonLocalizable::ToolWindowClassName, NonLocalizable::ToolWindowName, WS_POPUP, position.left(), position.top(), position.width(), position.height(), nullptr, nullptr, hinstance, owner);
                Logger::info("Creating new ZonesOverlay window, hWnd = {}", (void*)window);
                FancyZonesWindowUtils::MakeWindowTransparent(window);

                // According to ShowWindow docs, we must call it with SW_SHOWNORMAL the first time
                ShowWindow(window, SW_SHOWNORMAL);
                ShowWindow(window, SW_HIDE);
                return window;
            }
            else
            {
                Logger::info("Reusing ZonesOverlay window from pool, hWnd = {}", (void*)windowFromPool);
                SetWindowLongPtrW(windowFromPool, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(owner));
                MoveWindow(windowFromPool, position.left(), position.top(), position.width(), position.height(), TRUE);
                return windowFromPool;
            }
        }

        void FreeZonesOverlayWindow(HWND window)
        {
            Logger::info("Freeing ZonesOverlay window into pool, hWnd = {}", (void*)window);
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            ShowWindow(window, SW_HIDE);

            std::unique_lock lock(m_mutex);
            m_pool.push_back(window);
        }

        ~WindowPool()
        {
            for (HWND window : m_pool)
            {
                DestroyWindow(window);
            }
        }
    };

    WindowPool windowPool;
}

WorkArea::WorkArea(HINSTANCE hinstance, const FancyZonesDataTypes::WorkAreaId& uniqueId, const FancyZonesUtils::Rect& workAreaRect) :
    m_uniqueId(uniqueId),
    m_workAreaRect(workAreaRect)
{
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = s_WndProc;
    wcex.hInstance = hinstance;
    wcex.lpszClassName = NonLocalizable::ToolWindowClassName;
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wcex);
}

WorkArea::~WorkArea()
{
    // Tear down the renderer (joining its background thread) before returning
    // the HWND to the pool. Otherwise, the render thread can still be drawing
    // through m_renderTarget into an HWND that has already been recycled by a
    // subsequent NewZonesOverlayWindow call.
    m_zonesOverlay.reset();
    windowPool.FreeZonesOverlayWindow(m_window);
}

bool WorkArea::Snap(HWND window, const ZoneIndexSet& zones, bool updatePosition)
{
    if (!AreZonesInLayout(zones))
    {
        return false;
    }

    m_layoutWindows.Assign(window, zones);
    m_windowsAssignedByStamp.erase(window);
    AppZoneHistory::instance().SetAppLastZones(window, m_uniqueId, m_layout->Id(), zones);

    if (updatePosition)
    {
        MoveWindowToZones(window, zones, true);
    }

    // A window that is snapped to the zones it already has on this monitor keeps its zone index property as it is.
    // A property that was saved without the monitor gets no monitor this way.
    if (FancyZonesWindowProperties::RetrieveZoneIndexProperty(window) == zones &&
        !FancyZonesWindowProperties::IsZoneIndexPropertyFromAnotherMonitor(window, m_uniqueId.monitorId))
    {
        return true;
    }

    if (!FancyZonesWindowProperties::StampZoneIndexProperty(window, zones))
    {
        return false;
    }

    FancyZonesWindowProperties::StampZoneIndexMonitorProperty(window, m_uniqueId.monitorId);
    return true;
}

bool WorkArea::Unsnap(HWND window)
{
    if (!m_layout)
    {
        return false;
    }
    
    DismissWindow(window);
    AppZoneHistory::instance().RemoveAppLastZone(window, m_uniqueId, m_layout->Id());
    FancyZonesWindowProperties::RemoveZoneIndexProperty(window);

    return true;
}

bool WorkArea::AssignStampedWindow(HWND window, const ZoneIndexSet& stampedZones)
{
    const auto zones = ZonesForStampedWindow(window, stampedZones);
    if (!AreZonesInLayout(zones))
    {
        return false;
    }

    // assigning the same zones again would only move the window to the end of the window switching order
    if (m_layoutWindows.GetZoneIndexSetFromWindow(window) != zones)
    {
        m_layoutWindows.Assign(window, zones);
    }

    if (m_windowsAssignedByStamp.insert(window).second)
    {
        Logger::info(L"Window {} is assigned by its zone index property on work area {}", static_cast<void*>(window), m_uniqueId.toString());
    }

    return true;
}

void WorkArea::DismissStampedWindow(HWND window)
{
    // LayoutAssignedWindows::Dismiss removes the sort key of the window also when this list doesn't have the window,
    // and the window can be in the list of another work area then
    if (m_windowsAssignedByStamp.erase(window) > 0 && !m_layoutWindows.GetZoneIndexSetFromWindow(window).empty())
    {
        m_layoutWindows.Dismiss(window);
    }
}

const GUID WorkArea::GetLayoutId() const noexcept
{
    if (m_layout)
    {
        return m_layout->Id();
    }

    return GUID{};
}

void WorkArea::ShowZones(const ZoneIndexSet& highlight, HWND draggedWindow/* = nullptr*/)
{
    if (m_layout && m_zonesOverlay)
    {
        SetWorkAreaWindowAsTopmost(draggedWindow);
        m_zonesOverlay->DrawActiveZoneSet(m_layout->Zones(), highlight, Colors::GetZoneColors(), FancyZonesSettings::settings().showZoneNumber);
        m_zonesOverlay->Show();
    }
}

void WorkArea::HideZones()
{
    if (m_zonesOverlay)
    {
        m_zonesOverlay->Hide();
    }
}

void WorkArea::FlashZones()
{
    if (m_layout && m_zonesOverlay)
    {
        SetWorkAreaWindowAsTopmost(nullptr);
        m_zonesOverlay->DrawActiveZoneSet(m_layout->Zones(), {}, Colors::GetZoneColors(), FancyZonesSettings::settings().showZoneNumber);
        m_zonesOverlay->Flash();
    }
}

void WorkArea::ShowLayoutNameLabel(const std::wstring& name)
{
    if (m_zonesOverlay && FancyZonesSettings::settings().layoutNameLabelEnabled)
    {
        const auto& settings = FancyZonesSettings::settings();
        const ZonesOverlay::LayoutNameLabelOptions options{
            .textColor = FancyZonesUtils::HexToRGB(settings.layoutNameLabelTextColor),
            .backgroundColor = FancyZonesUtils::HexToRGB(settings.layoutNameLabelBackgroundColor, RGB(38, 38, 38)),
            .fontSize = settings.layoutNameLabelFontSize,
            .padding = settings.layoutNameLabelPadding,
            .durationMillis = settings.layoutNameLabelDuration,
            .placement = settings.layoutNameLabelPlacement,
        };
        m_zonesOverlay->ShowLayoutName(name, options);
    }
}

void WorkArea::ShowMonitorRotationPreview(const std::vector<RECT>& windowRects, size_t monitorNumber, std::optional<bool> reverse, bool animateRotation)
{
    if (!m_zonesOverlay)
    {
        return;
    }

    std::vector<RECT> localWindowRects;
    localWindowRects.reserve(windowRects.size());
    for (const auto& rect : windowRects)
    {
        localWindowRects.push_back(RECT{
            .left = rect.left - m_workAreaRect.left(),
            .top = rect.top - m_workAreaRect.top(),
            .right = rect.right - m_workAreaRect.left(),
            .bottom = rect.bottom - m_workAreaRect.top(),
        });
    }

    SetWorkAreaWindowAsTopmost(nullptr);
    m_zonesOverlay->DrawMonitorRotationPreview(localWindowRects, monitorNumber, reverse, animateRotation);
    m_zonesOverlay->Show();
}

void WorkArea::InitLayout()
{
    InitLayout({});

    if (m_window && m_layout)
    {
        m_zonesOverlay->DrawActiveZoneSet(m_layout->Zones(), {}, Colors::GetZoneColors(), FancyZonesSettings::settings().showZoneNumber);
    }
}

void WorkArea::UpdateWindowPositions()
{
    if (!m_layout || m_layout->Zones().empty())
    {
        // nothing can be placed without zones, the windows stay assigned for the next layout
        return;
    }

    const auto& snappedWindows = m_layoutWindows.SnappedWindows();

    // snapped windows first: Snap saves the app zone history for the current layout,
    // and the zones of a window from another monitor can depend on it
    for (const auto& [window, zones] : snappedWindows)
    {
        if (m_windowsAssignedByStamp.contains(window))
        {
            continue;
        }

        if (!IsWindow(window))
        {
            // The window was closed. Its app zone history is kept for the next window of the app.
            DismissWindow(window);
        }
        else if (AreZonesInLayout(zones))
        {
            // a minimized window stays minimized and gets its new position without the wait
            Snap(window, zones, false);
            MoveWindowToZones(window, zones, false);
        }
    }

    for (const auto& [window, _] : snappedWindows)
    {
        if (!m_windowsAssignedByStamp.contains(window))
        {
            continue;
        }

        // the zone index property and the app zone history of these windows stay as they are
        if (IsWindow(window) && AssignStampedWindow(window, FancyZonesWindowProperties::RetrieveZoneIndexProperty(window)))
        {
            MoveWindowToZones(window, m_layoutWindows.GetZoneIndexSetFromWindow(window), false);
        }
        else
        {
            // the window was closed, lost the zone index property or the layout doesn't have its zones anymore
            DismissWindow(window);
        }
    }
}

void WorkArea::CycleWindows(HWND window, bool reverse)
{
    m_layoutWindows.CycleWindows(window, reverse);
}

#pragma region private

bool WorkArea::InitWindow(HINSTANCE hinstance)
{
    m_window = windowPool.NewZonesOverlayWindow(m_workAreaRect, hinstance, this);
    if (!m_window)
    {
        Logger::error(L"No work area window");
        return false;
    }

    m_zonesOverlay = std::make_unique<ZonesOverlay>(m_window);
    return true;
}

void WorkArea::InitLayout(const FancyZonesDataTypes::WorkAreaId& parentUniqueId)
{
    Logger::info(L"Initialize layout on {}, work area rect = {}x{}", m_uniqueId.toString(), m_workAreaRect.width(), m_workAreaRect.height());

    const bool isLayoutAlreadyApplied = AppliedLayouts::instance().IsLayoutApplied(m_uniqueId);
    if (!isLayoutAlreadyApplied)
    {
        if (!AppliedLayouts::instance().CloneLayout(parentUniqueId, m_uniqueId))
        {
            AppliedLayouts::instance().ApplyDefaultLayout(m_uniqueId);
        }

        AppliedLayouts::instance().SaveData();
    }

    CalculateZoneSet();
}

void WorkArea::InitSnappedWindows()
{
    static bool updatePositionOnceOnStartFlag = true;
    Logger::info(L"Init work area {} windows, update positions = {}", m_uniqueId.toString(), updatePositionOnceOnStartFlag);

    for (const auto& window : VirtualDesktop::instance().GetWindowsFromCurrentDesktop())
    {
        auto indexes = FancyZonesWindowProperties::RetrieveZoneIndexProperty(window);
        if (indexes.size() == 0)
        {
            continue;
        }

        if (!m_uniqueId.monitorId.monitor) // one work area across monitors
        {
            Snap(window, indexes, updatePositionOnceOnStartFlag);
        }
        else
        {
            const auto monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONULL);
            if (monitor && m_uniqueId.monitorId.monitor == monitor)
            {
                // prioritize snapping on the current monitor if the window was snapped to several work areas
                Snap(window, indexes, updatePositionOnceOnStartFlag);
            }
            else
            {
                // if the window is not snapped on the current monitor, then check the others
                auto savedIndexes = AppZoneHistory::instance().GetAppLastZoneIndexSet(window, m_uniqueId, GetLayoutId());
                if (savedIndexes == indexes)
                {
                    Snap(window, indexes, updatePositionOnceOnStartFlag);
                }
            }
        }
    }

    updatePositionOnceOnStartFlag = false;
}

void WorkArea::CalculateZoneSet()
{
    auto appliedLayout = AppliedLayouts::instance().GetDeviceLayout(m_uniqueId);
    if (!appliedLayout.has_value())
    {
        Logger::error(L"Layout wasn't applied. Can't init layout on work area {}x{}", m_workAreaRect.width(), m_workAreaRect.height());
        return;
    }

    // For custom layouts the spacing, sensitivity radius and zone count live in the custom
    // layout definition (custom-layouts.json), while the applied-layouts.json snapshot keeps a
    // copy taken at apply time. Editing those properties only rewrites custom-layouts.json, so
    // without this sync the snapshot stays stale and the edits don't take effect until the layout
    // is re-applied (see GH #44058). Re-derive the scalar properties from the current custom
    // layout using the same logic the apply path uses (CustomLayouts::GetLayout also derives the
    // canvas zone count from the zone list, keeping it consistent with Layout::Init validation).
    if (appliedLayout->type == FancyZonesDataTypes::ZoneSetLayoutType::Custom)
    {
        if (const auto refreshed = CustomLayouts::instance().GetLayout(appliedLayout->uuid))
        {
            appliedLayout = refreshed;
        }
    }

    m_layout = std::make_unique<Layout>(appliedLayout.value());
    m_layout->Init(m_workAreaRect, m_uniqueId.monitorId.monitor);
}

LRESULT WorkArea::WndProc(UINT message, WPARAM wparam, LPARAM lparam) noexcept
{
    switch (message)
    {
    case WM_NCDESTROY:
    {
        ::DefWindowProc(m_window, message, wparam, lparam);
        SetWindowLongPtr(m_window, GWLP_USERDATA, 0);
    }
    break;

    case WM_ERASEBKGND:
        return 1;

    default:
    {
        return DefWindowProc(m_window, message, wparam, lparam);
    }
    }
    return 0;
}

void WorkArea::SetWorkAreaWindowAsTopmost(HWND draggedWindow) noexcept
{
    if (!m_window)
    {
        return;
    }

    HWND windowInsertAfter = draggedWindow ? draggedWindow : HWND_TOPMOST;

    constexpr UINT flags = SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE;
    SetWindowPos(m_window, windowInsertAfter, 0, 0, 0, 0, flags);
}

bool WorkArea::AreZonesInLayout(const ZoneIndexSet& zones) const noexcept
{
    if (!m_layout || zones.empty())
    {
        return false;
    }

    for (ZoneIndex zone : zones)
    {
        if (static_cast<size_t>(zone) >= m_layout->Zones().size())
        {
            return false;
        }
    }

    return true;
}

ZoneIndexSet WorkArea::ClampZonesToLayout(const ZoneIndexSet& zones) const
{
    if (!m_layout || m_layout->Zones().empty())
    {
        return {};
    }

    const auto zoneCount = m_layout->Zones().size();
    const auto lastZone = static_cast<ZoneIndex>(zoneCount - 1);

    ZoneIndexSet zonesInLayout{};
    for (ZoneIndex zone : zones)
    {
        const auto zoneInLayout = static_cast<size_t>(zone) < zoneCount ? zone : lastZone;
        if (std::find(zonesInLayout.begin(), zonesInLayout.end(), zoneInLayout) == zonesInLayout.end())
        {
            zonesInLayout.push_back(zoneInLayout);
        }
    }

    return zonesInLayout;
}

ZoneIndexSet WorkArea::ZonesForStampedWindow(HWND window, const ZoneIndexSet& stampedZones) const
{
    if (!FancyZonesWindowProperties::IsZoneIndexPropertyFromAnotherMonitor(window, m_uniqueId.monitorId))
    {
        return stampedZones;
    }

    // The zone numbers were assigned on another monitor and mean other places here.
    // The window keeps the zones it already has on this work area; otherwise it gets the last zones of its app here.
    const auto assignedZones = m_layoutWindows.GetZoneIndexSetFromWindow(window);
    if (AreZonesInLayout(assignedZones))
    {
        return assignedZones;
    }

    // one lookup for both uses of the app zone history below, because it reads the process path of the window
    const auto appZoneHistory = AppZoneHistory::instance().GetAppZoneHistory(window, m_uniqueId);
    const auto layoutId = GetLayoutId();

    // the zones that were saved for the current layout, from the entry that AppZoneHistory::GetAppLastZoneIndexSet reads
    const auto currentLayoutData = std::find_if(appZoneHistory.begin(), appZoneHistory.end(), [&](const auto& data) { return data.layoutId == layoutId; });
    if (currentLayoutData != appZoneHistory.end() && AreZonesInLayout(currentLayoutData->zoneIndexSet))
    {
        return currentLayoutData->zoneIndexSet;
    }

    // the layout doesn't have the zones of the window on this work area anymore
    if (!assignedZones.empty())
    {
        return ClampZonesToLayout(assignedZones);
    }

    // the zones that were saved for another layout of this work area
    for (const auto& data : appZoneHistory)
    {
        if (data.layoutId != layoutId && AreZonesInLayout(data.zoneIndexSet))
        {
            return data.zoneIndexSet;
        }
    }

    // the last zone of the layout replaces the zones that the layout doesn't have, in the window list only
    return ClampZonesToLayout(stampedZones);
}

void WorkArea::MoveWindowToZones(HWND window, const ZoneIndexSet& zones, bool waitIfMinimized)
{
    const auto rect = m_layout->GetCombinedZonesRect(zones);
    const auto adjustedRect = FancyZonesWindowUtils::AdjustRectForSizeWindowToRect(window, rect, m_window);
    FancyZonesWindowUtils::SaveWindowSizeAndOrigin(window);
    const auto ifMinimized = waitIfMinimized ? FancyZonesWindowUtils::IfMinimized::Wait : FancyZonesWindowUtils::IfMinimized::DoNotWait;
    FancyZonesWindowUtils::SizeWindowToRect(window, adjustedRect, true, ifMinimized);
}

void WorkArea::DismissWindow(HWND window)
{
    m_layoutWindows.Dismiss(window);
    m_windowsAssignedByStamp.erase(window);
}

#pragma endregion

LRESULT CALLBACK WorkArea::s_WndProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept
{
    auto thisRef = reinterpret_cast<WorkArea*>(GetWindowLongPtr(window, GWLP_USERDATA));
    if ((thisRef == nullptr) && (message == WM_CREATE))
    {
        auto createStruct = reinterpret_cast<LPCREATESTRUCT>(lparam);
        thisRef = static_cast<WorkArea*>(createStruct->lpCreateParams);
        SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(thisRef));
    }

    return (thisRef != nullptr) ? thisRef->WndProc(message, wparam, lparam) :
                                  DefWindowProc(window, message, wparam, lparam);
}
