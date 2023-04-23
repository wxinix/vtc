/*!
C++ Virtualization Library of Traffic Control Cabinet
Copyright (C) 2022  Wuping Xin

MPL 1.1/GPL 2.0/LGPL 2.1 tri-license
*/

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#define TEST

#include <doctest/doctest.h>
#include <vtc/vtc.hpp>

using namespace vtc;

//----------------------------------------------------
TEST_SUITE_BEGIN("Utility");

/*
// A trick to set up logger before entering any test cases.
[[maybe_unused]] static const auto logger_ready = []() noexcept {
  return vtc::setup_logger(std::filesystem::current_path(), "test");
}();
*/

TEST_CASE("VtcLogger is nullptr before calling setup_logger")
{
  CHECK(logger() == nullptr);
}

TEST_CASE("setup_logger works as expected")
{
  SUBCASE("setup_logger returns true with valid log file path") {
    CHECK(vtc::setup_logger(std::filesystem::current_path(), "test"));
  }

  SUBCASE("setup_logger throws exception with a logger name used before") {
    CHECK_THROWS(vtc::setup_logger(std::filesystem::current_path(), "test"));
  }

  SUBCASE("setup_logger returns false with invalid path") {
    std::string logger_name = "test";
    CHECK(vtc::setup_logger("C:/test/test", "test") == false);
    auto logger = vtc::logger();
    CHECK_EQ(logger->name(), logger_name + "_windbg");
  }
}

TEST_CASE("Channel composition channel IDs can be encoded as single index")
{
  Byte a = 1;
  Byte b = 2;

  Index I = a << 8 | b;
  CHECK(I == 0x0102);
}

TEST_CASE("Compile-time internal variable name can be retrieved")
{
  auto name_1 = type_name<mmu::_24VoltMonitor_I>();
  mmu::_24VoltMonitor_I var;
  CHECK(var.value == Bit::Off);
  auto name_2 = type_name<decltype(var)>();
  CHECK(name_1 == name_2);
}

TEST_CASE("get() integer sequence works as expected")
{
  auto seq = std::integer_sequence<unsigned, 9, 2, 5, 1, 9, 1, 15>{};
  auto index = 6;
  auto val = vtc::get(seq, index); // val equals to 15.
  CHECK_EQ(val, 15);
}

TEST_CASE("substring_as_array() works as expected")
{
  using namespace std::literals;
  constexpr auto sv = "substring_as_array() works as expected"sv;
  auto arr = vtc::substring_as_array(sv, std::make_index_sequence<sv.length()>{});
  CHECK_EQ(arr[1], 'u');
}

TEST_SUITE_END;

//----------------------------------------------------
TEST_SUITE_BEGIN("CU");

TEST_CASE("ValidCuVariable Concept passes for a valid cu variable.")
{
  auto cuv = cu::CuVariable<Byte, 0>{};
  CHECK(cu::ValidCuVariable<decltype(cuv)>);
}

TEST_SUITE_END;

//----------------------------------------------------
TEST_SUITE_BEGIN("IO");

TEST_CASE("Output variable NotActive can be set")
{
  using namespace io;

  io::variable<output::NotActive>() = Bit::Off;
  CHECK(io::variable<output::NotActive>() == Bit::Off);

  io::variable<output::NotActive>.value = Bit::On;
  CHECK(io::variable<output::NotActive>() == Bit::On);

  CHECK(std::is_same_v<std::remove_cvref_t<decltype(io::variable<output::NotActive>())>, std::atomic<Bit>>);
  CHECK(std::is_same_v<output::NotActive::value_t, Bit>);
  CHECK(std::is_same_v<output::NotActive::type, OutputType>);
}

