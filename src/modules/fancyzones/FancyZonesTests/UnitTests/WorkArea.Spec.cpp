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
        const GUID m_layoutId = FancyZonesUtils::GuidFromString(L"{57895235-47E1-48A4-A2E7-2FF151F40564}").value();
        AppliedLayouts::TAppliedLayoutsMap m_appliedLayouts{};
        std::vector<HWND> m_windows{};

        void ApplyGridLayout(int zoneCount)
        {
            LayoutData layout{
                .uuid = m_layoutId,
                .type = FancyZonesDataTypes::ZoneSetLayoutType::Grid,
                .showSpacing = false,
                .spacing = 0,
                .zoneCount = zoneCount,
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

        bool HasAppZoneHistory(HWND window)
        {
            return AppZoneHistory::instance().GetZoneHistory(get_process_path(window), m_workAreaId).has_value();
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

        void AddWorkArea(WorkAreaConfiguration& workAreaConfiguration, HMONITOR monitor, const FancyZonesDataTypes::WorkAreaId& workAreaId)
        {
            ApplyGridLayout(workAreaId, 4);

            workAreaConfiguration.AddWorkArea(monitor, WorkArea::Create(m_hInst, workAreaId, {}, m_workAreaRect));
        }

        HWND CreateStampedWindow(const ZoneIndexSet& zones)
        {
            const auto window = Mocks::WindowCreate(m_hInst, L"", L"", 0, WS_THICKFRAME);
            m_windows.push_back(window);

            SetWindowPos(window, nullptr, 150, 150, 450, 550, SWP_SHOWWINDOW);
            Assert::IsTrue(FancyZonesWindowProperties::StampZoneIndexProperty(window, zones));
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

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[0] } }, false);

            Assert::IsTrue(m_zones == AssignedZones(window, 0));
            Assert::IsTrue(AssignedZones(window, 1).empty());
            Assert::IsTrue(IsSnapped(window, 0));
        }

        TEST_METHOD (UpdateStampedWindowsAppZoneHistoryOnOtherMonitor)
        {
            const auto window = CreateStampedWindow(m_zones);
            SaveAppZoneHistory(window, 1, m_zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[0] } }, false);

            Assert::IsTrue(AssignedZones(window, 0).empty());
            Assert::IsTrue(m_zones == AssignedZones(window, 1));
            Assert::IsTrue(IsSnapped(window, 1));
        }

        TEST_METHOD (UpdateStampedWindowsAppZoneHistoryOnBothWorkAreasWindowOnFirst)
        {
            const auto window = CreateStampedWindow(m_zones);
            SaveAppZoneHistory(window, 0, m_zones);
            SaveAppZoneHistory(window, 1, m_zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[0] } }, false);

            Assert::IsTrue(m_zones == AssignedZones(window, 0));
            Assert::IsTrue(AssignedZones(window, 1).empty());
        }

        TEST_METHOD (UpdateStampedWindowsAppZoneHistoryOnBothWorkAreasWindowOnSecond)
        {
            const auto window = CreateStampedWindow(m_zones);
            SaveAppZoneHistory(window, 0, m_zones);
            SaveAppZoneHistory(window, 1, m_zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[1] } }, false);

            Assert::IsTrue(AssignedZones(window, 0).empty());
            Assert::IsTrue(m_zones == AssignedZones(window, 1));
        }

        TEST_METHOD (UpdateStampedWindowsNoAppZoneHistory)
        {
            const auto window = CreateStampedWindow(m_zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[0] } }, false);

            Assert::IsTrue(AssignedZones(window, 0).empty());
            Assert::IsTrue(AssignedZones(window, 1).empty());
            Assert::IsTrue(m_zones == FancyZonesWindowProperties::RetrieveZoneIndexProperty(window));
        }

        TEST_METHOD (UpdateStampedWindowsOtherZonesInAppZoneHistory)
        {
            const ZoneIndexSet appZoneHistoryZones = { 0 };
            const auto window = CreateStampedWindow(m_zones);
            SaveAppZoneHistory(window, 0, appZoneHistoryZones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[0] } }, false);

            Assert::IsTrue(AssignedZones(window, 0).empty());
            Assert::IsTrue(AssignedZones(window, 1).empty());
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

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = zones, .monitor = Mocks::Monitor() } }, false);

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

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = zones, .monitor = Mocks::Monitor() } }, false);

            Assert::IsTrue(zones == AssignedZones(window, 0));
            Assert::IsTrue(AssignedZones(window, 1).empty());
        }

        TEST_METHOD (UpdateStampedWindowsWindowOutsideOfMonitors)
        {
            const auto window = CreateStampedWindow(m_zones);
            SaveAppZoneHistory(window, 1, m_zones);

            m_workAreaConfiguration.UpdateStampedWindows({ StampedWindow{ .window = window, .zones = m_zones, .monitor = nullptr } }, false);

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
            workAreaConfiguration.UpdateStampedWindows(windows, false);

            Assert::IsTrue(m_zones == workAreaConfiguration.GetWorkArea(nullptr)->GetLayoutWindows().GetZoneIndexSetFromWindow(window));
            Assert::IsTrue(AppZoneHistory::instance().GetZoneHistory(get_process_path(window), workAreaId).has_value());

            // the window is snapped again when it's snapped already
            AppZoneHistory::instance().SetAppZoneHistory({});
            workAreaConfiguration.UpdateStampedWindows(windows, false);

            Assert::IsTrue(AppZoneHistory::instance().GetZoneHistory(get_process_path(window), workAreaId).has_value());
        }

        TEST_METHOD (UpdateStampedWindowsUpdatePositions)
        {
            const auto window = CreateStampedWindow(m_zones);
            SaveAppZoneHistory(window, 0, m_zones);
            const std::vector<StampedWindow> windows = { StampedWindow{ .window = window, .zones = m_zones, .monitor = m_monitors[0] } };

            RECT rect{};
            GetWindowRect(window, &rect);

            m_workAreaConfiguration.UpdateStampedWindows(windows, false);

            Assert::IsTrue(m_zones == AssignedZones(window, 0));
            CustomAssert::AreEqual(rect, WindowRect(window));

            m_workAreaConfiguration.UpdateStampedWindows(windows, true);

            const auto zoneRect = m_workAreaConfiguration.GetWorkArea(m_monitors[0])->GetLayout()->Zones().at(1).GetZoneRect();
            CustomAssert::AreEqual(zoneRect, Helpers::WaitForWindowRect(window, zoneRect));
        }
    };
}
