#include "pch.h"

#include <algorithm>
#include <filesystem>
#include <vector>

#include <FancyZonesLib/WorkArea.h>
#include <FancyZonesLib/WorkAreaConfiguration.h>
#include <FancyZonesLib/FancyZonesData/AppliedLayouts.h>
#include <FancyZonesLib/FancyZonesData/AppZoneHistory.h>
#include <FancyZonesLib/FancyZonesData/CustomLayouts.h>
#include <FancyZonesLib/FancyZonesData/DefaultLayouts.h>
#include <FancyZonesLib/FancyZonesWindowProperties.h>
#include <FancyZonesLib/LayoutAssignedWindows.h>
#include <FancyZonesLib/Settings.h>
#include "Util.h"

#include <common/utils/process_path.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace FancyZonesUnitTests
{
    const std::wstring m_deviceId = L"\\\\?\\DISPLAY#DELA026#5&10a58c63&0&UID16777488#{e6f07b5f-ee97-4a90-b076-33f57bf4eaa7}";
    const std::wstring m_virtualDesktopId = L"MyVirtualDesktopId";

    TEST_CLASS (WorkAreaCreationUnitTests)
    {
        FancyZonesDataTypes::WorkAreaId m_workAreaId;
        FancyZonesDataTypes::WorkAreaId m_emptyUniqueId;
        FancyZonesUtils::Rect m_workAreaRect{ RECT(0,0,1920,1080) };

        HINSTANCE m_hInst{};
        HMONITOR m_monitor{};

        TEST_METHOD_INITIALIZE(Init) noexcept
        {
            m_workAreaId.monitorId.deviceId.id = L"DELA026";
            m_workAreaId.monitorId.deviceId.instanceId = L"5&10a58c63&0&UID16777488";
            m_workAreaId.monitorId.serialNumber = L"serial-number";
            m_workAreaId.virtualDesktopId = FancyZonesUtils::GuidFromString(L"{39B25DD2-130D-4B5D-8851-4791D66B1539}").value();

            AppZoneHistory::instance().LoadData();
            AppliedLayouts::instance().LoadData();
            CustomLayouts::instance().LoadData();
            DefaultLayouts::instance().LoadData();
        }

        TEST_METHOD_CLEANUP(CleanUp) noexcept
        {
            std::filesystem::remove(AppliedLayouts::AppliedLayoutsFileName());
            std::filesystem::remove(AppZoneHistory::AppZoneHistoryFileName());
            std::filesystem::remove(CustomLayouts::CustomLayoutsFileName());
            std::filesystem::remove(DefaultLayouts::DefaultLayoutsFileName());
        }

        TEST_METHOD (CreateWorkArea)
        {
            const auto defaultLayout = DefaultLayouts::instance().GetDefaultLayout();

            auto workArea = WorkArea::Create({}, m_workAreaId, m_emptyUniqueId, m_workAreaRect);
            Assert::IsFalse(workArea == nullptr);
            Assert::IsTrue(m_workAreaId == workArea->UniqueId());

            const auto& layout = workArea->GetLayout();
            Assert::IsNotNull(layout.get());
            Assert::AreEqual(static_cast<int>(defaultLayout.type), static_cast<int>(layout->Type()));
            Assert::AreEqual(defaultLayout.zoneCount, static_cast<int>(layout->Zones().size()));
        }

        TEST_METHOD (CreateCombinedWorkArea)
        {
            const auto defaultLayout = DefaultLayouts::instance().GetDefaultLayout();

            auto workArea = WorkArea::Create({}, m_workAreaId, m_emptyUniqueId, m_workAreaRect);
            Assert::IsFalse(workArea == nullptr);
            Assert::IsTrue(m_workAreaId == workArea->UniqueId());

            const auto& layout = workArea->GetLayout();
            Assert::IsNotNull(layout.get());
            Assert::AreEqual(static_cast<int>(defaultLayout.type), static_cast<int>(layout->Type()));
            Assert::AreEqual(defaultLayout.zoneCount, static_cast<int>(layout->Zones().size()));
        }

        TEST_METHOD (CreateWorkAreaClonedFromParent)
        {
            using namespace FancyZonesDataTypes;
            
            FancyZonesDataTypes::WorkAreaId parentUniqueId;
            parentUniqueId.monitorId.deviceId.id = L"DELA026";
            parentUniqueId.monitorId.deviceId.instanceId = L"5&10a58c63&0&UID16777488";
            parentUniqueId.monitorId.serialNumber = L"serial-number";
            parentUniqueId.virtualDesktopId = FancyZonesUtils::GuidFromString(L"{61FA9FC0-26A6-4B37-A834-491C148DFC57}").value();

            LayoutData layout{
                .uuid = FancyZonesUtils::GuidFromString(L"{61FA9FC0-26A6-4B37-A834-491C148DFC58}").value(),
                .type = ZoneSetLayoutType::Rows,
                .showSpacing = true,
                .spacing = 10,
                .zoneCount = 10,
                .sensitivityRadius = 20,
            };

            auto parentWorkArea = WorkArea::Create(m_hInst, parentUniqueId, m_emptyUniqueId, m_workAreaRect);
            AppliedLayouts::instance().ApplyLayout(parentUniqueId, layout);

            auto actualWorkArea = WorkArea::Create(m_hInst, m_workAreaId, parentUniqueId, m_workAreaRect);
            Assert::IsNotNull(actualWorkArea->GetLayout().get());

            Assert::IsTrue(AppliedLayouts::instance().GetAppliedLayoutMap().contains(m_workAreaId));
            const auto& actualLayout = AppliedLayouts::instance().GetAppliedLayoutMap().at(m_workAreaId);

            Assert::AreEqual(static_cast<int>(layout.type), static_cast<int>(actualLayout.type));
            Assert::AreEqual(FancyZonesUtils::GuidToString(layout.uuid).value(), FancyZonesUtils::GuidToString(actualLayout.uuid).value());
            Assert::AreEqual(layout.sensitivityRadius, actualLayout.sensitivityRadius);
            Assert::AreEqual(layout.showSpacing, actualLayout.showSpacing);
            Assert::AreEqual(layout.spacing, actualLayout.spacing);
            Assert::AreEqual(layout.zoneCount, actualLayout.zoneCount);
        }

        TEST_METHOD (CreateWorkAreaWithCustomDefault)
        {
            // prepare
            json::JsonObject root{};
            json::JsonArray layoutsArray{};
            json::JsonObject layout{};
            layout.SetNamedValue(NonLocalizable::DefaultLayoutsIds::UuidID, json::value(L"{ACE817FD-2C51-4E13-903A-84CAB86FD17C}"));
            layout.SetNamedValue(NonLocalizable::DefaultLayoutsIds::TypeID, json::value(L"custom"));
            json::JsonObject item{};
            item.SetNamedValue(NonLocalizable::DefaultLayoutsIds::MonitorConfigurationTypeID, json::value(L"horizontal"));
            item.SetNamedValue(NonLocalizable::DefaultLayoutsIds::LayoutID, layout);
            layoutsArray.Append(item);
            root.SetNamedValue(NonLocalizable::DefaultLayoutsIds::DefaultLayoutsArrayID, layoutsArray);
            
            json::to_file(DefaultLayouts::DefaultLayoutsFileName(), root);
            DefaultLayouts::instance().LoadData();

            // test
            auto workArea = WorkArea::Create({}, m_workAreaId, m_emptyUniqueId, m_workAreaRect);
            Assert::IsFalse(workArea == nullptr);
            Assert::IsTrue(m_workAreaId == workArea->UniqueId());

            Assert::IsNotNull(workArea->GetLayout().get());

            const auto& actualLayout = workArea->GetLayout();
            Assert::AreEqual(static_cast<int>(FancyZonesDataTypes::ZoneSetLayoutType::Custom), static_cast<int>(actualLayout->Type()));
            Assert::IsTrue(FancyZonesUtils::GuidFromString(L"{ACE817FD-2C51-4E13-903A-84CAB86FD17C}").value() == actualLayout->Id());
        }

        TEST_METHOD (CreateWorkAreaWithTemplateDefault)
        {
            // prepare
            json::JsonObject root{};
            json::JsonArray layoutsArray{};
            json::JsonObject layout{};
            layout.SetNamedValue(NonLocalizable::DefaultLayoutsIds::TypeID, json::value(L"grid"));
            layout.SetNamedValue(NonLocalizable::DefaultLayoutsIds::ShowSpacingID, json::value(true));
            layout.SetNamedValue(NonLocalizable::DefaultLayoutsIds::SpacingID, json::value(1));
            layout.SetNamedValue(NonLocalizable::DefaultLayoutsIds::ZoneCountID, json::value(4));
            layout.SetNamedValue(NonLocalizable::DefaultLayoutsIds::SensitivityRadiusID, json::value(30));

            json::JsonObject item{};
            item.SetNamedValue(NonLocalizable::DefaultLayoutsIds::MonitorConfigurationTypeID, json::value(L"horizontal"));
            item.SetNamedValue(NonLocalizable::DefaultLayoutsIds::LayoutID, layout);
            layoutsArray.Append(item);
            root.SetNamedValue(NonLocalizable::DefaultLayoutsIds::DefaultLayoutsArrayID, layoutsArray);

            json::to_file(DefaultLayouts::DefaultLayoutsFileName(), root);
            DefaultLayouts::instance().LoadData();

            // test
            auto workArea = WorkArea::Create({}, m_workAreaId, m_emptyUniqueId, m_workAreaRect);
            Assert::IsFalse(workArea == nullptr);
            Assert::IsTrue(m_workAreaId == workArea->UniqueId());

            Assert::IsNotNull(workArea->GetLayout().get());
            
            const auto& actualLayout = workArea->GetLayout();
            Assert::AreEqual(static_cast<int>(FancyZonesDataTypes::ZoneSetLayoutType::Grid), static_cast<int>(actualLayout->Type()));
            Assert::AreEqual(static_cast<size_t>(4), actualLayout->Zones().size());
            Assert::IsTrue(GUID_NULL == actualLayout->Id());
        }

        // Saves a 1x2 custom grid layout to custom-layouts.json so it can be resolved by uuid.
        void SaveCustomGridLayout(const GUID& uuid, bool showSpacing, int spacing, int sensitivityRadius)
        {
            json::JsonObject root{};
            json::JsonArray layoutsArray{};

            json::JsonObject gridLayoutJson{};
            gridLayoutJson.SetNamedValue(NonLocalizable::CustomLayoutsIds::UuidID, json::value(FancyZonesUtils::GuidToString(uuid).value()));
            gridLayoutJson.SetNamedValue(NonLocalizable::CustomLayoutsIds::NameID, json::value(L"Custom grid layout"));
            gridLayoutJson.SetNamedValue(NonLocalizable::CustomLayoutsIds::TypeID, json::value(NonLocalizable::CustomLayoutsIds::GridID));

            json::JsonArray rowsPercentage{};
            rowsPercentage.Append(json::value(10000));

            json::JsonArray columnsPercentage{};
            columnsPercentage.Append(json::value(5000));
            columnsPercentage.Append(json::value(5000));

            json::JsonArray cells{};
            {
                json::JsonArray cellsRow{};
                cellsRow.Append(json::value(0));
                cellsRow.Append(json::value(1));
                cells.Append(cellsRow);
            }

            json::JsonObject info{};
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::RowsID, json::value(1));
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::ColumnsID, json::value(2));
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::RowsPercentageID, rowsPercentage);
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::ColumnsPercentageID, columnsPercentage);
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::CellChildMapID, cells);
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::ShowSpacingID, json::value(showSpacing));
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::SpacingID, json::value(spacing));
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::SensitivityRadiusID, json::value(sensitivityRadius));

            gridLayoutJson.SetNamedValue(NonLocalizable::CustomLayoutsIds::InfoID, info);
            layoutsArray.Append(gridLayoutJson);
            root.SetNamedValue(NonLocalizable::CustomLayoutsIds::CustomLayoutsArrayID, layoutsArray);

            json::to_file(CustomLayouts::CustomLayoutsFileName(), root);
            CustomLayouts::instance().LoadData();
        }

        // Regression test for GH #44058: editing a custom layout's spacing only rewrites
        // custom-layouts.json, while applied-layouts.json keeps a snapshot taken at apply time.
        // An already-created WorkArea must pick up the edited spacing when it is refreshed
        // (WorkArea::InitLayout, which is what the custom-layouts file-update handler calls via
        // RefreshLayouts). Without re-deriving the scalar properties from the current custom
        // layout the stale snapshot is used and the edit has no effect until re-apply.
        TEST_METHOD (EditedCustomLayoutSpacingRefreshesExistingWorkArea)
        {
            const auto uuid = FancyZonesUtils::GuidFromString(L"{5A9D6A0F-4C6E-4C0C-8B1B-9F3E7C2D1A11}").value();

            auto zoneGap = [](const auto& zones) -> LONG {
                std::vector<RECT> rects;
                for (const auto& [id, zone] : zones)
                {
                    rects.push_back(zone.GetZoneRect());
                }
                std::sort(rects.begin(), rects.end(), [](const RECT& a, const RECT& b) { return a.left < b.left; });
                return rects[1].left - rects[0].right;
            };

            // The layout and its applied snapshot initially have no spacing.
            SaveCustomGridLayout(uuid, /*showSpacing*/ false, /*spacing*/ 0, /*sensitivityRadius*/ 5);
            LayoutData snapshot{
                .uuid = uuid,
                .type = FancyZonesDataTypes::ZoneSetLayoutType::Custom,
                .showSpacing = false,
                .spacing = 0,
                .zoneCount = 2,
                .sensitivityRadius = 5,
            };
            AppliedLayouts::instance().ApplyLayout(m_workAreaId, snapshot);

            auto workArea = WorkArea::Create({}, m_workAreaId, m_emptyUniqueId, m_workAreaRect);
            Assert::IsFalse(workArea == nullptr);
            Assert::IsNotNull(workArea->GetLayout().get());
            Assert::AreEqual(static_cast<size_t>(2), workArea->GetLayout()->Zones().size());
            Assert::AreEqual(0L, zoneGap(workArea->GetLayout()->Zones()), L"Zones should be adjacent before the edit");

            // User edits the layout to add spacing; only custom-layouts.json is rewritten, the
            // applied-layouts snapshot stays stale.
            SaveCustomGridLayout(uuid, /*showSpacing*/ true, /*spacing*/ 100, /*sensitivityRadius*/ 30);

            // The custom-layouts file-update handler refreshes active work areas via InitLayout.
            workArea->InitLayout();

            Assert::AreEqual(static_cast<size_t>(2), workArea->GetLayout()->Zones().size());
            Assert::IsTrue(zoneGap(workArea->GetLayout()->Zones()) > 20, L"Edited spacing was not applied to the existing work area");
        }
    };

    TEST_CLASS (WorkAreaSnapUnitTests)
    {
        HINSTANCE m_hInst{};
        const HMONITOR m_monitor = Mocks::Monitor();
        const FancyZonesUtils::Rect m_workAreaRect{ RECT(0, 0, 1920, 1080) };
        const FancyZonesDataTypes::WorkAreaId m_parentUniqueId = {};
        const FancyZonesDataTypes::WorkAreaId m_workAreaId = {
            .monitorId = {
                .monitor = m_monitor,
                .deviceId = {
                    .id = L"device-id-1",
                    .instanceId = L"5&10a58c63&0&UID16777488",
                    .number = 1,
                },
                .serialNumber = L"serial-number-1" },
            .virtualDesktopId = FancyZonesUtils::GuidFromString(L"{310F2924-B587-4D87-97C2-90031BDBE3F1}").value()
        };

        TEST_METHOD_INITIALIZE(Init) noexcept
        {
            AppZoneHistory::instance().LoadData();
        }

        TEST_METHOD_CLEANUP(CleanUp) noexcept
        {
            std::filesystem::remove(AppZoneHistory::AppZoneHistoryFileName());
        }

        TEST_METHOD (WhenWindowIsNotResizablePlacingItIntoTheZoneShouldNotResizeIt)
        {
            LayoutData layout{
                .uuid = FancyZonesUtils::GuidFromString(L"{61FA9FC0-26A6-4B37-A834-491C148DFC58}").value(),
                .type = FancyZonesDataTypes::ZoneSetLayoutType::Grid,
                .showSpacing = false,
                .spacing = 0,
                .zoneCount = 4,
                .sensitivityRadius = 20,
            };
            AppliedLayouts::instance().ApplyLayout(m_workAreaId, layout);

            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst);

            constexpr int originalWidth = 450;
            constexpr int originalHeight = 550;

            SetWindowPos(window, nullptr, 150, 150, originalWidth, originalHeight, SWP_SHOWWINDOW);
            SetWindowLong(window, GWL_STYLE, GetWindowLong(window, GWL_STYLE) & ~WS_SIZEBOX);

            Assert::IsTrue(workArea->Snap(window, { 1 }, true));

            // wait for the window to be resized
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            RECT inZoneRect;
            GetWindowRect(window, &inZoneRect);

            Assert::AreEqual(originalWidth, (int)inZoneRect.right - (int)inZoneRect.left);
            Assert::AreEqual(originalHeight, (int)inZoneRect.bottom - (int)inZoneRect.top);
        }

        TEST_METHOD (WhenWindowIsResizablePlacingItIntoTheZoneShouldResizeIt)
        {
            LayoutData layout{
                .uuid = FancyZonesUtils::GuidFromString(L"{61FA9FC0-26A6-4B37-A834-491C148DFC58}").value(),
                .type = FancyZonesDataTypes::ZoneSetLayoutType::Grid,
                .showSpacing = false,
                .spacing = 0,
                .zoneCount = 4,
                .sensitivityRadius = 20,
            };
            AppliedLayouts::instance().ApplyLayout(m_workAreaId, layout);

            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst, L"", L"", 0, WS_THICKFRAME);

            SetWindowPos(window, nullptr, 150, 150, 450, 550, SWP_SHOWWINDOW);
            
            Assert::IsTrue(workArea->Snap(window, { 1 }, true));

            // wait for the window to be resized
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            RECT zonedWindowRect;
            GetWindowRect(window, &zonedWindowRect);

            RECT zoneRect = workArea->GetLayout()->Zones().at(1).GetZoneRect();

            Assert::AreEqual(zoneRect.left, zonedWindowRect.left);
            Assert::AreEqual(zoneRect.right, zonedWindowRect.right);
            Assert::AreEqual(zoneRect.top, zonedWindowRect.top);
            Assert::AreEqual(zoneRect.bottom, zonedWindowRect.bottom);
        }

        TEST_METHOD (SnapWindowPropertyTest)
        {
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst);

            const ZoneIndexSet expected = { 1, 2 };
            Assert::IsTrue(workArea->Snap(window, expected));

            const auto actual = FancyZonesWindowProperties::RetrieveZoneIndexProperty(window);
            Assert::AreEqual(expected.size(), actual.size());
            for (int i = 0; i < expected.size(); i++)
            {
                Assert::AreEqual(expected.at(i), actual.at(i));
            }
        }

        TEST_METHOD (SnapAppZoneHistoryTest)
        {
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst);

            const ZoneIndexSet expected = { 1, 2 };
            Assert::IsTrue(workArea->Snap(window, expected));

            const auto processPath = get_process_path(window);
            const auto history = AppZoneHistory::instance().GetZoneHistory(processPath, m_workAreaId);

            Assert::IsTrue(history.has_value());
            Assert::AreEqual(expected.size(), history->zoneIndexSet.size());
            for (int i = 0; i < expected.size(); i++)
            {
                Assert::AreEqual(expected.at(i), history->zoneIndexSet.at(i));
            }
        }

        TEST_METHOD (SnapLayoutAssignedWindowsTest)
        {
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst);

            const ZoneIndexSet expected = { 1, 2 };
            Assert::IsTrue(workArea->Snap(window, expected));

            const auto& layoutWindows = workArea->GetLayoutWindows();
            Assert::IsTrue(expected == layoutWindows.GetZoneIndexSetFromWindow(window));
        }

        TEST_METHOD(SnapEmptyZones)
        {
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst);

            Assert::IsFalse(workArea->Snap(window, {}));
        }

        TEST_METHOD(SnapToIncorrectZone)
        {
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst);

            const ZoneIndexSet zones = { 10 };
            Assert::IsFalse(workArea->Snap(window, zones));

            const auto processPath = get_process_path(window);
            const auto history = AppZoneHistory::instance().GetZoneHistory(processPath, m_workAreaId);

            Assert::IsFalse(history.has_value());
        }

        TEST_METHOD (UnsnapPropertyTest)
        {
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst);

            Assert::IsTrue(workArea->Snap(window, { 1, 2 }));
            Assert::IsTrue(workArea->Unsnap(window));

            const auto actual = FancyZonesWindowProperties::RetrieveZoneIndexProperty(window);
            Assert::IsTrue(actual.empty());
        }

        TEST_METHOD (UnsnapAppZoneHistoryTest)
        {
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst);

            Assert::IsTrue(workArea->Snap(window, { 1, 2 }));
            Assert::IsTrue(workArea->Unsnap(window));

            const auto processPath = get_process_path(window);
            const auto history = AppZoneHistory::instance().GetZoneHistory(processPath, m_workAreaId);

            Assert::IsFalse(history.has_value());
        }

        TEST_METHOD (UnsnapLayoutAssignedWindowsTest)
        {
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst);

            Assert::IsTrue(workArea->Snap(window, { 1, 2 }));
            Assert::IsTrue(workArea->Unsnap(window));

            const auto& layoutWindows = workArea->GetLayoutWindows();
            Assert::IsTrue(layoutWindows.GetZoneIndexSetFromWindow(window).empty());
        }
    };

    TEST_CLASS (WorkAreaZonedWindowUnitTests)
    {
        HINSTANCE m_hInst{};
        const HMONITOR m_monitor = Mocks::Monitor();
        const FancyZonesUtils::Rect m_workAreaRect{ RECT(0, 0, 960, 540) };
        const FancyZonesDataTypes::WorkAreaId m_parentUniqueId = {};
        const FancyZonesDataTypes::WorkAreaId m_workAreaId = {
            .monitorId = {
                .monitor = m_monitor,
                .deviceId = {
                    .id = L"stamped-window-device-id",
                    .instanceId = L"5&10a58c63&0&UID16777490",
                    .number = 1,
                },
                .serialNumber = L"stamped-window-serial-number" },
            .virtualDesktopId = FancyZonesUtils::GuidFromString(L"{011DC679-39FC-4ECF-ABAA-08FC4B5B7D39}").value()
        };
        const FancyZonesDataTypes::MonitorId m_otherMonitorId = {
            .monitor = Mocks::Monitor(),
            .deviceId = {
                .id = L"stamped-window-other-device-id",
                .instanceId = L"5&10a58c63&0&UID16777496",
                .number = 2,
            },
            .serialNumber = L"stamped-window-other-serial-number"
        };
        const GUID m_layoutId = FancyZonesUtils::GuidFromString(L"{57895235-47E1-48A4-A2E7-2FF151F40564}").value();
        const GUID m_otherLayoutId = FancyZonesUtils::GuidFromString(L"{6F620499-286C-467A-8241-DB5B006A35A1}").value();
        AppliedLayouts::TAppliedLayoutsMap m_appliedLayouts{};
        std::vector<HWND> m_windows{};

        void ApplyGridLayout(int zoneCount, const GUID& layoutId)
        {
            LayoutData layout{
                .uuid = layoutId,
                .type = FancyZonesDataTypes::ZoneSetLayoutType::Grid,
                .showSpacing = false,
                .spacing = 0,
                .zoneCount = zoneCount,
                .sensitivityRadius = 20,
            };
            AppliedLayouts::instance().ApplyLayout(m_workAreaId, layout);
        }

        void ApplyGridLayout(int zoneCount)
        {
            ApplyGridLayout(zoneCount, m_layoutId);
        }

        void ApplyBlankLayout()
        {
            LayoutData layout{
                .uuid = GUID_NULL,
                .type = FancyZonesDataTypes::ZoneSetLayoutType::Blank,
                .showSpacing = false,
                .spacing = 0,
                .zoneCount = 0,
                .sensitivityRadius = 20,
            };
            AppliedLayouts::instance().ApplyLayout(m_workAreaId, layout);
        }

        HWND CreateResizableWindow()
        {
            const auto window = Mocks::WindowCreate(m_hInst, L"", L"", 0, WS_THICKFRAME);
            m_windows.push_back(window);

            SetWindowPos(window, nullptr, 150, 150, 450, 550, SWP_SHOWWINDOW);
            return window;
        }

        RECT WindowRect(HWND window)
        {
            // windows are moved asynchronously, wait to see a move that isn't expected
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            RECT rect{};
            GetWindowRect(window, &rect);
            return rect;
        }

        bool HasAppZoneHistory(HWND window)
        {
            return AppZoneHistory::instance().GetZoneHistory(get_process_path(window), m_workAreaId).has_value();
        }

        ZoneIndexSet AppZoneHistoryZones(HWND window)
        {
            const auto history = AppZoneHistory::instance().GetZoneHistory(get_process_path(window), m_workAreaId);
            return history.has_value() ? history->zoneIndexSet : ZoneIndexSet{};
        }

        void SaveAppZoneHistory(HWND window, const WorkArea& workArea, const ZoneIndexSet& zones)
        {
            Assert::IsTrue(AppZoneHistory::instance().SetAppLastZones(window, m_workAreaId, workArea.GetLayoutId(), zones));
        }

        HANDLE MonitorOfZoneIndexProperty(HWND window)
        {
            // the name stays on the windows while FancyZones is updated, so it's repeated here and not taken from the library
            return GetPropW(window, L"FancyZones_zones_monitor");
        }

        void StampZoneIndexPropertyFromAnotherMonitor(HWND window, const ZoneIndexSet& zones)
        {
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(window, zones));
            FancyZonesWindowProperties::StampZoneIndexMonitorProperty(window, m_otherMonitorId);
        }

        void UpdateWindowPositions(WorkArea& workArea)
        {
            workArea.UpdateWindowPositions();
        }

        TEST_METHOD_INITIALIZE(Init) noexcept
        {
            m_appliedLayouts = AppliedLayouts::instance().GetAppliedLayoutMap();

            FancyZonesSettings::instance().SetSettings(Settings{});
            std::filesystem::remove(AppZoneHistory::AppZoneHistoryFileName());
            AppZoneHistory::instance().LoadData();
        }

        TEST_METHOD_CLEANUP(CleanUp) noexcept
        {
            for (const auto window : m_windows)
            {
                Mocks::WindowClose(window);
            }

            m_windows.clear();

            AppliedLayouts::instance().SetAppliedLayouts(m_appliedLayouts);

            FancyZonesSettings::instance().SetSettings(Settings{});
            std::filesystem::remove(AppZoneHistory::AppZoneHistoryFileName());
            AppZoneHistory::instance().LoadData();
        }

        TEST_METHOD (UpdateWindowPositionsDismissesClosedWindow)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            Assert::IsTrue(workArea->Snap(window, { 1 }, false));

            Mocks::WindowClose(window);
            Assert::IsFalse(IsWindow(window));
            UpdateWindowPositions(*workArea);

            Assert::IsTrue(workArea->GetLayoutWindows().SnappedWindows().empty());
        }

        TEST_METHOD (UpdateWindowPositionsDismissesClosedWindowWhenLayoutLostZone)
        {
            ApplyGridLayout(3);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            Assert::IsTrue(workArea->Snap(window, { 2 }, false));

            Mocks::WindowClose(window);
            Assert::IsFalse(IsWindow(window));
            ApplyGridLayout(2);
            workArea->InitLayout();
            Assert::AreEqual(static_cast<size_t>(2), workArea->GetLayout()->Zones().size());
            UpdateWindowPositions(*workArea);

            Assert::IsTrue(workArea->GetLayoutWindows().SnappedWindows().empty());
        }

        TEST_METHOD (UpdateWindowPositionsMovesOtherWindowsWhenWindowIsClosed)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window1 = CreateResizableWindow();
            const auto window2 = CreateResizableWindow();

            // windows are positioned in the order of their handles, close the one that would be the first
            const auto closedWindow = std::less<HWND>{}(window1, window2) ? window1 : window2;
            const auto openWindow = closedWindow == window1 ? window2 : window1;

            const ZoneIndexSet zones = { 1 };
            Assert::IsTrue(workArea->Snap(closedWindow, zones, false));
            Assert::IsTrue(workArea->Snap(openWindow, zones, false));

            Mocks::WindowClose(closedWindow);
            Assert::IsFalse(IsWindow(closedWindow));
            UpdateWindowPositions(*workArea);

            const auto zoneRect = workArea->GetLayout()->Zones().at(1).GetZoneRect();
            CustomAssert::AreEqual(zoneRect, Helpers::WaitForWindowRect(openWindow, zoneRect));
            Assert::AreEqual(static_cast<size_t>(1), workArea->GetLayoutWindows().SnappedWindows().size());
            Assert::IsTrue(zones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(openWindow));
        }

        TEST_METHOD (UpdateWindowPositionsMovesSnappedWindow)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            const ZoneIndexSet zones = { 1 };
            Assert::IsTrue(workArea->Snap(window, zones, false));
            AppZoneHistory::instance().SetAppZoneHistory({});

            UpdateWindowPositions(*workArea);

            const auto zoneRect = workArea->GetLayout()->Zones().at(1).GetZoneRect();
            CustomAssert::AreEqual(zoneRect, Helpers::WaitForWindowRect(window, zoneRect));
            Assert::IsTrue(zones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window));
            Assert::IsTrue(HasAppZoneHistory(window));
        }

        TEST_METHOD (UpdateWindowPositionsDoesNotWaitForMinimizedWindows)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);

            const ZoneIndexSet zones = { 1 };
            const std::vector<HWND> windows = {
                CreateResizableWindow(), CreateResizableWindow(), CreateResizableWindow(),
                CreateResizableWindow(), CreateResizableWindow(), CreateResizableWindow()
            };
            for (const auto window : windows)
            {
                Assert::IsTrue(workArea->Snap(window, zones, false));
                ShowWindow(window, SW_SHOWMINNOACTIVE);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            for (const auto window : windows)
            {
                Assert::IsTrue(IsIconic(window));
            }

            const auto start = std::chrono::steady_clock::now();
            UpdateWindowPositions(*workArea);
            const auto duration = std::chrono::steady_clock::now() - start;

            // a wait of 0.5 seconds for each of the minimized windows would take 3 seconds
            Assert::IsTrue(duration < std::chrono::seconds(2));

            // windows are positioned asynchronously, wait to see a window that isn't minimized anymore
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            const auto zoneRect = workArea->GetLayout()->Zones().at(1).GetZoneRect();
            for (const auto window : windows)
            {
                Assert::IsTrue(IsIconic(window));

                ShowWindow(window, SW_RESTORE);
                CustomAssert::AreEqual(zoneRect, Helpers::WaitForWindowRect(window, zoneRect));
            }
        }

        TEST_METHOD (SnapSavesMonitorOfZoneIndexProperty)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            Assert::IsTrue(workArea->Snap(window, { 1 }, false));

            Assert::IsFalse(FancyZonesWindowProperties::IsZoneIndexPropertyFromAnotherMonitor(window, m_workAreaId.monitorId));
            Assert::IsTrue(FancyZonesWindowProperties::IsZoneIndexPropertyFromAnotherMonitor(window, m_otherMonitorId));
            Assert::IsTrue(FancyZonesWindowProperties::IsZoneIndexPropertyFromMonitor(window, m_workAreaId.monitorId));
            Assert::IsFalse(FancyZonesWindowProperties::IsZoneIndexPropertyFromMonitor(window, m_otherMonitorId));
        }

        TEST_METHOD (MonitorOfZoneIndexPropertyIsDeviceId)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            Assert::IsTrue(workArea->Snap(window, { 1 }, false));

            // the same device with another handle, instance id, number and serial number is the same monitor
            auto sameDeviceId = m_otherMonitorId;
            sameDeviceId.deviceId.id = m_workAreaId.monitorId.deviceId.id;
            Assert::IsFalse(FancyZonesWindowProperties::IsZoneIndexPropertyFromAnotherMonitor(window, sameDeviceId));
            Assert::IsTrue(FancyZonesWindowProperties::IsZoneIndexPropertyFromMonitor(window, sameDeviceId));

            // another device with the same handle, instance id, number and serial number is another monitor
            auto otherDeviceId = m_workAreaId.monitorId;
            otherDeviceId.deviceId.id = m_otherMonitorId.deviceId.id;
            Assert::IsTrue(FancyZonesWindowProperties::IsZoneIndexPropertyFromAnotherMonitor(window, otherDeviceId));
            Assert::IsFalse(FancyZonesWindowProperties::IsZoneIndexPropertyFromMonitor(window, otherDeviceId));
        }

        TEST_METHOD (MonitorOfZoneIndexPropertyHasFixedValue)
        {
            const auto window = CreateResizableWindow();
            const FancyZonesDataTypes::MonitorId monitorId = { .deviceId = { .id = L"GSM1388" } };

            FancyZonesWindowProperties::StampZoneIndexMonitorProperty(window, monitorId);

            // the value stays on the windows while FancyZones is updated, so it has to be the same in every version
            Assert::AreEqual(0xC55652918BF74D72ull, reinterpret_cast<uint64_t>(MonitorOfZoneIndexProperty(window)));

            // a device id with a character above 0xFF
            const FancyZonesDataTypes::MonitorId highCharacterMonitorId = { .deviceId = { .id = L"GSM\u01388" } };
            FancyZonesWindowProperties::StampZoneIndexMonitorProperty(window, highCharacterMonitorId);
            Assert::AreEqual(0xAB21E08A1FA46FC6ull, reinterpret_cast<uint64_t>(MonitorOfZoneIndexProperty(window)));

            // the hash of this device id is 0, and 1 is saved in its place
            const FancyZonesDataTypes::MonitorId zeroHashMonitorId = { .deviceId = { .id = std::wstring{ 0xD5, 0x6B, 0xB9, 0x53, 0x42, 0x87, 0x08, 0x36 } } };
            FancyZonesWindowProperties::StampZoneIndexMonitorProperty(window, zeroHashMonitorId);
            Assert::AreEqual(1ull, reinterpret_cast<uint64_t>(MonitorOfZoneIndexProperty(window)));
        }

        TEST_METHOD (UnsnapRemovesMonitorOfZoneIndexProperty)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            Assert::IsTrue(workArea->Snap(window, { 1 }, false));
            Assert::IsNotNull(MonitorOfZoneIndexProperty(window));

            Assert::IsTrue(workArea->Unsnap(window));

            Assert::IsNull(MonitorOfZoneIndexProperty(window));
        }

        TEST_METHOD (StampZoneIndexPropertyRemovesMonitor)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            const ZoneIndexSet zones = { 1 };
            Assert::IsTrue(workArea->Snap(window, zones, false));
            Assert::IsNotNull(MonitorOfZoneIndexProperty(window));

            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(window, zones));

            Assert::IsNull(MonitorOfZoneIndexProperty(window));
        }

        TEST_METHOD (SnapWithSameZonesAddsNoMonitor)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            const ZoneIndexSet zones = { 1 };
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(window, zones));

            // a zone index property that was saved without the monitor isn't from another monitor, whatever the monitor is
            Assert::IsFalse(FancyZonesWindowProperties::IsZoneIndexPropertyFromAnotherMonitor(window, m_workAreaId.monitorId));
            Assert::IsFalse(FancyZonesWindowProperties::IsZoneIndexPropertyFromAnotherMonitor(window, m_otherMonitorId));

            // and it isn't from the monitor that is asked for either
            Assert::IsFalse(FancyZonesWindowProperties::IsZoneIndexPropertyFromMonitor(window, m_workAreaId.monitorId));
            Assert::IsFalse(FancyZonesWindowProperties::IsZoneIndexPropertyFromMonitor(window, m_otherMonitorId));

            Assert::IsTrue(workArea->Snap(window, zones, false));

            Assert::IsNull(MonitorOfZoneIndexProperty(window));
            Assert::IsTrue(zones == FancyZonesWindowProperties::RetrieveZoneIndexProperty(window));
        }

        TEST_METHOD (SnapWithOtherZonesSavesMonitor)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(window, { 1 }));

            Assert::IsTrue(workArea->Snap(window, { 2 }, false));

            Assert::IsFalse(FancyZonesWindowProperties::IsZoneIndexPropertyFromAnotherMonitor(window, m_workAreaId.monitorId));
            Assert::IsTrue(FancyZonesWindowProperties::IsZoneIndexPropertyFromAnotherMonitor(window, m_otherMonitorId));
        }

        TEST_METHOD (SnapWithSameZonesFromAnotherMonitorSavesMonitor)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            const ZoneIndexSet zones = { 1 };
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(window, zones));
            FancyZonesWindowProperties::StampZoneIndexMonitorProperty(window, m_otherMonitorId);
            Assert::IsTrue(FancyZonesWindowProperties::IsZoneIndexPropertyFromAnotherMonitor(window, m_workAreaId.monitorId));

            Assert::IsTrue(workArea->Snap(window, zones, false));

            Assert::IsFalse(FancyZonesWindowProperties::IsZoneIndexPropertyFromAnotherMonitor(window, m_workAreaId.monitorId));
            Assert::IsTrue(FancyZonesWindowProperties::IsZoneIndexPropertyFromAnotherMonitor(window, m_otherMonitorId));
            Assert::IsTrue(zones == FancyZonesWindowProperties::RetrieveZoneIndexProperty(window));
        }

        TEST_METHOD (AssignStampedWindow)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            RECT rect{};
            GetWindowRect(window, &rect);

            const ZoneIndexSet zones = { 1 };
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(window, zones));

            Assert::IsTrue(workArea->AssignStampedWindow(window, zones));

            Assert::IsTrue(zones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window));
            Assert::IsTrue(zones == FancyZonesWindowProperties::RetrieveZoneIndexProperty(window));
            Assert::IsNull(MonitorOfZoneIndexProperty(window));
            Assert::IsFalse(HasAppZoneHistory(window));
            CustomAssert::AreEqual(rect, WindowRect(window));
        }

        TEST_METHOD (AssignStampedWindowToIncorrectZone)
        {
            ApplyGridLayout(2);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            const ZoneIndexSet zones = { 2 };
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(window, zones));

            Assert::IsFalse(workArea->AssignStampedWindow(window, zones));
            Assert::IsFalse(workArea->AssignStampedWindow(window, {}));

            Assert::IsTrue(workArea->GetLayoutWindows().SnappedWindows().empty());
            Assert::IsTrue(zones == FancyZonesWindowProperties::RetrieveZoneIndexProperty(window));
        }

        TEST_METHOD (AssignStampedWindowFromAnotherMonitor)
        {
            // The app has no app zone history on this work area: the window gets the zones with the same numbers,
            // and the last zone of the layout for the numbers that the layout doesn't have.
            struct TestCase
            {
                int zoneCount;
                ZoneIndexSet stampedZones;
                ZoneIndexSet assignedZones;
            };
            const std::vector<TestCase> testCases = {
                { 4, { 1 }, { 1 } },
                { 2, { 3 }, { 1 } },
                { 2, { 0, 3 }, { 0, 1 } },
                { 2, { 2, 3 }, { 1 } },
            };

            for (const auto& testCase : testCases)
            {
                ApplyGridLayout(testCase.zoneCount);
                const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
                const auto window = CreateResizableWindow();

                StampZoneIndexPropertyFromAnotherMonitor(window, testCase.stampedZones);
                const auto monitor = MonitorOfZoneIndexProperty(window);
                Assert::IsNotNull(monitor);

                Assert::IsTrue(workArea->AssignStampedWindow(window, testCase.stampedZones));

                Assert::IsTrue(testCase.assignedZones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window));

                // nothing is saved for the window
                Assert::IsTrue(testCase.stampedZones == FancyZonesWindowProperties::RetrieveZoneIndexProperty(window));
                Assert::IsTrue(monitor == MonitorOfZoneIndexProperty(window));
                Assert::IsFalse(HasAppZoneHistory(window));
            }
        }

        TEST_METHOD (AssignStampedWindowAgainKeepsOrderInZone)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window1 = CreateResizableWindow();
            const auto window2 = CreateResizableWindow();

            const ZoneIndexSet zones = { 1 };
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(window1, zones));
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(window2, zones));
            Assert::IsTrue(workArea->AssignStampedWindow(window1, zones));
            Assert::IsTrue(workArea->AssignStampedWindow(window2, zones));

            const auto sortKey = FancyZonesWindowProperties::GetTabSortKeyWithinZone(window1);
            Assert::IsTrue(sortKey.has_value());

            Assert::IsTrue(workArea->AssignStampedWindow(window1, zones));

            // the window list isn't changed: LayoutAssignedWindows::Assign would move the window to the end of its zone
            Assert::IsTrue(sortKey == FancyZonesWindowProperties::GetTabSortKeyWithinZone(window1));
        }

        TEST_METHOD (SnapAfterAssignStampedWindow)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            const ZoneIndexSet zones = { 1 };
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(window, zones));
            Assert::IsTrue(workArea->AssignStampedWindow(window, zones));

            Assert::IsTrue(workArea->Snap(window, zones, false));

            // the window is a snapped window from now on, its app zone history is saved again
            AppZoneHistory::instance().SetAppZoneHistory({});
            UpdateWindowPositions(*workArea);

            Assert::IsTrue(HasAppZoneHistory(window));
        }

        TEST_METHOD (UnsnapAfterAssignStampedWindow)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            const ZoneIndexSet zones = { 1 };
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(window, zones));
            Assert::IsTrue(workArea->AssignStampedWindow(window, zones));
            Assert::IsTrue(zones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window));

            Assert::IsTrue(workArea->Unsnap(window));

            Assert::IsTrue(workArea->GetLayoutWindows().SnappedWindows().empty());
            Assert::IsTrue(FancyZonesWindowProperties::RetrieveZoneIndexProperty(window).empty());
        }

        TEST_METHOD (DismissStampedWindow)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto stampedWindow = CreateResizableWindow();
            const auto snappedWindow = CreateResizableWindow();

            const ZoneIndexSet zones = { 1 };
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(stampedWindow, zones));
            Assert::IsTrue(workArea->AssignStampedWindow(stampedWindow, zones));
            Assert::IsTrue(workArea->Snap(snappedWindow, zones, false));

            workArea->DismissStampedWindow(stampedWindow);
            workArea->DismissStampedWindow(snappedWindow);

            Assert::IsTrue(workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(stampedWindow).empty());
            Assert::IsTrue(zones == FancyZonesWindowProperties::RetrieveZoneIndexProperty(stampedWindow));
            Assert::IsTrue(zones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(snappedWindow));
        }

        TEST_METHOD (UpdateWindowPositionsMovesStampedWindow)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            const ZoneIndexSet zones = { 1 };
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(window, zones));
            Assert::IsTrue(workArea->AssignStampedWindow(window, zones));

            UpdateWindowPositions(*workArea);

            const auto zoneRect = workArea->GetLayout()->Zones().at(1).GetZoneRect();
            CustomAssert::AreEqual(zoneRect, Helpers::WaitForWindowRect(window, zoneRect));
            Assert::IsTrue(zones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window));
            Assert::IsTrue(zones == FancyZonesWindowProperties::RetrieveZoneIndexProperty(window));
            Assert::IsFalse(HasAppZoneHistory(window));
        }

        TEST_METHOD (UpdateWindowPositionsDismissesClosedStampedWindow)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            const ZoneIndexSet zones = { 1 };
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(window, zones));
            Assert::IsTrue(workArea->AssignStampedWindow(window, zones));

            Mocks::WindowClose(window);
            Assert::IsFalse(IsWindow(window));
            UpdateWindowPositions(*workArea);

            Assert::IsTrue(workArea->GetLayoutWindows().SnappedWindows().empty());
        }

        TEST_METHOD (UpdateWindowPositionsDismissesStampedWindowWithoutZoneIndexProperty)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            RECT rect{};
            GetWindowRect(window, &rect);

            const ZoneIndexSet zones = { 1 };
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(window, zones));
            Assert::IsTrue(workArea->AssignStampedWindow(window, zones));

            FancyZonesWindowProperties::RemoveZoneIndexProperty(window);
            UpdateWindowPositions(*workArea);

            Assert::IsTrue(workArea->GetLayoutWindows().SnappedWindows().empty());
            CustomAssert::AreEqual(rect, WindowRect(window));
        }

        TEST_METHOD (UpdateWindowPositionsDismissesStampedWindowWhenLayoutLostZone)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            RECT rect{};
            GetWindowRect(window, &rect);

            const ZoneIndexSet zones = { 3 };
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(window, zones));
            Assert::IsTrue(workArea->AssignStampedWindow(window, zones));

            ApplyGridLayout(2);
            workArea->InitLayout();
            Assert::AreEqual(static_cast<size_t>(2), workArea->GetLayout()->Zones().size());
            UpdateWindowPositions(*workArea);

            Assert::IsTrue(workArea->GetLayoutWindows().SnappedWindows().empty());
            Assert::IsTrue(zones == FancyZonesWindowProperties::RetrieveZoneIndexProperty(window));
            CustomAssert::AreEqual(rect, WindowRect(window));
        }

        TEST_METHOD (UpdateWindowPositionsKeepsWindowsWhenLayoutHasNoZones)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto snappedWindow = CreateResizableWindow();
            const auto stampedWindow = CreateResizableWindow();

            RECT snappedWindowRect{};
            GetWindowRect(snappedWindow, &snappedWindowRect);
            RECT stampedWindowRect{};
            GetWindowRect(stampedWindow, &stampedWindowRect);

            const ZoneIndexSet snappedZones = { 0 };
            const ZoneIndexSet stampedZones = { 1 };
            Assert::IsTrue(workArea->Snap(snappedWindow, snappedZones, false));
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(stampedWindow, stampedZones));
            Assert::IsTrue(workArea->AssignStampedWindow(stampedWindow, stampedZones));

            ApplyBlankLayout();
            workArea->InitLayout();
            Assert::IsTrue(workArea->GetLayout()->Zones().empty());
            UpdateWindowPositions(*workArea);

            Assert::IsTrue(snappedZones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(snappedWindow));
            Assert::IsTrue(stampedZones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(stampedWindow));
            CustomAssert::AreEqual(snappedWindowRect, WindowRect(snappedWindow));
            CustomAssert::AreEqual(stampedWindowRect, WindowRect(stampedWindow));

            // the windows are placed again by the next layout that has zones
            ApplyGridLayout(4);
            workArea->InitLayout();
            UpdateWindowPositions(*workArea);

            const auto snappedZoneRect = workArea->GetLayout()->Zones().at(0).GetZoneRect();
            const auto stampedZoneRect = workArea->GetLayout()->Zones().at(1).GetZoneRect();
            CustomAssert::AreEqual(snappedZoneRect, Helpers::WaitForWindowRect(snappedWindow, snappedZoneRect));
            CustomAssert::AreEqual(stampedZoneRect, Helpers::WaitForWindowRect(stampedWindow, stampedZoneRect));
        }

        TEST_METHOD (AssignStampedWindowFromAnotherMonitorWithAppZoneHistory)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            const ZoneIndexSet stampedZones = { 0 };
            const ZoneIndexSet appZoneHistoryZones = { 1 };
            SaveAppZoneHistory(window, *workArea, appZoneHistoryZones);
            StampZoneIndexPropertyFromAnotherMonitor(window, stampedZones);
            const auto monitor = MonitorOfZoneIndexProperty(window);
            Assert::IsNotNull(monitor);

            Assert::IsTrue(workArea->AssignStampedWindow(window, stampedZones));

            // the window gets the zones that its app was in last on this work area, and nothing is saved for it
            Assert::IsTrue(appZoneHistoryZones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window));
            Assert::IsTrue(stampedZones == FancyZonesWindowProperties::RetrieveZoneIndexProperty(window));
            Assert::IsTrue(monitor == MonitorOfZoneIndexProperty(window));
            Assert::IsTrue(appZoneHistoryZones == AppZoneHistoryZones(window));
        }

        TEST_METHOD (AssignStampedWindowFromAnotherMonitorWithAppZoneHistoryForMissingZone)
        {
            ApplyGridLayout(2);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            // the app zone history was saved when the layout had more zones
            const ZoneIndexSet stampedZones = { 0 };
            SaveAppZoneHistory(window, *workArea, { 3 });
            StampZoneIndexPropertyFromAnotherMonitor(window, stampedZones);

            Assert::IsTrue(workArea->AssignStampedWindow(window, stampedZones));

            Assert::IsTrue(stampedZones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window));
        }

        TEST_METHOD (AssignStampedWindowFromAnotherMonitorWithAppZoneHistoryOfAnotherLayout)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            const ZoneIndexSet stampedZones = { 0 };
            const ZoneIndexSet appZoneHistoryZones = { 2 };
            SaveAppZoneHistory(window, *workArea, appZoneHistoryZones);
            StampZoneIndexPropertyFromAnotherMonitor(window, stampedZones);

            // another layout that has the zone of the app zone history
            ApplyGridLayout(3, m_otherLayoutId);
            workArea->InitLayout();
            Assert::AreEqual(static_cast<size_t>(3), workArea->GetLayout()->Zones().size());

            Assert::IsTrue(workArea->AssignStampedWindow(window, stampedZones));

            // the window gets the zones that its app was in last on this work area, also when another layout was applied then
            Assert::IsTrue(appZoneHistoryZones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window));
        }

        TEST_METHOD (AssignStampedWindowFromAnotherMonitorWithAppZoneHistoryOfAnotherLayoutForMissingZone)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            const ZoneIndexSet stampedZones = { 0 };
            SaveAppZoneHistory(window, *workArea, { 2 });
            StampZoneIndexPropertyFromAnotherMonitor(window, stampedZones);

            // another layout that doesn't have the zone of the app zone history
            ApplyGridLayout(2, m_otherLayoutId);
            workArea->InitLayout();
            Assert::AreEqual(static_cast<size_t>(2), workArea->GetLayout()->Zones().size());

            Assert::IsTrue(workArea->AssignStampedWindow(window, stampedZones));

            Assert::IsTrue(stampedZones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window));
        }

        TEST_METHOD (AssignStampedWindowFromAnotherMonitorWithAppZoneHistoryOfSeveralLayouts)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            // the app has two entries for this work area, and the entry of the current layout isn't the first one
            const ZoneIndexSet stampedZones = { 2 };
            const ZoneIndexSet appZoneHistoryZones = { 1 };
            AppZoneHistory::TAppZoneHistoryMap history{};
            history.insert({ get_process_path(window), std::vector<FancyZonesDataTypes::AppZoneHistoryData>{
                FancyZonesDataTypes::AppZoneHistoryData{ .layoutId = m_otherLayoutId, .workAreaId = m_workAreaId, .zoneIndexSet = { 0 } },
                FancyZonesDataTypes::AppZoneHistoryData{ .layoutId = m_layoutId, .workAreaId = m_workAreaId, .zoneIndexSet = appZoneHistoryZones },
            } });
            AppZoneHistory::instance().SetAppZoneHistory(history);
            StampZoneIndexPropertyFromAnotherMonitor(window, stampedZones);

            Assert::IsTrue(workArea->AssignStampedWindow(window, stampedZones));

            // the window gets the zones that were saved for the current layout
            Assert::IsTrue(appZoneHistoryZones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window));
        }

        TEST_METHOD (AssignStampedWindowFromThisMonitorIgnoresAppZoneHistory)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto windowWithMonitor = CreateResizableWindow();
            const auto windowWithoutMonitor = CreateResizableWindow();

            const ZoneIndexSet zones = { 1 };
            SaveAppZoneHistory(windowWithMonitor, *workArea, { 0 });
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(windowWithMonitor, zones));
            FancyZonesWindowProperties::StampZoneIndexMonitorProperty(windowWithMonitor, m_workAreaId.monitorId);
            Assert::IsNotNull(MonitorOfZoneIndexProperty(windowWithMonitor));
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(windowWithoutMonitor, zones));

            Assert::IsTrue(workArea->AssignStampedWindow(windowWithMonitor, zones));
            Assert::IsTrue(workArea->AssignStampedWindow(windowWithoutMonitor, zones));

            // zone numbers that were saved on this monitor, or without the monitor, are used as they are
            Assert::IsTrue(zones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(windowWithMonitor));
            Assert::IsTrue(zones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(windowWithoutMonitor));
        }

        TEST_METHOD (UpdateWindowPositionsKeepsZonesWhenAppZoneHistoryChanges)
        {
            ApplyGridLayout(4);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            const ZoneIndexSet stampedZones = { 2 };
            const ZoneIndexSet zones = { 1 };
            SaveAppZoneHistory(window, *workArea, zones);
            StampZoneIndexPropertyFromAnotherMonitor(window, stampedZones);
            Assert::IsTrue(workArea->AssignStampedWindow(window, stampedZones));
            Assert::IsTrue(zones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window));

            SaveAppZoneHistory(window, *workArea, { 0 });
            UpdateWindowPositions(*workArea);

            // the window keeps the zones that it has on this work area
            const auto zoneRect = workArea->GetLayout()->Zones().at(1).GetZoneRect();
            Assert::IsTrue(zones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window));
            CustomAssert::AreEqual(zoneRect, Helpers::WaitForWindowRect(window, zoneRect));
        }

        TEST_METHOD (UpdateWindowPositionsKeepsZonesOfWindowFromAnotherMonitorOnLayoutChange)
        {
            ApplyGridLayout(2);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            // the layout doesn't have zone 3, the window gets the last zone of the layout
            const ZoneIndexSet stampedZones = { 3 };
            const ZoneIndexSet zones = { 1 };
            StampZoneIndexPropertyFromAnotherMonitor(window, stampedZones);
            Assert::IsTrue(workArea->AssignStampedWindow(window, stampedZones));
            Assert::IsTrue(zones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window));

            // a layout that has zone 3: the window keeps the zone number that it has on this work area
            ApplyGridLayout(4);
            workArea->InitLayout();
            Assert::AreEqual(static_cast<size_t>(4), workArea->GetLayout()->Zones().size());
            UpdateWindowPositions(*workArea);

            const auto zoneRect = workArea->GetLayout()->Zones().at(1).GetZoneRect();
            Assert::IsTrue(zones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window));
            CustomAssert::AreEqual(zoneRect, Helpers::WaitForWindowRect(window, zoneRect));

            // a layout with one zone
            ApplyGridLayout(1);
            workArea->InitLayout();
            Assert::AreEqual(static_cast<size_t>(1), workArea->GetLayout()->Zones().size());
            UpdateWindowPositions(*workArea);

            const auto onlyZoneRect = workArea->GetLayout()->Zones().at(0).GetZoneRect();
            Assert::IsTrue(ZoneIndexSet{ 0 } == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window));
            CustomAssert::AreEqual(onlyZoneRect, Helpers::WaitForWindowRect(window, onlyZoneRect));
        }

        TEST_METHOD (UpdateWindowPositionsMovesWindowFromAnotherMonitorToLastZoneOfLayout)
        {
            ApplyGridLayout(3);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = CreateResizableWindow();

            // the window gets zone 2 on this work area, the zone that its app was in last
            const ZoneIndexSet stampedZones = { 0 };
            SaveAppZoneHistory(window, *workArea, { 2 });
            StampZoneIndexPropertyFromAnotherMonitor(window, stampedZones);
            Assert::IsTrue(workArea->AssignStampedWindow(window, stampedZones));
            Assert::IsTrue(ZoneIndexSet{ 2 } == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window));

            // after that the app zone history changes to the zone number of the zone index property
            SaveAppZoneHistory(window, *workArea, stampedZones);

            // Another layout, without zone 2. The window gets the last zone of the layout in place of the zone that it has
            // on this work area, and not the zones of the app zone history of the first layout or of its zone index property.
            ApplyGridLayout(2, m_otherLayoutId);
            workArea->InitLayout();
            Assert::AreEqual(static_cast<size_t>(2), workArea->GetLayout()->Zones().size());
            UpdateWindowPositions(*workArea);

            const auto zoneRect = workArea->GetLayout()->Zones().at(1).GetZoneRect();
            Assert::IsTrue(ZoneIndexSet{ 1 } == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window));
            CustomAssert::AreEqual(zoneRect, Helpers::WaitForWindowRect(window, zoneRect));
        }

        TEST_METHOD (UpdateWindowPositionsSnappedWindowsFirst)
        {
            ApplyGridLayout(3);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window1 = CreateResizableWindow();
            const auto window2 = CreateResizableWindow();

            // the window list is in the order of the handles, the window from another monitor is the first one in it
            const auto stampedWindow = std::less<HWND>{}(window1, window2) ? window1 : window2;
            const auto snappedWindow = stampedWindow == window1 ? window2 : window1;

            // the window from another monitor gets the zones that its app was in last, which the snapped window saved
            const ZoneIndexSet stampedZones = { 1 };
            Assert::IsTrue(workArea->Snap(snappedWindow, { 2 }, false));
            StampZoneIndexPropertyFromAnotherMonitor(stampedWindow, stampedZones);
            Assert::IsTrue(workArea->AssignStampedWindow(stampedWindow, stampedZones));
            Assert::IsTrue(ZoneIndexSet{ 2 } == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(stampedWindow));

            const ZoneIndexSet snappedZones = { 0 };
            Assert::IsTrue(workArea->Snap(snappedWindow, snappedZones, false));

            // Another layout, without zone 2. The app zone history of that layout is saved when the snapped window is snapped again,
            // and the window from another monitor gets these zones in the same update.
            ApplyGridLayout(2, m_otherLayoutId);
            workArea->InitLayout();
            Assert::AreEqual(static_cast<size_t>(2), workArea->GetLayout()->Zones().size());
            UpdateWindowPositions(*workArea);

            Assert::IsTrue(snappedZones == workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(stampedWindow));
        }

        TEST_METHOD (UpdateWindowPositionsKeepsOrderOfWindowsFromAnotherMonitor)
        {
            ApplyGridLayout(2);
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window1 = CreateResizableWindow();
            const auto window2 = CreateResizableWindow();

            // both windows get the last zone of the layout, which isn't the zone of their zone index property
            const ZoneIndexSet stampedZones = { 3 };
            StampZoneIndexPropertyFromAnotherMonitor(window1, stampedZones);
            StampZoneIndexPropertyFromAnotherMonitor(window2, stampedZones);
            Assert::IsTrue(workArea->AssignStampedWindow(window1, stampedZones));
            Assert::IsTrue(workArea->AssignStampedWindow(window2, stampedZones));

            const auto sortKey1 = FancyZonesWindowProperties::GetTabSortKeyWithinZone(window1);
            const auto sortKey2 = FancyZonesWindowProperties::GetTabSortKeyWithinZone(window2);
            Assert::IsTrue(sortKey1.has_value());
            Assert::IsTrue(sortKey2.has_value());

            UpdateWindowPositions(*workArea);

            // assigning the window again would move it to the end of its zone
            Assert::IsTrue(sortKey1 == FancyZonesWindowProperties::GetTabSortKeyWithinZone(window1));
            Assert::IsTrue(sortKey2 == FancyZonesWindowProperties::GetTabSortKeyWithinZone(window2));
        }
    };

    TEST_CLASS (WorkAreaConfigurationUnitTests)
    {
        HINSTANCE m_hInst{};
        const std::vector<HMONITOR> m_monitors = { Mocks::Monitor(), Mocks::Monitor() };
        const FancyZonesUtils::Rect m_workAreaRect{ RECT(0, 0, 960, 540) };
        const GUID m_virtualDesktopId = FancyZonesUtils::GuidFromString(L"{0E7B3476-0A22-4F6E-9113-C687AD503E4B}").value();
        const std::vector<FancyZonesDataTypes::WorkAreaId> m_workAreaIds = {
            FancyZonesDataTypes::WorkAreaId{
                .monitorId = {
                    .monitor = m_monitors[0],
                    .deviceId = {
                        .id = L"stamped-windows-device-id-1",
                        .instanceId = L"5&10a58c63&0&UID16777491",
                        .number = 1,
                    },
                    .serialNumber = L"stamped-windows-serial-number-1" },
                .virtualDesktopId = m_virtualDesktopId },
            FancyZonesDataTypes::WorkAreaId{
                .monitorId = {
                    .monitor = m_monitors[1],
                    .deviceId = {
                        .id = L"stamped-windows-device-id-2",
                        .instanceId = L"5&10a58c63&0&UID16777492",
                        .number = 2,
                    },
                    .serialNumber = L"stamped-windows-serial-number-2" },
                .virtualDesktopId = m_virtualDesktopId }
        };
        const ZoneIndexSet m_zones = { 1 };

        WorkAreaConfiguration m_workAreaConfiguration{};
        AppliedLayouts::TAppliedLayoutsMap m_appliedLayouts{};
        std::vector<HWND> m_windows{};

        void ApplyGridLayout(const FancyZonesDataTypes::WorkAreaId& workAreaId, int zoneCount)
        {
            // one layout id, so that app zone history saved before the zone count changes is still found
            LayoutData layout{
                .uuid = FancyZonesUtils::GuidFromString(L"{CC199322-7BE7-4B91-9685-7B47CFC6796B}").value(),
                .type = FancyZonesDataTypes::ZoneSetLayoutType::Grid,
                .showSpacing = false,
                .spacing = 0,
                .zoneCount = zoneCount,
                .sensitivityRadius = 20,
            };
            AppliedLayouts::instance().ApplyLayout(workAreaId, layout);
        }

        void ApplyBlankLayout(const FancyZonesDataTypes::WorkAreaId& workAreaId)
        {
            LayoutData layout{
                .uuid = GUID_NULL,
                .type = FancyZonesDataTypes::ZoneSetLayoutType::Blank,
                .showSpacing = false,
                .spacing = 0,
                .zoneCount = 0,
                .sensitivityRadius = 20,
            };
            AppliedLayouts::instance().ApplyLayout(workAreaId, layout);
        }

        void AddWorkArea(WorkAreaConfiguration& workAreaConfiguration, HMONITOR monitor, const FancyZonesDataTypes::WorkAreaId& workAreaId)
        {
            ApplyGridLayout(workAreaId, 4);

            workAreaConfiguration.AddWorkArea(monitor, WorkArea::Create(m_hInst, workAreaId, {}, m_workAreaRect));
        }

        // The work areas are created again for one monitor, as after a display change that replaced the monitor.
        void ReplaceMonitor(size_t workAreaIndex, int zoneCount)
        {
            m_workAreaConfiguration.Clear();

            ApplyGridLayout(m_workAreaIds[workAreaIndex], zoneCount);
            m_workAreaConfiguration.AddWorkArea(m_monitors[workAreaIndex], WorkArea::Create(m_hInst, m_workAreaIds[workAreaIndex], {}, m_workAreaRect));
        }

        HWND CreateResizableWindow(const std::wstring& title)
        {
            const auto window = Mocks::WindowCreate(m_hInst, title, L"", 0, WS_THICKFRAME);
            m_windows.push_back(window);

            SetWindowPos(window, nullptr, 150, 150, 450, 550, SWP_SHOWWINDOW);
            return window;
        }

        HWND CreateStampedWindow(const ZoneIndexSet& zones, const std::wstring& title)
        {
            const auto window = CreateResizableWindow(title);
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(window, zones));
            return window;
        }

        HWND CreateStampedWindow(const ZoneIndexSet& zones)
        {
            return CreateStampedWindow(zones, L"");
        }

        // A window that is snapped without having the zone index property gets it together with the monitor of the work area.
        HWND CreateSnappedWindow(size_t workAreaIndex, const ZoneIndexSet& zones)
        {
            const auto window = CreateResizableWindow(L"");
            Assert::IsTrue(m_workAreaConfiguration.GetWorkArea(m_monitors[workAreaIndex])->Snap(window, zones, false));
            return window;
        }

        RECT WindowRect(HWND window)
        {
            // windows are moved asynchronously, wait to see a move that isn't expected
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            RECT rect{};
            GetWindowRect(window, &rect);
            return rect;
        }

        void SaveAppZoneHistory(HWND window, size_t workAreaIndex, const ZoneIndexSet& zones)
        {
            const auto workArea = m_workAreaConfiguration.GetWorkArea(m_monitors[workAreaIndex]);
            Assert::IsTrue(AppZoneHistory::instance().SetAppLastZones(window, workArea->UniqueId(), workArea->GetLayoutId(), zones));
        }

        ZoneIndexSet AssignedZones(HWND window, size_t workAreaIndex)
        {
            const auto workArea = m_workAreaConfiguration.GetWorkArea(m_monitors[workAreaIndex]);
            return workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window);
        }

        ZoneIndexSet AppZoneHistoryZones(HWND window, size_t workAreaIndex)
        {
            const auto history = AppZoneHistory::instance().GetZoneHistory(get_process_path(window), m_workAreaIds[workAreaIndex]);
            return history.has_value() ? history->zoneIndexSet : ZoneIndexSet{};
        }

        bool IsSnapped(HWND window, size_t workAreaIndex)
        {
            // the app zone history is saved again only for a window that is snapped
            const auto workArea = m_workAreaConfiguration.GetWorkArea(m_monitors[workAreaIndex]);
            AppZoneHistory::instance().SetAppZoneHistory({});
            workArea->UpdateWindowPositions();
            return AppZoneHistory::instance().GetZoneHistory(get_process_path(window), workArea->UniqueId()).has_value();
        }

        TEST_METHOD_INITIALIZE(Init) noexcept
        {
            m_appliedLayouts = AppliedLayouts::instance().GetAppliedLayoutMap();

            FancyZonesSettings::instance().SetSettings(Settings{});
            std::filesystem::remove(AppZoneHistory::AppZoneHistoryFileName());
            AppZoneHistory::instance().LoadData();

            AddWorkArea(m_workAreaConfiguration, m_monitors[0], m_workAreaIds[0]);
            AddWorkArea(m_workAreaConfiguration, m_monitors[1], m_workAreaIds[1]);
        }

        TEST_METHOD_CLEANUP(CleanUp) noexcept
        {
            m_workAreaConfiguration.Clear();

            for (const auto window : m_windows)
            {
                Mocks::WindowClose(window);
            }

            m_windows.clear();

            AppliedLayouts::instance().SetAppliedLayouts(m_appliedLayouts);

            FancyZonesSettings::instance().SetSettings(Settings{});
            std::filesystem::remove(AppZoneHistory::AppZoneHistoryFileName());
            AppZoneHistory::instance().LoadData();
        }

        TEST_METHOD (UpdateStampedWindowsAppZoneHistoryOnWindowMonitor)
        {
            const auto window = CreateStampedWindow(m_zones);
            SaveAppZoneHistory(window, 0, m_zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[0] } }, true, false);

            Assert::IsTrue(m_zones == AssignedZones(window, 0));
            Assert::IsTrue(AssignedZones(window, 1).empty());
            Assert::IsTrue(IsSnapped(window, 0));
        }

        TEST_METHOD (UpdateStampedWindowsAppZoneHistoryOnOtherMonitor)
        {
            const auto window = CreateStampedWindow(m_zones);
            SaveAppZoneHistory(window, 1, m_zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[0] } }, true, false);

            Assert::IsTrue(AssignedZones(window, 0).empty());
            Assert::IsTrue(m_zones == AssignedZones(window, 1));
            Assert::IsTrue(IsSnapped(window, 1));
        }

        TEST_METHOD (UpdateStampedWindowsAppZoneHistoryOnBothWorkAreasWindowOnFirst)
        {
            const auto window = CreateStampedWindow(m_zones);
            SaveAppZoneHistory(window, 0, m_zones);
            SaveAppZoneHistory(window, 1, m_zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[0] } }, true, false);

            Assert::IsTrue(m_zones == AssignedZones(window, 0));
            Assert::IsTrue(AssignedZones(window, 1).empty());
        }

        TEST_METHOD (UpdateStampedWindowsAppZoneHistoryOnBothWorkAreasWindowOnSecond)
        {
            const auto window = CreateStampedWindow(m_zones);
            SaveAppZoneHistory(window, 0, m_zones);
            SaveAppZoneHistory(window, 1, m_zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[1] } }, true, false);

            Assert::IsTrue(AssignedZones(window, 0).empty());
            Assert::IsTrue(m_zones == AssignedZones(window, 1));
        }

        TEST_METHOD (UpdateStampedWindowsNoAppZoneHistory)
        {
            const auto window = CreateStampedWindow(m_zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[0] } }, true, false);

            // the window is assigned by its zone index property on the monitor where it's placed
            Assert::IsTrue(m_zones == AssignedZones(window, 0));
            Assert::IsTrue(AssignedZones(window, 1).empty());
            Assert::IsTrue(m_zones == FancyZonesWindowProperties::RetrieveZoneIndexProperty(window));
            Assert::IsTrue(AppZoneHistory::instance().GetFullAppZoneHistory().empty());
            Assert::IsFalse(IsSnapped(window, 0));
        }

        TEST_METHOD (UpdateStampedWindowsOtherZonesInAppZoneHistory)
        {
            const ZoneIndexSet appZoneHistoryZones = { 0 };
            const auto window = CreateStampedWindow(m_zones);
            SaveAppZoneHistory(window, 0, appZoneHistoryZones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[0] } }, true, false);

            Assert::IsTrue(m_zones == AssignedZones(window, 0));
            Assert::IsTrue(AssignedZones(window, 1).empty());
            Assert::IsTrue(appZoneHistoryZones == AppZoneHistoryZones(window, 0));
        }

        TEST_METHOD (UpdateStampedWindowsAppZoneHistoryForMissingZoneOnFirst)
        {
            const ZoneIndexSet zones = { 3 };
            const auto window = CreateStampedWindow(zones);
            SaveAppZoneHistory(window, 0, zones);
            SaveAppZoneHistory(window, 1, zones);

            // the layout of the first work area doesn't have the zone anymore
            ApplyGridLayout(m_workAreaIds[0], 2);
            m_workAreaConfiguration.GetWorkArea(m_monitors[0])->InitLayout();
            Assert::AreEqual(static_cast<size_t>(2), m_workAreaConfiguration.GetWorkArea(m_monitors[0])->GetLayout()->Zones().size());

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = zones, .monitor = Mocks::Monitor() } }, true, false);

            Assert::IsTrue(AssignedZones(window, 0).empty());
            Assert::IsTrue(zones == AssignedZones(window, 1));
        }

        TEST_METHOD (UpdateStampedWindowsAppZoneHistoryForMissingZoneOnSecond)
        {
            const ZoneIndexSet zones = { 3 };
            const auto window = CreateStampedWindow(zones);
            SaveAppZoneHistory(window, 0, zones);
            SaveAppZoneHistory(window, 1, zones);

            // the layout of the second work area doesn't have the zone anymore
            ApplyGridLayout(m_workAreaIds[1], 2);
            m_workAreaConfiguration.GetWorkArea(m_monitors[1])->InitLayout();
            Assert::AreEqual(static_cast<size_t>(2), m_workAreaConfiguration.GetWorkArea(m_monitors[1])->GetLayout()->Zones().size());

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = zones, .monitor = Mocks::Monitor() } }, true, false);

            Assert::IsTrue(zones == AssignedZones(window, 0));
            Assert::IsTrue(AssignedZones(window, 1).empty());
        }

        TEST_METHOD (UpdateStampedWindowsAppZoneHistoryOnSeveralWorkAreas)
        {
            const auto window = CreateStampedWindow(m_zones);
            SaveAppZoneHistory(window, 0, m_zones);
            SaveAppZoneHistory(window, 1, m_zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = Mocks::Monitor() } }, true, false);

            // the window is snapped on one work area only
            Assert::IsTrue(AssignedZones(window, 0).empty() != AssignedZones(window, 1).empty());
        }

        TEST_METHOD (UpdateStampedWindowsAlreadyAssignedWindow)
        {
            const auto window = CreateStampedWindow(m_zones);
            Assert::IsTrue(m_workAreaConfiguration.GetWorkArea(m_monitors[0])->Snap(window, m_zones, false));
            SaveAppZoneHistory(window, 1, m_zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[1] } }, true, false);

            Assert::IsTrue(m_zones == AssignedZones(window, 0));
            Assert::IsTrue(AssignedZones(window, 1).empty());
        }

        TEST_METHOD (UpdateStampedWindowsKeepsOrderOfAssignedWindows)
        {
            const auto workArea = m_workAreaConfiguration.GetWorkArea(m_monitors[0]);
            const auto window1 = CreateStampedWindow(m_zones);
            const auto window2 = CreateStampedWindow(m_zones);
            Assert::IsTrue(workArea->Snap(window1, m_zones, false));
            Assert::IsTrue(workArea->Snap(window2, m_zones, false));

            const auto sortKey = FancyZonesWindowProperties::GetTabSortKeyWithinZone(window1);
            Assert::IsTrue(sortKey.has_value());

            const std::vector<StampedWindow> windows = { StampedWindow{ .window = window1, .zones = m_zones, .monitor = m_monitors[0] },
                                                         StampedWindow{ .window = window2, .zones = m_zones, .monitor = m_monitors[0] } };
            m_workAreaConfiguration.UpdateStampedWindows(windows, true, false);

            // snapping the window again would move it to the end of its zone
            Assert::IsTrue(sortKey == FancyZonesWindowProperties::GetTabSortKeyWithinZone(window1));
        }

        TEST_METHOD (UpdateStampedWindowsWindowOutsideOfMonitors)
        {
            const auto window = CreateStampedWindow(m_zones);
            SaveAppZoneHistory(window, 1, m_zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = nullptr } }, true, false);

            Assert::IsTrue(AssignedZones(window, 0).empty());
            Assert::IsTrue(m_zones == AssignedZones(window, 1));
        }

        TEST_METHOD (UpdateStampedWindowsOneWorkAreaAcrossMonitors)
        {
            const FancyZonesDataTypes::WorkAreaId workAreaId = {
                .monitorId = { .deviceId = { .id = L"stamped-windows-device-id-all", .instanceId = L"5&10a58c63&0&UID16777493" } },
                .virtualDesktopId = m_virtualDesktopId
            };
            WorkAreaConfiguration workAreaConfiguration;
            AddWorkArea(workAreaConfiguration, nullptr, workAreaId);

            const auto window = CreateStampedWindow(m_zones);
            const std::vector<StampedWindow> windows = { StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[0] } };

            // the window is snapped without app zone history
            workAreaConfiguration.UpdateStampedWindows(windows, true, false);

            Assert::IsTrue(m_zones == workAreaConfiguration.GetWorkArea(nullptr)->GetLayoutWindows().GetZoneIndexSetFromWindow(window));
            Assert::IsTrue(AppZoneHistory::instance().GetZoneHistory(get_process_path(window), workAreaId).has_value());

            // the window is snapped again when it's snapped already
            AppZoneHistory::instance().SetAppZoneHistory({});
            workAreaConfiguration.UpdateStampedWindows(windows, true, false);

            Assert::IsTrue(AppZoneHistory::instance().GetZoneHistory(get_process_path(window), workAreaId).has_value());
        }

        TEST_METHOD (UpdateStampedWindowsUpdatePositions)
        {
            // one window is snapped by the app zone history, the other one is assigned by its zone index property
            const ZoneIndexSet stampedZones = { 2 };
            const auto snappedWindow = CreateStampedWindow(m_zones);
            const auto stampedWindow = CreateStampedWindow(stampedZones);
            SaveAppZoneHistory(snappedWindow, 0, m_zones);
            const std::vector<StampedWindow> windows = { StampedWindow{ .window = snappedWindow, .zones = m_zones, .monitor = m_monitors[0] },
                                                         StampedWindow{ .window = stampedWindow, .zones = stampedZones, .monitor = m_monitors[0] } };

            RECT snappedWindowRect{};
            GetWindowRect(snappedWindow, &snappedWindowRect);
            RECT stampedWindowRect{};
            GetWindowRect(stampedWindow, &stampedWindowRect);

            m_workAreaConfiguration.UpdateStampedWindows(windows, true, false);

            Assert::IsTrue(m_zones == AssignedZones(snappedWindow, 0));
            Assert::IsTrue(stampedZones == AssignedZones(stampedWindow, 0));
            CustomAssert::AreEqual(snappedWindowRect, WindowRect(snappedWindow));
            CustomAssert::AreEqual(stampedWindowRect, WindowRect(stampedWindow));

            m_workAreaConfiguration.UpdateStampedWindows(windows, true, true);

            const auto& zones = m_workAreaConfiguration.GetWorkArea(m_monitors[0])->GetLayout()->Zones();
            const auto snappedZoneRect = zones.at(1).GetZoneRect();
            const auto stampedZoneRect = zones.at(2).GetZoneRect();
            CustomAssert::AreEqual(snappedZoneRect, Helpers::WaitForWindowRect(snappedWindow, snappedZoneRect));
            CustomAssert::AreEqual(stampedZoneRect, Helpers::WaitForWindowRect(stampedWindow, stampedZoneRect));
        }

        TEST_METHOD (UpdateStampedWindowsSameAppDifferentZones)
        {
            const ZoneIndexSet zones1 = { 0 };
            const ZoneIndexSet zones2 = { 1 };
            const auto window1 = CreateStampedWindow(zones1);
            const auto window2 = CreateStampedWindow(zones2);
            SaveAppZoneHistory(window1, 0, zones1);

            const std::vector<StampedWindow> windows = { StampedWindow{ .window = window1, .zones = zones1, .monitor = m_monitors[0] },
                                                         StampedWindow{ .window = window2, .zones = zones2, .monitor = m_monitors[0] } };
            m_workAreaConfiguration.UpdateStampedWindows(windows, true, false);

            // the app zone history has the zones of one window of the app only, the other window keeps its own zones
            Assert::IsTrue(zones1 == AssignedZones(window1, 0));
            Assert::IsTrue(zones2 == AssignedZones(window2, 0));
        }

        TEST_METHOD (UpdateStampedWindowsOtherWorkAreasNotChecked)
        {
            const auto window = CreateStampedWindow(m_zones);
            SaveAppZoneHistory(window, 1, m_zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[0] } }, false, true);

            // the window stays on the monitor where it's placed
            Assert::IsTrue(m_zones == AssignedZones(window, 0));
            Assert::IsTrue(AssignedZones(window, 1).empty());
        }

        TEST_METHOD (UpdateStampedWindowsAppZoneHistoryOnWindowMonitorOtherWorkAreasNotChecked)
        {
            const auto window = CreateStampedWindow(m_zones);
            SaveAppZoneHistory(window, 0, m_zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[0] } }, false, true);

            Assert::IsTrue(m_zones == AssignedZones(window, 0));
            Assert::IsTrue(AssignedZones(window, 1).empty());
            Assert::IsTrue(IsSnapped(window, 0));
        }

        TEST_METHOD (UpdateStampedWindowsIncorrectZone)
        {
            const ZoneIndexSet zones = { 5 };
            const auto window = CreateStampedWindow(zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = zones, .monitor = m_monitors[0] } }, true, false);

            Assert::IsTrue(AssignedZones(window, 0).empty());
            Assert::IsTrue(AssignedZones(window, 1).empty());
        }

        TEST_METHOD (UpdateStampedWindowsMonitorWithoutWorkArea)
        {
            const auto window = CreateStampedWindow(m_zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = Mocks::Monitor() } }, true, false);

            Assert::IsTrue(AssignedZones(window, 0).empty());
            Assert::IsTrue(AssignedZones(window, 1).empty());
        }

        TEST_METHOD (UpdateStampedWindowsNearestMonitor)
        {
            const auto window = CreateStampedWindow(m_zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = nullptr, .nearestMonitor = m_monitors[1] } }, true, false);

            Assert::IsTrue(AssignedZones(window, 0).empty());
            Assert::IsTrue(m_zones == AssignedZones(window, 1));
            Assert::IsTrue(AppZoneHistory::instance().GetFullAppZoneHistory().empty());
        }

        TEST_METHOD (UpdateStampedWindowsExcludedApp)
        {
            // case-sensitive, should be uppercase
            FancyZonesSettings::instance().SetSettings(Settings{ .excludedAppsArray = { L"STAMPED_WINDOW_EXCLUDED" } });

            // exclude by window title
            const auto window = CreateStampedWindow(m_zones, L"Stamped_Window_Excluded");
            const std::vector<StampedWindow> windows = { StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[0] } };

            m_workAreaConfiguration.UpdateStampedWindows(windows, true, false);

            Assert::IsTrue(AssignedZones(window, 0).empty());
            Assert::IsTrue(AssignedZones(window, 1).empty());

            // the window is snapped when the app zone history has the same zones, as a window of any other app
            SaveAppZoneHistory(window, 0, m_zones);
            m_workAreaConfiguration.UpdateStampedWindows(windows, true, false);

            Assert::IsTrue(m_zones == AssignedZones(window, 0));
            Assert::IsTrue(IsSnapped(window, 0));
        }

        TEST_METHOD (UpdateStampedWindowsWindowPlacedOnOtherMonitor)
        {
            const auto window = CreateStampedWindow(m_zones);
            Assert::IsTrue(m_workAreaConfiguration.GetWorkArea(m_monitors[0])->AssignStampedWindow(window, m_zones));
            Assert::IsTrue(m_zones == AssignedZones(window, 0));

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[1] } }, true, false);

            Assert::IsTrue(AssignedZones(window, 0).empty());
            Assert::IsTrue(m_zones == AssignedZones(window, 1));
        }

        TEST_METHOD (UpdateStampedWindowsKeepsStampedWindowOutsideOfMonitors)
        {
            const auto window = CreateStampedWindow(m_zones);
            Assert::IsTrue(m_workAreaConfiguration.GetWorkArea(m_monitors[0])->AssignStampedWindow(window, m_zones));

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = nullptr, .nearestMonitor = m_monitors[1] } }, true, false);

            // a window that isn't placed on any monitor stays on the work area that keeps it
            Assert::IsTrue(m_zones == AssignedZones(window, 0));
            Assert::IsTrue(AssignedZones(window, 1).empty());
        }

        TEST_METHOD (UpdateStampedWindowsKeepsOrderOfStampedWindows)
        {
            const auto window1 = CreateStampedWindow(m_zones);
            const auto window2 = CreateStampedWindow(m_zones);
            const StampedWindow stampedWindow1{ .window = window1, .zones = m_zones, .monitor = m_monitors[0] };
            const StampedWindow stampedWindow2{ .window = window2, .zones = m_zones, .monitor = m_monitors[0] };

            m_workAreaConfiguration.UpdateStampedWindows({ stampedWindow1, stampedWindow2 }, true, false);

            const auto sortKey = FancyZonesWindowProperties::GetTabSortKeyWithinZone(window1);
            Assert::IsTrue(sortKey.has_value());

            // the windows come in another order, assigning the window again would move it to the end of its zone
            m_workAreaConfiguration.UpdateStampedWindows({ stampedWindow2, stampedWindow1 }, true, false);

            Assert::IsTrue(sortKey == FancyZonesWindowProperties::GetTabSortKeyWithinZone(window1));
        }

        TEST_METHOD (UpdateStampedWindowsKeepsOrderOfWindowSnappedOnOtherWorkArea)
        {
            const auto window = CreateStampedWindow(m_zones);
            Assert::IsTrue(m_workAreaConfiguration.GetWorkArea(m_monitors[0])->AssignStampedWindow(window, m_zones));

            // the window leaves the first work area and is snapped on the second one
            Assert::IsTrue(m_workAreaConfiguration.GetWorkArea(m_monitors[0])->Unsnap(window));
            Assert::IsTrue(m_workAreaConfiguration.GetWorkArea(m_monitors[1])->Snap(window, m_zones, false));

            const auto sortKey = FancyZonesWindowProperties::GetTabSortKeyWithinZone(window);
            Assert::IsTrue(sortKey.has_value());

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[1] } }, true, false);

            // the first work area doesn't keep the window anymore, releasing it there would remove its place in the zone
            Assert::IsTrue(sortKey == FancyZonesWindowProperties::GetTabSortKeyWithinZone(window));
        }

        TEST_METHOD (UpdateStampedWindowsTwice)
        {
            // one window is snapped by the app zone history, the other one is assigned by its zone index property
            const ZoneIndexSet snappedZones = { 0 };
            const ZoneIndexSet stampedZones = { 1 };
            const auto snappedWindow = CreateStampedWindow(snappedZones);
            const auto stampedWindow = CreateStampedWindow(stampedZones);
            SaveAppZoneHistory(snappedWindow, 0, snappedZones);
            const std::vector<StampedWindow> windows = { StampedWindow{ .window = snappedWindow, .zones = snappedZones, .monitor = m_monitors[0] },
                                                         StampedWindow{ .window = stampedWindow, .zones = stampedZones, .monitor = m_monitors[0] } };

            m_workAreaConfiguration.UpdateStampedWindows(windows, true, true);

            const auto& zones = m_workAreaConfiguration.GetWorkArea(m_monitors[0])->GetLayout()->Zones();
            const auto snappedZoneRect = zones.at(0).GetZoneRect();
            const auto stampedZoneRect = zones.at(1).GetZoneRect();
            CustomAssert::AreEqual(snappedZoneRect, Helpers::WaitForWindowRect(snappedWindow, snappedZoneRect));
            CustomAssert::AreEqual(stampedZoneRect, Helpers::WaitForWindowRect(stampedWindow, stampedZoneRect));
            Assert::IsTrue(snappedZones == AssignedZones(snappedWindow, 0));
            Assert::IsTrue(stampedZones == AssignedZones(stampedWindow, 0));
            Assert::IsTrue(m_workAreaConfiguration.GetWorkArea(m_monitors[1])->GetLayoutWindows().SnappedWindows().empty());
            Assert::IsTrue(snappedZones == AppZoneHistoryZones(snappedWindow, 0));

            m_workAreaConfiguration.UpdateStampedWindows(windows, true, true);

            CustomAssert::AreEqual(snappedZoneRect, WindowRect(snappedWindow));
            CustomAssert::AreEqual(stampedZoneRect, WindowRect(stampedWindow));
            Assert::IsTrue(snappedZones == AssignedZones(snappedWindow, 0));
            Assert::IsTrue(stampedZones == AssignedZones(stampedWindow, 0));
            Assert::IsTrue(m_workAreaConfiguration.GetWorkArea(m_monitors[1])->GetLayoutWindows().SnappedWindows().empty());
            Assert::IsTrue(snappedZones == AppZoneHistoryZones(snappedWindow, 0));
        }

        TEST_METHOD (UpdateStampedWindowsLayoutWithoutZones)
        {
            const auto workArea = m_workAreaConfiguration.GetWorkArea(m_monitors[0]);
            const ZoneIndexSet snappedZones = { 0 };
            const ZoneIndexSet stampedZones = { 1 };
            const auto snappedWindow = CreateStampedWindow(snappedZones);
            const auto stampedWindow = CreateStampedWindow(stampedZones);
            Assert::IsTrue(workArea->Snap(snappedWindow, snappedZones, false));
            const std::vector<StampedWindow> windows = { StampedWindow{ .window = snappedWindow, .zones = snappedZones, .monitor = m_monitors[0] },
                                                         StampedWindow{ .window = stampedWindow, .zones = stampedZones, .monitor = m_monitors[0] } };

            RECT snappedWindowRect{};
            GetWindowRect(snappedWindow, &snappedWindowRect);
            RECT stampedWindowRect{};
            GetWindowRect(stampedWindow, &stampedWindowRect);

            ApplyBlankLayout(m_workAreaIds[0]);
            workArea->InitLayout();
            Assert::IsTrue(workArea->GetLayout()->Zones().empty());

            m_workAreaConfiguration.UpdateStampedWindows(windows, false, true);

            Assert::IsTrue(snappedZones == AssignedZones(snappedWindow, 0));
            Assert::IsTrue(AssignedZones(stampedWindow, 0).empty());
            Assert::IsTrue(AssignedZones(stampedWindow, 1).empty());
            CustomAssert::AreEqual(snappedWindowRect, WindowRect(snappedWindow));
            CustomAssert::AreEqual(stampedWindowRect, WindowRect(stampedWindow));

            // the windows are placed by the next layout that has zones
            ApplyGridLayout(m_workAreaIds[0], 4);
            workArea->InitLayout();

            m_workAreaConfiguration.UpdateStampedWindows(windows, false, true);

            const auto snappedZoneRect = workArea->GetLayout()->Zones().at(0).GetZoneRect();
            const auto stampedZoneRect = workArea->GetLayout()->Zones().at(1).GetZoneRect();
            CustomAssert::AreEqual(snappedZoneRect, Helpers::WaitForWindowRect(snappedWindow, snappedZoneRect));
            CustomAssert::AreEqual(stampedZoneRect, Helpers::WaitForWindowRect(stampedWindow, stampedZoneRect));
        }

        TEST_METHOD (UpdateStampedWindowsAssignsWindowAgainWhenLayoutHasZone)
        {
            const auto workArea = m_workAreaConfiguration.GetWorkArea(m_monitors[0]);
            const ZoneIndexSet zones = { 3 };
            const auto window = CreateStampedWindow(zones);
            const std::vector<StampedWindow> windows = { StampedWindow{ .window = window, .zones = zones, .monitor = m_monitors[0] } };

            RECT rect{};
            GetWindowRect(window, &rect);

            m_workAreaConfiguration.UpdateStampedWindows(windows, false, false);

            Assert::IsTrue(zones == AssignedZones(window, 0));

            // the layout doesn't have the zone of the window anymore
            ApplyGridLayout(m_workAreaIds[0], 2);
            workArea->InitLayout();
            Assert::AreEqual(static_cast<size_t>(2), workArea->GetLayout()->Zones().size());

            m_workAreaConfiguration.UpdateStampedWindows(windows, false, true);

            Assert::IsTrue(AssignedZones(window, 0).empty());
            CustomAssert::AreEqual(rect, WindowRect(window));

            // the layout has the zone again
            ApplyGridLayout(m_workAreaIds[0], 4);
            workArea->InitLayout();

            m_workAreaConfiguration.UpdateStampedWindows(windows, false, true);

            const auto zoneRect = workArea->GetLayout()->Zones().at(3).GetZoneRect();
            Assert::IsTrue(zones == AssignedZones(window, 0));
            CustomAssert::AreEqual(zoneRect, Helpers::WaitForWindowRect(window, zoneRect));
        }

        TEST_METHOD (UpdateStampedWindowsZoneIndexPropertyFromAnotherMonitor)
        {
            // The zone index property was saved on the second monitor and the window is placed on the first one.
            // The app zone history has the same zones on both.
            const ZoneIndexSet zones = { 0 };
            const auto window1 = CreateStampedWindow(zones);
            FancyZonesWindowProperties::StampZoneIndexMonitorProperty(window1, m_workAreaIds[1].monitorId);
            SaveAppZoneHistory(window1, 0, zones);
            SaveAppZoneHistory(window1, 1, zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window1, .zones = zones, .monitor = m_monitors[0] } }, true, false);

            // the window is snapped on the monitor where its zones were saved
            Assert::IsTrue(AssignedZones(window1, 0).empty());
            Assert::IsTrue(zones == AssignedZones(window1, 1));
            Assert::IsTrue(IsSnapped(window1, 1));

            // a second window with the same properties and the same app zone history, the other work areas aren't checked
            AppZoneHistory::instance().SetAppZoneHistory({});
            const auto window2 = CreateStampedWindow(zones);
            FancyZonesWindowProperties::StampZoneIndexMonitorProperty(window2, m_workAreaIds[1].monitorId);
            SaveAppZoneHistory(window2, 0, zones);
            SaveAppZoneHistory(window2, 1, zones);

            RECT rect{};
            GetWindowRect(window2, &rect);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window2, .zones = zones, .monitor = m_monitors[0] } }, false, true);

            // The window isn't snapped on the monitor where it's placed. It isn't assigned there either,
            // because the monitor where its zones were saved has a work area.
            Assert::IsTrue(AssignedZones(window2, 0).empty());
            Assert::IsTrue(AssignedZones(window2, 1).empty());
            CustomAssert::AreEqual(rect, WindowRect(window2));
        }

        TEST_METHOD (UpdateStampedWindowsMonitorOfZoneIndexPropertyHasWorkArea)
        {
            // The zone index property was saved on the second monitor and the window is placed on the first one.
            // The app zone history of the second monitor has other zones.
            const ZoneIndexSet zones = { 0 };
            const auto window = CreateStampedWindow(zones);
            FancyZonesWindowProperties::StampZoneIndexMonitorProperty(window, m_workAreaIds[1].monitorId);
            SaveAppZoneHistory(window, 1, { 2 });

            RECT rect{};
            GetWindowRect(window, &rect);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = zones, .monitor = m_monitors[0] } }, true, true);

            // the window stays as it is while the monitor where its zones were saved has a work area
            Assert::IsTrue(AssignedZones(window, 0).empty());
            Assert::IsTrue(AssignedZones(window, 1).empty());
            CustomAssert::AreEqual(rect, WindowRect(window));
        }

        TEST_METHOD (UpdateStampedWindowsMonitorOfZoneIndexPropertyWithoutWorkArea)
        {
            // the zone index property was saved on a monitor that has no work area, the window is placed on the first monitor
            const FancyZonesDataTypes::MonitorId monitorId = { .deviceId = { .id = L"stamped-windows-device-id-3" } };
            const ZoneIndexSet zones = { 0 };
            const auto window = CreateStampedWindow(zones);
            FancyZonesWindowProperties::StampZoneIndexMonitorProperty(window, monitorId);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = zones, .monitor = m_monitors[0] } }, true, false);

            // the window is assigned on the monitor where it's placed
            Assert::IsTrue(zones == AssignedZones(window, 0));
            Assert::IsTrue(AssignedZones(window, 1).empty());
        }

        TEST_METHOD (UpdateStampedWindowsMonitorReplaced)
        {
            // a monitor with two zones
            ReplaceMonitor(0, 2);
            const ZoneIndexSet zones1 = { 1 };
            const ZoneIndexSet zones2 = { 0 };
            const auto window1 = CreateSnappedWindow(0, zones1);
            const auto window2 = CreateSnappedWindow(0, zones2);

            // a monitor with one zone replaces it
            ReplaceMonitor(1, 1);
            const std::vector<StampedWindow> windowsOnSecondMonitor = { StampedWindow{ .window = window1, .zones = zones1, .monitor = m_monitors[1] },
                                                                        StampedWindow{ .window = window2, .zones = zones2, .monitor = m_monitors[1] } };
            m_workAreaConfiguration.UpdateStampedWindows(windowsOnSecondMonitor, true, true);

            // both windows are in the only zone, and nothing is saved for them
            const ZoneIndexSet onlyZone = { 0 };
            const auto onlyZoneRect = m_workAreaConfiguration.GetWorkArea(m_monitors[1])->GetLayout()->Zones().at(0).GetZoneRect();
            Assert::IsTrue(onlyZone == AssignedZones(window1, 1));
            Assert::IsTrue(onlyZone == AssignedZones(window2, 1));
            CustomAssert::AreEqual(onlyZoneRect, Helpers::WaitForWindowRect(window1, onlyZoneRect));
            CustomAssert::AreEqual(onlyZoneRect, Helpers::WaitForWindowRect(window2, onlyZoneRect));
            Assert::IsTrue(zones1 == FancyZonesWindowProperties::RetrieveZoneIndexProperty(window1));
            Assert::IsTrue(zones2 == FancyZonesWindowProperties::RetrieveZoneIndexProperty(window2));
            Assert::IsTrue(AppZoneHistoryZones(window1, 1).empty());

            // the monitor with two zones is back
            ReplaceMonitor(0, 2);
            const std::vector<StampedWindow> windowsOnFirstMonitor = { StampedWindow{ .window = window1, .zones = zones1, .monitor = m_monitors[0] },
                                                                       StampedWindow{ .window = window2, .zones = zones2, .monitor = m_monitors[0] } };
            m_workAreaConfiguration.UpdateStampedWindows(windowsOnFirstMonitor, true, true);

            // both windows are in their own zones again
            const auto& zones = m_workAreaConfiguration.GetWorkArea(m_monitors[0])->GetLayout()->Zones();
            const auto zoneRect1 = zones.at(1).GetZoneRect();
            const auto zoneRect2 = zones.at(0).GetZoneRect();
            Assert::IsTrue(zones1 == AssignedZones(window1, 0));
            Assert::IsTrue(zones2 == AssignedZones(window2, 0));
            CustomAssert::AreEqual(zoneRect1, Helpers::WaitForWindowRect(window1, zoneRect1));
            CustomAssert::AreEqual(zoneRect2, Helpers::WaitForWindowRect(window2, zoneRect2));
        }

        TEST_METHOD (UpdateStampedWindowsWindowFromOtherMonitorTakesAppZoneHistory)
        {
            // a window that was snapped on a monitor with one zone
            ReplaceMonitor(1, 1);
            const ZoneIndexSet zones = { 0 };
            const auto window = CreateSnappedWindow(1, zones);

            // a monitor with two zones replaces it, the app was in zone 1 there
            ReplaceMonitor(0, 2);
            const ZoneIndexSet appZoneHistoryZones = { 1 };
            SaveAppZoneHistory(window, 0, appZoneHistoryZones);
            const std::vector<StampedWindow> windows = { StampedWindow{ .window = window, .zones = zones, .monitor = m_monitors[0] } };

            m_workAreaConfiguration.UpdateStampedWindows(windows, true, true);

            // the window gets the zones of the app zone history, and nothing is saved for it
            const auto zoneRect = m_workAreaConfiguration.GetWorkArea(m_monitors[0])->GetLayout()->Zones().at(1).GetZoneRect();
            Assert::IsTrue(appZoneHistoryZones == AssignedZones(window, 0));
            CustomAssert::AreEqual(zoneRect, Helpers::WaitForWindowRect(window, zoneRect));
            Assert::IsTrue(zones == FancyZonesWindowProperties::RetrieveZoneIndexProperty(window));
            Assert::IsTrue(FancyZonesWindowProperties::IsZoneIndexPropertyFromAnotherMonitor(window, m_workAreaIds[0].monitorId));
            Assert::IsFalse(FancyZonesWindowProperties::IsZoneIndexPropertyFromAnotherMonitor(window, m_workAreaIds[1].monitorId));
            Assert::IsFalse(IsSnapped(window, 0));

            // The window keeps these zones only while the work area exists. After the app zone history changed
            // and the work areas were created again, it gets the new zones of the app zone history.
            const ZoneIndexSet newAppZoneHistoryZones = { 0, 1 };
            SaveAppZoneHistory(window, 0, newAppZoneHistoryZones);
            ReplaceMonitor(0, 2);

            m_workAreaConfiguration.UpdateStampedWindows(windows, true, false);

            Assert::IsTrue(newAppZoneHistoryZones == AssignedZones(window, 0));
        }

        TEST_METHOD (UpdateStampedWindowsSameAppDifferentZonesMonitorReplaced)
        {
            // Two windows of one app in different zones of a monitor with two zones.
            // The app zone history of that monitor has the zones of the window that was snapped first.
            ReplaceMonitor(0, 2);
            const ZoneIndexSet zones1 = { 1 };
            const ZoneIndexSet zones2 = { 0 };
            const auto window1 = CreateSnappedWindow(0, zones1);
            const auto window2 = CreateSnappedWindow(0, zones2);
            Assert::IsTrue(zones1 == AppZoneHistoryZones(window1, 0));

            // A monitor with one zone replaces it. The app zone history there has the zone number of the second window.
            ReplaceMonitor(1, 1);
            SaveAppZoneHistory(window1, 1, zones2);
            const std::vector<StampedWindow> windowsOnSecondMonitor = { StampedWindow{ .window = window1, .zones = zones1, .monitor = m_monitors[1] },
                                                                        StampedWindow{ .window = window2, .zones = zones2, .monitor = m_monitors[1] } };
            m_workAreaConfiguration.UpdateStampedWindows(windowsOnSecondMonitor, true, true);

            Assert::IsTrue(zones2 == AssignedZones(window1, 1));
            Assert::IsTrue(zones2 == AssignedZones(window2, 1));

            // the monitor with two zones is back
            ReplaceMonitor(0, 2);
            const std::vector<StampedWindow> windowsOnFirstMonitor = { StampedWindow{ .window = window1, .zones = zones1, .monitor = m_monitors[0] },
                                                                       StampedWindow{ .window = window2, .zones = zones2, .monitor = m_monitors[0] } };
            m_workAreaConfiguration.UpdateStampedWindows(windowsOnFirstMonitor, true, true);

            // both windows are in their own zones again
            const auto& zones = m_workAreaConfiguration.GetWorkArea(m_monitors[0])->GetLayout()->Zones();
            const auto zoneRect1 = zones.at(1).GetZoneRect();
            const auto zoneRect2 = zones.at(0).GetZoneRect();
            Assert::IsTrue(zones1 == AssignedZones(window1, 0));
            Assert::IsTrue(zones2 == AssignedZones(window2, 0));
            CustomAssert::AreEqual(zoneRect1, Helpers::WaitForWindowRect(window1, zoneRect1));
            CustomAssert::AreEqual(zoneRect2, Helpers::WaitForWindowRect(window2, zoneRect2));

            // and their zones are still the ones of the monitor with two zones
            for (const auto window : { window1, window2 })
            {
                Assert::IsFalse(FancyZonesWindowProperties::IsZoneIndexPropertyFromAnotherMonitor(window, m_workAreaIds[0].monitorId));
                Assert::IsTrue(FancyZonesWindowProperties::IsZoneIndexPropertyFromAnotherMonitor(window, m_workAreaIds[1].monitorId));
            }
        }
    };
}