TEST_CASE("Output variable ChannelGreenWalkDriver can be set")
{
  using namespace io;

  io::variable<output::ChannelGreenWalkDriver<1>>.value = Bit::Off;
  CHECK(io::variable<output::ChannelGreenWalkDriver<1>>.value == Bit::Off);

  io::variable<output::ChannelGreenWalkDriver<1>>.value = Bit::On;
  CHECK(io::variable<output::ChannelGreenWalkDriver<1>>.value == Bit::On);

  io::variable<output::ChannelGreenWalkDriver<1>>.value = Bit::Off;
}

TEST_CASE("Output variable has correct type tag")
{
  using namespace io;
  CHECK(std::is_same_v<output::ChannelGreenWalkDriver<1>::type, OutputType>);
}

TEST_CASE("Input variable has correct type tag")
{
  using namespace io;
  CHECK(std::is_same_v<input::RingForceOff<1>::type, InputType>);
}

TEST_SUITE_END;

//----------------------------------------------------
TEST_SUITE_BEGIN("MMU");

TEST_CASE("MMU variable LoadSwitchFlash can be set")
{
  using namespace mmu;

  mmu::variable<LoadSwitchFlash>.value = Bit::Off;
  CHECK(mmu::variable<LoadSwitchFlash>.value == Bit::Off);

  mmu::variable<LoadSwitchFlash>.value = Bit::On;
  CHECK(mmu::variable<LoadSwitchFlash>.value == Bit::On);
}

