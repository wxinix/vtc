/*!
  C++ Virtualization Library of Traffic Cabinet
  Copyright (C) 2022  Wuping Xin

  MPL 1.1/GPL 2.0/LGPL 2.1 tri-license
*/

#pragma warning(disable:4068)
#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedTypeAliasInspection"
#pragma ide diagnostic ignored "UnusedParameter"
#pragma ide diagnostic ignored "OCUnusedStructInspection"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

#ifndef VTC_VTC_HPP
#define VTC_VTC_HPP

#include <array>
#include <atomic>
#include <bitset>
#include <filesystem>
#include <functional>
#include <span>

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <dlfcn.h>
#else
#error Unsupported platform.
#endif

#include <matchit/matchit.h>
#include <pugixml/pugixml.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

namespace vtc {

namespace fs = std::filesystem;

/*!
 * Alias of a shared_ptr of spdlog::logger. Note std::shared_ptr is not thread-safe.
 */
using VtcLogger = std::shared_ptr<spdlog::logger>;

/*!
 * Unnamed namespace for VtcLoggerHolder in order to have a global thread-safe
 * singleton logger without defining it in a separate translation unit.
 */
namespace {
/*!
 * Proxy class for thread-safe singleton logger.
 */
struct VtcLoggerHolder
{
  /*!
   * Inline initialization is possible with C++17. Alternatively, we can define
   * the logger separately outside VtcLoggerHolder class declaration but inside
   * the unnamed namespace. In this case, VtcLoggerHolder is a member of unnamed
   * namespace, its static data member does not have external linkage, which
   * allows this header to be included multiple times in different translation
   * unit.
   */
  inline static std::atomic<VtcLogger> logger{nullptr};
};

} // End of unnamed namespace for VtcLoggerHolder

/*!
 * Thread-safe access to the singleton logger. It ought to be called only after
 * logger has been set up, i.e., setup_logger has been called; otherwise, nullptr
 * will be returned.
 */
VtcLogger logger()
{
  return VtcLoggerHolder::logger;
}

/*!
 * Setup the logger singleton with a given output path and file name. The log file
 * will be saved to the designated output path, using the specified logger name
 * appended with "-log.txt".
 *
 * The parent path needs to be pre-existing. If for any reason, the log file cannot
 * be created, default logger will be used (on Windows, that is is the debug output).
 * If the log file can be created, the log file will be rotated by 1MB file size
 * and up to 3 files.
 *
 * This function can be called multiple times, as long as a_logger_name is different
 * each time. However, if the same logger name has been used, an exception will
 * be thrown.
 *
 * @param a_path A path where to save the log file.
 * @param a_logger_name Name of internal logger as well as part of output file name.
 *
 * @returns true if the intended logger is created, false default logger.
 */
bool setup_logger(const fs::path &a_path, const std::string &a_logger_name)
{
  using namespace spdlog;

  const auto p = a_path / "log";
  std::error_code ec;

  VtcLogger the_logger{nullptr};
  bool default_logger_created;

  // create_directory would return if exists.
  if (fs::create_directory(p, ec) || fs::exists(p)) {
    const auto log_file = (p / (a_logger_name + "-log.txt")).string();
    the_logger = rotating_logger_mt(a_logger_name, // Internal logger name
                                    log_file,      // Output file name
                                    1024 * 1024,   // 1 MB log file size
                                    3);            // rotate by 3 files at most
    default_logger_created = false;
  } else {
#ifdef _WIN32
    the_logger = synchronous_factory::create<sinks::windebug_sink_mt>(a_logger_name + "_windbg");
#else
    the_logger = spdlog::default_logger();
#endif
    default_logger_created = true;
  }

  VtcLoggerHolder::logger = the_logger;
  return !default_logger_created;
}

/*!
 * Enum representing binary bit value.
 */
enum class Bit : bool
{
  Off = false,
  On = true
};

/*!
 * 8 bit unsigned integer.
 */
using Byte = uint8_t;

/*!
 * 16 bit unsigned integer.
 */
using Word = uint16_t;

/*!
 * 32 bit unsigned integer.
 */
using Integer = uint32_t;

/*!
 * Index for various controller cabinet input and output variables.
 */
using Index = uint16_t;

/*!
 * Validate if an index is valid given its allowed maximum value.
 * @tparam I The subject index value.
 * @tparam N Maximum allowed index value.
 */
template<Index I, size_t N>
concept ValidIndex = (I >= 1) && (I <= N);

/*!
 * Validate if the value type of a controller variable is valid. Can be one of
 * the four types: Bit, Byte, Word, and Integer.
 * @tparam T The value type.
 */
template<typename T>
concept ValidValueType = std::disjunction_v<
    std::is_same<T, Bit>,
    std::is_same<T, Byte>,
    std::is_same<T, Word>,
    std::is_same<T, Integer>
>;

/*!
 * A cabinet variable representing an indexed cabinet entity, for example,
 * Variable<Byte, 1> can be used to represent an entity that takes Byte storage
 * with Index 1. The template allows compile-time definition of "array-like"
 * entities while preventing dynamic memory allocation during run time.
 *
 * @tparam T The value type of the cabinet variable.
 * @tparam I Index of the cabinet variable.
 */
template<typename T, Index I> requires ValidValueType<T>
struct Variable
{
  using value_t = T;

  Variable() = default;
  Variable(Variable &) = delete;
  Variable(Variable &&) = delete;
  Variable &operator=(Variable &) = delete;
  Variable &operator=(Variable &&) = delete;

  /*!
   * Operator () returns a reference to the interval value.
   */
  auto &operator()()
  {
    return value;
  }

  /*!
   * Index of the subject variable.
   */
  static constexpr Index index{I};

