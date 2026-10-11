#include "pch.h"
#include "Util.h"
#include "FancyZonesLib\util.h"
#include "FancyZonesLib/JsonHelpers.h"
#include "FancyZonesLib/FancyZonesWindowProperties.h"
#include "FancyZonesLib/MonitorUtils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace FancyZonesUnitTests
{
    using namespace FancyZonesUtils;

    void TestMonitorSetPermutations(const std::vector<std::pair<HMONITOR, RECT>>& monitorInfo)
    {
        auto monitorInfoPermutation = monitorInfo;

        do {
            auto monitorInfoCopy = monitorInfoPermutation;
            OrderMonitors(monitorInfoCopy);
            CustomAssert::AreEqual(monitorInfo, monitorInfoCopy);
        } while (std::next_permutation(monitorInfoPermutation.begin(), monitorInfoPermutation.end(), [](auto x, auto y) { return x.first < y.first; }));
    }

    void TestMonitorSetPermutationsOffsets(const std::vector<std::pair<HMONITOR, RECT>>& monitorInfo)
    {
        for (int offsetX = -3000; offsetX <= 3000; offsetX += 1000)
        {
            for (int offsetY = -3000; offsetY <= 3000; offsetY += 1000)
            {
                auto monitorInfoCopy = monitorInfo;
                for (auto& [monitor, rect] : monitorInfoCopy)
                {
                    rect.left += offsetX;
                    rect.right += offsetX;
                    rect.top += offsetY;
                    rect.bottom += offsetY;
                }
                TestMonitorSetPermutations(monitorInfoCopy);
            }
        }
    }

    TEST_CLASS(UtilUnitTests)
    {
        TEST_METHOD(TestParseDeviceId01)
        {
            const std::wstring input = L"AOC0001#5&37ac4db&0&UID160002_1536_960_{E0A2904E-889C-4532-95B1-28FE15C16F66}";
            
            GUID guid;
            const auto expectedGuidStr = L"{E0A2904E-889C-4532-95B1-28FE15C16F66}";
            CLSIDFromString(expectedGuidStr, &guid);
            const BackwardsCompatibility::DeviceIdData expected{ L"AOC0001#5&37ac4db&0&UID160002", 1536, 960, guid };
            
            const auto actual = BackwardsCompatibility::DeviceIdData::ParseDeviceId(input);
            Assert::IsTrue(actual.has_value());

            Assert::AreEqual(expected.deviceName, actual->deviceName);
            Assert::AreEqual(expected.height, actual->height);
            Assert::AreEqual(expected.width, actual->width);
            Assert::AreEqual(expected.monitorId, actual->monitorId);
            
            wil::unique_cotaskmem_string actualGuidStr;
            StringFromCLSID(actual->virtualDesktopId, &actualGuidStr);
            Assert::AreEqual(expectedGuidStr, actualGuidStr.get());
        }

        TEST_METHOD (TestParseDeviceId02)
        {
            const std::wstring input = L"AOC0001#5&37ac4db&0&UID160002_1536_960_{E0A2904E-889C-4532-95B1-28FE15C16F66}_monitorId";

            GUID guid;
            const auto expectedGuidStr = L"{E0A2904E-889C-4532-95B1-28FE15C16F66}";
            CLSIDFromString(expectedGuidStr, &guid);
            const BackwardsCompatibility::DeviceIdData expected{ L"AOC0001#5&37ac4db&0&UID160002", 1536, 960, guid, L"monitorId" };

            const auto actual = BackwardsCompatibility::DeviceIdData::ParseDeviceId(input);
            Assert::IsTrue(actual.has_value());

            Assert::AreEqual(expected.deviceName, actual->deviceName);
            Assert::AreEqual(expected.height, actual->height);
            Assert::AreEqual(expected.width, actual->width);
            Assert::AreEqual(expected.monitorId, actual->monitorId);

            wil::unique_cotaskmem_string actualGuidStr;
            StringFromCLSID(actual->virtualDesktopId, &actualGuidStr);
            Assert::AreEqual(expectedGuidStr, actualGuidStr.get());
        }

        TEST_METHOD (TestParseDeviceId03)
        {
            // difference with previous tests is in the device name: there is no # symbol
            const std::wstring input = L"AOC00015&37ac4db&0&UID160002_1536_960_{E0A2904E-889C-4532-95B1-28FE15C16F66}";

            GUID guid;
            const auto expectedGuidStr = L"{E0A2904E-889C-4532-95B1-28FE15C16F66}";
            CLSIDFromString(expectedGuidStr, &guid);
            const BackwardsCompatibility::DeviceIdData expected{ L"AOC00015&37ac4db&0&UID160002", 1536, 960, guid };

            const auto actual = BackwardsCompatibility::DeviceIdData::ParseDeviceId(input);
            Assert::IsTrue(actual.has_value());

            Assert::AreEqual(expected.deviceName, actual->deviceName);
            Assert::AreEqual(expected.height, actual->height);
            Assert::AreEqual(expected.width, actual->width);
            Assert::AreEqual(expected.monitorId, actual->monitorId);

            wil::unique_cotaskmem_string actualGuidStr;
            StringFromCLSID(actual->virtualDesktopId, &actualGuidStr);
            Assert::AreEqual(expectedGuidStr, actualGuidStr.get());
        }

        TEST_METHOD (TestParseDeviceIdInvalid01)
        {
            // no width or height
            const std::wstring input = L"AOC00015&37ac4db&0&UID160002_1536960_{E0A2904E-889C-4532-95B1-28FE15C16F66}";
            const auto actual = BackwardsCompatibility::DeviceIdData::ParseDeviceId(input);
            Assert::IsFalse(actual.has_value());
        }

        TEST_METHOD (TestParseDeviceIdInvalid02)
        {
            // no width and height
            const std::wstring input = L"AOC00015&37ac4db&0&UID160002_{E0A2904E-889C-4532-95B1-28FE15C16F66}_monitorId";
            const auto actual = BackwardsCompatibility::DeviceIdData::ParseDeviceId(input);
            Assert::IsFalse(actual.has_value());
        }

        TEST_METHOD (TestParseDeviceIdInvalid03)
        {
            // no guid
            const std::wstring input = L"AOC00015&37ac4db&0&UID160002_1536960_";
            const auto actual = BackwardsCompatibility::DeviceIdData::ParseDeviceId(input);
            Assert::IsFalse(actual.has_value());
        }

        TEST_METHOD (TestParseDeviceIdInvalid04)
        {
            // invalid guid
            const std::wstring input = L"AOC00015&37ac4db&0&UID160002_1536960_{asdf}";
            const auto actual = BackwardsCompatibility::DeviceIdData::ParseDeviceId(input);
            Assert::IsFalse(actual.has_value());
        }

        TEST_METHOD (TestParseDeviceIdInvalid05)
        {
            // invalid width/height
            const std::wstring input = L"AOC00015&37ac4db&0&UID160002_15a6_960_{E0A2904E-889C-4532-95B1-28FE15C16F66}";
            const auto actual = BackwardsCompatibility::DeviceIdData::ParseDeviceId(input);
            Assert::IsFalse(actual.has_value());
        }

        TEST_METHOD (TestParseDeviceIdInvalid06)
        {
            // changed order
            const std::wstring input = L"AOC00015&37ac4db&0&UID160002_15a6_960_monitorId_{E0A2904E-889C-4532-95B1-28FE15C16F66}";
            const auto actual = BackwardsCompatibility::DeviceIdData::ParseDeviceId(input);
            Assert::IsFalse(actual.has_value());
        }

        TEST_METHOD(TestMonitorOrdering01)
        {
            // Three horizontally arranged monitors, bottom aligned, with increasing sizes
            std::vector<std::pair<HMONITOR, RECT>> monitorInfo = {
                {Mocks::Monitor(), RECT{.left = 0, .top = 200, .right = 1600, .bottom = 1100} },
                {Mocks::Monitor(), RECT{.left = 1600, .top = 100, .right = 3300, .bottom = 1100} },
                {Mocks::Monitor(), RECT{.left = 3300, .top = 0, .right = 5100, .bottom = 1100} },
            };

            TestMonitorSetPermutationsOffsets(monitorInfo);
        }

        TEST_METHOD(TestMonitorOrdering02)
        {
            // Three horizontally arranged monitors, bottom aligned, with equal sizes
            std::vector<std::pair<HMONITOR, RECT>> monitorInfo = {
                {Mocks::Monitor(), RECT{.left = 0, .top = 0, .right = 1600, .bottom = 900} },
                {Mocks::Monitor(), RECT{.left = 1600, .top = 0, .right = 3200, .bottom = 900} },
                {Mocks::Monitor(), RECT{.left = 3200, .top = 0, .right = 4800, .bottom = 900} },
            };

            TestMonitorSetPermutationsOffsets(monitorInfo);
        }

        TEST_METHOD(TestMonitorOrdering03)
        {
            // Three horizontally arranged monitors, bottom aligned, with decreasing sizes
            std::vector<std::pair<HMONITOR, RECT>> monitorInfo = {
                {Mocks::Monitor(), RECT{.left = 0, .top = 0, .right = 1800, .bottom = 1100} },
                {Mocks::Monitor(), RECT{.left = 1800, .top = 100, .right = 3500, .bottom = 1100} },
                {Mocks::Monitor(), RECT{.left = 3500, .top = 200, .right = 5100, .bottom = 1100} },
            };

            TestMonitorSetPermutationsOffsets(monitorInfo);
        }

        TEST_METHOD(TestMonitorOrdering04)
        {
            // Three horizontally arranged monitors, top aligned, with increasing sizes
            std::vector<std::pair<HMONITOR, RECT>> monitorInfo = {
                {Mocks::Monitor(), RECT{.left = 0, .top = 0, .right = 1600, .bottom = 900} },
                {Mocks::Monitor(), RECT{.left = 1600, .top = 0, .right = 3300, .bottom = 1000} },
                {Mocks::Monitor(), RECT{.left = 3300, .top = 0, .right = 5100, .bottom = 1100} },
            };

            TestMonitorSetPermutationsOffsets(monitorInfo);
        }

        TEST_METHOD(TestMonitorOrdering05)
        {
            // Three horizontally arranged monitors, top aligned, with equal sizes
            std::vector<std::pair<HMONITOR, RECT>> monitorInfo = {
                {Mocks::Monitor(), RECT{.left = 0, .top = 0, .right = 1600, .bottom = 900} },
                {Mocks::Monitor(), RECT{.left = 1600, .top = 0, .right = 3200, .bottom = 900} },
                {Mocks::Monitor(), RECT{.left = 3200, .top = 0, .right = 4800, .bottom = 900} },
            };

            TestMonitorSetPermutationsOffsets(monitorInfo);
        }

        TEST_METHOD(TestMonitorOrdering06)
        {
            // Three horizontally arranged monitors, top aligned, with decreasing sizes
            std::vector<std::pair<HMONITOR, RECT>> monitorInfo = {
                {Mocks::Monitor(), RECT{.left = 0, .top = 0, .right = 1800, .bottom = 1100} },
                {Mocks::Monitor(), RECT{.left = 1800, .top = 0, .right = 3500, .bottom = 1000} },
                {Mocks::Monitor(), RECT{.left = 3500, .top = 0, .right = 5100, .bottom = 900} },
            };

            TestMonitorSetPermutationsOffsets(monitorInfo);
        }

        TEST_METHOD(TestMonitorOrdering07)
        {
            // Three vertically arranged monitors, center aligned, with equal sizes, except the middle monitor is a bit wider
            std::vector<std::pair<HMONITOR, RECT>> monitorInfo = {
                {Mocks::Monitor(), RECT{.left = 100, .top = 0, .right = 1700, .bottom = 900} },
                {Mocks::Monitor(), RECT{.left = 0, .top = 900, .right = 1800, .bottom = 1800} },
                {Mocks::Monitor(), RECT{.left = 100, .top = 1800, .right = 1700, .bottom = 2700} },
            };

            TestMonitorSetPermutationsOffsets(monitorInfo);
        }

        TEST_METHOD(TestMonitorOrdering08)
        {
            // ------------------
            // |    ||    ||    |
            // |    ||    ||    |
            // ------------------
            // |       ||       |
            // |       ||       |
            // ------------------
            std::vector<std::pair<HMONITOR, RECT>> monitorInfo = {
                {Mocks::Monitor(), RECT{.left = 0, .top = 0, .right = 600, .bottom = 400} },
                {Mocks::Monitor(), RECT{.left = 600, .top = 0, .right = 1200, .bottom = 400} },
                {Mocks::Monitor(), RECT{.left = 1200, .top = 0, .right = 1800, .bottom = 400} },
                {Mocks::Monitor(), RECT{.left = 0, .top = 400, .right = 900, .bottom = 800} },
                {Mocks::Monitor(), RECT{.left = 900, .top = 400, .right = 1800, .bottom = 800} },
            };

            TestMonitorSetPermutationsOffsets(monitorInfo);
        }

        TEST_METHOD(TestMonitorOrdering09)
        {
            // Regular 3x3 grid
            std::vector<std::pair<HMONITOR, RECT>> monitorInfo = {
                {Mocks::Monitor(), RECT{.left = 0, .top = 0, .right = 400, .bottom = 300} },
                {Mocks::Monitor(), RECT{.left = 400, .top = 0, .right = 800, .bottom = 300} },
                {Mocks::Monitor(), RECT{.left = 800, .top = 0, .right = 1200, .bottom = 300} },
                {Mocks::Monitor(), RECT{.left = 0, .top = 300, .right = 400, .bottom = 600} },
                {Mocks::Monitor(), RECT{.left = 400, .top = 300, .right = 800, .bottom = 600} },
                {Mocks::Monitor(), RECT{.left = 800, .top = 300, .right = 1200, .bottom = 600} },
                {Mocks::Monitor(), RECT{.left = 0, .top = 600, .right = 400, .bottom = 900} },
                {Mocks::Monitor(), RECT{.left = 400, .top = 600, .right = 800, .bottom = 900} },
                {Mocks::Monitor(), RECT{.left = 800, .top = 600, .right = 1200, .bottom = 900} },
            };

            // Reduce running time by testing only rotations
            for (int i = 0; i < 9; i++)
            {
                auto monitorInfoCopy = monitorInfo;
                std::rotate(monitorInfoCopy.begin(), monitorInfoCopy.begin() + i, monitorInfoCopy.end());
                OrderMonitors(monitorInfoCopy);
                CustomAssert::AreEqual(monitorInfo, monitorInfoCopy);
            }
        }

        TEST_METHOD(TestMonitorOrdering10)
        {
            // ------------------
            // |       ||       |
            // |       ||       |
            // ------------------
            // |    ||    ||    |
            // |    ||    ||    |
            // ------------------
            std::vector<std::pair<HMONITOR, RECT>> monitorInfo = {
                {Mocks::Monitor(), RECT{.left = 0, .top = 0, .right = 900, .bottom = 400} },
                {Mocks::Monitor(), RECT{.left = 900, .top = 0, .right = 1800, .bottom = 400} },
                {Mocks::Monitor(), RECT{.left = 0, .top = 400, .right = 600, .bottom = 800} },
                {Mocks::Monitor(), RECT{.left = 600, .top = 400, .right = 1200, .bottom = 800} },
                {Mocks::Monitor(), RECT{.left = 1200, .top = 400, .right = 1800, .bottom = 800} },
            };

            TestMonitorSetPermutationsOffsets(monitorInfo);
        }

        TEST_METHOD(TestMonitorOrdering11)
        {
            // Random values, some monitors overlap, don't check order, just ensure it doesn't crash and it's the same every time
            std::vector<std::pair<HMONITOR, RECT>> monitorInfo = {
                {Mocks::Monitor(), RECT{.left = 410, .top = 630, .right = 988, .bottom = 631} },
                {Mocks::Monitor(), RECT{.left = 302, .top = 189, .right = 550, .bottom = 714} },
                {Mocks::Monitor(), RECT{.left = 158, .top = 115, .right = 657, .bottom = 499} },
                {Mocks::Monitor(), RECT{.left = 341, .top = 340, .right = 723, .bottom = 655} },
                {Mocks::Monitor(), RECT{.left = 433, .top = 393, .right = 846, .bottom = 544} },
            };

            auto monitorInfoPermutation = monitorInfo;
            auto firstTime = monitorInfo;
            OrderMonitors(firstTime);

            do {
                auto monitorInfoCopy = monitorInfoPermutation;
                OrderMonitors(monitorInfoCopy);
                CustomAssert::AreEqual(firstTime, monitorInfoCopy);
            } while (next_permutation(monitorInfoPermutation.begin(), monitorInfoPermutation.end(), [](auto x, auto y) { return x.first < y.first; }));
        }
    
        TEST_METHOD(TestHexToRGB_rgb)
        {
            const auto expected = RGB(163, 246, 255);
            const auto actual = HexToRGB(L"#A3F6FF");
            Assert::AreEqual(expected, actual);
        }

        TEST_METHOD (TestHexToRGB_argb)
        {
            const auto expected = RGB(163, 246, 255);
            const auto actual = HexToRGB(L"#FFA3F6FF");
            Assert::AreEqual(expected, actual);
        }

        TEST_METHOD (TestHexToRGB_invalid)
        {
            const auto expected = RGB(255, 255, 255);
            const auto actual = HexToRGB(L"zzz");
            Assert::AreEqual(expected, actual);
        }
    };

    TEST_CLASS (PickNextLayoutIdUnitTests)
    {
        const GUID m_id1 = GuidFromString(L"{10000000-0000-0000-0000-000000000001}").value();
        const GUID m_id2 = GuidFromString(L"{20000000-0000-0000-0000-000000000002}").value();
        const GUID m_id3 = GuidFromString(L"{30000000-0000-0000-0000-000000000003}").value();
        const GUID m_unknownId = GuidFromString(L"{40000000-0000-0000-0000-000000000004}").value();
        const std::vector<GUID> m_ids = { m_id1, m_id2, m_id3 };

        TEST_METHOD (Forward)
        {
            const auto actual = PickNextLayoutId(m_ids, m_id1, false);
            Assert::IsTrue(actual.has_value());
            Assert::IsTrue(m_id2 == actual.value());
        }

        TEST_METHOD (ForwardWrapsAround)
        {
            const auto actual = PickNextLayoutId(m_ids, m_id3, false);
            Assert::IsTrue(actual.has_value());
            Assert::IsTrue(m_id1 == actual.value());
        }

        TEST_METHOD (Reverse)
        {
            const auto actual = PickNextLayoutId(m_ids, m_id2, true);
            Assert::IsTrue(actual.has_value());
            Assert::IsTrue(m_id1 == actual.value());
        }

        TEST_METHOD (ReverseWrapsAround)
        {
            const auto actual = PickNextLayoutId(m_ids, m_id1, true);
            Assert::IsTrue(actual.has_value());
            Assert::IsTrue(m_id3 == actual.value());
        }

        TEST_METHOD (CurrentNotInList)
        {
            const auto actual = PickNextLayoutId(m_ids, m_unknownId, false);
            Assert::IsTrue(actual.has_value());
            Assert::IsTrue(m_id1 == actual.value());
        }

        TEST_METHOD (CurrentNotInListReverse)
        {
            const auto actual = PickNextLayoutId(m_ids, m_unknownId, true);
            Assert::IsTrue(actual.has_value());
            Assert::IsTrue(m_id3 == actual.value());
        }

        TEST_METHOD (NoCurrent)
        {
            const auto actual = PickNextLayoutId(m_ids, std::nullopt, false);
            Assert::IsTrue(actual.has_value());
            Assert::IsTrue(m_id1 == actual.value());
        }

        TEST_METHOD (EmptyList)
        {
            const auto actual = PickNextLayoutId({}, m_id1, false);
            Assert::IsFalse(actual.has_value());
        }

        TEST_METHOD (SingleLayout)
        {
            const auto actual = PickNextLayoutId({ m_id1 }, m_id1, false);
            Assert::IsFalse(actual.has_value());
        }
    };

    TEST_CLASS (MonitorIdentificationUnitTests)
    {
        const FancyZonesDataTypes::DeviceId m_laptopPanel{ .id = L"LEN414D", .instanceId = L"4&117e8519&0&UID8388688", .number = 1 };
        const FancyZonesDataTypes::DeviceId m_externalMonitor{ .id = L"DEL4308", .instanceId = L"4&117e8519&0&UID4165", .number = 2 };
        const FancyZonesDataTypes::DeviceId m_displayName1{ .id = L"\\\\.\\DISPLAY1", .instanceId = L"", .number = 1 };
        const FancyZonesDataTypes::DeviceId m_displayName2{ .id = L"\\\\.\\DISPLAY2", .instanceId = L"", .number = 2 };

        const HMONITOR m_monitor1 = Mocks::Monitor();
        const HMONITOR m_monitor2 = Mocks::Monitor();
        const FancyZonesUtils::Rect m_laptopRect{ RECT{ 0, 0, 2880, 1704 } };
        const FancyZonesUtils::Rect m_externalRect{ RECT{ 0, 0, 5120, 2076 } };

        static FancyZonesDataTypes::MonitorId Monitor(HMONITOR monitor, const FancyZonesDataTypes::DeviceId& deviceId, const std::wstring& serialNumber = L"")
        {
            return FancyZonesDataTypes::MonitorId{ .monitor = monitor, .deviceId = deviceId, .serialNumber = serialNumber };
        }

        static MonitorUtils::Display::KnownMonitor Known(HMONITOR monitor, const FancyZonesDataTypes::DeviceId& deviceId, const FancyZonesUtils::Rect& workAreaRect, const std::wstring& serialNumber = L"")
        {
            return MonitorUtils::Display::KnownMonitor{ .monitorId = Monitor(monitor, deviceId, serialNumber), .workAreaRect = workAreaRect };
        }

        std::function<FancyZonesUtils::Rect(HMONITOR)> WorkAreaRects() const
        {
            return [this](HMONITOR monitor) {
                return monitor == m_monitor1 ? m_laptopRect : m_externalRect;
            };
        }

        static void AssertDeviceId(const FancyZonesDataTypes::DeviceId& expected, const FancyZonesDataTypes::DeviceId& actual)
        {
            Assert::AreEqual(expected.id, actual.id);
            Assert::AreEqual(expected.instanceId, actual.instanceId);
            Assert::AreEqual(expected.number, actual.number);
        }

        TEST_METHOD (DisplayNameIsDisplayName)
        {
            Assert::IsTrue(MonitorUtils::Display::IsDisplayName(m_displayName1));
        }

        TEST_METHOD (DeviceIdIsNotDisplayName)
        {
            Assert::IsFalse(MonitorUtils::Display::IsDisplayName(m_laptopPanel));
        }

        TEST_METHOD (DeviceIdWithoutInstanceIdIsNotDisplayName)
        {
            // SplitDisplayDeviceId returns the whole string as id when it can't split it
            const auto deviceId = MonitorUtils::Display::SplitDisplayDeviceId(L"\\\\?\\DISPLAY#LEN414D");
            Assert::IsTrue(deviceId.instanceId.empty());
            Assert::IsFalse(MonitorUtils::Display::IsDisplayName(deviceId));
        }

        TEST_METHOD (MultiMonitorIdIsNotDisplayName)
        {
            const FancyZonesDataTypes::DeviceId deviceId{ .id = ZonedWindowProperties::MultiMonitorName, .instanceId = ZonedWindowProperties::MultiMonitorInstance };
            Assert::IsFalse(MonitorUtils::Display::IsDisplayName(deviceId));
        }

        TEST_METHOD (DisplayNameWithInstanceIdIsNotDisplayName)
        {
            auto deviceId = m_displayName1;
            deviceId.instanceId = m_laptopPanel.instanceId;
            Assert::IsFalse(MonitorUtils::Display::IsDisplayName(deviceId));
        }

        TEST_METHOD (DisplayNamesOfMonitors)
        {
            const std::vector<std::wstring> expected{ m_displayName2.id };
            Assert::IsTrue(expected == MonitorUtils::Display::GetDisplayNames({ Monitor(m_monitor1, m_laptopPanel), Monitor(m_monitor2, m_displayName2) }));
        }

        TEST_METHOD (NoDisplayNamesOfIdentifiedMonitors)
        {
            Assert::IsTrue(MonitorUtils::Display::GetDisplayNames({ Monitor(m_monitor1, m_laptopPanel), Monitor(m_monitor2, m_externalMonitor) }).empty());
        }

        TEST_METHOD (DisplayLostDeviceId)
        {
            const std::vector<std::wstring> expected{ m_displayName1.id };
            Assert::IsTrue(expected == MonitorUtils::Display::GetDisplayNamesWithLostDeviceId({ Monitor(m_monitor1, m_displayName1) }, { Known(m_monitor1, m_laptopPanel, m_laptopRect) }));
        }

        TEST_METHOD (DisplayWithAnotherNumberLostDeviceId)
        {
            // the known monitor was display 2 and the display without display device is display 1
            const std::vector<std::wstring> expected{ m_displayName1.id };
            Assert::IsTrue(expected == MonitorUtils::Display::GetDisplayNamesWithLostDeviceId({ Monitor(m_monitor1, m_displayName1) }, { Known(m_monitor2, m_externalMonitor, m_externalRect) }));
        }

        TEST_METHOD (OneOfTwoDisplaysLostDeviceId)
        {
            const std::vector<std::wstring> expected{ m_displayName2.id };
            const auto actual = MonitorUtils::Display::GetDisplayNamesWithLostDeviceId(
                { Monitor(m_monitor1, m_laptopPanel), Monitor(m_monitor2, m_displayName2) },
                { Known(m_monitor1, m_laptopPanel, m_laptopRect), Known(m_monitor2, m_externalMonitor, m_externalRect) });
            Assert::IsTrue(expected == actual);
        }


        TEST_METHOD (DisplayKnownByDisplayNameLostNothing)
        {
            Assert::IsTrue(MonitorUtils::Display::GetDisplayNamesWithLostDeviceId({ Monitor(m_monitor1, m_displayName1) }, { Known(m_monitor1, m_displayName1, m_laptopRect) }).empty());
        }

        TEST_METHOD (DisplayKnownByDisplayNameLostNothingWhenAnotherMonitorIsGone)
        {
            const auto actual = MonitorUtils::Display::GetDisplayNamesWithLostDeviceId(
                { Monitor(m_monitor2, m_displayName2) },
                { Known(m_monitor1, m_laptopPanel, m_laptopRect), Known(m_monitor2, m_displayName2, m_externalRect) });
            Assert::IsTrue(actual.empty());
        }

        TEST_METHOD (NewDisplayWithoutDisplayDeviceLostNothing)
        {
            const auto actual = MonitorUtils::Display::GetDisplayNamesWithLostDeviceId(
                { Monitor(m_monitor1, m_laptopPanel), Monitor(m_monitor2, m_displayName2) },
                { Known(m_monitor1, m_laptopPanel, m_laptopRect) });
            Assert::IsTrue(actual.empty());
        }

        TEST_METHOD (DisplayLostNothingWhenKnownMonitorIsOnAnotherDisplay)
        {
            // the known monitor was display 2, and it is identified as display 1 next to a display 2 without display device
            auto laptopPanelOnDisplay2 = m_laptopPanel;
            laptopPanelOnDisplay2.number = 2;
            const auto actual = MonitorUtils::Display::GetDisplayNamesWithLostDeviceId(
                { Monitor(m_monitor1, m_laptopPanel), Monitor(m_monitor2, m_displayName2) },
                { Known(m_monitor2, laptopPanelOnDisplay2, m_laptopRect) });
            Assert::IsTrue(actual.empty());
        }

        TEST_METHOD (DisplayLostNothingWithoutKnownMonitors)
        {
            Assert::IsTrue(MonitorUtils::Display::GetDisplayNamesWithLostDeviceId({ Monitor(m_monitor1, m_displayName1) }, {}).empty());
        }

        TEST_METHOD (TwoDisplayNamesOfWhichOneIsKnownLostOneDeviceId)
        {
            const std::vector<std::wstring> expected{ m_displayName1.id };
            const auto actual = MonitorUtils::Display::GetDisplayNamesWithLostDeviceId(
                { Monitor(m_monitor1, m_displayName1), Monitor(m_monitor2, m_displayName2) },
                { Known(m_monitor1, m_laptopPanel, m_laptopRect), Known(m_monitor2, m_displayName2, m_externalRect) });
            Assert::IsTrue(expected == actual);
        }

        TEST_METHOD (DisplayWithAnotherDisplayNameLostNothing)
        {
            // no known monitor with a device id is missing
            Assert::IsTrue(MonitorUtils::Display::GetDisplayNamesWithLostDeviceId({ Monitor(m_monitor1, m_displayName2) }, { Known(m_monitor1, m_displayName1, m_laptopRect) }).empty());
        }

        TEST_METHOD (OneOfTwoMonitorsWithSameIdLostDeviceId)
        {
            // two monitors of one model differ in the instance id only
            auto secondExternalMonitor = m_externalMonitor;
            secondExternalMonitor.instanceId = L"4&117e8519&0&UID4166";
            const std::vector<std::wstring> expected{ m_displayName2.id };
            const auto actual = MonitorUtils::Display::GetDisplayNamesWithLostDeviceId(
                { Monitor(m_monitor1, m_externalMonitor), Monitor(m_monitor2, m_displayName2) },
                { Known(m_monitor1, m_externalMonitor, m_externalRect), Known(m_monitor2, secondExternalMonitor, m_externalRect) });
            Assert::IsTrue(expected == actual);
        }

        TEST_METHOD (CheckDisplayNamesWithLostDeviceId)
        {
            const std::vector<std::wstring> expected{ m_displayName1.id };
            Assert::IsTrue(expected == MonitorUtils::Display::GetDisplayNamesToCheck({ Monitor(m_monitor1, m_displayName1) }, { Known(m_monitor1, m_laptopPanel, m_laptopRect) }, {}));
        }

        TEST_METHOD (CheckDisplayNameOnce)
        {
            // the display lost its device id and was checked before
            const std::vector<std::wstring> expected{ m_displayName1.id };
            Assert::IsTrue(expected == MonitorUtils::Display::GetDisplayNamesToCheck({ Monitor(m_monitor1, m_displayName1) }, { Known(m_monitor1, m_laptopPanel, m_laptopRect) }, { m_displayName1.id }));
        }

        TEST_METHOD (CheckDisplayNameThatWasCheckedBefore)
        {
            // the work area of the display has the display name as id, so the display lost no device id now
            const std::vector<std::wstring> expected{ m_displayName1.id };
            Assert::IsTrue(expected == MonitorUtils::Display::GetDisplayNamesToCheck({ Monitor(m_monitor1, m_displayName1) }, { Known(m_monitor1, m_displayName1, m_laptopRect) }, { m_displayName1.id }));
        }

        TEST_METHOD (CheckNewDisplayNameOfDisplayThatWasCheckedBefore)
        {
            // the display that was checked as display 1 is display 2 now
            const std::vector<std::wstring> expected{ m_displayName2.id };
            Assert::IsTrue(expected == MonitorUtils::Display::GetDisplayNamesToCheck({ Monitor(m_monitor1, m_displayName2) }, { Known(m_monitor1, m_displayName1, m_laptopRect) }, { m_displayName1.id }));
        }

        TEST_METHOD (DoNotCheckKnownDisplayNameThatWasNotCheckedBefore)
        {
            const std::vector<std::wstring> expected{ m_displayName1.id };
            const auto actual = MonitorUtils::Display::GetDisplayNamesToCheck(
                { Monitor(m_monitor1, m_displayName1), Monitor(m_monitor2, m_displayName2) },
                { Known(m_monitor1, m_displayName1, m_laptopRect), Known(m_monitor2, m_displayName2, m_externalRect) },
                { m_displayName1.id });
            Assert::IsTrue(expected == actual);
        }

        TEST_METHOD (DoNotCheckDisplayNameWithoutLostDeviceId)
        {
            Assert::IsTrue(MonitorUtils::Display::GetDisplayNamesToCheck({ Monitor(m_monitor1, m_displayName1) }, { Known(m_monitor1, m_displayName1, m_laptopRect) }, {}).empty());
        }

        TEST_METHOD (DoNotCheckIdentifiedDisplayThatWasCheckedBefore)
        {
            Assert::IsTrue(MonitorUtils::Display::GetDisplayNamesToCheck({ Monitor(m_monitor1, m_laptopPanel) }, { Known(m_monitor1, m_laptopPanel, m_laptopRect) }, { m_displayName1.id }).empty());
        }

        TEST_METHOD (KeepDeviceIdOfSameMonitorHandleAndWorkAreaRect)
        {
            std::vector<FancyZonesDataTypes::MonitorId> monitors{ Monitor(m_monitor1, m_displayName1) };

            Assert::IsTrue(MonitorUtils::Display::KeepDeviceIds(monitors, { m_displayName1.id }, { Known(m_monitor1, m_laptopPanel, m_laptopRect, L"serial-number") }, WorkAreaRects()));

            AssertDeviceId(m_laptopPanel, monitors[0].deviceId);
            Assert::AreEqual(std::wstring{ L"serial-number" }, monitors[0].serialNumber);
            Assert::IsTrue(m_monitor1 == monitors[0].monitor);
        }

        TEST_METHOD (KeepDeviceIdWithTheDisplayNumberOfTheKnownMonitor)
        {
            // the known monitor was display 2 and the display without display device is display 1
            auto laptopPanelOnDisplay2 = m_laptopPanel;
            laptopPanelOnDisplay2.number = 2;
            std::vector<FancyZonesDataTypes::MonitorId> monitors{ Monitor(m_monitor1, m_displayName1) };

            Assert::IsTrue(MonitorUtils::Display::KeepDeviceIds(monitors, { m_displayName1.id }, { Known(m_monitor1, laptopPanelOnDisplay2, m_laptopRect) }, WorkAreaRects()));

            AssertDeviceId(laptopPanelOnDisplay2, monitors[0].deviceId);
        }

        TEST_METHOD (KeepDeviceIdOfOneOfTwoDisplays)
        {
            std::vector<FancyZonesDataTypes::MonitorId> monitors{ Monitor(m_monitor1, m_laptopPanel), Monitor(m_monitor2, m_displayName2) };

            Assert::IsTrue(MonitorUtils::Display::KeepDeviceIds(monitors, { m_displayName2.id }, { Known(m_monitor1, m_laptopPanel, m_laptopRect), Known(m_monitor2, m_externalMonitor, m_externalRect) }, WorkAreaRects()));

            AssertDeviceId(m_laptopPanel, monitors[0].deviceId);
            AssertDeviceId(m_externalMonitor, monitors[1].deviceId);
        }

        TEST_METHOD (KeepDeviceIdOfTheMonitorWithTheSameInstanceId)
        {
            // two monitors of one model differ in the instance id only
            auto secondExternalMonitor = m_externalMonitor;
            secondExternalMonitor.instanceId = L"4&117e8519&0&UID4166";
            std::vector<FancyZonesDataTypes::MonitorId> monitors{ Monitor(m_monitor1, m_externalMonitor), Monitor(m_monitor2, m_displayName2) };

            Assert::IsTrue(MonitorUtils::Display::KeepDeviceIds(monitors, { m_displayName2.id }, { Known(m_monitor1, m_externalMonitor, m_laptopRect), Known(m_monitor2, secondExternalMonitor, m_externalRect) }, WorkAreaRects()));

            AssertDeviceId(secondExternalMonitor, monitors[1].deviceId);
        }

        TEST_METHOD (DoNotKeepDeviceIdOfAnotherMonitorHandle)
        {
            std::vector<FancyZonesDataTypes::MonitorId> monitors{ Monitor(m_monitor1, m_displayName1) };

            Assert::IsFalse(MonitorUtils::Display::KeepDeviceIds(monitors, { m_displayName1.id }, { Known(m_monitor2, m_laptopPanel, m_laptopRect) }, WorkAreaRects()));

            AssertDeviceId(m_displayName1, monitors[0].deviceId);
        }

        TEST_METHOD (DoNotKeepDeviceIdWhenWorkAreaRectChanged)
        {
            // the monitor handle of the external monitor is the handle of the laptop panel after the undock
            std::vector<FancyZonesDataTypes::MonitorId> monitors{ Monitor(m_monitor1, m_displayName1) };

            Assert::IsFalse(MonitorUtils::Display::KeepDeviceIds(monitors, { m_displayName1.id }, { Known(m_monitor1, m_externalMonitor, m_externalRect) }, WorkAreaRects()));

            AssertDeviceId(m_displayName1, monitors[0].deviceId);
        }

        TEST_METHOD (KeepDeviceIdOfAnotherMonitorWithTheSameHandleAndWorkAreaRect)
        {
            // known limit: the handle and the work area rectangle are all that identifies a display without display device
            std::vector<FancyZonesDataTypes::MonitorId> monitors{ Monitor(m_monitor1, m_displayName1) };

            Assert::IsTrue(MonitorUtils::Display::KeepDeviceIds(monitors, { m_displayName1.id }, { Known(m_monitor1, m_externalMonitor, m_laptopRect) }, WorkAreaRects()));

            AssertDeviceId(m_externalMonitor, monitors[0].deviceId);
        }

        TEST_METHOD (DoNotKeepDeviceIdWhenNumberOfMonitorsChanged)
        {
            std::vector<FancyZonesDataTypes::MonitorId> monitors{ Monitor(m_monitor1, m_displayName1) };

            Assert::IsFalse(MonitorUtils::Display::KeepDeviceIds(monitors, { m_displayName1.id }, { Known(m_monitor1, m_laptopPanel, m_laptopRect), Known(m_monitor2, m_externalMonitor, m_externalRect) }, WorkAreaRects()));

            AssertDeviceId(m_displayName1, monitors[0].deviceId);
        }

        TEST_METHOD (DoNotKeepDeviceIdForDisplayNameThatIsNotChecked)
        {
            std::vector<FancyZonesDataTypes::MonitorId> monitors{ Monitor(m_monitor1, m_displayName1) };

            Assert::IsFalse(MonitorUtils::Display::KeepDeviceIds(monitors, {}, { Known(m_monitor1, m_laptopPanel, m_laptopRect) }, WorkAreaRects()));

            AssertDeviceId(m_displayName1, monitors[0].deviceId);
        }

        TEST_METHOD (DoNotKeepDisplayName)
        {
            std::vector<FancyZonesDataTypes::MonitorId> monitors{ Monitor(m_monitor1, m_displayName1) };

            Assert::IsFalse(MonitorUtils::Display::KeepDeviceIds(monitors, { m_displayName1.id }, { Known(m_monitor1, m_displayName2, m_laptopRect) }, WorkAreaRects()));

            AssertDeviceId(m_displayName1, monitors[0].deviceId);
        }

        TEST_METHOD (DoNotKeepDeviceIdOfIdentifiedMonitor)
        {
            // the known monitor had the handle of the display without display device, and it is identified with another handle
            auto laptopPanelOnDisplay2 = m_laptopPanel;
            laptopPanelOnDisplay2.number = 2;
            std::vector<FancyZonesDataTypes::MonitorId> monitors{ Monitor(m_monitor1, m_displayName1), Monitor(m_monitor2, laptopPanelOnDisplay2) };

            Assert::IsFalse(MonitorUtils::Display::KeepDeviceIds(monitors, { m_displayName1.id }, { Known(m_monitor1, m_laptopPanel, m_laptopRect), Known(m_monitor2, m_externalMonitor, m_externalRect) }, WorkAreaRects()));

            AssertDeviceId(m_displayName1, monitors[0].deviceId);
            AssertDeviceId(laptopPanelOnDisplay2, monitors[1].deviceId);
        }

        TEST_METHOD (DoNotChangeIdentifiedMonitor)
        {
            std::vector<FancyZonesDataTypes::MonitorId> monitors{ Monitor(m_monitor1, m_externalMonitor, L"serial-number") };

            Assert::IsFalse(MonitorUtils::Display::KeepDeviceIds(monitors, { m_displayName1.id }, { Known(m_monitor1, m_laptopPanel, m_laptopRect) }, WorkAreaRects()));

            AssertDeviceId(m_externalMonitor, monitors[0].deviceId);
            Assert::AreEqual(std::wstring{ L"serial-number" }, monitors[0].serialNumber);
        }
    };
}