TEST_CASE("MMU variable LoadSwitchDriverFrame can be parsed")
{
  using namespace mmu;

  serial::LoadSwitchDriversFrame frame_1;
  std::array<Byte, 16> data = {0x10, 0x83, 0x00};
  data[0x03] = 0xC3;
  data[0x0F] = 0x80;
  frame_1 << data;

  CHECK(mmu::variable<LoadSwitchFlash>.value == Bit::On);

  CHECK(mmu::variable<ChannelGreenWalkDriver<1>>.value == Bit::On);
  CHECK(mmu::variable<ChannelGreenWalkDriver<2>>.value == Bit::Off);
  CHECK(mmu::variable<ChannelGreenWalkDriver<3>>.value == Bit::Off);
  CHECK(mmu::variable<ChannelGreenWalkDriver<4>>.value == Bit::On);

  data = {0x10, 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  serial::FrameType<0>::type frame_2;
  frame_2 << data;

  CHECK(mmu::variable<LoadSwitchFlash>.value == Bit::Off);
  CHECK(mmu::variable<ChannelGreenWalkDriver<1>>.value == Bit::Off);
  CHECK(mmu::variable<ChannelGreenWalkDriver<2>>.value == Bit::Off);
  CHECK(mmu::variable<ChannelGreenWalkDriver<3>>.value == Bit::Off);
  CHECK(mmu::variable<ChannelGreenWalkDriver<4>>.value == Bit::Off);
}

TEST_CASE("MMU compatibility card can be programmed")
{
  using namespace mmu;

  mmu::variable<ChannelCompatibilityStatus<1, 5>>.value = Bit::On;
  mmu::variable<ChannelCompatibilityStatus<1, 6>>.value = Bit::On;
  mmu::variable<ChannelCompatibilityStatus<2, 5>>.value = Bit::On;
  mmu::variable<ChannelCompatibilityStatus<2, 6>>.value = Bit::On;
  mmu::variable<ChannelCompatibilityStatus<3, 7>>.value = Bit::On;
  mmu::variable<ChannelCompatibilityStatus<3, 8>>.value = Bit::On;
  mmu::variable<ChannelCompatibilityStatus<4, 7>>.value = Bit::On;
  mmu::variable<ChannelCompatibilityStatus<4, 8>>.value = Bit::On;

  std::array<Byte, 3> data_in = {0x10, 0x83, 0x03};
  auto result = serial::Dispatch(data_in);
  CHECK(std::get<0>(result));
  CHECK(mmu::variable<ChannelCompatibilityStatus<1, 2>>.value == Bit::Off);

  // Tye 131 Response Frame bytes
  // Byte 3 - Bit 3 represents CH1-CH5 compatibility
  // Byte 3 - Bit 4 represents CH1-CH5 compatibility
  // 0x18 = 0b0001`1000
  CHECK(std::get<1>(result)[3] == 0x18); // We are checking CH1-Ch5 and CH1-CH6 are indeed set to ON.

  mmu::variable<ChannelCompatibilityStatus<1, 5>>.value = Bit::Off;
  mmu::variable<ChannelCompatibilityStatus<1, 6>>.value = Bit::Off;
  mmu::variable<ChannelCompatibilityStatus<2, 5>>.value = Bit::Off;
  mmu::variable<ChannelCompatibilityStatus<2, 6>>.value = Bit::Off;
  mmu::variable<ChannelCompatibilityStatus<3, 7>>.value = Bit::Off;
  mmu::variable<ChannelCompatibilityStatus<3, 8>>.value = Bit::Off;
  mmu::variable<ChannelCompatibilityStatus<4, 7>>.value = Bit::Off;
  mmu::variable<ChannelCompatibilityStatus<4, 8>>.value = Bit::Off;
}

TEST_CASE("MMU all channel compatibility can be set and get.")
{
  std::bitset<0x78> mmu16_comp_def_1{
      //@formatter:off
      /*    23456789ABCDEFG */
      /*1*/"000110000100000"
           /*2*/ "00110010100000"
           /*3*/  "0001100010000"
           /*4*/   "001101010000"
           /*5*/    "00010000000"
           /*6*/     "0010100000"
           /*7*/      "001000000"
           /*8*/       "01010000"
           /*9*/        "0100000"
           /*A*/         "010000"
           /*B*/          "00000"
           /*C*/           "0000"
           /*D*/            "000"
           /*E*/             "00"
           /*F*/              "0"
      //@formatter:on
  };

  mmu::reverse(mmu16_comp_def_1);
  auto str = mmu16_comp_def_1.to_string();

  mmu::SetMMU16ChannelCompatibility(mmu16_comp_def_1);

  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x02>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x03>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x04>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x05>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x06>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x07>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x08>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x09>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x0A>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x0B>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x0C>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x0D>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x0E>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x0F>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x10>>.value == Bit::Off);

  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x03>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x04>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x05>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x06>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x07>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x08>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x09>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x0A>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x0B>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x0C>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x0D>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x0E>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x0F>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x10>>.value == Bit::Off);

  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x04>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x05>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x06>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x07>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x08>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x09>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x0A>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x0B>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x0C>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x0D>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x0E>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x0F>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x10>>.value == Bit::Off);

  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x05>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x06>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x07>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x08>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x09>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x0A>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x0B>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x0C>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x0D>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x0E>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x0F>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x10>>.value == Bit::Off);

  std::bitset<0x78> mmu16_comp_def_2;
  mmu::GetMMU16ChannelCompatibility(mmu16_comp_def_2);
  CHECK(mmu16_comp_def_2[3] == mmu16_comp_def_1[3]);
  CHECK(mmu16_comp_def_2[4] == mmu16_comp_def_1[4]);
}

TEST_CASE("MMU all channel compatibility can be set by string.")
{
  mmu::SetMMU16ChannelCompatibility("00001020A020280202B02300A60218");

  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x02>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x03>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x04>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x05>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x06>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x07>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x08>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x09>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x0A>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x0B>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x0C>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x0D>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x0E>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x0F>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<1, 0x10>>.value == Bit::Off);

  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x03>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x04>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x05>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x06>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x07>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x08>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x09>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x0A>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x0B>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x0C>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x0D>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x0E>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x0F>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x10>>.value == Bit::Off);

  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x04>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x05>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x06>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x07>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x08>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x09>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x0A>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x0B>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x0C>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x0D>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x0E>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x0F>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<3, 0x10>>.value == Bit::Off);

  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x05>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x06>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x07>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x08>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x09>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x0A>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x0B>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x0C>>.value == Bit::On);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x0D>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x0E>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x0F>>.value == Bit::Off);
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<4, 0x10>>.value == Bit::Off);
}