  /*!
   * Thread-safe atomic access of the variable value.
   */
  std::atomic<T> value{};
};

/*!
 * Get the value type of a type derived from Variable type.
 */
template<typename T>
using ValueType = typename T::Variable::value_t;

/*!
 * Convert a string into character array.
 * @tparam Is Index sequence
 * @param str String view
 * @return
 */
template<std::size_t ...Is>
constexpr auto substring_as_array(std::string_view str, std::index_sequence<Is...>)
{
  return std::array{str[Is]..., '\n'};
}

/*!
 * Convert the textual name of a type to string_view at compile time.
 * @tparam T Type to extract name.
 * @return string_view of the textual name of the type. Null terminator not included.
 */
template<typename T>
constexpr auto type_name() -> std::string_view
{
#if defined(__clang__)
  constexpr auto prefix = std::string_view{"[T = "};
  constexpr auto suffix = std::string_view{"]"};
  constexpr auto function = std::string_view{__PRETTY_FUNCTION__};
#elif defined(__GNUC__)
  constexpr auto prefix = std::string_view{"with T = "};
  constexpr auto suffix = std::string_view{"]"};
  constexpr auto function = std::string_view{__PRETTY_FUNCTION__};
#elif defined(_MSC_VER)
  constexpr auto prefix = std::string_view{"type_name<"};
  constexpr auto suffix = std::string_view{">(void)"};
  constexpr auto function = std::string_view{__FUNCSIG__};
#else
# error
  Unsupported compiler
#endif
  constexpr auto start = function.find(prefix) + prefix.size();
  constexpr auto end = function.rfind(suffix);

  static_assert(start < end);
  return function.substr(start, (end - start));
}

template<Index Offset, typename SeqType> /* */
requires (Offset >= 0)
struct offset_sequence;

/*!
 * Offsetting a sequence by the given template argument.
 * @tparam Offset Offset value. Note 0 means offset by 1.
 * @tparam Is The sequence to be "offset".
 */
template<Index Offset, Index... Is>
struct offset_sequence<Offset, std::integer_sequence<Index, Is...>>
{
  using type = std::integer_sequence<Index, Is + (Offset + 1)...>;
};

/*!
 * Offset value 0 means to "offset" the sequence by 1.
 */
template<Index Offset, typename SeqType>
using offset_sequence_t = typename offset_sequence<Offset, SeqType>::type;

/*!
  Retrieve an element from integer sequence at compile time. For example,

  auto seq = std::integer_sequence<unsigned, 9, 2, 5, 1, 9, 1, 15>{};
  auto val = get(seq, 6); // val equals to 15.

  @param i The index of the element.
  @return The element value.
 */
template<typename T, T... Is>
constexpr T get(std::integer_sequence<T, Is...>, std::size_t i)
{
  constexpr auto arr = std::array{Is...};
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

/*!
 * namespace cu naming convention all follows the same names as NTCIP 1202 camelCase.
 */
namespace cu {

/*!
 * Type tag for Controller Unit (CU) variable.
 */
struct CuVariableType
{
};

/*!
 * Concept for validating CU variable.
 * @tparam T
 */
template<typename T>
concept ValidCuVariable = std::is_same_v<typename T::type, CuVariableType>;

template<typename ValueT, Index I = 0>
struct CuVariable : Variable<ValueT, I>
{
  using type = CuVariableType;

  CuVariable() = default;
  CuVariable(CuVariable &) = delete;
  CuVariable(CuVariable &&) = delete;
  CuVariable &operator=(CuVariable &) = delete;
  CuVariable &operator=(CuVariable &&) = delete;
};

/*!
 * Template variable pattern so we can instantiate and access global
 * CU variable instances in a unified fashion.
 * @tparam T The type of the variable.
 */
template<typename T> requires ValidCuVariable<T>
T variable{};

/*!
 * .1.3.6.1.4.1.1206.4.2.1.1
 */
namespace phase {

/*!
 * The Maximum Number of Phases this Controller Unit supports.
 * This object indicates the maximum rows which shall appear in the
 * phaseTable object.
 *
 * INTEGER (2..40)
 * OID 1.3.6.1.4.1.1206.4.2.1.1.1
 */
constexpr Byte maxPhases{40};

/*!
 * The Maximum Number of Phase Groups (8 Phases per group) this
 * Controller Unit supports. This value is equal to
 * TRUNCATE((maxPhases + 7) / 8). This object indicates the maximum
 * rows which shall appear in the phaseStatusGroupTable and
 * phaseControlGroupTable.
 *
 * INTEGER (1..5)
 * OID 1.3.6.1.4.1.1206.4.2.1.1.3
 */
constexpr Byte maxPhaseGroups{5};

}

/*!
 * .1.3.6.1.4.1.1206.4.2.1.2
 */
namespace detector {

/*!
 * The Maximum Number of Vehicle Detectors this Controller Unit supports. This object
 * indicates the maximum rows which shall appear in the vehicleDetectorTable object.
 *
 * .1.3.6.1.4.1.1206.4.2.1.2.1
 */
constexpr Byte maxVehicleDetectors{128};

/*!
 * The maximum number of detector status groups (8 detectors per group) this device supports.
 * This value is equal to TRUNCATE [(maxVehicleDetectors + 7 ) / 8]. This object indicates
 * the maximum number of rows which shall appear in the vehicleDetectorStatusGroupTable object.
 *
 * .1.3.6.1.4.1.1206.4.2.1.2.3
 */
constexpr Byte maxVehicleDetectorStatusGroups{16};

/*!
 * The Maximum Number of Pedestrian Detectors this Controller Unit supports.
 * This object indicates the maximum rows which shall appear in the pedestrianDetectorTable object.
 *
 * .1.3.6.1.4.1.1206.4.2.1.2.6
 */
constexpr Byte maxPedestrianDetectors{72};

}

/*!
 * .1.3.6.1.4.1.1206.4.2.1.3
 */
namespace unit {

/*!
 * This object contains the maximum number of alarm groups (8 alarm inputs per group)
 * this device supports. This object indicates the maximum rows which shall appear
 * in the alarmGroupTable object.
 *
 * .1.3.6.1.4.1.1206.4.2.1.3.11
 */
constexpr Byte maxAlarmGroups{1};

/*!
 * The Maximum Number of Special Functions this Actuated Controller Unit supports.
 *
 * .1.3.6.1.4.1.1206.4.2.1.3.13
 */
constexpr Byte maxSpecialFunctionOutputs{16};

}

/*!
 * .1.3.6.1.4.1.1206.4.2.1.4
 */
namespace coord {

/*!
 * The maximum number of Patterns this Controller Unit supports. This object
 * indicates how many rows are in the patternTable object (254 and 255 are defined
 * as non-pattern Status for Free and Flash).
 *
 * .1.3.6.1.4.1.1206.4.2.1.4.5
 */
constexpr Byte maxPatterns{128};

/*!
 * The maximum number of Split Plans this Actuated Controller Unit supports.
 * This object indicates how many Split plans are in the splitTable object.
 *
 * .1.3.6.1.4.1.1206.4.2.1.4.8
 */
constexpr Byte maxSplits{128};

}

/*!
 * .1.3.6.1.4.1.1206.4.2.1.5
 */
namespace timebaseAsc {

/*!
 *  The Maximum Number of Actions this device supports. This object indicates
 *  the maximum rows which shall appear in the timebaseAscActionTable object.
 *
 *  .1.3.6.1.4.1.1206.4.2.1.5.2
 */
constexpr Byte maxTimebaseAscActions{64};

}

/*!
 * .1.3.6.1.4.1.1206.4.2.1.6
 */
namespace preempt {

/*!
 * The Maximum Number of Preempts this Actuated Controller Unit supports.
 * This object indicates the maximum rows which shall appear in the
 * preemptTable object.
 * .1.3.6.1.4.1.1206.4.2.1.6.1
 */
constexpr Byte maxPreempts{40};

}

/*!
 * .1.3.6.1.4.1.1206.4.2.1.7
 */
namespace ring {

/*!
 * The value of this object shall specify the maximum number of rings
 * this device supports.
 * .1.3.6.1.4.1.1206.4.2.1.7.1
 */
constexpr Byte maxRings{16};

/*!
 * The value of this object shall specify the maximum number of sequence plans
 * this device supports.
 * .1.3.6.1.4.1.1206.4.2.1.7.2
 */
constexpr Byte maxSequences{20};

/*!
 * The maximum number of Ring Control Groups (8 rings per group) this Actuated
 * Controller Unit supports. This value is equal to TRUNCATE[(maxRings + 7) / 8].
 * This object indicates the maximum rows which shall appear in the
 * ringControlGroupTable object.
 * .1.3.6.1.4.1.1206.4.2.1.7.4
 */
constexpr Byte maxRingControlGroups{2};

}

/*!
 * .1.3.6.1.4.1.1206.4.2.1.8
 */
namespace channel {

/*!
 * The Maximum Number of Channels this Actuated Controller Unit supports.
 * This object indicates the maximum rows which shall appear in the
 * channelTable object.
 * .1.3.6.1.4.1.1206.4.2.1.8.1
 */
constexpr Byte maxChannels{32};

/*!
 * The maximum number of Channel Status Groups (8 channels per group) this
 * Actuated Controller Unit supports. This value is equal to
 * TRUNCATE [(maxChannels + 7) / 8]. This object indicates the maximum
 * rows which shall appear in the channelStatusGroupTable object.
 * .1.3.6.1.4.1.1206.4.2.1.8.3
 */
constexpr Byte maxChannelStatusGroups{4};

}

/*!
 * .1.3.6.1.4.1.1206.4.2.1.9
 */
namespace overlap {

/*!
 * The Maximum Number of Overlaps this Actuated Controller Unit supports.
 * This object indicates the maximum number of rows which shall appear
 * in the overlapTable object.
 * .1.3.6.1.4.1.1206.4.2.1.9.1
 */
constexpr Byte maxOverlaps{32};

/*!
 * The Maximum Number of Overlap Status Groups (8 overlaps per group)
 * this Actuated Controller Unit supports. This value is equal to
 * TRUNCATE [(maxOverlaps + 7) / 8]. This object indicates the maximum rows
 * which shall appear in the overlapStatusGroupTable object.
 * .1.3.6.1.4.1.1206.4.2.1.9.3
 */
constexpr Byte maxOverlapStatusGroups{4};

}

/*!
 * .1.3.6.1.4.1.1206.3.36.1.1.13
 */
namespace prioritor {

constexpr Byte maxPrioritors{16};
constexpr Byte maxPrioritorGroups{2};

}

} // end of namespace vtc::cu

namespace biu {

constexpr size_t max_det_bius{8};
constexpr size_t max_tf_bius{8};

struct BiuVariableType
{
};

template<typename T>
concept ValidBiuVariable = std::is_same_v<typename T::type, BiuVariableType>;

template<typename ValueT, Index I = 0>
struct BiuVariable : Variable<ValueT, I>
{
  using type = BiuVariableType;

  BiuVariable() = default;
  BiuVariable(BiuVariable &) = delete;
  BiuVariable(BiuVariable &&) = delete;
  BiuVariable &operator=(BiuVariable &) = delete;
  BiuVariable &operator=(BiuVariable &&) = delete;
};

/*!
 * Template variable pattern so we can instantiate and access global
 * BIU variable instances in a unified fashion.
 * @tparam T The type of the variable.
 */
template<typename T> requires ValidBiuVariable<T>
T variable{};

} // end of namespace vc::biu

namespace io {

struct IoVariableType
{
};

struct InputType : IoVariableType
{
};

struct OutputType : IoVariableType
{
};

template<typename T>
concept ValidIoVariable = std::is_base_of_v<IoVariableType, typename T::type>;

template<typename IoT, typename ValueT, Index I = 0>
struct IoVariable : Variable<ValueT, I>
{
  using type = IoT;

  IoVariable() = default;
  IoVariable(IoVariable &) = delete;
  IoVariable(IoVariable &&) = delete;
  IoVariable &operator=(IoVariable &) = delete;
  IoVariable &operator=(IoVariable &&) = delete;
};

/*!
 * Template variable pattern so we can instantiate and access global
 * IO variable instances in a unified fashion.
 * @tparam T The type of the variable.
 */
template<typename T> requires ValidIoVariable<T>
T variable{};

namespace output {

/*!
 * Mapped to an output and commonly used in user logic, in pair
 * with FlashState.
 */
struct AltFlashState : IoVariable<OutputType, Bit>
{
};

/*!
 * Mapped to an output and commonly used in user logic, in pair
 * with AltFlashState.
 */
struct FlashState : IoVariable<OutputType, Bit>
{
};

struct GlobalVariable : IoVariable<OutputType, Bit>
{
};

struct NotActive : IoVariable<OutputType, Bit>
{
};

/*!
 * Some "old" NEMA cabinets have inputs or test switches tied to AuxFunction.
 * Treated as interchangeable with SpecialFunctionOn. Both can be used as
 * placeholder variables to be called in Time of Day action schedules or
 * in user logic while being mapped to an cabinet output.
 */
struct AuxFunctionOn : IoVariable<OutputType, Bit>
{
};

/*!
 * Load Switch Driver Basic Vehicle (three per phase) for Channel I,
 * as provision of Green output for basic vehicle phase, or provision
 * of Walk output for Pedestrian Movement.
 *
 * Per Phase or Per Pedestrian Movement output - see NEMA-TS2 3.5.3.12
 * @tparam I Channel index, [1, maxChannels].
 */
template<Index I> /* */
requires ValidIndex<I, cu::channel::maxChannels>
struct ChannelGreenWalkDriver : IoVariable<OutputType, Bit, I>
{
};

/*!
 * Load Switch Driver Basic Vehicle (three per phase) for Channel I,
 * as provision of Red output for basic vehicle phase, or provision
 * of Don't Walk output for Pedestrian Movement.
 *
 * Per Phase or Per Pedestrian Movement output - see NEMA-TS2 3.5.3.12
 * @tparam I Channel index, [1, maxChannels].
 */
template<Index I> /* */
requires ValidIndex<I, cu::channel::maxChannels>
struct ChannelRedDoNotWalkDriver : IoVariable<OutputType, Bit, I>
{
};

/*!
 * Load Switch Driver Basic Vehicle (three per phase) for Channel I,
 * as provision of Yellow output for basic vehicle phase, or provision
 * of Pedestrian Clearance output for Pedestrian Movement.
 *
 * Per Phase or Per Pedestrian Movement output - see NEMA-TS2 3.5.3.12
 * @tparam I Channel index, [1, maxChannels].
 */
template<Index I> /* */
requires ValidIndex<I, cu::channel::maxChannels>
struct ChannelYellowPedClearDriver : IoVariable<OutputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::phase::maxPhases>
struct PhaseGreen : IoVariable<OutputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::phase::maxPhases>
struct PhaseYellow : IoVariable<OutputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::phase::maxPhases>
struct PhaseRed : IoVariable<OutputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::phase::maxPhases>
struct PhaseWalk : IoVariable<OutputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::phase::maxPhases>
struct PhasePedClearance : IoVariable<OutputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::phase::maxPhases>
struct PhaseDoNotWalk : IoVariable<OutputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::phase::maxPhases>
struct PhasePreClear : IoVariable<OutputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::phase::maxPhases>
struct PhasePreClear2 : IoVariable<OutputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::phase::maxPhases>
struct PhaseAdvWarning : IoVariable<OutputType, Bit, I>
{
};

/*!
 * An output to indicate call status (vehicle or pedestrian, or both) of the phase,
 * activated when the CU is not in the Green interval of that phase, which has a
 * demand in that phase. Neither the Phase Omit nor Pedestrian Omit inputs shall
 * affect the PhaseCheck output.
 *
 * Per Phase - see NEMA-TS2 3.5.3.12
 * @tparam I Index of the phase, [1, maxPhases]
 */
template<Index I> /* */
requires ValidIndex<I, cu::phase::maxPhases>
struct PhaseCheck : IoVariable<OutputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::phase::maxPhases>
struct PedCall : IoVariable<OutputType, Bit, I>
{
};

/*!
 * An output to indicate phase status. The PhaseOn output of a particular phase
 * activated during the Green, Yellow, and Red Clearance intervals of that phase.
 * It shall be permissible for this output to be active during the
 * Red Dwell state.
 *
 * Per Phase - see NEMA-TS2 3.5.3.12
 * @tparam I Index of the phase, [1, maxPhases]
 */
template<Index I> /* */
requires ValidIndex<I, cu::phase::maxPhases>
struct PhaseOn : IoVariable<OutputType, Bit, I>
{
};

/*!
 * An output of a particular phase activated when the phase is committed
 * to be next in sequence and remains present until the phase becomes
 * active. The phase next to be serviced shall be determined at the end of
 * Green interval of the terminating phase; except that if the decision
 * cannot be made at the end of the Green interval, it shall not be
 * made until after the end of all Vehicle Change and Clearance intervals.
 *
 * Per Phase - see NEMA-TS2 3.5.3.12
 * @tparam I Index of the phase, [1, maxPhases]
 */
template<Index I> /* */
requires ValidIndex<I, cu::phase::maxPhases>
struct PhaseNext : IoVariable<OutputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::phase::maxPhases>
struct PhaseOmitStatus : IoVariable<OutputType, Bit, I>
{
};

/*
 One or more of the coded status bit states shown in NEMA-TS2 Table 3-12
 might be omitted from a normal cycle of operation. (AEI) Only one of the
 coded status codes shall be active when the following conditions are
 present in the CU:

 --------------------------------------------------------------------
 Code                   Bit States                   State
   #            A           B            C           Names
 --------------------------------------------------------------------
   0           OFF          OFF          OFF         Min Green
   1           ON           OFF          OFF         Extension
   2           OFF          ON           OFF         Maximum
   3           ON           ON           OFF         Green Rest
 --------------------------------------------------------------------
   4           OFF          OFF          ON          Yellow Change
   5           ON           OFF          ON          Red Clearance
 --------------------------------------------------------------------
   6           OFF          ON           ON          Red Rest
   7           ON           ON           ON          Undefined
 --------------------------------------------------------------------

  The active phase is in Green interval and operating in ACTUATED MODE:
      Code 0 - Min Timing - When timing initial, Walk, or Ped Clearance
                            portion of the Green interval
      Code 1 - Ext Timing - The portion of Green interval following the
                            completion of min timings when timing an extension(s)
      Code 2 - Max Timing - That portion of Green interval following the
                            completion of min timings when not timing an extension
                            and the Max Green is Timing (e.g., when the Hold
                            input is active)
      Code 3 - Green Rest - That portion of Green interval when the min timings are
                            complete, Passage Timer is timed out, and the Max Green
                            timer is either timed out or has not started.

  The active phase is in Green interval and operating in NON-ACTUATED MODE:
      Code 0 - Walk       - When timing the Walk portion of the Green interval
                            (non-actuated State A)
      Code 1 - Walk Hold  - When the Walk output is active, Walk timing is complete
                            and Hold input is active (non-actuated State B)
      Code 2 - PedClr     - When timing the Pedestrian Clearance interval or the
                            remaining portion of Min Green (non-actuated State C)
      Code 3 - Green Rest - When the timing of Pedestrian and Min Green intervals
                            are complete (non-actuated State D)

  The active phase is not in its Green interval.
      Code 4 - YeChange   - When timing Yellow Change.
      Code 5 - RedClr     - When timing Red Clearance.
      Code 6 - Red Rest   - When timing is complete and a Red indication is displayed.
      Code 7 - Undefined  - What do say?
*/

/*!
 * Bit A of the coded Ring 1 status.
 * Per Ring - See NEMA-TS2 3.5.4.2
 */
struct StatusBitA_Ring_1 : IoVariable<OutputType, Bit>
{
};

/*!
 * Bit B of the coded Ring 1 status.
 * Per Ring - See NEMA-TS2 3.5.4.2
 */
struct StatusBitB_Ring_1 : IoVariable<OutputType, Bit>
{
};

/*!
 * Bit C of the coded Ring 1 status.
 * Per Ring - See NEMA-TS2 3.5.4.2
 */
struct StatusBitC_Ring_1 : IoVariable<OutputType, Bit>
{
};

/*!
 * Bit A of the coded Ring 2 status.
 * Per Ring - See NEMA-TS2 3.5.4.2
 */
struct StatusBitA_Ring_2 : IoVariable<OutputType, Bit>
{
};

/*!
 * Bit BV of the coded Ring 2 status.
 * Per Ring - See NEMA-TS2 3.5.4.2
 */
struct StatusBitB_Ring_2 : IoVariable<OutputType, Bit>
{
};

/*!
 * Bit C of the coded Ring 2 status.
 * Per Ring - See NEMA-TS2 3.5.4.2
 */
struct StatusBitC_Ring_2 : IoVariable<OutputType, Bit>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::overlap::maxOverlaps>
struct OverlapProtectedGreen : IoVariable<OutputType, Bit, I>
{
};

/*!
 * Load Switch Drivers, Vehicle Overlap (three per overlap)
 * Provision of Green output for the overlap channel.
 *
 * A circuit closure to Logic Ground shall be maintained at the output
 * at all times. The output shall energize the appropriate overlap
 * signal load switching circuit to result in a Green indication for the
 * duration of such required indication.
 *
 * Per Unit - See NEMA-TS2 3.5.5.6
 * @tparam I Overlap index, [1, maxOverlaps]
 */
template<Index I> /* */
requires ValidIndex<I, cu::overlap::maxOverlaps>
struct OverlapGreen : IoVariable<OutputType, Bit, I>
{
};

/*!
 * Load Switch Drivers, Vehicle Overlap (three per overlap)
 * Provision of Red output for the overlap channel.
 *
 * A circuit closure to Logic Ground shall be maintained at the output
 * at all times. The output shall energize the appropriate overlap
 * signal load switching circuit to result in a Red indication for the
 * duration of such required indication.
 *
 * Per Unit - See NEMA-TS2 3.5.5.6
 * @tparam I Overlap index, [1, maxOverlaps]
 */
template<Index I> /* */
requires ValidIndex<I, cu::overlap::maxOverlaps>
struct OverlapRed : IoVariable<OutputType, Bit, I>
{
};

/*!
 * Load Switch Drivers, Vehicle Overlap (three per overlap)
 * Provision of Yellow output for the overlap channel.
 *
 * A circuit closure to Logic Ground shall be maintained at the output
 * at all times. The output shall energize the appropriate overlap
 * signal load switching circuit to result in a Yellow indication for the
 * duration of such required indication.
 *
 * Per Unit - See NEMA-TS2 3.5.5.6
 * @tparam I Overlap index, [1, maxOverlaps]
 */
template<Index I> /* */
requires ValidIndex<I, cu::overlap::maxOverlaps>
struct OverlapYellow : IoVariable<OutputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::preempt::maxPreempts>
struct PreemptStatus : IoVariable<OutputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::preempt::maxPreempts>
struct PreemptStatusFlash : IoVariable<OutputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::unit::maxSpecialFunctionOutputs>
struct SpecialFunction : IoVariable<OutputType, Bit, I>
{
};

/*!
 * See NEMA-TS2 3.9.1.2
 */
struct UnitAutomaticFlashStatus : IoVariable<OutputType, Bit>
{
};

/*!
 * An open collector output which is maintained True (Low state) as long as the
 * voltages within the CU do not drop below predetermined levels required to
 * provide normal operation.
 *
 * The output will also be False (High state) during CU generated flash modes.
 *
 * Per Unit - See NEMA-TS2 3.5.5.6
 */
struct UnitFaultMonitor : IoVariable<OutputType, Bit>
{
};

/*!
 * CU Voltage Monitor (Type A2 Only). An open collector output which is maintained
 * True (Low state) as long as the voltages within the CU do not drop below
 * predetermined levels required to provide normal operation.
 *
 * The output will also be False (High state) during CU generated flash modes.
 *
 * This output operates concurrently with Fault Monitor output except during periods
 * of No Fault Flash (i.e., Automatic Flash, and Preempt Flash).
 *
 * Per Unit - See NEMA-TS2 3.5.5.6
 */
struct UnitVoltageMonitor : IoVariable<OutputType, Bit>
{
};

/*!
 *
 */
struct UnitFreeCoordStatus : IoVariable<OutputType, Bit>
{
};

/*
 Pattern Outputs — In the applicable mode, seven outputs for master type
 interconnect interface drivers shall be available. The outputs shall
 echo the active pattern.

 All outputs shall be constantly ON when active except offset which
 is OFF for a minimum of 3 seconds or 3% of the cycle once each cycle
 beginning at the “0” point of the cycle.

 Pattern outputs shall be interpreted as command requests in accordance
 with Table 3–6 and Table 3–7.

 See NEMA-TS2 3.4.5.3

 Table 3-7 Offset (See NEMA-TS2 3.4.5.2)
 --------------------------------------------
 Command      Off 1       Off 2      Off 3
 Request      Input       Input      Input
 --------------------------------------------
 Offset 1     ON          OFF        OFF
 Offset 2     OFF         ON         OFF
 Offset 3     OFF         OFF        ON
 Sync         OFF         OFF        OFF
 --------------------------------------------

 */

/*!
 * Pattern Output Offset Bit 1. See NEMA-TS2 3.4.5.2
 */
struct UnitOffset_1 : IoVariable<OutputType, Bit>
{
};

/*!
 * Pattern Output Offset Bit 2. See NEMA-TS2 3.4.5.2
 */
struct UnitOffset_2 : IoVariable<OutputType, Bit>
{
};

/*!
 * Pattern Output Offset Bit 3. See NEMA-TS2 3.4.5.2
 */
struct UnitOffset_3 : IoVariable<OutputType, Bit>
{
};

/*!
 See NEMA-TS2 3.4.5.2 Table 3-6 Timing Plan
 */
struct UnitTimingPlanA : IoVariable<OutputType, Bit>
{
};

/*!
 See NEMA-TS2 3.4.5.2 Table 3-6 Timing Plan
 */
struct UnitTimingPlanB : IoVariable<OutputType, Bit>
{
};

/*!
 See NEMA-TS2 3.4.5.2 Table 3-6 Timing Plan
 */
struct UnitTimingPlanC : IoVariable<OutputType, Bit>
{
};

/*!
 See NEMA-TS2 3.4.5.2 Table 3-6 Timing Plan
 */
struct UnitTimingPlanD : IoVariable<OutputType, Bit>
{
};

/*
 * See NEMA-TS2 3.8.3 External Interface
 * Time Base Outputs - Three outputs for Time Base Auxiliary. They shall be On
 * when their respective auxiliary is part of the current time base event.
 */

/*!
 * Time Base Auxiliary 1 See NEMA-TS2 3.8.3 External Interface
 */
struct UnitTBCAux_1 : IoVariable<OutputType, Bit>
{
};

/*!
 * Time Base Auxiliary 2 See NEMA-TS2 3.8.3 External Interface
 */
struct UnitTBCAux_2 : IoVariable<OutputType, Bit>
{
};

/*!
 * Time Base Auxiliary 3 See NEMA-TS2 3.8.3 External Interface
 */
struct UnitTBCAux_3 : IoVariable<OutputType, Bit>
{
};

struct Watchdog : IoVariable<OutputType, Bit>
{
};

template<Index I> /* */
requires ValidIndex<I, biu::max_det_bius>
struct DetectorReset : IoVariable<OutputType, Byte, I>
{
};

} // end of namespace vc::io::output

namespace input {

template<Index I> /* */
requires ValidIndex<I, cu::detector::maxVehicleDetectors>
struct ChannelFaultStatus : IoVariable<InputType, Bit, I>
{
};

/*!
 * TF BIU 2 for setting free (no coord).
 */
struct CoordFreeSwitch : IoVariable<InputType, Bit>
{
};

struct CustomAlarm : IoVariable<InputType, Bit>
{
};

struct DoorAjar : IoVariable<InputType, Bit>
{
};

struct ManualControlGroupAction : IoVariable<InputType, Bit>
{
};

struct MinGreen_2 : IoVariable<InputType, Bit>
{
};

struct NotActive : IoVariable<InputType, Bit>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::overlap::maxOverlaps>
struct OverlapOmit : IoVariable<InputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::coord::maxPatterns>
struct PatternInput : IoVariable<InputType, Bit, I>
{
};

/*!
 * Provision to enter a pedestrian demand for service into the appropriate
 * phase of the CU.
 * @tparam I
 */
template<Index I> /* */
requires ValidIndex<I, cu::detector::maxPedestrianDetectors>
struct PedDetCall : IoVariable<InputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::phase::maxPhases>
struct PhaseForceOff : IoVariable<InputType, Bit, I>
{
};

/*
  Hold retains the existing Green indications and has different CU responses:

  a. For a non-actuated phase, energizing the Hold input shall maintain the CU in
     the timed-out Walk period with a Green and Walk indication displayed.

     Energizing the Hold input while timing the Walk portion shall not inhibit
     the timing of this period. De-energizing the Hold input and with the Walk
     interval timed out shall cause the CU to advance into the Pedestrian
     Clearance interval.

     Re-application of the Hold input while timing the Pedestrian Clearance
     portion shall neither inhibit the timing of this period nor termination
     of the phase.

  b. For an actuated phase, energizing, and de-energizing the Hold input
     shall be as follows:

     Energizing the Hold input shall allow the CU to time normally but shall
     inhibit its advance into the vehicle change interval. Energizing the Hold
     input shall inhibit the recycle of the pedestrian service unless
     the Pedestrian Recycle input is active and a serviceable pedestrian call
     exists on the phase. The rest state indications for that phase shall be Green
     for traffic and Don’t Walk for pedestrians.

     De-energizing Hold input shall allow the CU to advance into the Green Dwell/Select
     state when all Green periods are timed out.

     De-energizing Hold input with all intervals timed out, shall allow the CU
     to recycle the Walk interval if there is no conflicting demand for service
     and a pedestrian call exists for that phase. However, if there is any
     serviceable demand on a conflicting phase with the Hold de-energized and
     with all intervals timed-out, the CU shall advance into the Yellow Change
     interval and not recycle the Walk on that phase until those demands have
     been served.
 */

/*!
 * Hold a phase.
 * @tparam I Index of the phase.
 */
template<Index I> /* */
requires ValidIndex<I, cu::phase::maxPhases>
struct PhaseHold : IoVariable<InputType, Bit, I>
{
};

/*
  Inhibit the selection of a phase due to a pedestrian call on that phase and to
  prohibit the servicing of a pedestrian call on the phase.

  This input when active shall prevent the starting of the pedestrian movement
  of that phase. After the beginning of the phase Green, a pedestrian call shall
  be serviced or recycled only in the absence of a serviceable conflicting call
  and with Pedestrian Omit on the phase nonactive. Activation of this input
  shall not affect a pedestrian movement in the process of timing.
 */

/*!
 * Inhibit the selection of a phase due to a pedestrian call on that phase.
 * @tparam I Index of the phase.
 */
template<Index I> /* */
requires ValidIndex<I, cu::phase::maxPhases>
struct PhasePedOmit : IoVariable<InputType, Bit, I>
{
};

/*
  Cause omission of a phase, even in presence of demand. It shall affect phase
  selection. The omission shall continue in effect until the signal is removed.
  The phase to be omitted shall not present a conflicting call to any other
  phase, but shall accept and store calls.

  Activation of this input shall not affect a phase in the process of timing.
*/

/*!
 * Omit a phase.
 * @tparam I Index of the phase.
 */
template<Index I> /* */
requires ValidIndex<I, cu::phase::maxPhases>
struct PhasePhaseOmit : IoVariable<InputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::preempt::maxPreempts>
struct PreemptGateDown : IoVariable<InputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::preempt::maxPreempts>
struct PreemptGateUp : IoVariable<InputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::preempt::maxPreempts>
struct PreemptHighPrioritorLow : IoVariable<InputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::preempt::maxPreempts>
struct PreemptInput : IoVariable<InputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::preempt::maxPreempts>
struct PreemptInputCRC : IoVariable<InputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::preempt::maxPreempts>
struct PreemptInputNormalOff : IoVariable<InputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::preempt::maxPreempts>
struct PreemptInputNormalOn : IoVariable<InputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::prioritor::maxPrioritors>
struct PrioritorCheckIn : IoVariable<InputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::prioritor::maxPrioritors>
struct PrioritorCheckOut : IoVariable<InputType, Bit, I>
{
};

template<Index I> /* */
requires (I >= 1)
struct PrioritorPreemptDetector : IoVariable<InputType, Bit, I>
{
};

/*!
 * Provision for termination of the Green timing in the actuated mode, or Walk Hold
 * in the non-actuated mode of the active phase in the timing ring. Such terminating
 * subject to presence of a serviceable conflicting call. The Force Off function
 * shall not be effective during the timing of the Initial, Walk, or Pedestrian
 * Clearance. The Force Off input shall be effective only as long as the input is
 * sustained.
 * @tparam I Index of the ring.
 */
template<Index I> /* */
requires ValidIndex<I, cu::ring::maxRings>
struct RingForceOff : IoVariable<InputType, Bit, I>
{
};

/*!
 * An input to disable the maximum termination functions of all phases in the
 * selected timing ring. The input shall not inhibit the timing of Maximum
 * Green.
 * @tparam I Index of the ring.
 */
template<Index I> /* */
requires ValidIndex<I, cu::ring::maxRings>
struct RingInhibitMaxTermination : IoVariable<InputType, Bit, I>
{
};

/*!
 * Input to allow the selection of a second maximum time setting on
 * all phases of the ring.
 * @tparam I Index of the ring.
 */
template<Index I> /* */
requires ValidIndex<I, cu::ring::maxRings>
struct RingMax2Selection : IoVariable<InputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::ring::maxRings>
struct RingMax3Selection : IoVariable<InputType, Bit, I>
{
};

/*!
 * An input to cause omission of Red Clearance interval timing(s).
 * @tparam I Index of the ring.
 */
template<Index I> /* */
requires ValidIndex<I, cu::ring::maxRings>
struct RingOmitRedClearance : IoVariable<InputType, Bit, I>
{
};

/*!
 * An input to control the recycling of the pedestrian movement. The
 * recycling operation is dependent upon whether the phase is
 * in the actuated or non-actuated mode.
 *
 * When the phase is in actuated model, if a serviceable pedestrian
 * call exists on the phase, and Hold input is active, the pedestrian movement
 * shall be recycled when Pedestrian Recycle input is active, regardless
 * whether a serviceable conflicting call exists.
 *
 * @tparam I Index of the ring.
 */
template<Index I> /* */
requires ValidIndex<I, cu::ring::maxRings>
struct RingPedestrianRecycle : IoVariable<InputType, Bit, I>
{
};

/*!
 * Input to require rest in Red of all phases in the timing ring by continuous
 * application of an external signal. Registration of a serviceable conflicting
 * call shall result in immediate advance from Red Rest to Green of the
 * demanding phase. Registration of a serviceable conflicting call before entry
 * into Red Rest state, even with this signal applied, shall result in
 * termination of the active phase and selection of the next phase in the normal
 * manner and with appropriate change and clearance intervals.
 * @tparam I Index of the ring.
 */
template<Index I> /* */
requires ValidIndex<I, cu::ring::maxRings>
struct RingRedRest : IoVariable<InputType, Bit, I>
{
};

/*!
 * An input which when activated causes cessation of CU ring timing for the duration
 * of such activation. Upon removal of activation from this input, all portions which
 * were timing will resume timing. During stop timing, vehicle actuation on non-Green
 * phases shall be recognized; vehicle actuation on Green phases shall reset the
 * Passage Time timer in the normal manner; and the CU shall not terminate any
 * interval or interval portion or select another phase, except by activation of the
 * Interval Advance input. Operation of the Interval Advance with Stop Time activated
 * shall clear any stored calls on a phase when the CU is advanced through the Green
 * interval of that phase.
 * @tparam I Index of the ring.
 */
template<Index I> /* */
requires ValidIndex<I, cu::ring::maxRings>
struct RingStopTiming : IoVariable<InputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::ring::maxRings>
struct SpecialFunctionInput : IoVariable<InputType, Bit, I>
{
};

struct UnitAlarm_1 : IoVariable<InputType, Bit>
{
};

struct UnitAlarm_2 : IoVariable<InputType, Bit>
{
};

struct UnitAlternateSequenceA : IoVariable<InputType, Bit>
{
};

struct UnitAlternateSequenceB : IoVariable<InputType, Bit>
{
};

struct UnitAlternateSequenceC : IoVariable<InputType, Bit>
{
};

struct UnitAlternateSequenceD : IoVariable<InputType, Bit>
{
};

struct UnitAutomaticFlash : IoVariable<InputType, Bit>
{
};

struct UnitCallPedNAPlus : IoVariable<InputType, Bit>
{
};

/*!
 * Call to Non-actuated Mode for Ring 1
 *
 * When activated will cause any phases appropriately programed to operate
 * in the Non-actuated Model. Only phases equipped for pedestrian service
 * shall be used for Non-actuated Mode operation.
 *
 * Basically this is equivalent to placing Max Recall + Ped Recall on the
 * phase.
 */
struct UnitCallToNonActuated_1 : IoVariable<InputType, Bit>
{
};

/*!
 * Call to Non-actuated Mode for Ring 2
 *
 * When activated will cause any phases appropriately programed to operate
 * in the Non-actuated Model. Only phases equipped for pedestrian service
 * shall be used for Non-actuated Mode operation.
 *
 * Basically this is equivalent to placing Max Recall + Ped Recall on the
 * phase.
 */
struct UnitCallToNonActuated_2 : IoVariable<InputType, Bit>
{
};

struct UnitClockReset : IoVariable<InputType, Bit>
{
};

struct UnitCMUMMUFlashStatus : IoVariable<InputType, Bit>
{
};

struct UnitDimming : IoVariable<InputType, Bit>
{
};

struct UnitExternWatchDog : IoVariable<InputType, Bit>
{
};

/*!
 * Input to place a recurring demand on all vehicle phases for a minimum
 * vehicle service.
 */
struct UnitExternalMinRecall : IoVariable<InputType, Bit>
{
};

/*!
 * An input to cause the CU to revert to its programed initialization phases
 * and intervals upon application. Upon removal of this input, the CU shall
 * commence normal timing.
 */
struct UnitExternalStart : IoVariable<InputType, Bit>
{
};

/*!
 * Input to disable CU indicators. The CU might not employ this input (AEI)
 */
struct UnitIndicatorLampControl : IoVariable<InputType, Bit>
{
};

/*!
 * A complete ON-OFF operation of this input shall cause immediate
 * termination of the interval in process of timing. Where concurrent
 * interval timing exists, use of this input shall cause immediate
 * termination of the interval which would terminate next without
 * such actuation.
 *
 * See NEMA-TS2 3.5.5.5
 */
struct UnitIntervalAdvance : IoVariable<InputType, Bit>
{
};

/*
 I/O Mode bits allows NEMA-TS2 Type 2 to select I/O function of
 specific hardware I/O based on the bits.

 See NEMA-TS2 3.5.5.5, Item 15
 */

/*!
 * I/O Mode Bit 0
 */
struct UnitIOModeBit_0 : IoVariable<InputType, Bit>
{
};

/*!
 * I/O Mode Bit 1
 */
struct UnitIOModeBit_1 : IoVariable<InputType, Bit>
{
};

/*!
 * I/O Mode Bit 2
 */
struct UnitIOModeBit_2 : IoVariable<InputType, Bit>
{
};

struct UnitIOModeBit_3 : IoVariable<InputType, Bit>
{
};

struct UnitITSLocalFlashSense : IoVariable<InputType, Bit>
{
};

struct UnitLocalFlash : IoVariable<InputType, Bit>
{
};

struct UnitLocalFlashSense : IoVariable<InputType, Bit>
{
};

/*!
 * An input to place vehicle and pedestrian calls on all phases, stop
 * CU timing in all intervals except vehicle change and clearance intervals,
 * and inhibit the operation of Interval Advance during vehicle change
 * and clearance intervals.
 *
 * See NEMA-TS2 3.5.5.5
 */
struct UnitManualControlEnable : IoVariable<InputType, Bit>
{
};

struct UnitOffset_1 : IoVariable<InputType, Bit>
{
};

struct UnitOffset_2 : IoVariable<InputType, Bit>
{
};

struct UnitOffset_3 : IoVariable<InputType, Bit>
{
};

struct UnitSignalPlanA : IoVariable<InputType, Bit>
{
};

struct UnitSignalPlanB : IoVariable<InputType, Bit>
{
};

struct UnitStopTIme : IoVariable<InputType, Bit>
{
};

/*!
  System Address Bit 0
 */
struct UnitSystemAddressBit_0 : IoVariable<InputType, Bit>
{
};

/*!
  System Address Bit 1
 */
struct UnitSystemAddressBit_1 : IoVariable<InputType, Bit>
{
};

/*!
  System Address Bit 2
 */
struct UnitSystemAddressBit_2 : IoVariable<InputType, Bit>
{
};

/*!
  System Address Bit 3
 */
struct UnitSystemAddressBit_3 : IoVariable<InputType, Bit>
{
};

/*!
  System Address Bit 4
 */
struct UnitSystemAddressBit_4 : IoVariable<InputType, Bit>
{
};

struct UnitTBCHoldOnline : IoVariable<InputType, Bit>
{
};

struct UnitTBCOnline : IoVariable<InputType, Bit>
{
};

/*!
 * Test input A, for manufacture's use only.
 */
struct UnitTestInputA : IoVariable<InputType, Bit>
{
};

/*!
 * Test input B, for manufacture's use only.
 */
struct UnitTestInputB : IoVariable<InputType, Bit>
{
};

/*!
 * Test input C, for manufacture's use only.
 */
struct UnitTestInputC : IoVariable<InputType, Bit>
{
};

struct UnitTimingPlanA : IoVariable<InputType, Bit>
{
};

struct UnitTimingPlanB : IoVariable<InputType, Bit>
{
};

struct UnitTimingPlanC : IoVariable<InputType, Bit>
{
};

struct UnitTimingPlanD : IoVariable<InputType, Bit>
{
};

/*!
 * This input when true shall modify non-actuated operation only. With this
 * input active, non-actuated phases shall remain in the timed-out Walk state
 * (rest in Walk) in the absence of a serviceable conflicting call without
 * regard to the Hold input status. With this input non-active, non-actuated
 * phases shall not remain in the timed-out Walk state unless the Hold
 * input is active.
 */
struct UnitWalkRestModifier : IoVariable<InputType, Bit>
{
};

/*!
 * At minimum, each vehicle detector input shall be enabled by assignment to
 * any one phase, via program entry. Each shall be capable of Delay, Extension,
 * and Switching as follows:
 *
 *   Delay     - Actuation shall be capable of being delayed when the phase is
 *               not green (0-255, 1 sec increment)
 *   Extension - Actuation shall be capable of being extended from the point of
 *               by an adjustable program entered time (0-25.5, 0.1 sec inc)
 *               when the phase is green
 *   Switch    - Actuation shall be capable of being switched to another phase
 *               when the assigned phase is Yellow or Red and the program
 *               entered phase is Green.
 *
 * See NEMA-TS2 3.5.5.5
 * @tparam I
 */
template<Index I> /* */
requires ValidIndex<I, cu::detector::maxVehicleDetectors>
struct VehicleDetCall : IoVariable<InputType, Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, biu::max_det_bius>
struct VehicleDetReset : IoVariable<InputType, Byte, I>
{
};

} // end of namespace vc::io::input

} // end of namespace vtc::io

namespace mmu {

struct MmuVariableType
{
};

template<typename T>
concept ValidMmuVariable = std::is_same_v<typename T::type, MmuVariableType>;

template<typename ValueT, Index I = 0>
struct MmuVariable : Variable<ValueT, I>
{
  using type = MmuVariableType;

  MmuVariable() = default;
  MmuVariable(MmuVariable &) = delete;
  MmuVariable(MmuVariable &&) = delete;
  MmuVariable &operator=(MmuVariable &) = delete;
  MmuVariable &operator=(MmuVariable &&) = delete;
};

template<Index I> /* */
requires ValidIndex<I, cu::channel::maxChannels>
struct ChannelGreenWalkStatus : MmuVariable<Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::channel::maxChannels>
struct ChannelRedDoNotWalkStatus : MmuVariable<Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::channel::maxChannels>
struct ChannelYellowPedClearStatus : MmuVariable<Bit, I>
{
};

struct ControllerVoltMonitor : MmuVariable<Bit>
{
};

struct _24VoltMonitor_I : MmuVariable<Bit>
{
};

struct _24VoltMonitor_II : MmuVariable<Bit>
{
};

struct _24VoltMonitorInhibit : MmuVariable<Bit>
{
};

struct Reset : MmuVariable<Bit>
{
};

struct RedEnable : MmuVariable<Bit>
{
};

struct Conflict : MmuVariable<Bit>
{
};

struct RedFailure : MmuVariable<Bit>
{
};

struct DiagnosticFailure : MmuVariable<Bit>
{
};

struct MinimumClearanceFailure : MmuVariable<Bit>
{
};

struct Port1TimeoutFailure : MmuVariable<Bit>
{
};

struct FailedAndOutputRelayTransferred : MmuVariable<Bit>
{
};

struct FailedAndImmediateResponse : MmuVariable<Bit>
{
};

struct LocalFlashStatus : MmuVariable<Bit>
{
};

struct StartupFlashCall : MmuVariable<Bit>
{
};

struct FYAFlashRateFailure : MmuVariable<Bit>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::channel::maxChannels>
struct MinimumYellowChangeDisable : MmuVariable<Bit, I>
{
};

struct MinimumFlashTimeBit_0 : MmuVariable<Bit>
{
};

struct MinimumFlashTimeBit_1 : MmuVariable<Bit>
{
};

struct MinimumFlashTimeBit_2 : MmuVariable<Bit>
{
};

struct MinimumFlashTimeBit_3 : MmuVariable<Bit>
{
};

struct _24VoltLatch : MmuVariable<Bit>
{
};

struct CVMFaultMonitorLatch : MmuVariable<Bit>
{
};

/*!
 * The compatibility status of two MMU channels.
 * @tparam Ix - MMU channel.
 * @tparam Iy - MMU channel.
 * @remarks The two channel IDs are encoded as the index. index : left-hand-side channel as high-byte,
 * right-hand-size channel as low-byte
 */
template<Index Ix, Index Iy> /* */
requires ValidIndex<Ix, cu::channel::maxChannels> && ValidIndex<Iy, cu::channel::maxChannels> && (Ix < Iy)
struct ChannelCompatibilityStatus : MmuVariable<Bit, (Ix << 8) | Iy>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::channel::maxChannels>
struct ChannelGreenWalkDriver : MmuVariable<Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::channel::maxChannels>
struct ChannelRedDoNotWalkDriver : MmuVariable<Bit, I>
{
};

template<Index I> /* */
requires ValidIndex<I, cu::channel::maxChannels>
struct ChannelYellowPedClearDriver : MmuVariable<Bit, I>
{
};

struct LoadSwitchFlash : MmuVariable<Bit>
{
};

template<typename T> /* */
requires ValidMmuVariable<T>
T variable{};

/*!
 * The size of channel compatibility set. For example, for Channel 1 of MMU16,
 * its compatibility set includes 1-2, 1-3, 1-4, ..., 1-16, thus the size is 15.
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

/*!
 * For a given MMU channel, the IDs of other channels that is paired to the subject
 * chanel for compatibility definition. Note, std::make_integer_sequence<T, T N>
 * would create 0....N-1.
 *
 * For example, if ChannelID = 1, this will generate 2, ..., MaxChannel
 *                 ChannelID = 2, this will generate 3, ..., MaxChannel
*/
template<Index ChannelID, Index MaxChannel = 16> requires (ChannelID >= 1) &&(ChannelID<MaxChannel)
using ChannelCompatibilityPairedIndexes
    = offset_sequence_t<ChannelID, std::make_integer_sequence<Index, MaxChannel - ChannelID>>;

namespace impl {
/*!
 * Save MMU16 compatibility definition, carried by a bit set, to internal mmu variable.
 *
 * @tparam Ix  - Subject channel.
 * @tparam Iy  - The first channel to be compared to the subject channel.
 * @tparam Iys - The rest of the channels to be compared to the subject channel.
 * @param a_mmu16_comp - Bit set representing the MMU16 compatibility card, total 120 bits (i.e., 0x78).
 * @param a_seq - Compile time integer sequence, the first one being the subject channel.
 */
template<Index Ix, Index Iy, Index... Iys>
void SetMMU16ChannelCompatibility(const std::bitset<0x78> &a_mmu16_comp,
                                  std::integer_sequence<Index, Ix, Iy, Iys...> a_seq)
{
  // @formatter:off
  mmu::variable<ChannelCompatibilityStatus<Ix, Iy>>.value
      = static_cast<Bit>(a_mmu16_comp[ChannelSegmentStartPos<Ix>() + Iy - Ix - 1]);

  if constexpr (a_seq.size() > 2) {
    return SetMMU16ChannelCompatibility(a_mmu16_comp, std::integer_sequence<Index, Ix, Iys...>{});
  } else {
    return;
  }
  // @formatter:on
}

/*!
 * Load MMU16 compatibility value out of interval mmu variable into a bit set.
 *
 * @tparam Ix  - Subject channel.
 * @tparam Iy  - The first channel to be compared to the subject channel.
 * @tparam Iys - The rest of the channels to be compared to the subject channel.
 * @param a_mmu16_comp - Bit set representing the MMU16 compatibility card.
 * @param a_seq - Compile time integer sequence, the first one being the subject channel.
 */
template<Index Ix, Index Iy, Index... Iys>
void GetMMU16ChannelCompatibility(std::bitset<0x78> &a_mmu16_comp,
                                  std::integer_sequence<Index, Ix, Iy, Iys...> a_seq)
{
  // @formatter:off
  a_mmu16_comp[ChannelSegmentStartPos<Ix>() + Iy - Ix - 1]
      = (mmu::variable<ChannelCompatibilityStatus<Ix, Iy>>.value == Bit::On) ? 1 : 0;

  if constexpr (a_seq.size() > 2) {
    return GetMMU16ChannelCompatibility(a_mmu16_comp,
                                        std::integer_sequence<Index, Ix, Iys...>{});
  } else {
    return;
  }
  // @formatter:on
}

} // end of namespace mmu::impl

/*!
 * Save MMU16 channel compatibility to internal mmu variables.
 * @tparam Ix - The starting channel, default to 1.
 * @param [out] a_mmu16_comp - The compatibility bit set.
 */
template<Index Ix = 1>
void SetMMU16ChannelCompatibility(const std::bitset<0x78> &a_mmu16_comp)
{
  // @formatter:off
  if constexpr (Ix < 16) {
    using T = typename add_sequence_front<Ix, ChannelCompatibilityPairedIndexes<Ix>>::type;
    impl::SetMMU16ChannelCompatibility(a_mmu16_comp, T{});
    return SetMMU16ChannelCompatibility<Ix+1>(a_mmu16_comp);
  } else {
    return;
  }
  // @formatter:on
}

/*!
 * Load MMU16 channel compatibility from internal mmu variables.
 * @tparam Ix - The starting channel, default to 1.
 * @param a_mmu16_comp - The compatibility bit set.
 */
template<Index Ix = 1>
void GetMMU16ChannelCompatibility(std::bitset<0x78> &a_mmu16_comp)
{
  // @formatter:off
  if constexpr (Ix < 16) {
    using T = typename add_sequence_front<Ix, ChannelCompatibilityPairedIndexes<Ix>>::type;
    impl::GetMMU16ChannelCompatibility(a_mmu16_comp, T{});
    return GetMMU16ChannelCompatibility<Ix+1>(a_mmu16_comp);
  } else {
    return;
  }
  // @formatter:on
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
      // @formatter:off
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
      // @formatter:on
  };
  // The bitset needs to be reversed, so the least significant bit
  // represents the starting compatibility bit, i.e., compat<1,2>
  reverse(mmu16_comp_def);
  SetMMU16ChannelCompatibility(mmu16_comp_def);
}

/*!
 * Set MMU16 channel compatibility using a HEX string. The least significant (i.e., rightmost) bit
 * of the HEX string represents the compatibility of channel 1 and 2.
 *
 * Note: std::bitset [] operator, the position is counted from least significant bit. For example,
 * std::bitset<8> b1{0b00101010}; b1[0] is 0, b1[1] is 1, b1[2] = 0, b1[3] = 1
 * @param a_hexstr
 */
void SetMMU16ChannelCompatibility(const std::string &a_hexstr)
{
  std::string bitstr{};
  for (auto &c : a_hexstr) {
    switch (std::toupper(c)) {
      case '0':/* */
        bitstr.append("0000");
        break;
      case '1':/* */
        bitstr.append("0001");
        break;
      case '2':/* */
        bitstr.append("0010");
        break;
      case '3':/* */
        bitstr.append("0011");
        break;
      case '4':/* */
        bitstr.append("0100");
        break;
      case '5':/* */
        bitstr.append("0101");
        break;
      case '6':/* */
        bitstr.append("0110");
        break;
      case '7':/* */
        bitstr.append("0111");
        break;
      case '8':/* */
        bitstr.append("1000");
        break;
      case '9':/* */
        bitstr.append("1001");
        break;
      case 'A':/* */
        bitstr.append("1010");
        break;
      case 'B':/* */
        bitstr.append("1011");
        break;
      case 'C':/* */
        bitstr.append("1100");
        break;
      case 'D':/* */
        bitstr.append("1101");
        break;
      case 'E':/* */
        bitstr.append("1110");
        break;
      case 'F':/* */
        bitstr.append("1111");
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

template<typename ValueT, Index I = 0>
struct BroadcastVariable : Variable<ValueT, I>
{
  using type = BroadcastVariableType;

  BroadcastVariable() = default;
  BroadcastVariable(BroadcastVariable &) = delete;
  BroadcastVariable(BroadcastVariable &&) = delete;
  BroadcastVariable &operator=(BroadcastVariable &) = delete;
  BroadcastVariable &operator=(BroadcastVariable &&) = delete;
};

struct CuReportedMonth : BroadcastVariable<Byte>
{
};

struct CuReportedDay : BroadcastVariable<Byte>
{
};

struct CuReportedYear : BroadcastVariable<Byte>
{
};

struct CuReportedHour : BroadcastVariable<Byte>
{
};

struct CuReportedMinutes : BroadcastVariable<Byte>
{
};

struct CuReportedSeconds : BroadcastVariable<Byte>
{
};

struct CuReportedTenthsOfSeconds : BroadcastVariable<Byte>
{
};

template<Index I> requires ValidIndex<I, biu::max_tf_bius>
struct CuReportedTfBiuPresence : BroadcastVariable<Bit, I>
{
};

template<Index I> requires ValidIndex<I, biu::max_det_bius>
struct CuReportedDrBiuPresence : BroadcastVariable<Bit, I>
{
};

template<typename T> /* */
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

template<typename T, size_t BitPos> requires std::is_same_v<ValueType<T>, Bit>
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

template<typename T, size_t BytePos> requires std::is_same_v<ValueType<T>, Byte>
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

template<
    Byte Address,
    Byte FrameID,
    size_t FrameByteSize,
    typename T,
    typename ...Ts
> requires ValidFrame<FrameByteSize, T, Ts...>
class Frame
{
public:
  using type = T;

  Frame() noexcept = default;
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
      //@formatter:off
      Assign<I+1>(a_data_in);
      //@formatter:on
    }
  }

  template<size_t I = 0>
  inline void Generate(std::span<Byte> a_data_out)
  {
    if constexpr (I < sizeof...(Ts)) {
      std::get<I>(m_frame_elements) >> a_data_out;
      //@formatter:off
      Generate<I+1>(a_data_out);
      //@formatter:on
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
    FrameBit<io::output::UnitAutomaticFlashStatus, 0x1F>,
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
      return Dispatch<I+1>(a_data_in);
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
  BRG = 0x0800,     /* TxClk generated by Baud Rate Generator internally. Need to specify clock in param. */
  DPLL = 0x0400,    /* TxClk recovered by Digital Phase-locked Loop from data signal. Need to specify clock in param. */
  RxClkPin = 0x0008,/* TxClk supplied by external device on RxC input pin. */
  TxClkPin = 0x0000 /* TxClk supplied by external device on TxC input pin. */
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

  /* 1 byte  */
  uint8_t b1;
  /* 1 byte  */
  uint8_t b2;
  /* 1 byte: Padding */
  /* 1 byte: Padding */

  /* 4 bytes  */
  uint32_t dw1;

  /* 1 byte  */
  uint8_t b3;
  /* 1 byte  */
  uint8_t b4;
  /* 1 byte  */
  uint8_t b5;
  /* 1 byte: Padding */
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
    SerialApiModule() noexcept: serialapi_{nullptr}
    {
#ifdef _WIN32
      lib_ = ::LoadLibraryA("vtcdev");
      ec_ = ::GetLastError();
#elif  __linux__
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
#elif  __linux__
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

    /*!
     * Function signature for simple command function that takes a device
     * handle and returns an integer error code.
     */
    using SimpleCommandFunc = uint32_t __stdcall(DeviceHandle);

    /*!
     * Function signature for I/O read and write to a buffer.
     * The first arg is the device handle, the second is the buffer
     * the third is the buffer size.
     */
    using IoFunc = uint32_t __stdcall(DeviceHandle, uint8_t *, int32_t);

    /*!
     * Function signature for setting integer value by parameter ID.
     */
    using SetValueByIdFunc = uint32_t __stdcall(DeviceHandle, uint32_t, int32_t);

    /*!
     * Function signature for enable or disable option identified by the
     * second parameter.
     */
    using SetValueFunc = uint32_t __stdcall(DeviceHandle, int32_t);

    /*!
     * Open device. The first is device name, the second is
     * device handle returned.
     */
    using OpenFunc = uint32_t __stdcall(char *, void **);

    /*!
     * Set parameter. The first parameter is the device handle,
     * the second is the pointer to param struct.
     */
    using SetParamsFunc = uint32_t __stdcall(DeviceHandle, SerialDeviceParams *);

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
            {
                {
                    0x4D, 0x67, 0x73, 0x6C, 0x43, 0x61, 0x6E, 0x63,
                    0x65, 0x6C, 0x52, 0x65, 0x63, 0x65, 0x69, 0x76,
                    0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                }
            } // 0x00: 0x02
            ,
            {
                {
                    0x4D, 0x67, 0x73, 0x6C, 0x43, 0x61, 0x6E, 0x63,
                    0x65, 0x6C, 0x54, 0x72, 0x61, 0x6E, 0x73, 0x6D,
                    0x69, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                }
            } // 0x01: 0x03
            ,
            {
                {
                    0x4D, 0x67, 0x73, 0x6C, 0x43, 0x6C, 0x6F, 0x73,
                    0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                }
            } // 0x02: 0x05
            ,
            {
                {
                    0x4D, 0x67, 0x73, 0x6C, 0x45, 0x6E, 0x61, 0x62,
                    0x6C, 0x65, 0x52, 0x65, 0x63, 0x65, 0x69, 0x76,
                    0x65, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                }
            } // 0x03: 0x06
            ,
            {
                {
                    0x4D, 0x67, 0x73, 0x6C, 0x53, 0x65, 0x74, 0x50,
                    0x61, 0x72, 0x61, 0x6D, 0x73, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                }
            } // 0x04: 0x11
            ,
            {
                {
                    0x4D, 0x67, 0x73, 0x6C, 0x53, 0x65, 0x74, 0x49,
                    0x64, 0x6C, 0x65, 0x4D, 0x6F, 0x64, 0x65, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                }
            } // 0x05: 0x18
            ,
            {
                {
                    0x4D, 0x67, 0x73, 0x6C, 0x53, 0x65, 0x74, 0x4F,
                    0x70, 0x74, 0x69, 0x6F, 0x6E, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                }
            } // 0x06: 0x30
            ,
            {
                {
                    0x4D, 0x67, 0x73, 0x6C, 0x4F, 0x70, 0x65, 0x6E,
                    0x42, 0x79, 0x4E, 0x61, 0x6D, 0x65, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                }
            } // 0x07: 0x38
            ,
            {
                {
                    0x4D, 0x67, 0x73, 0x6C, 0x52, 0x65, 0x61, 0x64,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                }
            } // 0x08: 0x3B
            ,
            {
                {
                    0x4D, 0x67, 0x73, 0x6C, 0x57, 0x72, 0x69, 0x74,
                    0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                }
            } // 0x09: 0x3C
        }
    };

    std::array<FuncPtr, keys_.size()> serialapi_{nullptr};

#ifdef _WIN32
    HMODULE lib_{nullptr};
#elif
    __linux__
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
      vtc::logger()->error("Serial device \"{}\" failed to set params, code {}",
                           a_dev_name,
                           apimodule_.ec());
      return;
    }

    if (apimodule_.set_option(dev_, DeviceOptionTag::RxPoll, 0)) {
      vtc::logger()->error("Serial device \"{}\" failed to set option RxPoll = 0, code {}",
                           a_dev_name,
                           apimodule_.ec());
      return;
    }

    if (apimodule_.set_option(dev_, DeviceOptionTag::TxPoll, 0)) {
      vtc::logger()->error("Serial device \"{}\" failed to set option TxPoll = 0, code {}",
                           a_dev_name,
                           apimodule_.ec());
      return;
    }

    if (apimodule_.set_option(dev_, DeviceOptionTag::RxErrorMask, 1)) {
      vtc::logger()->error("Serial device \"{}\" failed to set option RxErrorMask = 1, code {}",
                           a_dev_name,
                           apimodule_.ec());
      return;
    }

    if (apimodule_.set_idle_mode(dev_, HdlcIdleMode::Ones)) {
      vtc::logger()->error("Serial device \"{}\" failed to set idle mode to ones, code {}",
                           a_dev_name,
                           apimodule_.ec());
      return;
    }

    if (apimodule_.enable_read(dev_)) {
      vtc::logger()->error("Serial device \"{}\" failed to enable read, code {}",
                           a_dev_name,
                           apimodule_.ec());
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
   * @return True for ready, False otherwise.
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

template<LoadswitchChannelID I>
auto make_loadswitch_driver()
{
  return std::make_tuple(
      std::ref(io::variable<ChannelGreenWalkDriver<I>>),      /**/
      std::ref(io::variable<ChannelYellowPedClearDriver<I>>), /**/
      std::ref(io::variable<ChannelRedDoNotWalkDriver<I>>));  /**/
}

template<LoadswitchChannelID I> requires (I <= num_loadswitches) &&(I
>= 1)
class LoadswitchChannel
{
public:
  [[nodiscard]] auto constexpr state() const noexcept
  {
    auto &[g, y, r] = driver_;
    return match(std::make_tuple(std::ref(g.value), std::ref(y.value), std::ref(r.value)))(
        pattern | ds(Bit::On, Bit::Off, Bit::Off) = expr(LoadswitchChannelState::Green),
        pattern | ds(Bit::Off, Bit::On, Bit::Off) = expr(LoadswitchChannelState::Yellow),
        pattern | ds(Bit::Off, Bit::Off, Bit::On) = expr(LoadswitchChannelState::Red),
        pattern | ds(_, _, _) /* Blank, or error? */ = expr(LoadswitchChannelState::Blank)
    );
  }

  auto constexpr operator()() const noexcept
  {
    return I;
  }

private:
  LoadswitchDriver<I> driver_{make_loadswitch_driver<I>()};
};

template<LoadswitchChannelID I> requires (I >= 1) &&(I
<= num_loadswitches)
using LoadswitchWiring = std::tuple<LoadswitchChannel<I>, SignalHead>;

struct LoadswitchWiringFactory
{
  template<LoadswitchChannelID I>
  static auto make()
  {
    return LoadswitchWiring < I > {};
  }
};

using LoadswitchChannelIndexes =
    offset_sequence_t<
        0,
        std::make_integer_sequence<
            LoadswitchChannelID,
            num_loadswitches
        >
    >;

template<DetectorChannelID I> requires (I <= num_detector_channels) &&(I
>= 1)
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

template<DetectorChannelID I> requires (I <= num_detector_channels) &&(I
>= 1)
using DetectorWiring = std::tuple<DetectorChannel<I>, SensorIDs>;

struct DetectorWiringFactory
{
  template<DetectorChannelID I>
  static auto make()
  {
    return DetectorWiring < I > {};
  }
};

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
using DetectorWirings = decltype(make_wirings(DetectorWiringFactory{},
                                              DetectorChannelID{1},
                                              DetectorChannelIndexes{}));

/*!
 * A trick of aliasing the type of loadswitch wirings. The args instances to make_wirings()
 * are just supplied for the purpose of obtaining their respective types.
 */
using LoadswitchWirings = decltype(make_wirings(LoadswitchWiringFactory{},
                                                LoadswitchChannelID{1},
                                                LoadswitchChannelIndexes{}));

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

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedTypeAliasInspection"

using ProcessLoadswitchWiringFunc = std::function<void(const LoadswitchChannelID,
                                                       const LoadswitchChannelState,
                                                       const Approach,
                                                       const Turn)>;

#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedTypeAliasInspection"

using ProcessDetectorWiringFunc = std::function<bool(const DetectorChannelID,
                                                     const SensorID)>;

#pragma clang diagnostic pop

using VerifyLoadswitchWiringFunc = std::function<bool(const LoadswitchChannelID,
                                                      const Approach,
                                                      const Turn)>;

using VerifyDetectorWiringFunc = std::function<bool(const DetectorChannelID,
                                                    const SensorID)>;

using VerifySimulationStepFunc = std::function<bool(const double)>;

using VerifyFuncGroup = std::tuple<VerifySimulationStepFunc, VerifyLoadswitchWiringFunc, VerifyDetectorWiringFunc>;

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
   * Derived class implemented for a specific simulator, e.g., TransModeler, should
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

  bool load_config(const fs::path &a_path, const VerifyFuncGroup &a_verify_funcs = {}) noexcept
  {
    auto config = pugi::xml_document{};
    auto parse_result = config.load_file(a_path.c_str());

    if (!parse_result) {
      vtc::logger()->error("Failed to load config {}: {}", a_path.string(), parse_result.description());
      return false;
    }

    load_mmu16_channel_compatibility(config);

    auto doc_elem = config.document_element();
    auto dev_name = std::string{{0x4D, 0x47, 0x48, 0x44, 0x4C, 0x43}} + doc_elem.attribute("device").value();
    device_ = std::make_unique<SerialDevice>(dev_name.c_str());

    log_sdlc_frames_ = std::string{doc_elem.attribute("log_sdlc_frames").value()} == std::string{"true"};

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
      if (!result)
        return;

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
      if (!result)
        return;

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

#pragma clang diagnostic pop
#pragma warning(default:4068)
