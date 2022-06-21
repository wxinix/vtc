/*!
  C++ Virtualization Library of Traffic Cabinet
  Copyright (C) 2022  Wuping Xin

  MPL 1.1/GPL 2.0/LGPL 2.1 tri-license
*/

#ifndef VTC_VTC_HPP
#define VTC_VTC_HPP

#include <array>
#include <atomic>
#include <bitset>
#include <filesystem>
#include <functional>
#include <span>

#ifdef _WIN32
#include <Windows.h>
#elif __linux__
#include <dlfcn.h>
#else
#error Unsupported platform.
#endif

#include <matchit/matchit.hpp>
#include <pugixml/pugixml.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

namespace vtc {

namespace fs = std::filesystem;

namespace {

struct LoggerHolder
{
  static std::atomic<std::shared_ptr<spdlog::logger>> logger;
};

std::atomic<std::shared_ptr<spdlog::logger>> LoggerHolder::logger = std::atomic<std::shared_ptr<spdlog::logger>>{};

}

std::shared_ptr<spdlog::logger> logger()
{
  return LoggerHolder::logger;
}

void setup_logger(const fs::path &a_path, const std::string &a_name)
{
  auto p = a_path / "log";
  std::error_code ec;

  std::shared_ptr<spdlog::logger> the_logger;

  if (fs::create_directory(p, ec) || fs::exists(p))
    the_logger = spdlog::rotating_logger_mt(a_name, (p / (a_name + "-log.txt")).string(), 1024 * 1024, 3);
  else
#if _WIN32
    the_logger = spdlog::synchronous_factory::template create<spdlog::sinks::windebug_sink_mt>(a_name);
#else
  the_logger = spdlog::default_logger();
#endif
  LoggerHolder::logger = the_logger;
}


// A dirty trick to generate template specialization tag.
#define AUTO_TAG_ID __LINE__

enum class Bit : bool
{
  Off = false,
  On = true
};

using Byte = uint8_t;
using Integer = uint32_t;
using Word = uint16_t;

using Index = uint16_t;
using Tag = uint32_t;

template<Index I, size_t N>
concept ValidIndex = (I >= 1) && (I <= N);

template<typename T>
concept ValidValueType = std::is_same_v<T, Bit>
    || std::is_same_v<T, Byte>
    || std::is_same_v<T, Word>
    || std::is_same_v<T, Integer>;

template<typename T, Index I> /* */
requires ValidValueType<T>
struct Variable
{
  using value_t = T;

  Variable() = default;
  Variable(Variable &) = delete;
  Variable(Variable &&) = delete;
  Variable &operator=(Variable &) = delete;
  Variable &operator=(Variable &&) = delete;

  auto &operator()()
  {
    return value;
  }

  static constexpr Index index{I};
  std::atomic<T> value{};
};

template<typename T>
using ValueType = typename T::Variable::value_t;

/* For getting variable name during compile time.

   References:

   1. StackOverflow: A standard way for getting variable name at compile time
      https://stackoverflow.com/questions/38696440/a-standard-way-for-getting-variable-name-at-compile-time

   2. Getting an Unmangled Type Name at Compile Time - a really useful and obscure technique
      https://bitwizeshift.github.io/posts/2021/03/09/getting-an-unmangled-type-name-at-compile-time/
 */

template<std::size_t ...Is>
constexpr auto substring_as_array(std::string_view str, std::index_sequence<Is...>)
{
  return std::array{str[Is]..., '\n'};
}

template<typename T>
constexpr auto type_name_array()
{
#if defined(__clang__)
  constexpr auto prefix   = std::string_view{"[T = "};
  constexpr auto suffix   = std::string_view{"]"};
  constexpr auto function = std::string_view{__PRETTY_FUNCTION__};
#elif defined(__GNUC__)
  constexpr auto prefix = std::string_view{"with T = "};
  // Note signature type_name_array() is constexpr auto, then the return type is removed, so suffix is "]".
  // Check this https://godbolt.org/z/eK4j6xTbe
  constexpr auto suffix = std::string_view{"]"};
  constexpr auto function = std::string_view{__PRETTY_FUNCTION__};
#elif defined(_MSC_VER)
  constexpr auto prefix = std::string_view{"type_name_array<"};
  constexpr auto suffix = std::string_view{">(void)"};
  constexpr auto function = std::string_view{__FUNCSIG__};
#else
# error Unsupported compiler
#endif

  constexpr auto start = function.find(prefix) + prefix.size();
  constexpr auto end = function.rfind(suffix);

  static_assert(start < end);

  constexpr auto name = function.substr(start, (end - start));
  return substring_as_array(name, std::make_index_sequence<name.size()>{});
}

template<typename T>
struct type_name_holder
{
  static inline constexpr auto value = type_name_array<T>();
};

template<typename T>
constexpr auto variable_type_name() -> std::string_view
{
  constexpr auto &value = type_name_holder<T>::value;
  return std::string_view{value.data(), value.size()};
}

template<Index Offset, typename SeqType> /* */
requires (Offset >= 0)
struct offset_sequence;

template<Index Offset, Index... Is>
struct offset_sequence<Offset, std::integer_sequence<Index, Is...>>
{
  using type = std::integer_sequence<Index, Is + (Offset + 1)...>;
};

template<Index Offset, typename SeqType>
using offset_sequence_t = typename offset_sequence<Offset, SeqType>::type;

template<typename T, T... Is>
[[maybe_unused]] constexpr T get(std::integer_sequence<T, Is...>, std::size_t i)
{
  constexpr T arr[] = {Is...};
  return arr[i];
}

template<Index I, typename SeqType>
struct add_sequence_front;

/*!
 * Add a new integer to the front of an existing integer sequence.
 * @tparam I  - The integer value to be added to the front of the existing sequence.
 * @tparam Is - The integer sequence, for which the extra integer value will be added to its front.
 */
template<Index I, Index... Is>
struct add_sequence_front<I, std::integer_sequence<Index, Is...>>
{
  using type = std::integer_sequence<Index, I, Is...>;
};

namespace cu {

struct CuVariableType
{
};

template<typename T>
concept ValidCuVariable = std::is_same_v<typename T::type, CuVariableType>;

template<Tag, typename ValueT, Index I = 0>
struct [[maybe_unused]] CuVariable : Variable<ValueT, I>
{
  using type = CuVariableType;

  CuVariable() = default;
  CuVariable(CuVariable &) = delete;
  CuVariable(CuVariable &&) = delete;
  CuVariable &operator=(CuVariable &) = delete;
  CuVariable &operator=(CuVariable &&) = delete;
};

namespace phase {

constexpr size_t max_phases{40};
[[maybe_unused]] constexpr size_t max_phase_groups{5};

}

namespace detector {

constexpr size_t max_vehicle_detectors{128};
[[maybe_unused]] constexpr size_t max_vehicle_detector_status_groups{40};
constexpr size_t max_pedestrian_detectors{72};

}

namespace ring {

constexpr size_t max_rings{16};
[[maybe_unused]] constexpr size_t max_sequences{20};
[[maybe_unused]] constexpr size_t max_ring_control_groups{2};

}

namespace channel {

constexpr size_t max_channels{32};
[[maybe_unused]] constexpr size_t max_channel_status_groups{4};

}

namespace overlap {

constexpr size_t max_overlaps{32};
[[maybe_unused]] constexpr size_t max_overlap_status_groups{4};

}

namespace preempt {

constexpr size_t max_preempts{40};

}

namespace unit {

[[maybe_unused]] constexpr size_t max_alarm_groups{1};
constexpr size_t max_special_function_outputs{16};

}

namespace coord {

constexpr size_t max_patterns{128};
[[maybe_unused]] constexpr size_t max_splits{128};

}

namespace timebase_asc {

[[maybe_unused]] constexpr size_t max_timebase_asc_actions{64};

}

namespace prioritor {

constexpr size_t max_prioritors{16};
[[maybe_unused]] constexpr size_t max_prioritor_groups{9};

}

template<typename T>/**/
requires ValidCuVariable<T>
T variable{};

} // end of namespace vc::cu

namespace biu {

constexpr size_t max_det_bius{8};
constexpr size_t max_tf_bius{8};

struct BiuVariableType
{
};

template<typename T>
concept ValidBiuVariable = std::is_same_v<typename T::type, BiuVariableType>;

template<Tag, typename ValueT, Index I = 0>
struct [[maybe_unused]] BiuVariable : Variable<ValueT, I>
{
  using type = BiuVariableType;

  BiuVariable() = default;
  BiuVariable(BiuVariable &) = delete;
  BiuVariable(BiuVariable &&) = delete;
  BiuVariable &operator=(BiuVariable &) = delete;
  BiuVariable &operator=(BiuVariable &&) = delete;
};

template<typename T>/**/
requires ValidBiuVariable<T>
T variable{};

} // end of namespace vc::biu

namespace io {

struct IoVariableType
{
};

template<typename T>
concept ValidIoVariable = std::is_same_v<typename T::type, IoVariableType>;

template<Tag, typename ValueT, Index I = 0>
struct IoVariable : Variable<ValueT, I>
{
  using type = IoVariableType;