TEST_CASE("MMU all channel compatibility can be zeroed-out.")
{
  mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x03>>.value = Bit::On;
  mmu::ZeroOutMMU16ChannelCompatibility();
  CHECK(mmu::variable<mmu::ChannelCompatibilityStatus<2, 0x03>>.value == Bit::Off);
}

TEST_SUITE_END;

//----------------------------------------------------
TEST_SUITE_BEGIN("SDLC Frame");

TEST_CASE("FrameBit can be instantiated")
{
  serial::FrameBit<mmu::LoadSwitchFlash, 127> framebit;
  CHECK(framebit.pos == 127);
}

TEST_CASE("Type 1 serial frame InputStatusRequestFrame can be parsed")
{
  serial::MMUInputStatusRequestFrame frame;
  std::array<Byte, 3> data = {0x10, 0x83, 0x01};
  frame << data;

  CHECK(frame.id == 0x01);
}

TEST_CASE("Type 3 serial frame MMUProgrammingRequestFrame can be parsed")
{
  serial::MMUProgrammingRequestFrame frame;
  std::array<Byte, 3> data = {0x10, 0x83, 0x03};
  frame << data;

  CHECK(frame.id == 0x03);
}

TEST_CASE("Type 3 serial frame MMUProgrammingRequestFrame can be dispatched")
{
  // Type 3 Command Frame bytes
  std::array<Byte, 3> data_in = {0x10, 0x83, 0x03};
  mmu::variable<mmu::ChannelCompatibilityStatus<0x01, 0x02>>.value = Bit::On;
  auto result = serial::Dispatch(data_in);
  CHECK(std::get<0>(result));
  // Tye 131 Response Frame bytes
  CHECK(std::get<1>(result).size() == serial::FrameType<131>::type::bytesize);
  CHECK(std::get<1>(result)[3] == 0x01); // Byte 3 represents CH1-CH2 compatibility
  CHECK(std::get<1>(result)[4] == 0x00);
  mmu::variable<mmu::ChannelCompatibilityStatus<0x01, 0x02>>.value = Bit::Off;
}

TEST_CASE("Type 9 serial frame DateTimeBroadcastFrame can be parsed")
{
  serial::DateTimeBroadcastFrame frame;
  std::array<Byte, 12>     // M     D     Y     H     M     S     0.1   TF    DET
  data = {0xFF, 0x83, 0x09, 0x03, 0x12, 0x16, 0x11, 0x20, 0x00, 0x00, 0x01, 0x02}; // 03/18/2022, 17:32:00.0
  frame << data;

  using namespace broadcast;

  CHECK(frame.id == 0x09);
  CHECK(broadcast::variable<CuReportedDay>.value == 18);
  CHECK(broadcast::variable<CuReportedMonth>.value == 3);
  CHECK(broadcast::variable<CuReportedYear>.value == 22);
  CHECK(broadcast::variable<CuReportedHour>.value == 17);
  CHECK(broadcast::variable<CuReportedMinutes>.value == 32);
  CHECK(broadcast::variable<CuReportedSeconds>.value == 0);
  CHECK(broadcast::variable<CuReportedTenthsOfSeconds>.value == 0);
  CHECK(broadcast::variable<CuReportedTfBiuPresence<1>>.value == Bit::On);
  CHECK(broadcast::variable<CuReportedDrBiuPresence<2>>.value == Bit::On);
}

TEST_SUITE_END;

//----------------------------------------------------
TEST_SUITE_BEGIN("Serial Device");

TEST_CASE("SerialApiModule can be loaded.")
{
  CHECK(serial::device::SerialDevice::is_apimodule_loaded());
}

TEST_CASE("SerialApiModule can be set idle mode.")
{
  REQUIRE(serial::device::SerialDevice::is_apimodule_loaded());

  serial::device::DeviceHandle dev{nullptr};
  auto result = serial::device::SerialDevice::apimodule()
      .open("\x4D\x47\x48\x44\x4C\x43\x31", dev); // NOLINT(modernize-raw-string-literal)
  REQUIRE(0 == result);

  result = serial::device::SerialDevice::apimodule().set_idle_mode(dev, serial::device::HdlcIdleMode::Ones);
  CHECK(0 == result);

  result = serial::device::SerialDevice::apimodule().close(dev);
  CHECK(0 == result);
}

TEST_CASE("SerialApiModule can set option.")
{
  REQUIRE(serial::device::SerialDevice::is_apimodule_loaded());

  serial::device::DeviceHandle dev{nullptr};
  auto result = serial::device::SerialDevice::apimodule()
      .open("\x4D\x47\x48\x44\x4C\x43\x31", dev); // NOLINT(modernize-raw-string-literal)
  REQUIRE(0 == result);

  result = serial::device::SerialDevice::apimodule().set_option(dev, serial::device::DeviceOptionTag::RxPoll, 0);
  CHECK(0 == result);

  result = serial::device::SerialDevice::apimodule().close(dev);
  CHECK(0 == result);
}

TEST_CASE("SerialApiModule can set params.")
{
  using namespace vtc::serial::device;
  REQUIRE(SerialDevice::is_apimodule_loaded());

  DeviceHandle dev{nullptr};
  auto result =
      SerialDevice::apimodule().open("\x4D\x47\x48\x44\x4C\x43\x31", dev); // NOLINT(modernize-raw-string-literal)
  REQUIRE(0 == result);

  SerialDeviceParams params{};
  memset(&params, 0, sizeof(params));
  params.mode = 2;
  params.loopback = 0;
  params.flags = static_cast<uint16_t>(HdlcRxClkSource::RxClkPin) + static_cast<uint16_t>(HdlcTxClkSource::BRG);
  params.encoding = static_cast<uint8_t>(HdlcEncoding::NRZ);
  params.clock = 153600;
  params.crc = static_cast<uint16_t>(HdlcCrcType::CcittCrc16);
  params.addr = 0xFF;

  result = serial::device::SerialDevice::apimodule().set_params(dev, params);
  CHECK(0 == result);

  result = serial::device::SerialDevice::apimodule().close(dev);
  CHECK(0 == result);
}

TEST_CASE("SerialDevice can be created ready.")
{
  using namespace vtc::serial;
  auto device = serial::device::SerialDevice{"MGHDLC1"};
  CHECK(device.ready());
}

TEST_SUITE_END;

//----------------------------------------------------
TEST_SUITE_BEGIN("HILS Controller Interface");

struct HilsCITestObject : hils::HilsCI
{
  HilsCITestObject() = default;

  bool load_config()
  {
    auto path = fs::current_path() / "hilsci.xml";
    return fs::exists(path) && hils::HilsCI::load_config(path);
  }

  static bool device_ready()
  {
    using namespace vtc::serial;
    auto device = serial::device::SerialDevice{"MGHDLC1"};
    return serial::device::SerialDevice::ready();
  }
};

TEST_CASE("Hardware-in-the-Loop controller interface config can be loaded")
{
  using namespace vtc::hils;
  auto test_obj = HilsCITestObject{};
  REQUIRE(test_obj.device_ready());

  SUBCASE("load_config test") {
    CHECK(test_obj.load_config());
    auto &loadswitch_wirings = test_obj.loadswitch_wirings();
    auto &loadswitch_wiring = std::get<0>(loadswitch_wirings);
    auto state = std::get<0>(loadswitch_wiring).state();
    CHECK(state == LoadswitchChannelState::Blank);

    io::variable<io::output::ChannelGreenWalkDriver<1>>.value = Bit::On;
    io::variable<io::output::ChannelYellowPedClearDriver<1>>.value = Bit::On;
    state = std::get<0>(loadswitch_wiring).state();
    CHECK(state == LoadswitchChannelState::Blank);
  }
}

TEST_SUITE_END;