  IoVariable() = default;
  IoVariable(IoVariable &) = delete;
  IoVariable(IoVariable &&) = delete;
  IoVariable &operator=(IoVariable &) = delete;
  IoVariable &operator=(IoVariable &&) = delete;
};

template<typename T>/**/
requires ValidIoVariable<T>
T variable{};

namespace output {

using AltFlashState [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using AuxFunctionState [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

template<Index I>
/**/requires ValidIndex<I, cu::channel::max_channels>
using ChannelGreenWalkDriver = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::channel::max_channels>
using ChannelRedDoNotWalkDriver = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::channel::max_channels>
using ChannelYellowPedClearDriver = IoVariable<AUTO_TAG_ID, Bit, I>;

using CustomAlarm [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

template<Index I>
/**/requires ValidIndex<I, biu::max_det_bius>
using DetectorReset = IoVariable<AUTO_TAG_ID, Byte, I>;

using FlashState [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using GlobalVariable [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using NotActive
    = IoVariable<AUTO_TAG_ID, Bit>;

template<Index I>
/**/requires ValidIndex<I, cu::overlap::max_overlaps>
using OverlapGreen [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::overlap::max_overlaps>
using OverlapProtectedGreen [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::overlap::max_overlaps>
using OverlapRed [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::overlap::max_overlaps>
using OverlapYellow [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::phase::max_phases>
using PedCall [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::phase::max_phases>
using PhaseAdvWarning [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::phase::max_phases>
using PhaseCheck = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::phase::max_phases>
using PhaseDoNotWalk [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::phase::max_phases>
using PhaseGreen [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::phase::max_phases>
using PhaseNext = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::phase::max_phases>
using PhaseOmit [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::phase::max_phases>
using PhaseOn = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::phase::max_phases>
using PhasePedClearance [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::phase::max_phases>
using PhasePreClear [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::phase::max_phases>
using PhasePreClear2 [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::phase::max_phases>
using PhaseRed [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::phase::max_phases>
using PhaseWalk [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::phase::max_phases>
using PhaseYellow [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::preempt::max_preempts>
using PreemptStatus = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::preempt::max_preempts>
using PreemptStatusFlash [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

using StatusBitA_Ring_1
    = IoVariable<AUTO_TAG_ID, Bit>;

using StatusBitB_Ring_1
    = IoVariable<AUTO_TAG_ID, Bit>;

using StatusBitC_Ring_1
    = IoVariable<AUTO_TAG_ID, Bit>;

using StatusBitA_Ring_2
    = IoVariable<AUTO_TAG_ID, Bit>;

using StatusBitB_Ring_2
    = IoVariable<AUTO_TAG_ID, Bit>;

using StatusBitC_Ring_2
    = IoVariable<AUTO_TAG_ID, Bit>;

template<Index I>
/**/requires ValidIndex<I, cu::unit::max_special_function_outputs>
using SpecialFunction = IoVariable<AUTO_TAG_ID, Bit, I>;

using UnitAutomaticFlash
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitFaultMonitor [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitFreeCoordStatus
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitOffset_1
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitOffset_2
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitOffset_3
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitTBCAux_1
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitTBCAux_2
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitTBCAux_3
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitTimingPlanA
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitTimingPlanB
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitTimingPlanC
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitTimingPlanD
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitVoltageMonitor [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using Watchdog [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

} // end of namespace vc::io::output

namespace input {

template<Index I>
/**/requires ValidIndex<I, cu::detector::max_vehicle_detectors>
using ChannelFaultStatus [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

using CoordFreeSwitch
    = IoVariable<AUTO_TAG_ID, Bit>;

using CustomAlarm [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using DoorAjor [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using ManualControlGroupAction [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using MinGreen_2 [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using NotActive
    = IoVariable<AUTO_TAG_ID, Bit>;

template<Index I>
/**/requires ValidIndex<I, cu::overlap::max_overlaps>
using OverlapOmit [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::coord::max_patterns>
using PatternInput [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::detector::max_pedestrian_detectors>
using PedDetCall = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::phase::max_phases>
using PhaseForceOff [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::phase::max_phases>
using PhaseHold [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::phase::max_phases>
using PhasePedOmit = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::phase::max_phases>
using PhasePhaseOmit = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::preempt::max_preempts>
using PreemptGateDown [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::preempt::max_preempts>
using PreemptGateUp [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::preempt::max_preempts>
using PreemptHighPrioritorLow [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::preempt::max_preempts>
using PreemptInput = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::preempt::max_preempts>
using PreemptInputCRC [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::preempt::max_preempts>
using PreemptInputNormalOff [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::preempt::max_preempts>
using PreemptInputNormalOn [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::prioritor::max_prioritors>
using PrioritorCheckIn [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::prioritor::max_prioritors>
using PrioritorCheckOut [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires (I >= 1)
using PrioritorPreemptDetector [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::ring::max_rings>
using RingForceOff = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::ring::max_rings>
using RingInhibitMaxTermination = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::ring::max_rings>
using RingMax2Selection = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::ring::max_rings>
using RingMax3Selection [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::ring::max_rings>
using RingOmitRedClearance = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::ring::max_rings>
using RingPedestrianRecycle = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::ring::max_rings>
using RingRedRest = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::ring::max_rings>
using RingStopTiming = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::ring::max_rings>
using SpecialFunctionInput [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Bit, I>;

using UnitAlarm_1
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitAlarm_2
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitAlternateSequenceA
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitAlternateSequenceB
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitAlternateSequenceC
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitAlternateSequenceD
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitAutomaticFlash
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitCallPedNAPlus [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitCallToNonActuated_1
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitCallToNonActuated_2
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitClockReset [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitCMUMMUFlashStatus
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitDimming
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitExternWatchDog [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitExternalMinRecall
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitExternalStart
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitIndicatorLampControl [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitIntervalAdvance
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitIOModeBit_0 [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitIOModeBit_1 [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitIOModeBit_2 [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitIOModeBit_3 [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitITSLocalFlashSense [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitLocalFlash
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitLocalFlashSense [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitManualControlEnable
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitOffset_1
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitOffset_2
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitOffset_3
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitSignalPlanA [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitSignalPlanB [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitStopTIme [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitSystemAddressBit_0
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitSystemAddressBit_1
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitSystemAddressBit_2
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitSystemAddressBit_3
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitSystemAddressBit_4
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitTBCHoldOnline [[maybe_unused]]
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitTBCOnline
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitTestInputA
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitTestInputB
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitTestInputC
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitTimingPlanA
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitTimingPlanB
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitTimingPlanC
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitTimingPlanD
    = IoVariable<AUTO_TAG_ID, Bit>;

using UnitWalkRestModifier
    = IoVariable<AUTO_TAG_ID, Bit>;

template<Index I>
/**/requires ValidIndex<I, cu::detector::max_vehicle_detectors>
using VehicleDetCall = IoVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, biu::max_det_bius>
using VehicleDetReset [[maybe_unused]] = IoVariable<AUTO_TAG_ID, Byte, I>;

} // end of namespace vc::io::input

} // end of namespace vc::io

namespace mmu {

struct MmuVariableType
{
};

template<typename T>
concept ValidMmuVariable = std::is_same_v<typename T::type, MmuVariableType>;

template<Tag, typename ValueT, Index I = 0>
struct MmuVariable : Variable<ValueT, I>
{
  using type = MmuVariableType;

  MmuVariable() = default;
  MmuVariable(MmuVariable &) = delete;
  MmuVariable(MmuVariable &&) = delete;
  MmuVariable &operator=(MmuVariable &) = delete;
  MmuVariable &operator=(MmuVariable &&) = delete;
};

template<Index I>
/**/requires ValidIndex<I, cu::channel::max_channels>
using ChannelGreenWalkStatus = MmuVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::channel::max_channels>
using ChannelRedDoNotWalkStatus = MmuVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::channel::max_channels>
using ChannelYellowPedClearStatus = MmuVariable<AUTO_TAG_ID, Bit, I>;

using ControllerVoltMonitor
    = MmuVariable<AUTO_TAG_ID, Bit>;

using _24VoltMonitor_I
    = MmuVariable<AUTO_TAG_ID, Bit>;

using _24VoltMonitor_II
    = MmuVariable<AUTO_TAG_ID, Bit>;

using _24VoltMonitorInhibit
    = MmuVariable<AUTO_TAG_ID, Bit>;

using Reset
    = MmuVariable<AUTO_TAG_ID, Bit>;

using RedEnable
    = MmuVariable<AUTO_TAG_ID, Bit>;

using Conflict
    = MmuVariable<AUTO_TAG_ID, Bit>;

using RedFailure
    = MmuVariable<AUTO_TAG_ID, Bit>;

using DiagnosticFailure
    = MmuVariable<AUTO_TAG_ID, Bit>;

using MinimumClearanceFailure
    = MmuVariable<AUTO_TAG_ID, Bit>;

using Port1TimeoutFailure
    = MmuVariable<AUTO_TAG_ID, Bit>;

using FailedAndOutputRelayTransferred
    = MmuVariable<AUTO_TAG_ID, Bit>;

using FailedAndImmediateResponse
    = MmuVariable<AUTO_TAG_ID, Bit>;

using LocalFlashStatus
    = MmuVariable<AUTO_TAG_ID, Bit>;

using StartupFlashCall
    = MmuVariable<AUTO_TAG_ID, Bit>;

using FYAFlashRateFailure
    = MmuVariable<AUTO_TAG_ID, Bit>;

template<Index I>
/**/requires ValidIndex<I, cu::channel::max_channels>
using MinimumYellowChangeDisable = MmuVariable<AUTO_TAG_ID, Bit, I>;

using MinimumFlashTimeBit_0
    = MmuVariable<AUTO_TAG_ID, Bit>;

using MinimumFlashTimeBit_1
    = MmuVariable<AUTO_TAG_ID, Bit>;

using MinimumFlashTimeBit_2
    = MmuVariable<AUTO_TAG_ID, Bit>;

using MinimumFlashTimeBit_3
    = MmuVariable<AUTO_TAG_ID, Bit>;

using _24VoltLatch
    = MmuVariable<AUTO_TAG_ID, Bit>;

using CVMFaultMonitorLatch
    = MmuVariable<AUTO_TAG_ID, Bit>;

/*!
 * The compatibility status of two MMU channels.
 * @tparam Ix - MMU channel.
 * @tparam Iy - MMU channel.
 * @remarks The two channel IDs are encoded as the index. index = left-hand-side channel as high-byte,
 * right-hand-size channel as low-byte
 */
template<Index Ix, Index Iy>
/**/requires ValidIndex<Ix, cu::channel::max_channels> && ValidIndex<Iy, cu::channel::max_channels> && (Ix < Iy)
using ChannelCompatibilityStatus = MmuVariable<AUTO_TAG_ID, Bit, (Ix << 8) | Iy>;

template<Index I>
/**/requires ValidIndex<I, cu::channel::max_channels>
using ChannelGreenWalkDriver = MmuVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::channel::max_channels>
using ChannelRedDoNotWalkDriver = MmuVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, cu::channel::max_channels>
using ChannelYellowPedClearDriver = MmuVariable<AUTO_TAG_ID, Bit, I>;

using LoadSwitchFlash
    = MmuVariable<AUTO_TAG_ID, Bit>;

template<typename T>/**/
requires ValidMmuVariable<T>
T variable{};

/*!
 * The size of channel compatibility set. For example, for Channel 1 of MMU16,
 * its compatability set includes 1-2, 1-3, 1-4, ..., 1-16, thus the size is 15.
 * @tparam Channel - The given MMU chanel.
 * @tparam MaxChannel - Max number of channels the MMU supports.
 * @return The size of the compatibility set of the given channel.
 */
template<size_t Channel, size_t MaxChannel>
requires ((Channel >= 1) && (Channel <= MaxChannel))
constexpr size_t ChannelSegmentSize()
{
  return (MaxChannel - Channel);
}

/*!
 * The start position (0-based) for the given MMU channel in the fixed-size MMU channel compatibility byte array.
 * @tparam Channel - The given MMU channel.
 * @tparam MaxChannel - Max number of channels the MMU supports.
 * @return The start position (0-based) for the given MMU channel.
 * @remarks MMU channel compatibility is represented by a fixed-size byte array, for
 * MMU16, the byte array has 120 bytes. Each channel has a start position and total number of relevant
 * bytes in the stream describing the channel's compatibility.
 */
template<size_t Channel, size_t MaxChannel = 16>
requires ((Channel >= 1) && (Channel <= MaxChannel))
constexpr size_t ChannelSegmentStartPos()
{
  if constexpr (Channel == 1) {
    return 0;
  } else if constexpr (Channel == 2) {
    return ChannelSegmentSize<1, MaxChannel>();
  } else {
    return ChannelSegmentSize<Channel - 1, MaxChannel>() + ChannelSegmentStartPos<Channel - 1>();
  }
}

/* 
  Sample Channel Compatibility Indexes for Each Chanel 1 - 16
  
   Channel 1 - 2, 3, 4, 5, 6, 7, 8, 9, A, B, C, D, E, F, G
           2 -    3, 4, 5, 6, 7, 8, 9, A, B, C, D, E, F, G
           3 -       4, 5, 6, 7, 8, 9, A, B, C, D, E, F, G
           4 -          5, 6, 7, 8, 9, A, B, C, D, E, F, G
           5 -             6, 7, 8, 9, A, B, C, D, E, F, G
           6 -                7, 8, 9, A, B, C, D, E, F, G
           7 -                   8, 9, A, B, C, D, E, F, G
           8 -                      9, A, B, C, D, E, F, G
           9 -                         A, B, C, D, E, F, G
           A -                            B, C, D, E, F, G
           B -                               C, D, E, F, G
           C -                                  D, E, F, G
           D -                                     E, F, G
           E -                                        F, G
           F -                                           G
*/

// @formatter:off
/*!
 * For a given MMU channel, the IDs of other channels that is paired to the subject
 * chanel for compatibility definition.
*/
template<Index ChannelID, Index MaxChannel = 16> /* */
requires (ChannelID >= 1) && (ChannelID < MaxChannel)
using ChannelCompatibilityPairedIndexes = offset_sequence_t<ChannelID, std::make_integer_sequence<Index, MaxChannel - ChannelID>>;
// @formatter:on

namespace impl {
/*!
 * Set MMU16 compatibility relationship to the relevant bit position of the fixed-size bitset
 * for a given channel, with regard to other channels.
 * @tparam Ix  - Subject channel.
 * @tparam Iy  - The first channel to be compared to the subject channel.
 * @tparam Iys - The rest of the channels to be compared to the subject channel.
 * @param a_mmu16_comp - Bit set representing the MMU16 compatibility card.
 * @param a_seq - Compile time integer sequence, the first one being the subject channel.
 */
template<Index Ix, Index Iy, Index... Iys> // @formatter:off
void SetMMU16ChannelCompatibility(const std::bitset<0x78> &a_mmu16_comp, std::integer_sequence<Index, Ix, Iy, Iys...> a_seq)
{
  mmu::variable<ChannelCompatibilityStatus < Ix, Iy>>.value = static_cast<Bit>(a_mmu16_comp[ChannelSegmentStartPos<Ix>() + Iy - Ix - 1]);

  if constexpr (a_seq.size() > 2) {
    return SetMMU16ChannelCompatibility(a_mmu16_comp, std::integer_sequence<Index, Ix, Iys...>{});
  } else {
    return;
  } // @formatter:on
}

/*!
 * Get MMU16 compatibility relationship from the relevant bit position of the fixed-size bit set
 * for a given channel, with regard to other channels.
 * @tparam Ix  - Subject channel.
 * @tparam Iy  - The first channel to be compared to the subject channel.
 * @tparam Iys - The rest of the channels to be compared to the subject channel.
 * @param a_mmu16_comp - Bit set representing the MMU16 compatibility card.
 * @param a_seq - Compile time integer sequence, the first one being the subject channel.
 */
template<Index Ix, Index Iy, Index... Iys>
void GetMMU16ChannelCompatibility(std::bitset<0x78> &a_mmu16_comp, std::integer_sequence<Index, Ix, Iy, Iys...> a_seq)
{ // @formatter:off
  a_mmu16_comp[ChannelSegmentStartPos<Ix>() + Iy - Ix - 1] = (mmu::variable<ChannelCompatibilityStatus<Ix, Iy>>.value == Bit::On) ? 1 : 0;

  if constexpr (a_seq.size() > 2) {
    return GetMMU16ChannelCompatibility(a_mmu16_comp, std::integer_sequence<Index, Ix, Iys...>{});
  } else {
    return;
  } // @formatter:on
}

} // end of namespace mmu::impl

/*!
 * Set MMU16 channel compatibility to the fixed-size bit set.
 * @tparam Ix - The starting channel, default to 1.
 * @param [out] a_mmu16_comp - The compatibility bit set.
 */
template<Index Ix = 1>
void SetMMU16ChannelCompatibility(const std::bitset<0x78> &a_mmu16_comp)
{
  if constexpr(Ix < 16) { // @formatter:off
    impl::SetMMU16ChannelCompatibility(a_mmu16_comp, typename add_sequence_front<Ix, ChannelCompatibilityPairedIndexes<Ix>>::type{});
    return SetMMU16ChannelCompatibility<Ix+1>(a_mmu16_comp);
  } else {
    return;
  } // @formatter:on
}

/*!
 * Get MMU16 channel compatibility from the fixed-size bit set.
 * @tparam Ix - The starting channel, default to 1.
 * @param a_mmu16_comp - The compatibility bit set.
 */
template<Index Ix = 1>
void GetMMU16ChannelCompatibility(std::bitset<0x78> &a_mmu16_comp)
{
  if constexpr(Ix < 16) { // @formatter:off
    impl::GetMMU16ChannelCompatibility(a_mmu16_comp, typename add_sequence_front<Ix, ChannelCompatibilityPairedIndexes<Ix>>::type{});
    return GetMMU16ChannelCompatibility<Ix+1>(a_mmu16_comp);
  } else {
    return;
  } // @formatter:on
}

/*!
 * Zero out (reset all to 0) MMU16 compatibility for all channels.
 */
void ZeroOutMMU16ChannelCompatibility()
{
  constexpr std::bitset<0x78> mmu_comp_def;
  SetMMU16ChannelCompatibility(mmu_comp_def);
}

/*! 
 * Reverse the bitset sequence, so the most significant bit becomes the least significant bit.
 * @tparam N Size of the bitset
 * @param a_bitset The bitset to be reversed.
 */
template<std::size_t N>
void reverse(std::bitset<N> &a_bitset)
{
  for (std::size_t i = 0; i < N / 2; ++i) {
    bool t = a_bitset[i];
    a_bitset[i] = a_bitset[N - i - 1];
    a_bitset[N - i - 1] = t;
  }
}

/*!
 * Set default MMU16 channel compatibility, where the ring-barrier is laid out as follows:
 *    1 - 5, 6, 11
 *    2 - 5, 6, 9, 11
 *    3 - 7, 8, 12
 *    4 - 7, 8, 10, 12
 *    5 - 9
 *    6 - 9, 11
 *    7 - 10
 *    8 - 10, 12
 *    9 - 11
 *    10 - 12
 */
void SetDefaultMMU16ChannelCompatibility()
{
  std::bitset<0x78> mmu16_comp_def{
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
  // The bitset needs to be reversed, so the least significant bit
  // represents the starting compatibility bit, i.e., compat<1,2>
  reverse(mmu16_comp_def);
  SetMMU16ChannelCompatibility(mmu16_comp_def);
}

/*!
 * Set MMU16 channel compatibility using a HEX string. The least significant (i.e., rightmost) bit
 * of the HEX string represents the compatibility of channel 1 and 2.
 * @param a_hexstr
 */
void SetMMU16ChannelCompatibility(const std::string &a_hexstr)
{
  std::string bitstr{};
  for (auto &c : a_hexstr) {
    switch (std::toupper(c)) {
      case '0': bitstr.append("0000");
        break;
      case '1': bitstr.append("0001");
        break;
      case '2': bitstr.append("0010");
        break;
      case '3': bitstr.append("0011");
        break;
      case '4': bitstr.append("0100");
        break;
      case '5': bitstr.append("0101");
        break;
      case '6': bitstr.append("0110");
        break;
      case '7': bitstr.append("0111");
        break;
      case '8': bitstr.append("1000");
        break;
      case '9': bitstr.append("1001");
        break;
      case 'A': bitstr.append("1010");
        break;
      case 'B': bitstr.append("1011");
        break;
      case 'C': bitstr.append("1100");
        break;
      case 'D': bitstr.append("1101");
        break;
      case 'E': bitstr.append("1110");
        break;
      case 'F': bitstr.append("1111");
        break;
    }
  }
  std::bitset<0x78> comp{bitstr};
  SetMMU16ChannelCompatibility(comp);
}

} // end of namespace vtc::mmu

namespace broadcast {

struct BroadcastVariableType
{
};

template<typename T>
concept ValidBroadcastVariable = std::is_same_v<typename T::type, BroadcastVariableType>;

template<Tag, typename ValueT, Index I = 0>
struct BroadcastVariable : Variable<ValueT, I>
{
  using type = BroadcastVariableType;

  BroadcastVariable() = default;
  BroadcastVariable(BroadcastVariable &) = delete;
  BroadcastVariable(BroadcastVariable &&) = delete;
  BroadcastVariable &operator=(BroadcastVariable &) = delete;
  BroadcastVariable &operator=(BroadcastVariable &&) = delete;
};

using CuReportedMonth
    = BroadcastVariable<AUTO_TAG_ID, Byte>;

using CuReportedDay
    = BroadcastVariable<AUTO_TAG_ID, Byte>;

using CuReportedYear
    = BroadcastVariable<AUTO_TAG_ID, Byte>;

using CuReportedHour
    = BroadcastVariable<AUTO_TAG_ID, Byte>;

using CuReportedMinutes
    = BroadcastVariable<AUTO_TAG_ID, Byte>;

using CuReportedSeconds
    = BroadcastVariable<AUTO_TAG_ID, Byte>;

using CuReportedTenthsOfSeconds
    = BroadcastVariable<AUTO_TAG_ID, Byte>;

template<Index I>
/**/requires ValidIndex<I, biu::max_tf_bius>
using CuReportedTfBiuPresence = BroadcastVariable<AUTO_TAG_ID, Bit, I>;

template<Index I>
/**/requires ValidIndex<I, biu::max_det_bius>
using CuReportedDrBiuPresence = BroadcastVariable<AUTO_TAG_ID, Bit, I>;

template<typename T>/**/
requires ValidBroadcastVariable<T>
T variable{};

} // end of namespace vtc::broadcast

template<typename T>
requires broadcast::ValidBroadcastVariable<T>
constexpr T &variable()
{
  return broadcast::variable<T>;
}

template<typename T>
requires cu::ValidCuVariable<T>
constexpr T &variable()
{
  return cu::variable<T>;
}

template<typename T>
requires mmu::ValidMmuVariable<T>
constexpr T &variable()
{
  return mmu::variable<T>;
}

template<typename T>
requires io::ValidIoVariable<T>
constexpr T &variable()
{
  return io::variable<T>;
}

template<typename T>
requires biu::ValidBiuVariable<T>
constexpr T &variable()
{
  return biu::variable<T>;
}

namespace serial {

/*!
 * SDLC encoding using non-return-to-zero NRZ encoding, high = 1, low = 0.
 * Reserved bits will be set to low. Spare bits are vendor specific, but since
 * we don't deal with vendor specific features, spare bits will also be set to 0.
 */
constexpr int max_sdlc_frame_bytesize = 64; // max byte size = 64 byte

namespace {

struct FrameElementType
{
};

template<typename T, size_t BitPos>/**/
requires std::is_same_v<ValueType<T>, Bit>
struct FrameBit
{
  using type = FrameElementType;

  void operator<<(const std::span<const Byte> a_data_in)
  {
    static auto bytepos = pos / 8;
    static auto nbits_to_shift = pos % 8;
    auto value = (a_data_in[bytepos] & (0x01 << nbits_to_shift)) != 0;
    ref_var.value = static_cast<Bit>(value);
  }

  void operator>>(const std::span<Byte> a_data_out)
  {
    static auto byte_pos = pos / 8;
    static auto num_of_bits_to_shift = pos % 8;
    Byte i = (ref_var.value == Bit::On) ? 1 : 0;
    a_data_out[byte_pos] = a_data_out[byte_pos] | (i << num_of_bits_to_shift);
  }

  size_t pos{BitPos};
  T &ref_var{variable<T>()};
};

template<typename T, size_t BytePos>/**/
requires std::is_same_v<ValueType<T>, Byte>
struct FrameByte
{
  using type = FrameElementType;

  void operator<<(const std::span<const Byte> a_data_in)
  {
    ref_var.value = a_data_in[pos];
  }

  void operator>>(const std::span<Byte> a_data_out)
  {
    a_data_out[pos] = ref_var.value;
  }

  size_t pos{BytePos};
  T &ref_var{variable<T>()};
};

template<typename T, size_t BytePos>/* BytePos: position of Word value low byte */
requires std::is_same_v<ValueType<T>, Word>
struct FrameWord
{
  using type = FrameElementType;

  void operator<<(const std::span<const Byte> a_data_in)
  {
    ref_var.value = (a_data_in[pos] & 0x00FF) | (a_data_in[pos + 1] << 8); // LByte | HByte
  }

  void operator>>(const std::span<Byte> a_data_out)
  {
    a_data_out[pos] = ref_var.value & 0x00FF; // LByte
    a_data_out[pos + 1] = ref_var.value >> 8; // HByte
  }

  size_t pos{BytePos};
  T &ref_var{variable<T>()};
};

// Primary Station Generated Command Frame
struct PSG_CommandFrameType
{
};

// Primary Station Received Response Frame
struct PSR_ResponseFrameType
{
};

// Secondary Station Received Command Frame
struct SSR_CommandFrameType
{
};

// Secondary Station Generated Response Frame
struct SSG_ResponseFrameType
{
};

template<typename T>
concept ValidPrimaryStationFrame = std::is_same_v<T, PSG_CommandFrameType>
    || std::is_same_v<T, PSR_ResponseFrameType>;

template<typename T>
concept ValidSecondaryStationFrame = std::is_same_v<T, SSR_CommandFrameType>
    || std::is_same_v<T, SSG_ResponseFrameType>;

template<size_t N>
concept ValidFrameByteSize = (N >= 1) && (N <= max_sdlc_frame_bytesize);

template<typename T>
concept ValidFrameElement = std::is_same_v<typename T::type, FrameElementType>;

template<size_t ByteSize, typename T, typename ...Ts>
concept ValidFrame = (ValidPrimaryStationFrame<T> || ValidSecondaryStationFrame<T>)
    && (ValidFrameElement<Ts> &&...)
    && ValidFrameByteSize<ByteSize>;

template<typename T>
concept GenerativeFrame = std::is_same_v<T, PSG_CommandFrameType>
    || std::is_same_v<T, SSG_ResponseFrameType>;

template<typename T>
concept ReceivableFrame = std::is_same_v<T, PSR_ResponseFrameType>
    || std::is_same_v<T, SSR_CommandFrameType>;

template<Byte Address, Byte FrameID, size_t FrameByteSize, typename T, typename ...Ts>/**/
requires ValidFrame<FrameByteSize, T, Ts...>
class Frame
{
public:
  using type = T;

  Frame() noexcept = default ;
  Frame(Frame &) = delete;
  Frame(Frame &&) = delete;
  Frame &operator=(Frame &) = delete;
  Frame &operator=(Frame &&) = delete;

  template<typename = T>
  requires ReceivableFrame<T>
  void operator<<(const std::span<const Byte> a_data_in)
  {
    // [0]: Address; [1]: SDLC control code 0x83; [2]: FrameID.
    // Last 16 bits CCITT-CRC of the SDLC payload stripped away, not part of a_data_in.
    // assert(a_data_in[0] == address;
    // assert(a_data_in[1] == 0x83;
    // assert(a_data_in[2] == id);
    Assign(a_data_in);
  }

  template<typename = T>
  requires GenerativeFrame<T>
  void operator>>(std::span<Byte> a_data_out)
  {
    std::fill(a_data_out.begin(), a_data_out.end(), 0);
    a_data_out[0] = address;
    a_data_out[1] = 0x83;
    a_data_out[2] = id;
    Generate(a_data_out);
  }

  static constexpr Byte address{Address};
  static constexpr Byte id{FrameID};
  static constexpr size_t bytesize{FrameByteSize};
private:
  template<size_t I = 0>
  inline void Assign(const std::span<const Byte> a_data_in)
  {
    if constexpr (I < sizeof...(Ts)) {
      std::get<I>(m_frame_elements) << a_data_in;
      // @formatter:off
      Assign<I+1>(a_data_in);
      // @formatter:on
    }
  }

  template<size_t I = 0>
  inline void Generate(std::span<Byte> a_data_out)
  {
    if constexpr (I < sizeof...(Ts)) {
      std::get<I>(m_frame_elements) >> a_data_out;
      // @formatter:off
      Generate<I+1>(a_data_out);
      // @formatter:on
    }
  }

  std::tuple<Ts...> m_frame_elements;
};

} // end of namespace anonymous

template<Byte FrameID>
struct FrameType
{
};

// ----------------------------------------------
// Frame Type 0
// ----------------------------------------------
/* MMU LoadSwitchDriverFrame (TYPE 0 Command Frame)
   For each channel, there are two bits for dimming purpose
   ----------------------------------------------------
   LS+ Bit   LS- Bit    Function
   ----------------------------------------------------
   0         0          OFF
   1         0          Dimmed by eliminating + halfwave
   0         1          Dimmed by eliminating - halfwave
   1         1          ON
   ----------------------------------------------------

   In the frame definition, we use the same MMU variable to
   back-up both the positive and negative bits, thus having
   a state of either 00 (OFF) or 11 (ON), ignoring dimming,
   which is obsolete for modern traffic controllers.
 */
using LoadSwitchDriversFrame
    = Frame<
    0x10, // MMU Address = 16
    0x00, // FrameID = 0
    16,   // Total number of bytes of the frame.
    SSR_CommandFrameType,
    // ----------------------------------------------
    // Byte 0 - Address, 0x10 for MMU
    // Byte 1 - Control, always 0x83
    // Byte 2 - FrameID, 0x00 for Type 0 Command Frame
    // ----------------------------------------------
    // Byte 3 - Channel Green Driver
    //-----------------------------------------------
    FrameBit<mmu::ChannelGreenWalkDriver<0x01>, 0x18>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x01>, 0x19>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x02>, 0x1A>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x02>, 0x1B>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x03>, 0x1C>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x03>, 0x1D>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x04>, 0x1E>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x04>, 0x1F>,
    // Byte 4
    FrameBit<mmu::ChannelGreenWalkDriver<0x05>, 0x20>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x05>, 0x21>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x06>, 0x22>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x06>, 0x23>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x07>, 0x24>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x07>, 0x25>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x08>, 0x26>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x08>, 0x27>,
    // Byte 5
    FrameBit<mmu::ChannelGreenWalkDriver<0x09>, 0x28>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x09>, 0x29>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x0A>, 0x2A>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x0A>, 0x2B>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x0B>, 0x2C>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x0B>, 0x2D>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x0C>, 0x2E>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x0C>, 0x2F>,
    // Byte 6
    FrameBit<mmu::ChannelGreenWalkDriver<0x0D>, 0x30>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x0D>, 0x31>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x0E>, 0x32>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x0E>, 0x33>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x0F>, 0x34>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x0F>, 0x35>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x10>, 0x36>,
    FrameBit<mmu::ChannelGreenWalkDriver<0x10>, 0x37>,
    // ----------------------------------------------
    // Byte 7 - Channel Yellow Driver
    // ----------------------------------------------
    FrameBit<mmu::ChannelYellowPedClearDriver<0x01>, 0x38>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x01>, 0x39>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x02>, 0x3A>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x02>, 0x3B>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x03>, 0x3C>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x03>, 0x3D>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x04>, 0x3E>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x04>, 0x3F>,
    // Byte 8
    FrameBit<mmu::ChannelYellowPedClearDriver<0x05>, 0x40>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x05>, 0x41>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x06>, 0x42>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x06>, 0x43>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x07>, 0x44>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x07>, 0x45>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x08>, 0x46>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x08>, 0x47>,
    // Byte 9
    FrameBit<mmu::ChannelYellowPedClearDriver<0x09>, 0x48>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x09>, 0x49>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x0A>, 0x4A>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x0A>, 0x4B>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x0B>, 0x4C>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x0B>, 0x4D>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x0C>, 0x4E>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x0C>, 0x4F>,
    // Byte 10
    FrameBit<mmu::ChannelYellowPedClearDriver<0x0D>, 0x50>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x0D>, 0x51>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x0E>, 0x52>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x0E>, 0x53>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x0F>, 0x54>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x0F>, 0x55>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x10>, 0x56>,
    FrameBit<mmu::ChannelYellowPedClearDriver<0x10>, 0x57>,
    // ----------------------------------------------
    // Byte 11 - Channel Red Driver
    // ----------------------------------------------
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x01>, 0x58>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x01>, 0x59>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x02>, 0x5A>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x02>, 0x5B>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x03>, 0x5C>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x03>, 0x5D>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x04>, 0x5E>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x04>, 0x5F>,
    // Byte 12
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x05>, 0x60>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x05>, 0x61>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x06>, 0x62>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x06>, 0x63>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x07>, 0x64>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x07>, 0x65>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x08>, 0x66>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x08>, 0x67>,
    // Byte 13
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x09>, 0x68>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x09>, 0x69>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x0A>, 0x6A>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x0A>, 0x6B>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x0B>, 0x6C>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x0B>, 0x6D>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x0C>, 0x6E>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x0C>, 0x6F>,
    // Byte 14
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x0D>, 0x70>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x0D>, 0x71>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x0E>, 0x72>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x0E>, 0x73>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x0F>, 0x74>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x0F>, 0x75>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x10>, 0x76>,
    FrameBit<mmu::ChannelRedDoNotWalkDriver<0x10>, 0x77>,
    // ----------------------------------------------
    // Byte 15 : Bit 0x78 ~ 0x7E are reserved bits.
    // ----------------------------------------------
    FrameBit<mmu::LoadSwitchFlash, 0x7F>
>;

template<>
struct FrameType<0>
{
  using type = LoadSwitchDriversFrame;
};

// ----------------------------------------------
// Frame Type 1
// ----------------------------------------------
using MMUInputStatusRequestFrame
    = Frame<
    0x10, // MMU Address = 16
    0x01, // FrameID = 1
    3,
    SSR_CommandFrameType
>;

template<>
struct FrameType<1>
{
  using type = MMUInputStatusRequestFrame;
};

// ----------------------------------------------
// Frame Type 3
// ----------------------------------------------

using MMUProgrammingRequestFrame
    = Frame<
    0x10, // MMU Address = 16
    0x03, // FrameID = 3
    3,
    SSR_CommandFrameType
>;

template<>
struct FrameType<3>
{
  using type = MMUProgrammingRequestFrame;
};

// ----------------------------------------------
// Frame Type 9
// ----------------------------------------------
using DateTimeBroadcastFrame
    = Frame<
    0xFF, // Broadcast Address = 255
    0x09, // FrameID = 9
    12,   // 12 Bytes
    SSR_CommandFrameType,
    // ----------------------------------------------
    // Byte 0 - Address, 0xFF for broadcast message
    // Byte 1 - Control, always 0x83
    // Byte 2 - FrameID, 0x09 for Type 9 Command Frame
    // ----------------------------------------------
    // Byte 3~9: Mon/Day/Year/Hour/Min/Sec/TenthSec
    //-----------------------------------------------
    FrameByte<broadcast::CuReportedMonth, 3>,
    FrameByte<broadcast::CuReportedDay, 4>,
    FrameByte<broadcast::CuReportedYear, 5>,
    FrameByte<broadcast::CuReportedHour, 6>,
    FrameByte<broadcast::CuReportedMinutes, 7>,
    FrameByte<broadcast::CuReportedSeconds, 8>,
    FrameByte<broadcast::CuReportedTenthsOfSeconds, 9>,
    //-----------------------------------------------
    // Byte 10 - TF BIU # 1 ~ 8 Present State
    //-----------------------------------------------
    FrameBit<broadcast::CuReportedTfBiuPresence<0x01>, 0x50>,
    FrameBit<broadcast::CuReportedTfBiuPresence<0x02>, 0x51>,
    FrameBit<broadcast::CuReportedTfBiuPresence<0x03>, 0x52>,
    FrameBit<broadcast::CuReportedTfBiuPresence<0x04>, 0x53>,
    FrameBit<broadcast::CuReportedTfBiuPresence<0x05>, 0x54>,
    FrameBit<broadcast::CuReportedTfBiuPresence<0x06>, 0x55>,
    FrameBit<broadcast::CuReportedTfBiuPresence<0x07>, 0x56>,
    FrameBit<broadcast::CuReportedTfBiuPresence<0x08>, 0x57>,
    //-----------------------------------------------
    // Byte 11 - DET BIU # 1 ~ 8 Present State
    //-----------------------------------------------
    FrameBit<broadcast::CuReportedDrBiuPresence<0x01>, 0x58>,
    FrameBit<broadcast::CuReportedDrBiuPresence<0x02>, 0x59>,
    FrameBit<broadcast::CuReportedDrBiuPresence<0x03>, 0x5A>,
    FrameBit<broadcast::CuReportedDrBiuPresence<0x04>, 0x5B>,
    FrameBit<broadcast::CuReportedDrBiuPresence<0x05>, 0x5C>,
    FrameBit<broadcast::CuReportedDrBiuPresence<0x06>, 0x5D>,
    FrameBit<broadcast::CuReportedDrBiuPresence<0x07>, 0x5E>,
    FrameBit<broadcast::CuReportedDrBiuPresence<0x08>, 0x5F>
>;

template<>
struct FrameType<9>
{
  using type = DateTimeBroadcastFrame;
};

// ----------------------------------------------
// Frame Type 10
// ----------------------------------------------
using TfBiu01_OutputsInputsRequestFrame
    = Frame<
    0x00, // TF BIU#1 Address = 0
    0x0A, // FrameID = 10
    11,   // 11 Bytes
    SSR_CommandFrameType,
    // ----------------------------------------------
    // Byte 3
    // ---------------------------------------------- 
    FrameBit<io::output::ChannelRedDoNotWalkDriver<1>, 0x18>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<1>, 0x19>,
    FrameBit<io::output::ChannelYellowPedClearDriver<1>, 0x1A>,
    FrameBit<io::output::ChannelYellowPedClearDriver<1>, 0x1B>,
    FrameBit<io::output::ChannelGreenWalkDriver<1>, 0x1C>,
    FrameBit<io::output::ChannelGreenWalkDriver<1>, 0x1D>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<2>, 0x1E>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<2>, 0x1F>,
    // ----------------------------------------------
    // Byte 4
    // ----------------------------------------------
    FrameBit<io::output::ChannelYellowPedClearDriver<2>, 0x20>,
    FrameBit<io::output::ChannelYellowPedClearDriver<2>, 0x21>,
    FrameBit<io::output::ChannelGreenWalkDriver<2>, 0x22>,
    FrameBit<io::output::ChannelGreenWalkDriver<2>, 0x23>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<3>, 0x24>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<3>, 0x25>,
    FrameBit<io::output::ChannelYellowPedClearDriver<3>, 0x26>,
    FrameBit<io::output::ChannelYellowPedClearDriver<3>, 0x27>,
    // ----------------------------------------------
    // Byte 5
    // ----------------------------------------------
    FrameBit<io::output::ChannelGreenWalkDriver<3>, 0x28>,
    FrameBit<io::output::ChannelGreenWalkDriver<3>, 0x29>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<4>, 0x2A>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<4>, 0x2B>,
    FrameBit<io::output::ChannelYellowPedClearDriver<4>, 0x2C>,
    FrameBit<io::output::ChannelYellowPedClearDriver<4>, 0x2D>,
    FrameBit<io::output::ChannelGreenWalkDriver<4>, 0x2E>,
    FrameBit<io::output::ChannelGreenWalkDriver<4>, 0x2F>,
    // ----------------------------------------------
    // Byte 6
    // ----------------------------------------------
    FrameBit<io::output::ChannelRedDoNotWalkDriver<5>, 0x30>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<5>, 0x31>,
    FrameBit<io::output::ChannelYellowPedClearDriver<5>, 0x32>,
    FrameBit<io::output::ChannelYellowPedClearDriver<5>, 0x33>,
    FrameBit<io::output::ChannelGreenWalkDriver<5>, 0x34>,
    FrameBit<io::output::ChannelGreenWalkDriver<5>, 0x35>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<6>, 0x36>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<6>, 0x37>,
    // ----------------------------------------------
    // Byte 7
    // ----------------------------------------------
    FrameBit<io::output::ChannelYellowPedClearDriver<6>, 0x38>,
    FrameBit<io::output::ChannelYellowPedClearDriver<6>, 0x39>,
    FrameBit<io::output::ChannelGreenWalkDriver<6>, 0x3A>,
    FrameBit<io::output::ChannelGreenWalkDriver<6>, 0x3B>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<7>, 0x3C>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<7>, 0x3D>,
    FrameBit<io::output::ChannelYellowPedClearDriver<7>, 0x3E>,
    FrameBit<io::output::ChannelYellowPedClearDriver<7>, 0x3F>,
    // ----------------------------------------------
    // Byte 8
    // ----------------------------------------------
    FrameBit<io::output::ChannelGreenWalkDriver<7>, 0x40>,
    FrameBit<io::output::ChannelGreenWalkDriver<7>, 0x41>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<8>, 0x42>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<8>, 0x43>,
    FrameBit<io::output::ChannelYellowPedClearDriver<8>, 0x44>,
    FrameBit<io::output::ChannelYellowPedClearDriver<8>, 0x45>,
    FrameBit<io::output::ChannelGreenWalkDriver<8>, 0x46>,
    FrameBit<io::output::ChannelGreenWalkDriver<8>, 0x47>,
    // ----------------------------------------------
    // Byte 9
    // ----------------------------------------------
    FrameBit<io::output::UnitTBCAux_1, 0x48>,
    FrameBit<io::output::UnitTBCAux_2, 0x49>,
    FrameBit<io::output::PreemptStatus<1>, 0x4A>,
    FrameBit<io::output::PreemptStatus<2>, 0x4B>
    // Bit 0x4C - 0x4F designated as inputs, should be driven to logic 0 all times.
    // ----------------------------------------------
    // Byte 10
    // ----------------------------------------------
    // Bit 0x50 - 0x56 designated as inputs, should be driven to logic 0 all times.
    // Bit 0x57 Reserved.
>;

template<>
struct FrameType<10>
{
  using type = TfBiu01_OutputsInputsRequestFrame;
};

// ----------------------------------------------
// Frame Type 11
// ----------------------------------------------
using TfBiu02_OutputsInputsRequestFrame
    = Frame<
    0x01, // TF BIU#2 Address = 1
    0x0B, // FrameID = 11
    11,   // 11 Bytes
    SSR_CommandFrameType,
    // ----------------------------------------------
    // Byte 3
    // ---------------------------------------------- 
    FrameBit<io::output::ChannelRedDoNotWalkDriver<9>, 0x18>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<9>, 0x19>,
    FrameBit<io::output::ChannelYellowPedClearDriver<9>, 0x1A>,
    FrameBit<io::output::ChannelYellowPedClearDriver<9>, 0x1B>,
    FrameBit<io::output::ChannelGreenWalkDriver<9>, 0x1C>,
    FrameBit<io::output::ChannelGreenWalkDriver<9>, 0x1D>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<10>, 0x1E>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<10>, 0x1F>,
    // ----------------------------------------------
    // Byte 4
    // ----------------------------------------------
    FrameBit<io::output::ChannelYellowPedClearDriver<10>, 0x20>,
    FrameBit<io::output::ChannelYellowPedClearDriver<10>, 0x21>,
    FrameBit<io::output::ChannelGreenWalkDriver<10>, 0x22>,
    FrameBit<io::output::ChannelGreenWalkDriver<10>, 0x23>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<11>, 0x24>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<11>, 0x25>,
    FrameBit<io::output::ChannelYellowPedClearDriver<11>, 0x26>,
    FrameBit<io::output::ChannelYellowPedClearDriver<11>, 0x27>,
    // ----------------------------------------------
    // Byte 5
    // ----------------------------------------------
    FrameBit<io::output::ChannelGreenWalkDriver<11>, 0x28>,
    FrameBit<io::output::ChannelGreenWalkDriver<11>, 0x29>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<12>, 0x2A>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<12>, 0x2B>,
    FrameBit<io::output::ChannelYellowPedClearDriver<12>, 0x2C>,
    FrameBit<io::output::ChannelYellowPedClearDriver<12>, 0x2D>,
    FrameBit<io::output::ChannelGreenWalkDriver<12>, 0x2E>,
    FrameBit<io::output::ChannelGreenWalkDriver<12>, 0x2F>,
    // ----------------------------------------------
    // Byte 6
    // ----------------------------------------------
    FrameBit<io::output::ChannelRedDoNotWalkDriver<13>, 0x30>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<13>, 0x31>,
    FrameBit<io::output::ChannelYellowPedClearDriver<13>, 0x32>,
    FrameBit<io::output::ChannelYellowPedClearDriver<13>, 0x33>,
    FrameBit<io::output::ChannelGreenWalkDriver<13>, 0x34>,
    FrameBit<io::output::ChannelGreenWalkDriver<13>, 0x35>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<14>, 0x36>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<14>, 0x37>,
    // ----------------------------------------------
    // Byte 7
    // ----------------------------------------------
    FrameBit<io::output::ChannelYellowPedClearDriver<14>, 0x38>,
    FrameBit<io::output::ChannelYellowPedClearDriver<14>, 0x39>,
    FrameBit<io::output::ChannelGreenWalkDriver<14>, 0x3A>,
    FrameBit<io::output::ChannelGreenWalkDriver<14>, 0x3B>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<15>, 0x3C>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<15>, 0x3D>,
    FrameBit<io::output::ChannelYellowPedClearDriver<15>, 0x3E>,
    FrameBit<io::output::ChannelYellowPedClearDriver<15>, 0x3F>,
    // ----------------------------------------------
    // Byte 8
    // ----------------------------------------------
    FrameBit<io::output::ChannelGreenWalkDriver<15>, 0x40>,
    FrameBit<io::output::ChannelGreenWalkDriver<15>, 0x41>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<16>, 0x42>,
    FrameBit<io::output::ChannelRedDoNotWalkDriver<16>, 0x43>,
    FrameBit<io::output::ChannelYellowPedClearDriver<16>, 0x44>,
    FrameBit<io::output::ChannelYellowPedClearDriver<16>, 0x45>,
    FrameBit<io::output::ChannelGreenWalkDriver<16>, 0x46>,
    FrameBit<io::output::ChannelGreenWalkDriver<16>, 0x47>,
    // ----------------------------------------------
    // Byte 9
    // ----------------------------------------------
    FrameBit<io::output::UnitTBCAux_3, 0x48>,
    FrameBit<io::output::UnitFreeCoordStatus, 0x49>,
    FrameBit<io::output::PreemptStatus<3>, 0x4A>,
    FrameBit<io::output::PreemptStatus<4>, 0x4B>,
    FrameBit<io::output::PreemptStatus<5>, 0x4C>,
    FrameBit<io::output::PreemptStatus<6>, 0x4D>
    // Bit 0x4E - 0x4F designated as inputs, should be driven to logic 0 all times.
    // ----------------------------------------------
    // Byte 10
    // ----------------------------------------------
    // Bit 0x50 - 0x52 designated as inputs, should be driven to logic 0 all times.
    // Bit 0x53 - 0x56 Spare, vendor specific (logic 0 or 1).
    // Bit 0x57 - Reserved.
>;

template<>
struct FrameType<11>
{
  using type = TfBiu02_OutputsInputsRequestFrame;
};

// ----------------------------------------------
// Frame Type 12
// ----------------------------------------------
using TfBiu03_OutputsInputsRequestFrame
    = Frame<
    0x02, // TF BIU#1 Address = 2
    0x0C, // FrameID = 12
    8,    // 8 Bytes
    SSR_CommandFrameType,
    // ----------------------------------------------
    // Byte 3
    // ----------------------------------------------
    FrameBit<io::output::UnitTimingPlanA, 0x18>,
    FrameBit<io::output::UnitTimingPlanB, 0x19>,
    FrameBit<io::output::UnitTimingPlanC, 0x1A>,
    FrameBit<io::output::UnitTimingPlanD, 0x1B>,
    FrameBit<io::output::UnitOffset_1, 0x1C>,
    FrameBit<io::output::UnitOffset_2, 0x1D>,
    FrameBit<io::output::UnitOffset_3, 0x1E>,
    FrameBit<io::output::UnitAutomaticFlash, 0x1F>,
    // ----------------------------------------------
    // Byte 4
    // ----------------------------------------------
    FrameBit<io::output::SpecialFunction<1>, 0x20>,
    FrameBit<io::output::SpecialFunction<2>, 0x21>,
    FrameBit<io::output::SpecialFunction<3>, 0x22>,
    FrameBit<io::output::SpecialFunction<4>, 0x23>,
    // 0x24 - Reserved.
    // 0x25 - Reserved.
    // 0x26 - Reserved.
    // 0x27 - Reserved.
    // ----------------------------------------------
    // Byte 5
    // ----------------------------------------------
    FrameBit<io::output::StatusBitA_Ring_1, 0x28>,
    FrameBit<io::output::StatusBitB_Ring_1, 0x29>,
    FrameBit<io::output::StatusBitC_Ring_1, 0x2A>,
    FrameBit<io::output::StatusBitA_Ring_2, 0x2B>,
    FrameBit<io::output::StatusBitB_Ring_2, 0x2C>,
    FrameBit<io::output::StatusBitC_Ring_2, 0x2D>
    // 0x2E - Designated Input
    // 0x2F - Designated Input
    // ----------------------------------------------
    // Byte 6
    // ----------------------------------------------
    // 0x30 - Designated Input
    // 0x31 - Designated Input
    // 0x32 - Designated Input
    // 0x33 - Designated Input
    // 0x34 - Designated Input
    // 0x35 - Designated Input
    // 0x36 - Designated Input
    // 0x37 - Designated Input
    // ----------------------------------------------
    // Byte 7
    // ----------------------------------------------
    // 0x38 - Designated Input
    // 0x39 - Designated Input
    // 0x3A - Designated Input
    // 0x3B - Designated Input
    // 0x3C - Designated Input
    // 0x3D - Designated Input
    // 0x3E - Designated Input
    // 0x3F - Designated Input
>;

template<>
struct FrameType<12>
{
  using type = TfBiu03_OutputsInputsRequestFrame;
};

// ----------------------------------------------
// Frame Type 13
// ----------------------------------------------
using TfBiu04_OutputsInputsRequestFrame
    = Frame<
    0x03, // TF BIU#1 Address = 3
    0x0D, // FrameID = 13
    8,    // 8 Bytes
    SSR_CommandFrameType,
    // ----------------------------------------------
    // Byte 3
    // ----------------------------------------------
    FrameBit<io::output::PhaseOn<1>, 0x18>,
    FrameBit<io::output::PhaseOn<2>, 0x19>,
    FrameBit<io::output::PhaseOn<3>, 0x1A>,
    FrameBit<io::output::PhaseOn<4>, 0x1B>,
    FrameBit<io::output::PhaseOn<5>, 0x1C>,
    FrameBit<io::output::PhaseOn<6>, 0x1D>,
    FrameBit<io::output::PhaseOn<7>, 0x1E>,
    FrameBit<io::output::PhaseOn<8>, 0x1F>,
    // ----------------------------------------------
    // Byte 4
    // ----------------------------------------------
    FrameBit<io::output::PhaseNext<1>, 0x20>,
    FrameBit<io::output::PhaseNext<2>, 0x21>,
    FrameBit<io::output::PhaseNext<3>, 0x22>,
    FrameBit<io::output::PhaseNext<4>, 0x23>,
    FrameBit<io::output::PhaseNext<5>, 0x24>,
    FrameBit<io::output::PhaseNext<6>, 0x25>,
    FrameBit<io::output::PhaseNext<7>, 0x26>,
    // 0x27 - Reserved.
    // ----------------------------------------------
    // Byte 5
    // ----------------------------------------------
    FrameBit<io::output::PhaseNext<8>, 0x28>,
    FrameBit<io::output::PhaseCheck<1>, 0x29>,
    FrameBit<io::output::PhaseCheck<2>, 0x2A>,
    FrameBit<io::output::PhaseCheck<3>, 0x2B>,
    FrameBit<io::output::PhaseCheck<4>, 0x2C>,
    FrameBit<io::output::PhaseCheck<5>, 0x2D>,
    FrameBit<io::output::PhaseCheck<6>, 0x2E>,
    FrameBit<io::output::PhaseCheck<7>, 0x2F>,
    // ----------------------------------------------
    // Byte 6
    // ----------------------------------------------
    FrameBit<io::output::PhaseCheck<8>, 0x30>
    // 0x31 - Designated Input
    // 0x32 - Designated Input
    // 0x33 - Designated Input
    // 0x34 - Designated Input
    // 0x35 - Designated Input
    // 0x36 - Spare
    // 0x37 - Spare
    // ----------------------------------------------
    // Byte 7
    // ----------------------------------------------
    // 0x38 - Spare
    // 0x39 - Spare
    // 0x3A - Spare
    // 0x3B - Designated Input
    // 0x3C - Designated Input
    // 0x3D - Designated Input
    // 0x3E - Designated Input
    // 0x3F - Designated Input
>;

template<>
struct FrameType<13>
{
  using type = TfBiu04_OutputsInputsRequestFrame;
};

// ----------------------------------------------
// Frame Type 18
// ----------------------------------------------
using OutputTransferFrame
    = Frame<
    0xFF, // Broadcast address 255
    0x12, // FrameID = 18
    3,
    SSR_CommandFrameType
>;

template<>
struct FrameType<18>
{
  using type = OutputTransferFrame;
};

// ----------------------------------------------
// Frame Type 20
// ----------------------------------------------
using DrBiu01_CallRequestFrame
    = Frame<
    0x08, // DET BIU#1 Address = 8
    0x14, // FrameID = 20
    3,
    SSR_CommandFrameType
>;

template<>
struct FrameType<20>
{
  using type = DrBiu01_CallRequestFrame;
};

// ----------------------------------------------
// Frame Type 21
// ----------------------------------------------
using DrBiu02_CallRequestFrame
    = Frame<
    0x09, // DET BIU#2 Address = 9
    0x15, // FrameID = 21
    3,
    SSR_CommandFrameType
>;

template<>
struct FrameType<21>
{
  using type = DrBiu02_CallRequestFrame;
};

// ----------------------------------------------
// Frame Type 22
// ----------------------------------------------
using DrBiu03_CallRequestFrame
    = Frame<
    0x0A, // DET BIU#3 Address = 10
    0x16, // FrameID = 22
    3,
    SSR_CommandFrameType
>;

template<>
struct FrameType<22>
{
  using type = DrBiu03_CallRequestFrame;
};

// ----------------------------------------------
// Frame Type 23
// ----------------------------------------------
using DrBiu04_CallRequestFrame
    = Frame<
    0x0B, // DET BIU#3 Address = 1
    0x17, // FrameID = 23
    3,
    SSR_CommandFrameType
>;

template<>
struct FrameType<23>
{
  using type = DrBiu04_CallRequestFrame;
};

// ----------------------------------------------
// Frame Type 24
// ----------------------------------------------
using DrBiu01_ResetDiagnosticRequestFrame
    = Frame<
    0x08, // DET BIU#1 Address = 8
    0x18, // FrameID = 24
    4,
    SSR_CommandFrameType,
    FrameByte<io::output::DetectorReset<1>, 3>
>;

template<>
struct FrameType<24>
{
  using type = DrBiu01_ResetDiagnosticRequestFrame;
};

// ----------------------------------------------
// Frame Type 25
// ----------------------------------------------
using DrBiu02_ResetDiagnosticRequestFrame
    = Frame<
    0x09, // DET BIU#2 Address = 9
    0x19, // FrameID = 25
    4,
    SSR_CommandFrameType,
    FrameByte<io::output::DetectorReset<2>, 3>
>;

template<>
struct FrameType<25>
{
  using type = DrBiu02_ResetDiagnosticRequestFrame;
};

// ----------------------------------------------
// Frame Type 26
// ----------------------------------------------
using DrBiu03_ResetDiagnosticRequestFrame
    = Frame<
    0x0A, // DET BIU#3 Address = 10
    0x1A, // FrameID = 26
    4,
    SSR_CommandFrameType,
    FrameByte<io::output::DetectorReset<3>, 3>
>;

template<>
struct FrameType<26>
{
  using type = DrBiu03_ResetDiagnosticRequestFrame;
};

// ----------------------------------------------
// Frame Type 27
// ----------------------------------------------
using DrBiu04_ResetDiagnosticRequestFrame
    = Frame<
    0x0B, // DET BIU#3 Address = 1
    0x1B, // FrameID = 27
    4,
    SSR_CommandFrameType,
    FrameByte<io::output::DetectorReset<4>, 3>
>;

template<>
struct FrameType<27>
{
  using type = DrBiu04_ResetDiagnosticRequestFrame;
};

// ----------------------------------------------
// Frame 30, 40, 42, 43 - nobody cares; we neither.
// ----------------------------------------------


// ----------------------------------------------
// Frame Type 128
// ----------------------------------------------
using LoadSwitchDriversAckFrame
    = Frame<
    0x10, // MMU Address = 16
    0x80, // FrameID = 128, Type 0 ACK
    3,
    SSG_ResponseFrameType
>;

template<>
struct FrameType<128>
{
  using type = LoadSwitchDriversAckFrame;
};

// ----------------------------------------------
// Frame Type 129
// ----------------------------------------------
using MMUInputStatusRequestAckFrame
    = Frame<
    0x10, // MMU Address = 16
    0x81, // FrameID = 129
    13,
    SSG_ResponseFrameType,
    // ----------------------------------------------
    // Byte 0 - Address, 0x10 for MMU
    // Byte 1 - Control, always 0x83
    // Byte 2 - FrameID, 0x81 for Type 129 Response Frame
    // ----------------------------------------------
    // Byte 3 - Channel Green Status 1 ~ 8
    //-----------------------------------------------
    FrameBit<mmu::ChannelGreenWalkStatus<0x01>, 0x18>,
    FrameBit<mmu::ChannelGreenWalkStatus<0x02>, 0x19>,
    FrameBit<mmu::ChannelGreenWalkStatus<0x03>, 0x1A>,
    FrameBit<mmu::ChannelGreenWalkStatus<0x04>, 0x1B>,
    FrameBit<mmu::ChannelGreenWalkStatus<0x05>, 0x1C>,
    FrameBit<mmu::ChannelGreenWalkStatus<0x06>, 0x1D>,
    FrameBit<mmu::ChannelGreenWalkStatus<0x07>, 0x1E>,
    FrameBit<mmu::ChannelGreenWalkStatus<0x08>, 0x1F>,
    // ----------------------------------------------
    // Byte 4 - Channel Green Status 9 ~ 16
    // ----------------------------------------------
    FrameBit<mmu::ChannelGreenWalkStatus<0x09>, 0x20>,
    FrameBit<mmu::ChannelGreenWalkStatus<0x0A>, 0x21>,
    FrameBit<mmu::ChannelGreenWalkStatus<0x0B>, 0x22>,
    FrameBit<mmu::ChannelGreenWalkStatus<0x0C>, 0x23>,
    FrameBit<mmu::ChannelGreenWalkStatus<0x0D>, 0x24>,
    FrameBit<mmu::ChannelGreenWalkStatus<0x0E>, 0x25>,
    FrameBit<mmu::ChannelGreenWalkStatus<0x0E>, 0x26>,
    FrameBit<mmu::ChannelGreenWalkStatus<0x10>, 0x27>,
    // ----------------------------------------------
    // Byte 5 - Channel Yellow Status 1 ~ 8
    // ----------------------------------------------
    FrameBit<mmu::ChannelYellowPedClearStatus<0x01>, 0x28>,
    FrameBit<mmu::ChannelYellowPedClearStatus<0x02>, 0x29>,
    FrameBit<mmu::ChannelYellowPedClearStatus<0x03>, 0x2A>,
    FrameBit<mmu::ChannelYellowPedClearStatus<0x04>, 0x2B>,
    FrameBit<mmu::ChannelYellowPedClearStatus<0x05>, 0x2C>,
    FrameBit<mmu::ChannelYellowPedClearStatus<0x06>, 0x2D>,
    FrameBit<mmu::ChannelYellowPedClearStatus<0x07>, 0x2E>,
    FrameBit<mmu::ChannelYellowPedClearStatus<0x08>, 0x2F>,
    // ----------------------------------------------
    // Byte 6 - Channel Yellow Status 9 ~ 16
    // ----------------------------------------------
    FrameBit<mmu::ChannelYellowPedClearStatus<0x09>, 0x30>,
    FrameBit<mmu::ChannelYellowPedClearStatus<0x0A>, 0x31>,
    FrameBit<mmu::ChannelYellowPedClearStatus<0x0B>, 0x32>,
    FrameBit<mmu::ChannelYellowPedClearStatus<0x0C>, 0x33>,
    FrameBit<mmu::ChannelYellowPedClearStatus<0x0D>, 0x34>,
    FrameBit<mmu::ChannelYellowPedClearStatus<0x0E>, 0x35>,
    FrameBit<mmu::ChannelYellowPedClearStatus<0x0F>, 0x36>,
    FrameBit<mmu::ChannelYellowPedClearStatus<0x10>, 0x37>,
    // ----------------------------------------------
    // Byte 7 - Channel Red Status 1 ~ 8
    // ----------------------------------------------
    FrameBit<mmu::ChannelRedDoNotWalkStatus<0x01>, 0x38>,
    FrameBit<mmu::ChannelRedDoNotWalkStatus<0x02>, 0x39>,
    FrameBit<mmu::ChannelRedDoNotWalkStatus<0x03>, 0x3A>,
    FrameBit<mmu::ChannelRedDoNotWalkStatus<0x04>, 0x3B>,
    FrameBit<mmu::ChannelRedDoNotWalkStatus<0x05>, 0x3C>,
    FrameBit<mmu::ChannelRedDoNotWalkStatus<0x06>, 0x3D>,
    FrameBit<mmu::ChannelRedDoNotWalkStatus<0x07>, 0x3E>,
    FrameBit<mmu::ChannelRedDoNotWalkStatus<0x08>, 0x3F>,
    // ----------------------------------------------
    // Byte 8 - Channel Red Status 9 ~ 16
    // ----------------------------------------------
    FrameBit<mmu::ChannelRedDoNotWalkStatus<0x09>, 0x40>,
    FrameBit<mmu::ChannelRedDoNotWalkStatus<0x0A>, 0x41>,
    FrameBit<mmu::ChannelRedDoNotWalkStatus<0x0B>, 0x42>,
    FrameBit<mmu::ChannelRedDoNotWalkStatus<0x0C>, 0x43>,
    FrameBit<mmu::ChannelRedDoNotWalkStatus<0x0D>, 0x44>,
    FrameBit<mmu::ChannelRedDoNotWalkStatus<0x0E>, 0x45>,
    FrameBit<mmu::ChannelRedDoNotWalkStatus<0x0F>, 0x46>,
    FrameBit<mmu::ChannelRedDoNotWalkStatus<0x10>, 0x47>,
    // ----------------------------------------------
    // Byte 9
    // ----------------------------------------------
    FrameBit<mmu::ControllerVoltMonitor, 0x48>,
    FrameBit<mmu::_24VoltMonitor_I, 0x49>,
    FrameBit<mmu::_24VoltMonitor_II, 0x4A>,
    FrameBit<mmu::_24VoltMonitorInhibit, 0x4B>,
    FrameBit<mmu::Reset, 0x4C>,
    FrameBit<mmu::RedEnable, 0x4D>,
    //  0x4E Reserved
    //  0x4F Reserved
    // ----------------------------------------------
    // Byte 10
    // ----------------------------------------------
    FrameBit<mmu::Conflict, 0x50>,
    FrameBit<mmu::RedFailure, 0x51>,
    //  0x52 Spare
    //  0x53 Spare
    //  0x54 Spare
    //  0x55 Spare
    //  0x56 Spare
    //  0x57 Spare
    // ----------------------------------------------
    // Byte 11
    // ----------------------------------------------
    FrameBit<mmu::DiagnosticFailure, 0x58>,
    FrameBit<mmu::MinimumClearanceFailure, 0x59>,
    FrameBit<mmu::Port1TimeoutFailure, 0x5A>,
    FrameBit<mmu::FailedAndOutputRelayTransferred, 0x5B>,
    FrameBit<mmu::FailedAndImmediateResponse, 0x5C>,
    //  0x5D Reserved
    FrameBit<mmu::LocalFlashStatus, 0x5E>,
    FrameBit<mmu::StartupFlashCall, 0x5F>,
    // ----------------------------------------------
    // Byte 12
    // ----------------------------------------------
    FrameBit<mmu::FYAFlashRateFailure, 0x60>
    //  0x61 Reserved
    //  0x62 Reserved
    //  0x63 Reserved
    //  0x64 Reserved
    //  0x65 Reserved
    //  0x66 Reserved
    //  0x67 Reserved
>;

template<>
struct FrameType<129>
{
  using type = MMUInputStatusRequestAckFrame;
};

// ----------------------------------------------
// Frame Type 131
// ----------------------------------------------
using MMUProgrammingRequestAckFrame
    = Frame<
    0x10, // MMU Address = 16
    0x83, // FrameID = 131
    23,
    SSG_ResponseFrameType,
    // ----------------------------------------------
    // Byte 0 - Address, 0x10 for MMU
    // Byte 1 - Control, always 0x83
    // Byte 2 - FrameID, 0x83 for Type 131 Response Frame
    // ----------------------------------------------
    // Byte 3
    //-----------------------------------------------
    FrameBit<mmu::ChannelCompatibilityStatus<0x01, 0x02>, 0x18>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x01, 0x03>, 0x19>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x01, 0x04>, 0x1A>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x01, 0x05>, 0x1B>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x01, 0x06>, 0x1C>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x01, 0x07>, 0x1D>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x01, 0x08>, 0x1E>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x01, 0x09>, 0x1F>,
    // ----------------------------------------------
    // Byte 4
    // ----------------------------------------------
    FrameBit<mmu::ChannelCompatibilityStatus<0x01, 0x0A>, 0x20>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x01, 0x0B>, 0x21>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x01, 0x0C>, 0x22>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x01, 0x0D>, 0x23>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x01, 0x0E>, 0x24>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x01, 0x0F>, 0x25>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x01, 0x10>, 0x26>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x02, 0x03>, 0x27>,
    // ----------------------------------------------
    // Byte 5
    // ----------------------------------------------
    FrameBit<mmu::ChannelCompatibilityStatus<0x02, 0x04>, 0x28>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x02, 0x05>, 0x29>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x02, 0x06>, 0x2A>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x02, 0x07>, 0x2B>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x02, 0x08>, 0x2C>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x02, 0x09>, 0x2D>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x02, 0x0A>, 0x2E>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x02, 0x0B>, 0x2F>,
    // ----------------------------------------------
    // Byte 6
    // ----------------------------------------------
    FrameBit<mmu::ChannelCompatibilityStatus<0x02, 0x0C>, 0x30>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x02, 0x0D>, 0x31>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x02, 0x0E>, 0x32>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x02, 0x0F>, 0x33>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x02, 0x10>, 0x34>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x03, 0x04>, 0x35>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x03, 0x05>, 0x36>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x03, 0x06>, 0x37>,
    // ----------------------------------------------
    // Byte 7
    // ----------------------------------------------
    FrameBit<mmu::ChannelCompatibilityStatus<0x03, 0x07>, 0x38>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x03, 0x08>, 0x39>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x03, 0x09>, 0x3A>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x03, 0x0A>, 0x3B>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x03, 0x0B>, 0x3C>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x03, 0x0C>, 0x3D>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x03, 0x0D>, 0x3E>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x03, 0x0E>, 0x3F>,
    // ----------------------------------------------
    // Byte 8
    // ----------------------------------------------
    FrameBit<mmu::ChannelCompatibilityStatus<0x03, 0x0F>, 0x40>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x03, 0x10>, 0x41>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x04, 0x05>, 0x42>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x04, 0x06>, 0x43>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x04, 0x07>, 0x44>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x04, 0x08>, 0x45>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x04, 0x09>, 0x46>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x04, 0x0A>, 0x47>,
    // ----------------------------------------------
    // Byte 9
    // ----------------------------------------------
    FrameBit<mmu::ChannelCompatibilityStatus<0x04, 0x0B>, 0x48>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x04, 0x0C>, 0x49>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x04, 0x0D>, 0x4A>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x04, 0x0E>, 0x4B>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x04, 0x0F>, 0x4C>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x04, 0x10>, 0x4D>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x05, 0x06>, 0x4E>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x05, 0x07>, 0x4F>,
    // ----------------------------------------------
    // Byte 10
    // ----------------------------------------------
    FrameBit<mmu::ChannelCompatibilityStatus<0x05, 0x08>, 0x50>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x05, 0x09>, 0x51>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x05, 0x0A>, 0x52>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x05, 0x0B>, 0x53>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x05, 0x0C>, 0x54>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x05, 0x0D>, 0x55>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x05, 0x0E>, 0x56>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x05, 0x0F>, 0x57>,
    // ----------------------------------------------
    // Byte 11
    // ----------------------------------------------
    FrameBit<mmu::ChannelCompatibilityStatus<0x05, 0x10>, 0x58>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x06, 0x07>, 0x59>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x06, 0x08>, 0x5A>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x06, 0x09>, 0x5B>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x06, 0x0A>, 0x5C>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x06, 0x0B>, 0x5D>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x06, 0x0C>, 0x5E>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x06, 0x0D>, 0x5F>,
    // ----------------------------------------------
    // Byte 12
    // ----------------------------------------------
    FrameBit<mmu::ChannelCompatibilityStatus<0x06, 0x0E>, 0x60>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x06, 0x0F>, 0x61>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x06, 0x10>, 0x62>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x07, 0x08>, 0x63>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x07, 0x09>, 0x64>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x07, 0x0A>, 0x65>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x07, 0x0B>, 0x66>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x07, 0x0C>, 0x67>,
    // ----------------------------------------------
    // Byte 13
    // ----------------------------------------------
    FrameBit<mmu::ChannelCompatibilityStatus<0x07, 0x0D>, 0x68>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x07, 0x0E>, 0x69>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x07, 0x0F>, 0x6A>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x07, 0x10>, 0x6B>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x08, 0x09>, 0x6C>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x08, 0x0A>, 0x6D>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x08, 0x0B>, 0x6E>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x08, 0x0C>, 0x6F>,
    // ----------------------------------------------
    // Byte 14
    // ----------------------------------------------
    FrameBit<mmu::ChannelCompatibilityStatus<0x08, 0x0D>, 0x70>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x08, 0x0E>, 0x71>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x08, 0x0F>, 0x72>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x08, 0x10>, 0x73>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x09, 0x0A>, 0x74>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x09, 0x0B>, 0x75>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x09, 0x0C>, 0x76>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x09, 0x0D>, 0x77>,
    // ----------------------------------------------
    // Byte 15
    // ----------------------------------------------
    FrameBit<mmu::ChannelCompatibilityStatus<0x09, 0x0E>, 0x78>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x09, 0x0F>, 0x79>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x09, 0x10>, 0x7A>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0A, 0x0B>, 0x7B>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0A, 0x0C>, 0x7C>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0A, 0x0D>, 0x7D>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0A, 0x0E>, 0x7E>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0A, 0x0F>, 0x7F>,
    // ----------------------------------------------
    // Byte 16
    // ----------------------------------------------
    FrameBit<mmu::ChannelCompatibilityStatus<0x0A, 0x10>, 0x80>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0B, 0x0C>, 0x81>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0B, 0x0D>, 0x82>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0B, 0x0E>, 0x83>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0B, 0x0F>, 0x84>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0B, 0x10>, 0x85>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0C, 0x0D>, 0x86>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0C, 0x0E>, 0x87>,
    // ----------------------------------------------
    // Byte 17
    // ----------------------------------------------
    FrameBit<mmu::ChannelCompatibilityStatus<0x0C, 0x0F>, 0x88>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0C, 0x10>, 0x89>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0D, 0x0E>, 0x8A>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0D, 0x0F>, 0x8B>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0D, 0x10>, 0x8C>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0E, 0x0F>, 0x8D>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0E, 0x10>, 0x8E>,
    FrameBit<mmu::ChannelCompatibilityStatus<0x0F, 0x10>, 0x8F>,
    // ----------------------------------------------
    // Byte 18
    // ----------------------------------------------
    FrameBit<mmu::MinimumYellowChangeDisable<0x01>, 0x90>,
    FrameBit<mmu::MinimumYellowChangeDisable<0x02>, 0x91>,
    FrameBit<mmu::MinimumYellowChangeDisable<0x03>, 0x92>,
    FrameBit<mmu::MinimumYellowChangeDisable<0x04>, 0x93>,
    FrameBit<mmu::MinimumYellowChangeDisable<0x05>, 0x94>,
    FrameBit<mmu::MinimumYellowChangeDisable<0x06>, 0x95>,
    FrameBit<mmu::MinimumYellowChangeDisable<0x07>, 0x96>,
    FrameBit<mmu::MinimumYellowChangeDisable<0x08>, 0x97>,
    // ----------------------------------------------
    // Byte 19
    // ----------------------------------------------
    FrameBit<mmu::MinimumYellowChangeDisable<0x09>, 0x98>,
    FrameBit<mmu::MinimumYellowChangeDisable<0x0A>, 0x99>,
    FrameBit<mmu::MinimumYellowChangeDisable<0x0B>, 0x9A>,
    FrameBit<mmu::MinimumYellowChangeDisable<0x0C>, 0x9B>,
    FrameBit<mmu::MinimumYellowChangeDisable<0x0D>, 0x9C>,
    FrameBit<mmu::MinimumYellowChangeDisable<0x0E>, 0x9D>,
    FrameBit<mmu::MinimumYellowChangeDisable<0x0F>, 0x9E>,
    FrameBit<mmu::MinimumYellowChangeDisable<0x10>, 0x9F>,
    // ----------------------------------------------
    // Byte 20
    // ----------------------------------------------
    FrameBit<mmu::MinimumFlashTimeBit_0, 0xA0>,
    FrameBit<mmu::MinimumFlashTimeBit_1, 0xA1>,
    FrameBit<mmu::MinimumFlashTimeBit_2, 0xA2>,
    FrameBit<mmu::MinimumFlashTimeBit_3, 0xA3>,
    FrameBit<mmu::_24VoltLatch, 0xA4>,
    FrameBit<mmu::CVMFaultMonitorLatch, 0xA5>
    // 0xA6 - Reserved
    // 0xA7 - Reserved
    // ----------------------------------------------
    // Byte 21 - Reserved
    // ----------------------------------------------
    // ----------------------------------------------
    // Byte 22 - Reserved
    // ----------------------------------------------
>;

template<>
struct FrameType<131>
{
  using type = MMUProgrammingRequestAckFrame;
};

// ----------------------------------------------
// Frame Type 138
// ----------------------------------------------
using TfBiu01_InputFrame
    = Frame<
    0x00, // TF BIU#1 Address = 0
    0x8A, // FrameID = 138
    8,
    SSG_ResponseFrameType,
    // ----------------------------------------------
    // Byte 0 - Address, 0x00 for TF BIU#1
    // Byte 1 - Control, always 0x83
    // Byte 2 - FrameID, 0x8A for Type 138 Response Frame
    // ----------------------------------------------
    // Byte 3
    //-----------------------------------------------
    // 0x18 - Designated Output
    // 0x19 - Designated Output
    // 0x1A - Designated Output
    // 0x1B - Designated Output
    // 0x1C - Designated Output
    // 0x1D - Designated Output
    // 0x1E - Designated Output
    // 0x1F - Designated Output
    // ----------------------------------------------
    // Byte 4
    // ----------------------------------------------
    // 0x20 - Designated Output
    // 0x21 - Designated Output
    // 0x22 - Designated Output
    // 0x23 - Designated Output
    // 0x24 - Designated Output
    FrameBit<io::input::PreemptInput<1>, 0x25>,
    FrameBit<io::input::PreemptInput<2>, 0x26>,
    FrameBit<io::input::UnitTestInputA, 0x27>,
    // ----------------------------------------------
    // Byte 5
    // ----------------------------------------------
    FrameBit<io::input::UnitTestInputB, 0x28>,
    FrameBit<io::input::UnitAutomaticFlash, 0x29>,
    FrameBit<io::input::UnitDimming, 0x2A>,
    FrameBit<io::input::UnitManualControlEnable, 0x2B>,
    FrameBit<io::input::UnitIntervalAdvance, 0x2C>,
    FrameBit<io::input::UnitExternalMinRecall, 0x2D>,
    FrameBit<io::input::UnitExternalStart, 0x2E>,
    FrameBit<io::input::UnitTBCOnline, 0x2F>,
    // ----------------------------------------------
    // Byte 6
    // ----------------------------------------------
    FrameBit<io::input::RingStopTiming<1>, 0x30>,
    FrameBit<io::input::RingStopTiming<2>, 0x31>,
    FrameBit<io::input::RingMax2Selection<1>, 0x32>,
    FrameBit<io::input::RingMax2Selection<2>, 0x33>,
    FrameBit<io::input::RingForceOff<1>, 0x34>,
    FrameBit<io::input::RingForceOff<2>, 0x35>,
    FrameBit<io::input::UnitCallToNonActuated_1, 0x36>,
    FrameBit<io::input::UnitWalkRestModifier, 0x37>,
    // ----------------------------------------------
    // Byte 7
    // ----------------------------------------------
    FrameBit<io::input::PedDetCall<1>, 0x38>,
    FrameBit<io::input::PedDetCall<2>, 0x39>,
    FrameBit<io::input::PedDetCall<3>, 0x3A>,
    FrameBit<io::input::PedDetCall<4>, 0x3B>
    // 0x3C - Reserved
    // 0x3D - Reserved
    // 0x3E - Reserved
    // 0x3F - Reserved
>;

template<>
struct FrameType<138>
{
  using type = TfBiu01_InputFrame;
};

// ----------------------------------------------
// Frame Type 139
// ----------------------------------------------
using TfBiu02_InputFrame
    = Frame<
    0x01, // TF BIU#2 Address = 1
    0x8B, // FrameID = 139
    8,
    SSG_ResponseFrameType,
    // ----------------------------------------------
    // Byte 0 - Address, 0x01 for TF BIU#2
    // Byte 1 - Control, always 0x83
    // Byte 2 - FrameID, 0x8B for Type 139 Response Frame
    // ----------------------------------------------
    // Byte 3
    //-----------------------------------------------
    // 0x18 - Designated Output
    // 0x19 - Designated Output
    // 0x1A - Designated Output
    // 0x1B - Designated Output
    // 0x1C - Designated Output
    // 0x1D - Designated Output
    // 0x1E - Designated Output
    // 0x1F - Designated Output
    // ----------------------------------------------
    // Byte 4
    // ----------------------------------------------
    // 0x20 - Designated Output
    // 0x21 - Designated Output
    // 0x22 - Designated Output
    // 0x23 - Designated Output
    // 0x24 - Designated Output
    // 0x25 - Designated Output
    // 0x26 - Designated Output
    FrameBit<io::input::PreemptInput<3>, 0x27>,
    // ----------------------------------------------
    // Byte 5
    // ----------------------------------------------
    FrameBit<io::input::PreemptInput<4>, 0x28>,
    FrameBit<io::input::PreemptInput<5>, 0x29>,
    FrameBit<io::input::PreemptInput<6>, 0x2A>,
    FrameBit<io::input::UnitCallToNonActuated_2, 0x2B>,
    // 0x2C - Spare
    // 0x2D - Spare
    // 0x2E - Spare
    // 0x2F - Spare
    // ----------------------------------------------
    // Byte 6
    // ----------------------------------------------
    FrameBit<io::input::RingInhibitMaxTermination<1>, 0x30>,
    FrameBit<io::input::RingInhibitMaxTermination<2>, 0x31>,
    FrameBit<io::input::UnitLocalFlash, 0x32>,
    FrameBit<io::input::UnitCMUMMUFlashStatus, 0x33>,
    FrameBit<io::input::UnitAlarm_1, 0x34>,
    FrameBit<io::input::UnitAlarm_2, 0x35>,
    FrameBit<io::input::CoordFreeSwitch, 0x36>,
    FrameBit<io::input::UnitTestInputC, 0x37>,
    // ----------------------------------------------
    // Byte 7
    // ----------------------------------------------
    FrameBit<io::input::PedDetCall<5>, 0x38>,
    FrameBit<io::input::PedDetCall<6>, 0x39>,
    FrameBit<io::input::PedDetCall<7>, 0x3A>,
    FrameBit<io::input::PedDetCall<8>, 0x3B>
    // 0x3C - Reserved
    // 0x3D - Reserved
    // 0x3E - Reserved
    // 0x3F - Reserved
>;

template<>
struct FrameType<139>
{
  using type = TfBiu02_InputFrame;
};

// ----------------------------------------------
// Frame Type 140
// ----------------------------------------------
using TfBiu03_InputFrame
    = Frame<
    0x02, // TF BIU#3 Address = 2
    0x8C, // FrameID = 140
    8,
    SSG_ResponseFrameType,
    // ----------------------------------------------
    // Byte 0 - Address, 0x02 for TF BIU#3
    // Byte 1 - Control, always 0x83
    // Byte 2 - FrameID, 0x8C for Type 140 Response Frame
    // ----------------------------------------------
    // Byte 3
    //-----------------------------------------------
    // 0x18 - Designated Output
    // 0x19 - Designated Output
    // 0x1A - Designated Output
    // 0x1B - Designated Output
    // 0x1C - Designated Output
    // 0x1D - Designated Output
    FrameBit<io::input::RingRedRest<1>, 0x1E>,
    FrameBit<io::input::RingRedRest<2>, 0x1E>,
    // ----------------------------------------------
    // Byte 4
    // ----------------------------------------------
    FrameBit<io::input::RingOmitRedClearance<1>, 0x20>,
    FrameBit<io::input::RingOmitRedClearance<2>, 0x21>,
    FrameBit<io::input::RingPedestrianRecycle<1>, 0x22>,
    FrameBit<io::input::RingPedestrianRecycle<2>, 0x23>,
    FrameBit<io::input::UnitAlternateSequenceA, 0x24>,
    FrameBit<io::input::UnitAlternateSequenceB, 0x25>,
    FrameBit<io::input::UnitAlternateSequenceC, 0x26>,
    FrameBit<io::input::UnitAlternateSequenceD, 0x27>,
    // ----------------------------------------------
    // Byte 5
    // ----------------------------------------------
    FrameBit<io::input::PhasePhaseOmit<1>, 0x28>,
    FrameBit<io::input::PhasePhaseOmit<2>, 0x29>,
    FrameBit<io::input::PhasePhaseOmit<3>, 0x2A>,
    FrameBit<io::input::PhasePhaseOmit<4>, 0x2B>,
    FrameBit<io::input::PhasePhaseOmit<5>, 0x2C>,
    FrameBit<io::input::PhasePhaseOmit<6>, 0x2D>,
    FrameBit<io::input::PhasePhaseOmit<7>, 0x2E>,
    FrameBit<io::input::PhasePhaseOmit<8>, 0x2F>,
    // ----------------------------------------------
    // Byte 6
    // ----------------------------------------------
    FrameBit<io::input::PhasePedOmit<1>, 0x30>,
    FrameBit<io::input::PhasePedOmit<2>, 0x31>,
    FrameBit<io::input::PhasePedOmit<3>, 0x32>,
    FrameBit<io::input::PhasePedOmit<4>, 0x33>,
    FrameBit<io::input::PhasePedOmit<5>, 0x34>,
    FrameBit<io::input::PhasePedOmit<6>, 0x35>,
    FrameBit<io::input::PhasePedOmit<7>, 0x36>,
    FrameBit<io::input::PhasePedOmit<8>, 0x37>,
    // ----------------------------------------------
    // Byte 7
    // ----------------------------------------------
    FrameBit<io::input::UnitTimingPlanA, 0x38>,
    FrameBit<io::input::UnitTimingPlanB, 0x39>,
    FrameBit<io::input::UnitTimingPlanC, 0x3A>,
    FrameBit<io::input::UnitTimingPlanD, 0x3B>
    // 0x3C - Reserved
    // 0x3D - Reserved
    // 0x3E - Reserved
    // 0x3F - Reserved
>;

template<>
struct FrameType<140>
{
  using type = TfBiu03_InputFrame;
};

// ----------------------------------------------
// Frame Type 141
// ----------------------------------------------
using TfBiu04_InputFrame
    = Frame<
    0x03, // TF BIU#4 Address = 3
    0x8C, // FrameID = 141
    8,
    SSG_ResponseFrameType,
    // ----------------------------------------------
    // Byte 0 - Address, 0x03 for TF BIU#4
    // Byte 1 - Control, always 0x83
    // Byte 2 - FrameID, 0x8C for Type 140 Response Frame
    // ----------------------------------------------
    // Byte 3
    //-----------------------------------------------
    // 0x18 - Designated Output
    // 0x19 - Designated Output
    // 0x1A - Designated Output
    // 0x1B - Designated Output
    // 0x1C - Designated Output
    // 0x1D - Designated Output
    // 0x1E - Designated Output
    // 0x1E-  Designated Output
    // ----------------------------------------------
    // Byte 4
    // 0x20 - Designated Output
    FrameBit<io::input::UnitSystemAddressBit_0, 0x21>,
    FrameBit<io::input::UnitSystemAddressBit_1, 0x22>,
    FrameBit<io::input::UnitSystemAddressBit_2, 0x23>,
    FrameBit<io::input::UnitSystemAddressBit_3, 0x24>,
    FrameBit<io::input::UnitSystemAddressBit_4, 0x25>,
    // 0x26 - Spare
    // 0x27 - Spare
    // ----------------------------------------------
    // Byte 5
    // ----------------------------------------------
    // 0x28 - Spare
    // 0x29 - Spare
    // 0x2A - Spare
    // 0x2B - Reserved
    // 0x2C - Reserved
    // 0x2D - Reserved
    // 0x2E - Reserved
    // 0x2F - Reserved
    // ----------------------------------------------
    // Byte 6
    // ----------------------------------------------
    FrameBit<io::input::PhasePedOmit<1>, 0x30>,
    FrameBit<io::input::PhasePedOmit<2>, 0x31>,
    FrameBit<io::input::PhasePedOmit<3>, 0x32>,
    FrameBit<io::input::PhasePedOmit<4>, 0x33>,
    FrameBit<io::input::PhasePedOmit<5>, 0x34>,
    FrameBit<io::input::PhasePedOmit<6>, 0x35>,
    FrameBit<io::input::PhasePedOmit<7>, 0x36>,
    FrameBit<io::input::PhasePedOmit<8>, 0x37>,
    // ----------------------------------------------
    // Byte 7
    // ----------------------------------------------
    FrameBit<io::input::UnitOffset_1, 0x38>,
    FrameBit<io::input::UnitOffset_2, 0x39>,
    FrameBit<io::input::UnitOffset_3, 0x3A>
    // 0x3B - Spare
    // 0x3C - Reserved
    // 0x3D - Reserved
    // 0x3E - Reserved
    // 0x3F - Reserved
>;

template<>
struct FrameType<141>
{
  using type = TfBiu04_InputFrame;
};

/* DR BIU CallDataFrame should be transmitted only if Type 20 Frame
   has been correctly received.

   Bit 024 - 279:  Timestamp data nobody uses
   Bit 280 - 295:  Det 1 - 16 Call Status Bit 0
   Bit 296 - 311:  Det 1 - 16 Call Status Bit 1
   
   The two detector call status bits for each detector channel shall be
   defined as:
   
   --------------------------------------
   Bit 1   Bit 0  Definition            
   --------------------------------------
   0       0      No call, no change      
   0       1      Constant call, no change
   1       0      Call gone
   1       1      New call
   --------------------------------------
   
   The 16 detector timestamps for each detector channel shall contain
   the value of the detector BIU interval time stamp generator at the 
   instant in time when the detector call last changed status.
   
   The detector BIU internal timestamp generator is intended to provide 
   the means by which precision timing information about detector calls
   can be obtained, with a resolution of 1ms.
   
   It seems that nobody, neither controller nor detector BIU vendor seems
   to actually implement the timestamp bits based on NEMA-TS2, and only
   detector call status Bit 0 is needed without any timing information for
   most applications.
   
   So, in Type 148 - 151 Frame below, we only use Bit 0, based on the above
   rationale.
 * */
// ----------------------------------------------
// Frame Type 148
// ----------------------------------------------
using DrBiu01_CallDataFrame
    = Frame<
    0x08, // DR BIU#1 Address = 8
    0x94, // FrameID = 148
    39,
    SSG_ResponseFrameType,
// ----------------------------------------------
// Byte 0 - Address, 0x08 for DR BIU#1
// Byte 1 - Control, always 0x83
// Byte 2 - FrameID, 0x94 for Type 148 Response Frame
//----------------------------------------------

// Byte 03 - 04 Timestamp Word for Det 01
// ...
// Byte 33 - 34 Timestamp Word for Det 16
//----------------------------------------------
// Byte 35 - 36 Det 1 - Det 16 Call Status Bit 0
//----------------------------------------------
    FrameBit<io::input::VehicleDetCall<0x01>, 0x0118>, // Bit 280
    FrameBit<io::input::VehicleDetCall<0x02>, 0x0119>,
    FrameBit<io::input::VehicleDetCall<0x03>, 0x011A>,
    FrameBit<io::input::VehicleDetCall<0x04>, 0x011B>,
    FrameBit<io::input::VehicleDetCall<0x05>, 0x011C>,
    FrameBit<io::input::VehicleDetCall<0x06>, 0x011D>,
    FrameBit<io::input::VehicleDetCall<0x07>, 0x011E>,
    FrameBit<io::input::VehicleDetCall<0x08>, 0x011F>,
    FrameBit<io::input::VehicleDetCall<0x09>, 0x0120>,
    FrameBit<io::input::VehicleDetCall<0x0A>, 0x0121>,
    FrameBit<io::input::VehicleDetCall<0x0B>, 0x0122>,
    FrameBit<io::input::VehicleDetCall<0x0C>, 0x0123>,
    FrameBit<io::input::VehicleDetCall<0x0D>, 0x0124>,
    FrameBit<io::input::VehicleDetCall<0x0E>, 0x0125>,
    FrameBit<io::input::VehicleDetCall<0x0F>, 0x0126>,
    FrameBit<io::input::VehicleDetCall<0x10>, 0x0127>  // Bit 295
>;

template<>
struct FrameType<148>
{
  using type = DrBiu01_CallDataFrame;
};

// ----------------------------------------------
// Frame Type 149
// ----------------------------------------------
using DrBiu02_CallDataFrame
    = Frame<
    0x09, // DR BIU#2 Address = 9
    0x95, // FrameID = 149
    39,
    SSG_ResponseFrameType,
// ----------------------------------------------
// Byte 0 - Address, 0x09 for DR BIU#2
// Byte 1 - Control, always 0x83
// Byte 2 - FrameID, 0x95 for Type 149 Response Frame
//----------------------------------------------

// Byte 03 - 04 Timestamp Word for Det 17
// ...
// Byte 33 - 34 Timestamp Word for Det 32
//----------------------------------------------
// Byte 35 - 36 Det 17 - Det 32 Call Status Bit 0
//----------------------------------------------
    FrameBit<io::input::VehicleDetCall<0x11>, 0x0118>, // Bit 280
    FrameBit<io::input::VehicleDetCall<0x12>, 0x0119>,
    FrameBit<io::input::VehicleDetCall<0x13>, 0x011A>,
    FrameBit<io::input::VehicleDetCall<0x14>, 0x011B>,
    FrameBit<io::input::VehicleDetCall<0x15>, 0x011C>,
    FrameBit<io::input::VehicleDetCall<0x16>, 0x011D>,
    FrameBit<io::input::VehicleDetCall<0x17>, 0x011E>,
    FrameBit<io::input::VehicleDetCall<0x18>, 0x011F>,
    FrameBit<io::input::VehicleDetCall<0x19>, 0x0120>,
    FrameBit<io::input::VehicleDetCall<0x1A>, 0x0121>,
    FrameBit<io::input::VehicleDetCall<0x1B>, 0x0122>,
    FrameBit<io::input::VehicleDetCall<0x1C>, 0x0123>,
    FrameBit<io::input::VehicleDetCall<0x1D>, 0x0124>,
    FrameBit<io::input::VehicleDetCall<0x1E>, 0x0125>,
    FrameBit<io::input::VehicleDetCall<0x1F>, 0x0126>,
    FrameBit<io::input::VehicleDetCall<0x20>, 0x0127>  // Bit 295
>;

template<>
struct FrameType<149>
{
  using type = DrBiu02_CallDataFrame;
};

// ----------------------------------------------
// Frame Type 150
// ----------------------------------------------
using DrBiu03_CallDataFrame
    = Frame<
    0x0A, // DR BIU#3 Address = 10
    0x96, // FrameID = 150
    39,
    SSG_ResponseFrameType,
// ----------------------------------------------
// Byte 0 - Address, 0x08 for DR BIU#3
// Byte 1 - Control, always 0x83
// Byte 2 - FrameID, 0x94 for Type 148 Response Frame
//----------------------------------------------

// Byte 03 - 04 Timestamp Word for Det 33
// ...
// Byte 33 - 34 Timestamp Word for Det 48
//----------------------------------------------
// Byte 35 - 36 Det 33 - Det 48 Call Status Bit 0
//----------------------------------------------
    FrameBit<io::input::VehicleDetCall<0x21>, 0x0118>, // Bit 280
    FrameBit<io::input::VehicleDetCall<0x22>, 0x0119>,
    FrameBit<io::input::VehicleDetCall<0x23>, 0x011A>,
    FrameBit<io::input::VehicleDetCall<0x24>, 0x011B>,
    FrameBit<io::input::VehicleDetCall<0x25>, 0x011C>,
    FrameBit<io::input::VehicleDetCall<0x26>, 0x011D>,
    FrameBit<io::input::VehicleDetCall<0x27>, 0x011E>,
    FrameBit<io::input::VehicleDetCall<0x28>, 0x011F>,
    FrameBit<io::input::VehicleDetCall<0x29>, 0x0120>,
    FrameBit<io::input::VehicleDetCall<0x2A>, 0x0121>,
    FrameBit<io::input::VehicleDetCall<0x2B>, 0x0122>,
    FrameBit<io::input::VehicleDetCall<0x2C>, 0x0123>,
    FrameBit<io::input::VehicleDetCall<0x2D>, 0x0124>,
    FrameBit<io::input::VehicleDetCall<0x2E>, 0x0125>,
    FrameBit<io::input::VehicleDetCall<0x2F>, 0x0126>,
    FrameBit<io::input::VehicleDetCall<0x30>, 0x0127> // Bit 295
>;

template<>
struct FrameType<150>
{
  using type = DrBiu03_CallDataFrame;
};

// ----------------------------------------------
// Frame Type 151
// ----------------------------------------------
using DrBiu04_CallDataFrame
    = Frame<
    0x0B, // DR BIU#4 Address = 11
    0x97, // FrameID = 151
    39,
    SSG_ResponseFrameType,
// ----------------------------------------------
// Byte 0 - Address, 0x0B for DR BIU#4
// Byte 1 - Control, always 0x83
// Byte 2 - FrameID, 0x97 for Type 151 Response Frame
//----------------------------------------------

// Byte 03 - 04 Timestamp Word for Det 49
// ...
// Byte 33 - 34 Timestamp Word for Det 64
//----------------------------------------------
// Byte 35 - 36 Det 49 - Det 64 Call Status Bit 0
//----------------------------------------------
    FrameBit<io::input::VehicleDetCall<0x31>, 0x0118>, // Bit 280
    FrameBit<io::input::VehicleDetCall<0x32>, 0x0119>,
    FrameBit<io::input::VehicleDetCall<0x33>, 0x011A>,
    FrameBit<io::input::VehicleDetCall<0x34>, 0x011B>,
    FrameBit<io::input::VehicleDetCall<0x35>, 0x011C>,
    FrameBit<io::input::VehicleDetCall<0x36>, 0x011D>,
    FrameBit<io::input::VehicleDetCall<0x37>, 0x011E>,
    FrameBit<io::input::VehicleDetCall<0x38>, 0x011F>,
    FrameBit<io::input::VehicleDetCall<0x39>, 0x0120>,
    FrameBit<io::input::VehicleDetCall<0x3A>, 0x0121>,
    FrameBit<io::input::VehicleDetCall<0x3B>, 0x0122>,
    FrameBit<io::input::VehicleDetCall<0x3C>, 0x0123>,
    FrameBit<io::input::VehicleDetCall<0x3D>, 0x0124>,
    FrameBit<io::input::VehicleDetCall<0x3E>, 0x0125>,
    FrameBit<io::input::VehicleDetCall<0x3F>, 0x0126>,
    FrameBit<io::input::VehicleDetCall<0x40>, 0x0127> // Bit 295
>;

template<>
struct FrameType<151>
{
  using type = DrBiu04_CallDataFrame;
};

// ----------------------------------------------
// Frame Type 152
// ----------------------------------------------
using DrBiu01_DiagnosticFrame
    = Frame<
    0x08, // DR BIU#1 Address = 8
    0x98, // FrameID = 152
    19,
    SSG_ResponseFrameType

    /* Byte 3 - 18 will be all set to 0

       The diagnostics is only relevant to loop detectors
       and supposedly to be generated by the DR BIU based
       on detector call inputs.

       Designated bits for "Watchdog Failure" "open Loop"
       "Shorted Loop" and "Excessive Change in Inductance"
       shall indicate failures.

       Logical 1 represents the failed state, and logical
       0 otherwise. For software defined environment, the
       diagnostics is moot,  and we here simply set those
       bits to logical 0.
     * */
>;

template<>
struct FrameType<152>
{
  using type = DrBiu01_DiagnosticFrame;
};

// ----------------------------------------------
// Frame Type 153
// ----------------------------------------------
using DrBiu02_DiagnosticFrame
    = Frame<
    0x09, // DR BIU#2 Address = 9
    0x99, // FrameID = 153
    19,
    SSG_ResponseFrameType

    /* Byte 3 - 18 will be all set to 0

       The diagnostics is only relevant to loop detectors
       and supposedly to be generated by the DR BIU based
       on detector call inputs.

       Designated bits for "Watchdog Failure" "open Loop"
       "Shorted Loop" and "Excessive Change in Inductance"
       shall indicate failures.

       Logical 1 represents the failed state, and logical
       0 otherwise. For software defined environment, the
       diagnostics is moot,  and we here simply set those
       bits to logical 0.
     * */
>;

template<>
struct FrameType<153>
{
  using type = DrBiu02_DiagnosticFrame;
};

// ----------------------------------------------
// Frame Type 154
// ----------------------------------------------
using DrBiu03_DiagnosticFrame
    = Frame<
    0x0A, // DR BIU#3 Address = 10
    0x9A, // FrameID = 154
    19,
    SSG_ResponseFrameType

    /* Byte 3 - 18 will be all set to 0

       The diagnostics is only relevant to loop detectors
       and supposedly to be generated by the DR BIU based
       on detector call inputs.

       Designated bits for "Watchdog Failure" "open Loop"
       "Shorted Loop" and "Excessive Change in Inductance"
       shall indicate failures.

       Logical 1 represents the failed state, and logical
       0 otherwise. For software defined environment, the
       diagnostics is moot,  and we here simply set those
       bits to logical 0.
     * */
>;

template<>
struct FrameType<154>
{
  using type = DrBiu03_DiagnosticFrame;
};

// ----------------------------------------------
// Frame Type 155
// ----------------------------------------------
using DrBiu04_DiagnosticFrame
    = Frame<
    0x0A, // DR BIU#4 Address = 10
    0x9B, // FrameID = 155
    19,
    SSG_ResponseFrameType

    /* Byte 3 - 18 will be all set to 0

       The diagnostics is only relevant to loop detectors
       and supposedly to be generated by the DR BIU based
       on detector call inputs.

       Designated bits for "Watchdog Failure" "open Loop"
       "Shorted Loop" and "Excessive Change in Inductance"
       shall indicate failures.

       Logical 1 represents the failed state, and logical
       0 otherwise. For software defined environment, the
       diagnostics is moot,  and we here simply set those
       bits to logical 0.
     * */
>;

template<>
struct FrameType<155>
{
  using type = DrBiu04_DiagnosticFrame;
};

//-------------------------------------------------
namespace {

template<Byte CommandFrameID, Byte ResponseFrameID>
using FrameMap = std::tuple<typename FrameType<CommandFrameID>::type, typename FrameType<ResponseFrameID>::type>;

template<typename ...Ts>
using FrameMapsType = std::tuple<Ts...>;

using FrameMaps = FrameMapsType<
    FrameMap<0x00, 128>,
    FrameMap<0x01, 129>,
    FrameMap<0x03, 131>,
    FrameMap<0x0A, 138>,
    FrameMap<0x0B, 139>,
    FrameMap<0x0C, 140>,
    FrameMap<0x0D, 141>,
    FrameMap<0x14, 148>,
    FrameMap<0x15, 149>,
    FrameMap<0x16, 150>,
    FrameMap<0x17, 151>,
    FrameMap<0x18, 152>,
    FrameMap<0x19, 153>,
    FrameMap<0x1A, 154>,
    FrameMap<0x1B, 155>
>;

/*!
 * Global map of SDLC command frame and response frame pair.
 */
FrameMaps frame_maps;
constexpr auto frame_maps_size = std::tuple_size_v<decltype(frame_maps)>;

/*!
 * Global buffer for SDLC I/O.
 */
std::array<Byte, max_sdlc_frame_bytesize> buffer = {0};

} // end of namespace anonymous

template<size_t I = 0>
std::tuple<bool, std::span<Byte>> Dispatch(std::span<const Byte> a_data_in)
{
  if constexpr (I < frame_maps_size) {
    auto &cmd_frame = std::get<0>(std::get<I>(frame_maps));
    auto &res_frame = std::get<1>(std::get<I>(frame_maps));

    if (cmd_frame.id == a_data_in[2]) {
      cmd_frame << a_data_in;
      res_frame >> serial::buffer; // Buffer will be emptied at the beginning of >>().
      return {true, {serial::buffer.data(), res_frame.bytesize}};
    } else {
      // @formatter:off
      return Dispatch<I+1> (a_data_in);
      // @formatter:on
    }
  } else {
    return {false, serial::buffer};
  }
}

} // end of namespace vtc::serial

constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

std::string BytesToHexStr(const unsigned char *a_data, size_t a_nbyte)
{
  std::string result(a_nbyte * 2, ' ');
  for (size_t i = 0; i < a_nbyte; ++i) {
    result[2 * i] = hexmap[(a_data[i] & 0xF0) >> 4];
    result[2 * i + 1] = hexmap[a_data[i] & 0x0F];
  }

  return result;
}

} // end of namespace vtc

namespace vtc::serial::device {

enum class HdlcRxClkSource : uint16_t
{
  BRG = 0x0200,       /* RxClk generated by Baud Rate Generator internally. Need specify clock in param. */
  DPLL = 0x0100,      /* RxClk recovered by Digital Phase-locked Loop from data signal. Need specify clock in param. */
  RxClkPin = 0x0000,  /* RxClk supplied by external device on RxClk input pin. */
  TxClkPin = 0x8000   /* RxClk supplied by external device on TxClk input pin. */
};

enum class HdlcTxClkSource : uint16_t
{
  BRG = 0x0800,      /* TxClk generated by Baud Rate Generator internally. Need to specify clock in param. */
  DPLL = 0x0400,     /* TxClk recovered by Digital Phase-locked Loop from data signal. Need to specify clock in param. */
  RxClkPin = 0x0008, /* TxClk supplied by external device on RxC input pin. */
  TxClkPin = 0x0000  /* TxClk supplied by external device on TxC input pin. */
};

enum class HdlcCrcType : uint16_t
{
  None = 0,
  CcittCrc16 = 1,
  CcittCrc32 = 2
};

enum class HdlcIdleMode : uint32_t
{
  AltZeroOnes = 1,
  Zeros = 2,
  Ones = 3,
  AltMarkSpace = 4,
  Space = 5,
  Mark = 6
};

enum class HdlcEncoding : uint32_t
{
  NRZ = 0,      /* Non-return to zero. High = 1, low = 0. TxD is logical value w/ no encoding. */
  NRZ_B = 1,    /* Bipolar non-return to zero. TxD is logical value inverted. High = 0, low = 1. */
  NRZ_M = 2,    /* For logical 0, invert TxD at start of bit. */
  NRZ_S = 3,    /* For logical 1, invert TxD at start of bit. */
  NRZ_I = NRZ_S /* Shorthand for NRZI-Space */
};

enum class DeviceOptionTag : uint32_t
{
  RxDiscardTooLarge = 1,
  UnderRunRetryLimit = 2,
  EnableLocalLoopback = 3,
  EnableRemoteLoopback = 4,
  Interface = 6,
  RtsDriverControl = 7,
  RxErrorMask = 8,
  ClockSwitch = 9,
  ClockBaseFreq = 10,
  HalfDuplex = 11,
  MsbFirst = 12,
  RxCount = 13,
  TxCount = 14,
  RxPoll = 16,
  TxPoll = 17,
  NoTermination = 18,
  Tdm = 19,
  AuxClkEnable = 20,
  UnderRunCount = 21,
  TxIdleCount = 22,
  ResetDPLL = 23,
  Rs422OutputEnable = 24
};

// Be careful of memory alignment
// sizeof(SerialAdapterParams) == 32
struct alignas(4) SerialDeviceParams
{
  /* 4 bytes: Asynchronous or HDLC */
  uint32_t mode;

  /* 1 byte : Internal loopback mode */
  uint8_t loopback;
  /* 1 byte : Padding */
  /* 2 bytes: Specify TxC, RxC source. */
  uint16_t flags;

  /* 1 byte : NRZ, NRZ_I, etc. */
  uint8_t encoding;
  /* 1 byte : Padding */
  /* 1 byte : Padding */
  /* 1 byte : Padding */

  /* 4 bytes: External clock speed in bits per second */
  uint32_t clock;

  /* 1 bytes: Receive HDLC address filter, 0xFF = disable */
  uint8_t addr;
  /* 1 bytes: Padding */
  /* 2 bytes: none/16/32 */
  uint16_t crc;

  /* 1 bytes  */
  uint8_t b1;
  /* 1 bytes  */
  uint8_t b2;
  /* 1 bytes: Padding */
  /* 1 bytes: Padding */

  /* 4 bytes  */
  uint32_t dw1;

  /* 1 bytes  */
  uint8_t b3;
  /* 1 bytes  */
  uint8_t b4;
  /* 1 bytes  */
  uint8_t b5;
  /* 1 bytes: Padding */
};

using DeviceHandle = void *;

class SerialDevice
{

#ifdef _WIN32
  using FuncPtr = FARPROC;
#elif __linux__
  using FuncPtr = void*;
#endif

  class SerialApiModule
  {
  public:
    SerialApiModule() noexcept : serialapi_{nullptr}
    {
#ifdef _WIN32
      lib_ = ::LoadLibraryA("vtcdev");
      ec_ = ::GetLastError();
#elif __linux__
      lib_ = dlopen("vtcdev.so");
#endif
      if (!lib_)
        return;

      get_proc_address();
    }

    SerialApiModule(SerialApiModule &) = delete;
    SerialApiModule(SerialApiModule &&) = delete;
    SerialApiModule &operator=(SerialApiModule &) = delete;
    SerialApiModule &operator=(SerialApiModule &&) = delete;

    ~SerialApiModule()
    {
      if (lib_) {
#ifdef _WIN32
        ::FreeLibrary(lib_);
#elif __linux__
        dlclose(lib_);
#endif
      }
    }

    [[nodiscard]] bool is_loaded() noexcept
    {
      auto result = lib_ && verify_proc_address();
      return result;
    }

    // Error code as uint32_t, defined in winerror.h on Windows platform.
    using SimpleCommandFunc
        = uint32_t __stdcall(DeviceHandle);

    using IoFunc
        = uint32_t __stdcall(DeviceHandle, uint8_t *, int32_t);

    using SetValueByIdFunc
        = uint32_t __stdcall(DeviceHandle, uint32_t, int32_t);

    using SetValueFunc
        = uint32_t __stdcall(DeviceHandle, int32_t);

    using OpenFunc
        = uint32_t __stdcall(char *, void **);

    using SetParamsFunc
        = uint32_t __stdcall(DeviceHandle, SerialDeviceParams *);

    uint32_t cancel_reading(DeviceHandle a_dev)
    {
      return ec_ = ((SimpleCommandFunc *) (serialapi_[0x00]))(a_dev);
    }

    uint32_t cancel_writing(DeviceHandle a_dev)
    {
      return ec_ = ((SimpleCommandFunc *) (serialapi_[0x01]))(a_dev);
    }

    uint32_t read(DeviceHandle a_dev, std::span<uint8_t> a_buf)
    {
      // Note - we don't assign ec here, because read returns num of bytes read.
      return ((IoFunc *) (serialapi_[0x08]))(a_dev, a_buf.data(), static_cast<int32_t>(a_buf.size()));
    }

    uint32_t write(DeviceHandle a_dev, const std::span<const uint8_t> a_buf)
    {
      return ec_ = ((IoFunc *) (serialapi_[0x09]))(a_dev,
                                                   const_cast<uint8_t *>(a_buf.data()),
                                                   static_cast<int32_t>(a_buf.size()));
    }

    uint32_t open(const char *a_dev_name, DeviceHandle &a_dev)
    {
      return ec_ = ((OpenFunc *) (serialapi_[0x07]))(const_cast<char *>(a_dev_name), &a_dev);
    }

    uint32_t close(DeviceHandle a_dev)
    {
      return ec_ = ((SimpleCommandFunc *) (serialapi_[0x02]))(a_dev);
    }

    uint32_t enable_read(DeviceHandle a_dev)
    {
      return ec_ = ((SetValueFunc *) (serialapi_[0x03]))(a_dev, 1);
    }

    uint32_t set_params(DeviceHandle a_dev, SerialDeviceParams &a_params)
    {
      return ec_ = ((SetParamsFunc *) (serialapi_[0x04]))(a_dev, &a_params);
    }

    uint32_t set_idle_mode(DeviceHandle a_dev, HdlcIdleMode a_mode)
    {
      return ec_ = ((SetValueFunc *) (serialapi_[0x05]))(a_dev, static_cast<int32_t>(a_mode));
    }

    uint32_t set_option(DeviceHandle a_dev, DeviceOptionTag a_opt_tag, int32_t a_opt_val)
    {
      return ec_ = ((SetValueByIdFunc *) (serialapi_[0x06]))(a_dev, static_cast<uint32_t>(a_opt_tag), a_opt_val);
    }

    [[nodiscard]] uint32_t ec() const
    {
      return ec_;
    }
  private:
    template<int I = 0>
    void get_proc_address()
    {
      if constexpr (I < keys_.size()) {
#ifdef _WIN32
        auto proc_addr = ::GetProcAddress(lib_, keys_[I].data());
#elif __linux__
        auto proc_addr = dlsm(lib_, m_ordinals[I].data());
#endif
        return (proc_addr) ? (serialapi_[I] = proc_addr), get_proc_address<I + 1>() : void();
      }
    }

    template<int I = 0>
    bool verify_proc_address()
    {
      if constexpr (I < keys_.size()) {
        return (serialapi_[I]) && verify_proc_address<I + 1>();
      } else {
        return true;
      }
    }

    static constexpr std::array<std::array<char, 0x18>, 10> keys_ = {
        {
            {{0x4D, 0x67, 0x73, 0x6C, 0x43, 0x61, 0x6E, 0x63,
              0x65, 0x6C, 0x52, 0x65, 0x63, 0x65, 0x69, 0x76,
              0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}} // 0x00: 0x02
            ,
            {{0x4D, 0x67, 0x73, 0x6C, 0x43, 0x61, 0x6E, 0x63,
              0x65, 0x6C, 0x54, 0x72, 0x61, 0x6E, 0x73, 0x6D,
              0x69, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}} // 0x01: 0x03
            ,
            {{0x4D, 0x67, 0x73, 0x6C, 0x43, 0x6C, 0x6F, 0x73,
              0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}} // 0x02: 0x05
            ,
            {{0x4D, 0x67, 0x73, 0x6C, 0x45, 0x6E, 0x61, 0x62,
              0x6C, 0x65, 0x52, 0x65, 0x63, 0x65, 0x69, 0x76,
              0x65, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}} // 0x03: 0x06
            ,
            {{0x4D, 0x67, 0x73, 0x6C, 0x53, 0x65, 0x74, 0x50,
              0x61, 0x72, 0x61, 0x6D, 0x73, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}} // 0x04: 0x11
            ,
            {{0x4D, 0x67, 0x73, 0x6C, 0x53, 0x65, 0x74, 0x49,
              0x64, 0x6C, 0x65, 0x4D, 0x6F, 0x64, 0x65, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}} // 0x05: 0x18
            ,
            {{0x4D, 0x67, 0x73, 0x6C, 0x53, 0x65, 0x74, 0x4F,
              0x70, 0x74, 0x69, 0x6F, 0x6E, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}} // 0x06: 0x30
            ,
            {{0x4D, 0x67, 0x73, 0x6C, 0x4F, 0x70, 0x65, 0x6E,
              0x42, 0x79, 0x4E, 0x61, 0x6D, 0x65, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}} // 0x07: 0x38
            ,
            {{0x4D, 0x67, 0x73, 0x6C, 0x52, 0x65, 0x61, 0x64,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}} // 0x08: 0x3B
            ,
            {{0x4D, 0x67, 0x73, 0x6C, 0x57, 0x72, 0x69, 0x74,
              0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}} // 0x09: 0x3C
        }
    };

    std::array<FuncPtr, keys_.size()> serialapi_{nullptr};

#ifdef _WIN32
    HMODULE lib_{nullptr};
#elif __linux__
    void* lib_{ nullptr };
#endif

    uint32_t ec_{0};
  };

public:
  explicit SerialDevice(const char *a_dev_name, uint32_t a_clock_speed = 153600, uint8_t a_addr_filter = 0xFF) noexcept
  {
    if (!apimodule_.is_loaded()) {
      vtc::logger()->error("apimodule was not loaded, code {}", apimodule_.ec());
      return;
    }

    if (apimodule_.open(a_dev_name, dev_)) {
      dev_ = nullptr;
      vtc::logger()->error("Serial device \"{}\" failed to open, code {}", a_dev_name, apimodule_.ec());
      return;
    }

    SerialDeviceParams params{};
    memset(&params, 0, sizeof(params));
    params.mode = 2;
    params.loopback = 0;
    params.flags = static_cast<uint16_t>(HdlcRxClkSource::RxClkPin) + static_cast<uint16_t>(HdlcTxClkSource::BRG);
    params.encoding = static_cast<uint8_t>(HdlcEncoding::NRZ);
    params.clock = a_clock_speed;
    params.crc = static_cast<uint16_t>(HdlcCrcType::CcittCrc16);
    params.addr = a_addr_filter;

    if (apimodule_.set_params(dev_, params)) {
      vtc::logger()->error("Serial device \"{}\" failed to set params, code {}", a_dev_name, apimodule_.ec());
      return;
    }

    if (apimodule_.set_option(dev_, DeviceOptionTag::RxPoll, 0)) {
      vtc::logger()->error("Serial device \"{}\" failed to set option RxPoll = 0, code {}", a_dev_name, apimodule_.ec());
      return;
    }

    if (apimodule_.set_option(dev_, DeviceOptionTag::TxPoll, 0)) {
      vtc::logger()->error("Serial device \"{}\" failed to set option TxPoll = 0, code {}", a_dev_name, apimodule_.ec());
      return;
    }

    if (apimodule_.set_option(dev_, DeviceOptionTag::RxErrorMask, 1)) {
      vtc::logger()->error("Serial device \"{}\" failed to set option RxErrorMask = 1, code {}", a_dev_name, apimodule_.ec());
      return;
    }

    if (apimodule_.set_idle_mode(dev_, HdlcIdleMode::Ones)) {
      vtc::logger()->error("Serial device \"{}\" failed to set idle mode to ones, code {}", a_dev_name, apimodule_.ec());
      return;
    }

    if (apimodule_.enable_read(dev_)) {
      vtc::logger()->error("Serial device \"{}\" failed to enable read, code {}", a_dev_name, apimodule_.ec());
      return;
    }
  }

  ~SerialDevice()
  {
    if (dev_) {
      apimodule_.cancel_reading(dev_);
      apimodule_.cancel_writing(dev_);
      apimodule_.close(dev_);
    }
  }

  SerialDevice(const SerialDevice &) = delete;
  SerialDevice(SerialDevice &&) = delete;
  SerialDevice &operator=(SerialDevice &) = delete;
  SerialDevice &operator=(SerialDevice &&) = delete;

  /*!
   * Only when the device is ready, the read and write method can be called.
   * @return
   */
  static bool ready()
  {
    return !apimodule_.ec();
  }

#ifdef _WIN32
  static std::string err_what()
  {
    if (apimodule_.ec() == 0) {
      return std::string{}; // No error message has been recorded
    }

    LPSTR buf = nullptr;

    // Ask Win32 to give us the string version of that message ID.
    // The parameters we pass in, tell Win32 to create the buffer that holds the message for us
    // (because we don't yet know how long the message string will be).
    size_t bufsz = ::FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        apimodule_.ec(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR) &buf,
        0,
        nullptr);

    //Copy the error message into a std::string.
    std::string result(buf, bufsz);
    //Free the Win32's string's buffer.
    ::LocalFree(buf);
    return result;
  }
#endif

  uint32_t read(std::span<uint8_t> a_buf)
  {
    return apimodule_.read(dev_, a_buf);
  }

  uint32_t write(const std::span<const uint8_t> a_buf)
  {
    return apimodule_.write(dev_, a_buf);
  }

  [[nodiscard]] static bool is_apimodule_loaded()
  {
    return apimodule_.is_loaded();
  }

  [[nodiscard]] static uint32_t ec()
  {
    return apimodule_.ec();
  }

#ifdef TEST
  static SerialApiModule &apimodule()
  {
    return apimodule_;
  }
#endif

private:
  static SerialApiModule apimodule_;
  DeviceHandle dev_{nullptr};
};

SerialDevice::SerialApiModule SerialDevice::apimodule_{};

} // end of vtc::serial::device namespace


namespace vtc::hils {

using namespace io::input;
using namespace io::output;
using namespace serial::device;
using namespace matchit;

auto constexpr num_loadswitches{16};
auto constexpr num_detector_channels{64};

enum class Turn : short
{
  Right = 0,
  Through = 1,
  Left = 2,
  UTurn = 3
};

using Approach = uint32_t;
using SensorID = uint32_t;
using SensorIDs = std::vector<SensorID>;
using TurningMovement = std::tuple<Approach, Turn>;
using SignalHead = std::vector<TurningMovement>;
using LoadswitchChannelID = vtc::Index;
using DetectorChannelID = vtc::Index;

enum class LoadswitchChannelState : short
{
  Blank = 0,
  Red = 1,
  Yellow = 2,
  Green = 3
};

/*!
 * Loadswitch channel driver is grouped by a tuple of three individual channels, corresponding
 * to GreenWalk, YellowPedClear and RedDoNotWalk drivers, respectively.
 */
template<LoadswitchChannelID I>
using LoadswitchDriver =
    std::tuple<
        ChannelGreenWalkDriver<I> &,
        ChannelYellowPedClearDriver<I> &,
        ChannelRedDoNotWalkDriver<I> &
    >;

// @formatter:off
template<LoadswitchChannelID I>
auto make_loadswitch_driver()
{
  return std::make_tuple(
      std::ref(io::variable<ChannelGreenWalkDriver<I>>),      /**/
      std::ref(io::variable<ChannelYellowPedClearDriver<I>>), /**/
      std::ref(io::variable<ChannelRedDoNotWalkDriver<I>>));  /**/
}
//@formatter:on

template<LoadswitchChannelID I>/**/
requires (I <= num_loadswitches) && (I >= 1)
class LoadswitchChannel
{
public:
  [[nodiscard]] auto constexpr state() const noexcept
  {
    auto &[g, y, r] = driver_;
    //@formatter:off
    return match(std::make_tuple(std::ref(g.value), std::ref(y.value), std::ref(r.value))) (
        pattern | ds(Bit::On,  Bit::Off, Bit::Off) = expr(LoadswitchChannelState::Green),
        pattern | ds(Bit::Off, Bit::On,  Bit::Off) = expr(LoadswitchChannelState::Yellow),
        pattern | ds(Bit::Off, Bit::Off, Bit::On)  = expr(LoadswitchChannelState::Red),
        pattern | ds(_,_,_) /* Blank, or error? */ = expr(LoadswitchChannelState::Blank)
    );
    //@formatter:on
  }

  auto constexpr operator()() const noexcept
  {
    return I;
  }
private:
  LoadswitchDriver<I> driver_{make_loadswitch_driver<I>()};
};

template<LoadswitchChannelID I>
/**/requires (I <= num_loadswitches) && (I >= 1)
using LoadswitchWiring = std::tuple<LoadswitchChannel<I>, SignalHead>;

struct LoadswitchWiringFactory
{
  template<LoadswitchChannelID I>
  static auto make()
  { //@formatter:off
    return LoadswitchWiring<I> {};
  } //@formatter:on
};

using LoadswitchChannelIndexes =
    offset_sequence_t<
        0,
        std::make_integer_sequence<
            LoadswitchChannelID,
            num_loadswitches
        >
    >;

template<DetectorChannelID I>/**/
requires (I <= num_detector_channels) && (I >= 1)
class DetectorChannel
{
public:
  [[nodiscard]] auto &activated() const noexcept
  {
    return state_.value;
  }

  auto constexpr operator()() noexcept
  {
    return I;
  }
private:
  VehicleDetCall<I> &state_{io::variable<io::input::VehicleDetCall<I>>};
};

template<DetectorChannelID I>
/**/requires (I <= num_detector_channels) && (I >= 1)
using DetectorWiring = std::tuple<DetectorChannel<I>, SensorIDs>;

//@formatter:off
struct DetectorWiringFactory
{
  template<DetectorChannelID I>
  static auto make()
  {
    return DetectorWiring<I> {};
  }
};
//@formatter:on

using DetectorChannelIndexes =
    offset_sequence_t<
        0,
        std::make_integer_sequence<
            DetectorChannelID,
            num_detector_channels
        >
    >;

template<typename WiringFactoryT, typename ChannelIDT, ChannelIDT ...Is>
auto make_wirings(WiringFactoryT, ChannelIDT, std::integer_sequence<ChannelIDT, Is...>)
{
  return std::make_tuple(decltype(WiringFactoryT::template make<Is>()){}...);
}

/*!
 * A trick of aliasing the type of detector wirings. The args instances to make_wirings()
 * are just supplied for the purpose of obtaining their respective types.
 */
using DetectorWirings
    = decltype(make_wirings(DetectorWiringFactory{}, DetectorChannelID{1}, DetectorChannelIndexes{}));

/*!
 * A trick of aliasing the type of loadswitch wirings. The args instances to make_wirings()
 * are just supplied for the purpose of obtaining their respective types.
 */
using LoadswitchWirings
    = decltype(make_wirings(LoadswitchWiringFactory{}, LoadswitchChannelID{1}, LoadswitchChannelIndexes{}));

/*!
 * Wirings concept is limited to detector wirings and load switch wirings.
 * @tparam T
 */
template<typename T>
concept Wirings = std::is_same_v<std::remove_cvref_t<T>, LoadswitchWirings>
    || std::is_same_v<std::remove_cvref_t<T>, DetectorWirings>;

/*!
 * Provide a convenience function for enumerating the individual wiring of a list of wirings.
 * @tparam WiringsT The wirings type.
 * @tparam F The callback functor type to be applied to each subject individual wiring.
 * @param a_wirings
 * @param a_func
 */
template<Wirings WiringsT, typename F>
void for_each(WiringsT &&a_wirings, F &&a_func)
{
  std::apply(
      [&a_func]<typename ...T>(T &&...args) {
        (a_func(std::forward<T>(args)), ...);
      },
      std::forward<WiringsT>(a_wirings)
  );
}

using ProcessLoadswitchWiringFunc
    = std::function<void(const LoadswitchChannelID, const LoadswitchChannelState, const Approach, const Turn)>;

using ProcessDetectorWiringFunc
    = std::function<bool(const DetectorChannelID, const SensorID)>;

using VerifyLoadswitchWiringFunc
    = std::function<bool(const LoadswitchChannelID, const Approach, const Turn)>;

using VerifyDetectorWiringFunc
    = std::function<bool(const DetectorChannelID, const SensorID)>;

using VerifySimulationStepFunc
    = std::function<bool(const double)>;

using VerifyFuncGroup
    = std::tuple<VerifySimulationStepFunc, VerifyLoadswitchWiringFunc, VerifyDetectorWiringFunc>;

/*!
 * Generic Hardware-in-Loop Simulation Controller Interface
 */
class HilsCI
{
public:
  HilsCI(const HilsCI &) = delete;
  HilsCI(HilsCI &&) = delete;
  HilsCI &operator=(HilsCI &) = delete;
  HilsCI &operator=(HilsCI &&) = delete;

  auto &loadswitch_wirings() const
  {
    return loadswitch_wirings_;
  }

  auto &detector_wirings() const
  {
    return detector_wirings_;
  }

protected:
  HilsCI() = default;

  /*!
   * Derived class implemented for a specific simulator, e.g., Aimsun, SUMO, Vissim or TransModeler, should
   * call this method when the simulation clock "ticks".
   */
  void process_wirings(auto a_process_loadswitch_wiring, auto a_process_detector_wiring)
  {
    for_each(detector_wirings_, [&](auto &&el) {
      auto &[channel, sensor_ids] = el;
      bool activated = false;

      if (!sensor_ids.empty()) {
        for (const auto sensor_id : sensor_ids) {
          activated = activated || a_process_detector_wiring(channel(), sensor_id);
        }
      }

      channel.activated() = static_cast<Bit>(activated);
    });

    for_each(loadswitch_wirings_, [&](auto &&el) {
      auto &[channel, signal_head] = el;

      if (!signal_head.empty()) {
        for (const auto &turning_movement : signal_head) {
          const auto &[approach, turn] = turning_movement;
          a_process_loadswitch_wiring(channel(), channel.state(), approach, turn);
        }
      }
    });
  }

  bool load_config(const fs::path &a_path, const VerifyFuncGroup& a_verify_funcs = {}) noexcept
  {
    auto config = pugi::xml_document{};
    auto parse_result = config.load_file(a_path.c_str());

    if (!parse_result) {
      vtc::logger()->error("Failed to load config {}: {}", a_path.string(), parse_result.description());
      return false;
    }

    load_mmu16_channel_compatibility(config);

    auto dev_name = std::string{{0x4D, 0x47, 0x48, 0x44, 0x4C, 0x43}} + config.document_element().attribute("device").value();
    device_ = std::make_unique<SerialDevice>(dev_name.c_str());
    log_sdlc_frames_ = std::string{config.document_element().attribute("log_sdlc_frames").value()} == std::string{"true"};

    auto step = std::stod(config.document_element().attribute("simulation_step").value());
    auto [verify_simstep, verify_loadswitch_wiring, verify_detector_wiring] = a_verify_funcs;

    return (device_->ready())
        && (!verify_simstep || verify_simstep(step))
        && load_config(config, verify_loadswitch_wiring)
        && load_config(config, verify_detector_wiring);
  }

  bool enable_sdlc() noexcept
  {
    if (device_->ready() && (!sdlc_enabled_)) {
      sdlc_enabled_ = true;
      std::thread(
          [&]() {
            while (sdlc_enabled_) {
              auto count = device_->read(serial::buffer);

              if (log_sdlc_frames_) {
                auto frame_str = vtc::BytesToHexStr(serial::buffer.data(), count);
                vtc::logger()->info("Command Frame {} Addr {}: {}", serial::buffer[2], serial::buffer[0], frame_str);
              }

              auto [success, response_data] = vtc::serial::Dispatch({serial::buffer.data(), count});
              device_->write(response_data);

              if (log_sdlc_frames_) {
                auto frame_str = vtc::BytesToHexStr(response_data.data(), response_data.size());
                vtc::logger()->info("Response Frame {} Addr {}: {}", response_data[2], response_data[0], frame_str);
              }
            }
          }
      ).detach();
    }

    return sdlc_enabled_;
  }

  void disable_sdlc() noexcept
  {
    sdlc_enabled_ = false;
  }

private:

  bool load_config(pugi::xml_document &a_config, VerifyLoadswitchWiringFunc a_func)
  {
    bool result = true;

    for_each(loadswitch_wirings_, [&](auto &&el) {
      if (!result) return;

      auto &[channel, signal_head] = el;
      auto xpath = a_config.select_node(std::format("//loadswitch_wiring[@channel={}]", channel()).c_str());

      if (xpath) {
        for (auto tm : xpath.node().child("signal_head").children("turning_movement")) {
          auto a = std::stol(tm.attribute("approach").value());
          auto t = static_cast<Turn>(std::stol(tm.attribute("turn").value()));

          if (a_func && a_func(channel(), a, t))
            signal_head.emplace_back(a, t);
          else
            result = !a_func;
        }
      }
    });

    return result;
  }

  bool load_config(pugi::xml_document &a_config, VerifyDetectorWiringFunc a_func)
  {
    bool result = true;

    for_each(detector_wirings_, [&](auto &&el) {
      if (!result) return;

      auto &[channel, sensor_ids] = el;
      auto xpath = a_config.select_node(std::format("//detector_wiring[@channel={}]", channel()).c_str());

      if (xpath) {
        for (auto s : xpath.node().child("sensors").children("sensor")) {
          auto id = std::stol(s.attribute("id").value());

          if (a_func && a_func(channel(), id))
            sensor_ids.emplace_back(id);
          else
            result = !a_func;
        }
      }
    });
    return result;
  }

  static void load_mmu16_channel_compatibility(pugi::xml_document &a_config)
  {
    auto xpath = a_config.select_node("/HilsCI/mmu/@channel_compatibility");

    if (xpath) {
      auto compat_str = std::string{xpath.attribute().value()};

      if (compat_str.length() != 30)
        vtc::logger()->error("Invalid MMU16 compatibility string {}. Default used.", compat_str);
      else
        vtc::mmu::SetMMU16ChannelCompatibility(compat_str);
    } else {
      vtc::mmu::SetDefaultMMU16ChannelCompatibility();
      vtc::logger()->info("Default MMU16 compatibility is set.");
    }
  }

private:
  LoadswitchWirings loadswitch_wirings_{};
  DetectorWirings detector_wirings_{};
  std::atomic<bool> sdlc_enabled_{false};
  std::unique_ptr<SerialDevice> device_{nullptr};
  bool log_sdlc_frames_{false};
};

}

#endif