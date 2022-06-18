/*!
  MPL 1.1/GPL 2.0/LGPL 2.1 tri-license
  Copyright (C) 2022  Wuping Xin
*/

/*!
  Apache-2.0 License
  Copyright (c) 2021 Bowen Fu
*/

#ifndef WXLIB_MATCHIT_HPP
#define WXLIB_MATCHIT_HPP

#include <algorithm>
#include <any>
#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <variant>

namespace matchit {

namespace impl {

template<typename T, bool ByRef>
struct ValueType
{
  using type = T;
};

template<typename T>
struct ValueType<T, true>
{
  using type = T &&;
};

template<typename T, typename... Ts>
constexpr auto match_patterns(T &&a_value, Ts const &...a_patterns);

template<typename T, bool ByRef>
class MatchHelper
{
  using ValueT = typename ValueType<T, ByRef>::type;
public:
  template<typename Tv>
  constexpr explicit MatchHelper(Tv &&value) : m_value{std::forward<Tv>(value)}
  {
  }

  template<typename... Ts>
  constexpr auto operator()(Ts const &...a_patterns)
  {
    return match_patterns(std::forward<ValueT &&>(m_value), a_patterns...);
  }

private:
  ValueT m_value;
};

template<typename T>
constexpr auto match(T &&value)
{
  return MatchHelper<T, true>{std::forward<T>(value)};
}

template<typename T, typename... Ts>
constexpr auto match(T &&first, Ts &&...values)
{
  auto result = std::forward_as_tuple(std::forward<T>(first), std::forward<Ts>(values)...);
  return MatchHelper<decltype(result), false>{std::forward<decltype(result)>(result)};
}

template<typename T>
struct Nullary : public T
{
  using T::operator();
};

template<typename T>
constexpr auto nullary(T const &t)
{
  return Nullary<T>{t};
}

template<typename T>
class Id;

template<typename T>
constexpr auto expr(Id<T> &id)
{
  return nullary([&] { return *id; });
}

template<typename T>
constexpr auto expr(T const &v)
{
  return nullary([&] { return v; });
}

// for constant
template<typename T>
struct EvalTraits
{
  template<typename... ArgTs>
  constexpr static decltype(auto) eval_impl(T const &v, ArgTs const &...)
  {
    return v;
  }
};

template<typename T>
struct EvalTraits<Nullary<T>>
{
  constexpr static decltype(auto) eval_impl(Nullary<T> const &e)
  { return e(); }
};

// Only allowed in nullary
template<typename T>
struct EvalTraits<Id<T>>
{
  constexpr static decltype(auto) eval_impl(Id<T> const &id)
  {
    return *const_cast<Id<T> &>(id);
  }
};

template<typename P>
struct Meet;

// Unary is an alias of Meet.
template<typename T>
using Unary = Meet<T>;

template<typename T>
struct EvalTraits<Unary<T>>
{
  template<typename ArgT>
  constexpr static decltype(auto) eval_impl(Unary<T> const &e, ArgT const &arg)
  {
    return e(arg);
  }
};

struct Wildcard;

template<>
struct EvalTraits<Wildcard>
{
  template<typename ArgT>
  constexpr static decltype(auto) eval_impl(Wildcard const &, ArgT const &arg)
  {
    return arg;
  }
};

template<typename T, typename... ArgTs>
constexpr decltype(auto) evaluate_(T const &t, ArgTs const &...args)
{
  return EvalTraits<T>::eval_impl(t, args...);
}

template<typename T>
struct IsNullaryOrId : public std::false_type
{
};

template<typename T>
struct IsNullaryOrId<Id<T>> : public std::true_type
{
};

template<typename T>
struct IsNullaryOrId<Nullary<T>> : public std::true_type
{
};

template<typename T>
constexpr auto is_nullary_or_id_v = IsNullaryOrId<std::decay_t<T>>::value;

#define UN_OP_FOR_NULLARY(op)                             \
    template <typename T>                                 \
    requires is_nullary_or_id_v<T>                        \
    constexpr auto operator op(T const &t)                \
    {                                                     \
        return nullary([&] { return op evaluate_(t); });  \
    }

#define BIN_OP_FOR_NULLARY(op)                                         \
    template <typename T, typename U>                                  \
    requires is_nullary_or_id_v<T> || is_nullary_or_id_v<U>            \
    constexpr auto operator op(T const &t, U const &u)                 \
    {                                                                  \
        return nullary([&] { return evaluate_(t) op evaluate_(u); });  \
    }

// ADL will find these operators.
UN_OP_FOR_NULLARY(!)
UN_OP_FOR_NULLARY(-)

#undef UN_OP_FOR_NULLARY

BIN_OP_FOR_NULLARY(+)
BIN_OP_FOR_NULLARY(-)
BIN_OP_FOR_NULLARY(*)
BIN_OP_FOR_NULLARY(/)
BIN_OP_FOR_NULLARY(%)
BIN_OP_FOR_NULLARY(<)
BIN_OP_FOR_NULLARY(<=)
BIN_OP_FOR_NULLARY(==)
BIN_OP_FOR_NULLARY(!=)
BIN_OP_FOR_NULLARY(>=)
BIN_OP_FOR_NULLARY(>)
BIN_OP_FOR_NULLARY(||)
BIN_OP_FOR_NULLARY(&&)
BIN_OP_FOR_NULLARY(^)

#undef BIN_OP_FOR_NULLARY

// Unary
template<typename T>
struct IsUnaryOrWildcard : public std::false_type
{
};

template<>
struct IsUnaryOrWildcard<Wildcard> : public std::true_type
{
};

template<typename T>
struct IsUnaryOrWildcard<Unary<T>> : public std::true_type
{
};

template<typename T>
constexpr auto is_unary_or_wildcard_v = IsUnaryOrWildcard<std::decay_t<T>>::value;

// unary is an alias of meet.
template<typename T>
constexpr auto unary(T &&t)
{
  return meet(std::forward<T>(t));
}

#define UN_OP_FOR_UNARY(op)\
    template <typename T>\
    requires is_unary_or_wildcard_v<T>\
    constexpr auto operator op(T const &t)\
    {\
        return unary([&](auto &&arg) constexpr {\
           return op evaluate_(t, arg);\
        });\
    }

#define BIN_OP_FOR_UNARY(op)\
    template <typename T, typename U>\
    requires is_unary_or_wildcard_v<T> || is_unary_or_wildcard_v<U>\
    constexpr auto operator op(T const &t, U const &u)\
    {\
        return unary([&](auto &&arg) constexpr {\
           return evaluate_(t, arg) op evaluate_(u, arg);\
        });\
    }

UN_OP_FOR_UNARY(!)
UN_OP_FOR_UNARY(-)

#undef UN_OP_FOR_UNARY

BIN_OP_FOR_UNARY(+)
BIN_OP_FOR_UNARY(-)
BIN_OP_FOR_UNARY(*)
BIN_OP_FOR_UNARY(/)
BIN_OP_FOR_UNARY(%)
BIN_OP_FOR_UNARY(<)
BIN_OP_FOR_UNARY(<=)
BIN_OP_FOR_UNARY(==)
BIN_OP_FOR_UNARY(!=)
BIN_OP_FOR_UNARY(>=)
BIN_OP_FOR_UNARY(>)
BIN_OP_FOR_UNARY(||)
BIN_OP_FOR_UNARY(&&)
BIN_OP_FOR_UNARY(^)

#undef BIN_OP_FOR_UNARY

template<typename BeginIterT, typename EndIterT = BeginIterT>
struct Subrange
{
public:
  constexpr Subrange(BeginIterT const begin, EndIterT const end) : m_begin{begin}, m_end{end}
  {
  }

  constexpr Subrange(Subrange const &other) : m_begin{other.begin()}, m_end{other.end()}
  {
  }

  Subrange &operator=(Subrange const &other)
  {
    m_begin = other.begin();
    m_end = other.end();
    return *this;
  }

  size_t size() const
  {
    return static_cast<size_t>(std::distance(m_begin, m_end));
  }

  auto begin() const
  {
    return m_begin;
  }

  auto end() const
  {
    return m_end;
  }

private:
  BeginIterT m_begin;
  EndIterT m_end;
};

template<typename BeginIterT, typename EndIterT>
constexpr auto make_subrange(BeginIterT begin, EndIterT end)
{
  return Subrange<BeginIterT, EndIterT>{begin, end};
}

template<typename RangeT>
struct IterUnderlyingT
{
  using begin_iter_t = decltype(std::begin(std::declval<RangeT &>()));
  using end_iter_t = decltype(std::end(std::declval<RangeT &>()));
};

// force array iterators fallback to pointers.
template<typename ElemT, size_t N>
struct IterUnderlyingT<std::array<ElemT, N>>
{
  using begin_iter_t = decltype(&*std::begin(std::declval<std::array<ElemT, N> &>()));
  using end_iter_t = begin_iter_t;
};

// force array iterators fallback to pointers.
template<typename ElemT, size_t N>
struct IterUnderlyingT<std::array<ElemT, N> const>
{
  using begin_iter_t = decltype(&*std::begin(std::declval<std::array<ElemT, N> const &>()));
  using end_iter_t = begin_iter_t;
};

template<typename RangeT>
using SubrangeT = Subrange<typename IterUnderlyingT<RangeT>::begin_iter_t, typename IterUnderlyingT<RangeT>::end_iter_t>;

template<typename BeginIterT, typename EndIterT>
bool operator==(Subrange<BeginIterT, EndIterT> const &lhs, Subrange<BeginIterT, EndIterT> const &rhs)
{
  using std::operator==;
  return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template<typename K1, typename V1, typename K2, typename V2>
auto operator==(std::pair<K1, V1> const &t, std::pair<K2, V2> const &u)
{
  return t.first == u.first && t.second == u.second;
}

template<typename K1, typename V1, typename K2, typename V2>
requires (std::is_same_v<V1, const char *> && std::is_same_v<V2, const char *>)
auto operator==(std::pair<K1, V1> const &t, std::pair<K2, V2> const &u)
{
  return t.first == u.first && std::strcmp(t.second, u.second) == 0;
}

template<typename K, typename V>
requires(std::is_same_v<const char *, V>)
constexpr bool operator==(const std::pair<K, V> &_Left, const std::pair<K, V> &_Right)
{
  return _Left.first == _Right.first && std::strcmp(_Left.second, _Right.second) == 0;
}

template<typename T, typename... Ts>
struct WithinTypes
{
  constexpr static auto value = (std::is_same_v<T, Ts> || ...);
};

template<typename T, typename TupleT>
struct PrependUnique;

template<typename T, typename... Ts>
struct PrependUnique<T, std::tuple<Ts...>>
{
  using type = std::conditional_t<
      !WithinTypes<T, Ts...>::value,
      std::tuple<T, Ts...>,
      std::tuple<Ts...>
  >;
};

template<typename T, typename TupleT>
using PrependUniqueT = typename PrependUnique<T, TupleT>::type;

template<typename T>
struct Unique;

template<typename T>
using UniqueT = typename Unique<T>::type;

template<>
struct Unique<std::tuple<>>
{
  using type = std::tuple<>;
};

template<typename T, typename... Ts>
struct Unique<std::tuple<T, Ts...>>
{
  using type = PrependUniqueT<T, UniqueT<std::tuple<Ts...>>>;
};

static_assert(std::is_same_v<std::tuple<int32_t>, UniqueT<std::tuple<int32_t, int32_t>>>);
static_assert(std::is_same_v<std::tuple<std::tuple<>, int32_t>, UniqueT<std::tuple<int32_t, std::tuple<>, int32_t>>>);

namespace detail {

template<std::size_t StartI, typename T, std::size_t... I>
constexpr decltype(auto) subtuple_impl(T &&a_tuple, std::index_sequence<I...>)
{
  return std::forward_as_tuple(std::get<StartI + I>(std::forward<T>(a_tuple))...);
}

} // namespace detail

// [start, end)
template<std::size_t StartI, std::size_t EndI, typename T>
constexpr decltype(auto) subtuple(T &&t)
{
  constexpr auto tuple_size = std::tuple_size_v<std::remove_reference_t<T>>;
  static_assert(StartI <= EndI);
  static_assert(EndI <= tuple_size);
  return detail::subtuple_impl<StartI>(std::forward<T>(t), std::make_index_sequence<EndI - StartI>{
  });
}

template<std::size_t StartI, typename T>
constexpr decltype(auto) drop(T &&t)
{
  constexpr auto tuple_size = std::tuple_size_v<std::remove_reference_t<T>>;
  static_assert(StartI <= tuple_size);
  return subtuple<StartI, tuple_size>(std::forward<T>(t));
}

template<std::size_t N, typename T>
constexpr decltype(auto) take(T &&t)
{
  constexpr auto tuple_size = std::tuple_size_v<std::remove_reference_t<T>>;
  static_assert(N <= tuple_size);
  return subtuple<0, N>(std::forward<T>(t));
}

template<class F, typename T>
constexpr decltype(auto) apply_(F &&f, T &&t)
{
  return std::apply(std::forward<F>(f), drop<0>(std::forward<T>(t)));
}

// as constexpr
template<class F, typename... ArgTs>
constexpr std::invoke_result_t<F, ArgTs...>
invoke_(F &&f, ArgTs &&...args) noexcept(std::is_nothrow_invocable_v<F, ArgTs...>)
{
  return std::apply(std::forward<F>(f), std::forward_as_tuple(std::forward<ArgTs>(args)...));
}

template<typename T>
struct DecayArray
{
  using type = typename std::conditional_t<
      std::is_array_v<std::remove_reference_t<T>>,
      typename std::remove_extent_t<std::remove_reference_t<T>> *,
      T>;
};

template<typename T>
using DecayArrayT = typename DecayArray<T>::type;

static_assert(std::is_same_v<DecayArrayT<int32_t[]>, int32_t *>);
static_assert(std::is_same_v<DecayArrayT<int32_t const[]>, int32_t const *>);
static_assert(std::is_same_v<DecayArrayT<int32_t const &>, int32_t const &>);

template<typename T>
struct AddConstToPointer
{
  using type = std::conditional_t<
      !std::is_pointer_v<T>,
      T,
      std::add_pointer_t<std::add_const_t<std::remove_pointer_t<T>>>>;
};

template<typename T>
using AddConstToPointerT = typename AddConstToPointer<T>::type;

static_assert(std::is_same_v<AddConstToPointerT<void *>, void const *>);
static_assert(std::is_same_v<AddConstToPointerT<int32_t>, int32_t>);

template<typename T>
using InternalPatternT = std::remove_reference_t<AddConstToPointerT<DecayArrayT<T>>>;

template<typename T>
struct PatternTraits;

template<typename... T>
struct PatternPairsReturnType
{
  using ReturnT = std::common_type_t<typename T::ReturnT...>;
};

enum class IdProcess : int32_t
{
  kCancel,
  kConfirm
};

template<typename T>
constexpr void process_id(T const &a_pattern, int32_t depth, IdProcess id_process)
{
  PatternTraits<T>::process_id_impl(a_pattern, depth, id_process);
}

template<typename T>
struct Variant;

template<typename T, typename... Ts>
struct Variant<std::tuple<T, Ts...>>
{
  using type = std::variant<std::monostate, T, Ts...>;
};

template<typename... Ts>
class Context
{
public:
  using ElemT = typename Variant<UniqueT<std::tuple<Ts...>>>::type;

  template<typename T>
  constexpr void emplace_back(T &&t)
  {
    m_memholder[m_size] = std::forward<T>(t);
    ++m_size;
  }

  constexpr auto back() -> ElemT &
  {
    return m_memholder[m_size - 1];
  }

private:
  std::array<ElemT, sizeof...(Ts)> m_memholder;
  size_t m_size = 0;
};

template<>
class Context<>
{
};

template<typename T>
struct ContextTrait;

template<typename... Ts>
struct ContextTrait<std::tuple<Ts...>>
{
  using ContextT = Context<Ts...>;
};

template<typename Tv, typename Tp, typename Tc>
constexpr auto match_pattern(Tv &&value, Tp const &a_pattern, int32_t depth, Tc &context)
{
  auto const result = PatternTraits<Tp>::match_pattern_impl(std::forward<Tv>(value), a_pattern, depth, context);
  auto const process = result ? IdProcess::kConfirm : IdProcess::kCancel;
  process_id(a_pattern, depth, process);
  return result;
}

template<typename T, typename F>
class PatternPair
{
public:
  using ReturnT = std::invoke_result_t<F>;
  using PatternT = T;

  template<typename Func = F>
  requires std::is_function_v<F>
  constexpr PatternPair(T const &a_pattern, Func &func) : m_pattern{a_pattern}, m_handler{func}
  {
  }

  template<typename Func = F>
  requires (!std::is_function_v<F>)
  constexpr PatternPair(T const &a_pattern, Func const &func) : m_pattern{a_pattern}, m_handler{func}
  {
  }

  template<typename Tv, typename Tc>
  constexpr bool match_value(Tv &&value, Tc &context) const
  {
    return match_pattern(std::forward<Tv>(value), m_pattern, /*depth*/ 0, context);
  }

  constexpr auto execute() const
  {
    return m_handler();
  }

private:
  T const m_pattern;
  std::conditional_t<std::is_function_v<F>, F &, F> m_handler;
};

template<typename T, typename P>
class PostCheck;

template<typename P>
struct When
{
  P pred;
};

template<typename P>
constexpr auto when(P const &pred)
{
  return When<P>{pred};
}

template<typename T>
class PatternHelper
{
public:
  constexpr explicit PatternHelper(T const &a_pattern) : m_pattern{a_pattern}
  {
  }

  template<typename F>
  constexpr auto operator=(F const &func)
  {
    return PatternPair<T, F>{m_pattern, func};
  }

  template<typename P>
  constexpr auto operator|(When<P> const &w)
  {
    return PatternHelper<PostCheck<T, P>>(PostCheck(m_pattern, w.pred));
  }

private:
  T const m_pattern;
};

template<typename... Ts>
struct Ds;

template<typename... Ts>
constexpr auto ds(Ts const &...a_patterns) -> Ds<Ts...>;

template<typename T>
struct OooBinder;

class PatternPipable
{
public:
  template<typename T>
  constexpr auto operator|(T const &p) const
  {
    return PatternHelper<T>{p};
  }

  template<typename T>
  constexpr auto operator|(T const *p) const
  {
    return PatternHelper<T const *>{p};
  }

  template<typename T>
  constexpr auto operator|(OooBinder<T> const &p) const
  {
    return operator|(ds(p));
  }
};

constexpr PatternPipable pattern{};

template<typename T>
struct PatternTraits
{
  template<typename>
  using AppResultTuple = std::tuple<>;

  constexpr static auto num_id_v = 0;

  template<typename Tv, typename Tc>
  constexpr static auto match_pattern_impl(Tv &&value, T const &a_pattern, int32_t /* depth */, Tc & /*context*/)
  {
    return a_pattern == std::forward<Tv>(value);
  }

  constexpr static void process_id_impl(T const &, int32_t /*depth*/, IdProcess)
  {
  }
};

struct Wildcard
{
};

constexpr Wildcard _;

template<>
class PatternTraits<Wildcard>
{
  using PatternT = Wildcard;

public:
  template<typename>
  using AppResultTuple = std::tuple<>;

  constexpr static auto num_id_v = 0;

  template<typename Tv, typename Tc>
  constexpr static bool match_pattern_impl(Tv &&, PatternT const &, int32_t, Tc &)
  {
    return true;
  }

  constexpr static void process_id_impl(PatternT const &, int32_t /*depth*/, IdProcess)
  {
  }
};

template<typename... Ts>
class Or
{
public:
  constexpr explicit Or(Ts const &...a_patterns) : m_patterns{a_patterns...}
  {
  }

  constexpr auto const &patterns() const
  {
    return m_patterns;
  }

private:
  std::tuple<InternalPatternT<Ts>...> m_patterns;
};

template<typename... Ts>
constexpr auto or_(Ts const &...a_patterns)
{
  return Or<Ts...>{a_patterns...};
}

template<typename... Ts>
class PatternTraits<Or<Ts...>>
{
public:
  template<typename Tv>
  using AppResultTuple = decltype(std::tuple_cat(typename PatternTraits<Ts>::template AppResultTuple<Tv>{}...));

  constexpr static auto num_id_v = (PatternTraits<Ts>::num_id_v + ... + 0);

  template<typename Tv, typename Tc>
  constexpr static auto match_pattern_impl(Tv &&value, Or<Ts...> const &or_pat, int32_t depth, Tc &context)
  {
    constexpr auto pat_size = sizeof...(Ts);
    return std::apply(
        [&value, depth, &context](auto const &...a_patterns) {
          return (match_pattern(value,
                                a_patterns,
                                depth + 1,
                                context) || ...);
        },
        take<pat_size - 1>(or_pat.patterns()))
        || match_pattern(std::forward<Tv>(value), get<pat_size - 1>(or_pat.patterns()), depth + 1, context);
  }

  constexpr static void process_id_impl(Or<Ts...> const &or_pat, int32_t depth, IdProcess id_process)
  {
    return std::apply(
        [depth, id_process](Ts const &...a_patterns) { return (process_id(a_patterns, depth, id_process), ...); },
        or_pat.patterns());
  }
};

template<typename P>
struct Meet : public P
{
  using P::operator();
};

template<typename P>
constexpr auto meet(P const &pred)
{
  return Meet<P>{pred};
}

template<typename P>
struct PatternTraits<Meet<P>>
{
  template<typename>
  using AppResultTuple = std::tuple<>;

  constexpr static auto num_id_v = 0;

  template<typename Tv, typename Tc>
  constexpr static auto match_pattern_impl(Tv &&value, Meet<P> const &meet_pat, int32_t /* depth */, Tc &)
  {
    return meet_pat(std::forward<Tv>(value));
  }

  constexpr static void process_id_impl(Meet<P> const &, int32_t /*depth*/, IdProcess)
  {
  }
};

template<typename UnaryT, typename PatternT>
class App
{
public:
  constexpr App(UnaryT &&unary, PatternT const &a_pattern) : m_unary{std::forward<UnaryT>(unary)}, m_pattern{a_pattern}
  {
  }

  constexpr auto const &unary() const
  {
    return m_unary;
  }

  constexpr auto const &pattern() const
  {
    return m_pattern;
  }

private:
  UnaryT const m_unary;
  InternalPatternT<PatternT> const m_pattern;
};

template<typename UnaryT, typename PatternT>
constexpr auto app(UnaryT &&unary, PatternT const &a_pattern)
{
  return App<UnaryT, PatternT>{std::forward<UnaryT>(unary), a_pattern};
}

constexpr auto y = 1;
static_assert(std::holds_alternative<int32_t const *>(std::variant<std::monostate, const int32_t *>{&y}));

template<typename UnaryT, typename PatternT>
class PatternTraits<App<UnaryT, PatternT>>
{
public:
  template<typename Tv>
  using AppResult = std::invoke_result_t<UnaryT, Tv>;

  // We store value for scalar types in id, and they can not be moved. So to
  // support constexpr.
  template<typename Tv>
  using AppResultCurTuple = std::conditional_t<
      std::is_lvalue_reference_v<AppResult<Tv>> || std::is_scalar_v<AppResult<Tv>>,
      std::tuple<>,
      std::tuple<std::decay_t<AppResult<Tv>>>
  >;

  template<typename Tv>
  using AppResultTuple = decltype(std::tuple_cat(
      std::declval<AppResultCurTuple<Tv>>(),
      std::declval<typename PatternTraits<PatternT>::template AppResultTuple<AppResult<Tv>>>()));

  constexpr static auto num_id_v = PatternTraits<PatternT>::num_id_v;

  template<typename Tv, typename Tc>
  constexpr static auto match_pattern_impl(Tv &&value,
                                           App<UnaryT, PatternT> const &app_pat,
                                           int32_t depth,
                                           Tc &context)
  {
    if constexpr (std::is_same_v<AppResultCurTuple<Tv>, std::tuple<>>) {
      return match_pattern(std::forward<AppResult<Tv>>(invoke_(app_pat.unary(), value)),
                           app_pat.pattern(),
                           depth + 1,
                           context);
    } else {
      context.emplace_back(invoke_(app_pat.unary(), value));
      decltype(auto) result = get<std::decay_t<AppResult<Tv>>>(context.back());
      return match_pattern(std::forward<AppResult<Tv>>(result),
                           app_pat.pattern(),
                           depth + 1,
                           context);
    }
  }

  constexpr static void process_id_impl(App<UnaryT, PatternT> const &app_pat, int32_t depth, IdProcess id_process)
  {
    return process_id(app_pat.pattern(), depth, id_process);
  }
};

template<typename... Ts>
class And
{
public:
  constexpr explicit And(Ts const &...a_patterns) : m_patterns{a_patterns...}
  {
  }

  constexpr auto const &patterns() const
  {
    return m_patterns;
  }

private:
  std::tuple<InternalPatternT<Ts>...> m_patterns;
};

template<typename... Ts>
constexpr auto and_(Ts const &...a_patterns)
{
  return And<Ts...>{a_patterns...};
}

template<typename T>
struct NumIdInTuple;

template<typename... Ts>
struct NumIdInTuple<std::tuple<Ts...>>
{
  constexpr static auto num_id_v = (PatternTraits<std::decay_t<Ts>>::num_id_v + ... + 0);
};

template<typename... Ts>
class PatternTraits<And<Ts...>>
{
public:
  template<typename Tv>
  using AppResultTuple = decltype(std::tuple_cat(std::declval<typename PatternTraits<Ts>::template AppResultTuple<Tv>>()...));

  constexpr static auto num_id_v = (PatternTraits<Ts>::num_id_v + ... + 0);

  template<typename Tv, typename Tc>
  constexpr static auto match_pattern_impl(Tv &&value, And<Ts...> const &and_pat, int32_t depth, Tc &context)
  {
    constexpr auto pat_size = sizeof...(Ts);

    auto const except_last = std::apply(
        [&value, depth, &context](auto const &...a_patterns) {
          return (match_pattern(value, a_patterns, depth + 1, context) && ...);
        },
        take<pat_size - 1>(and_pat.patterns()));

    // No id in patterns except the last one.
    if constexpr (NumIdInTuple<std::decay_t<decltype(take<pat_size - 1>(and_pat.patterns()))>>::num_id_v == 0)
      return except_last && match_pattern(std::forward<Tv>(value),
                                          get<pat_size - 1>(and_pat.patterns()),
                                          depth + 1,
                                          context);
    else
      return except_last && match_pattern(value,
                                          get<pat_size - 1>(and_pat.patterns()),
                                          depth + 1,
                                          context);
  }

  constexpr static void process_id_impl(And<Ts...> const &and_pat, int32_t depth, IdProcess id_process)
  {
    return std::apply(
        [depth, id_process](Ts const &...a_patterns) {
          return (process_id(a_patterns, depth, id_process), ...);
        },
        and_pat.patterns());
  }
};

template<typename T>
class Not
{
public:
  explicit Not(T const &a_pattern) : m_pattern{a_pattern}
  {
  }

  auto const &pattern() const
  {
    return m_pattern;
  }

private:
  InternalPatternT<T> m_pattern;
};

template<typename T>
constexpr auto not_(T const &a_pattern)
{
  return Not<T>{a_pattern};
}

template<typename T>
struct PatternTraits<Not<T>>
{
  template<typename Tv>
  using AppResultTuple = typename PatternTraits<T>::template AppResultTuple<Tv>;

  constexpr static auto num_id_v = PatternTraits<T>::num_id_v;

  template<typename Tv, typename Tc>
  constexpr static auto match_pattern_impl(Tv &&value, Not<T> const &not_pat, int32_t depth, Tc &context)
  {
    return !match_pattern(std::forward<Tv>(value), not_pat.pattern(), depth + 1, context);
  }

  constexpr static void process_id_impl(Not<T> const &not_pat, int32_t depth, IdProcess id_process)
  {
    process_id(not_pat.pattern(), depth, id_process);
  }
};

template<typename PtrT, typename Tv, typename = std::void_t<>>
struct StorePointer : std::false_type
{
};

template<typename T>
using ValueVariant = std::conditional_t<
    std::is_abstract_v<T>,
    std::variant<std::monostate, T const *>,
    std::variant<std::monostate, T, T const *>
>;

template<typename T, typename Tv> //
requires requires { std::declval<ValueVariant<T> &>() = &std::declval<Tv>(); }
struct StorePointer<T, Tv> : std::conjunction<std::is_lvalue_reference<Tv>, std::negation<std::is_scalar<Tv>>>
{
};

static_assert(StorePointer<char, char &>::value);
static_assert(StorePointer<const char, char &>::value);
static_assert(StorePointer<const char, const char &>::value);
static_assert(StorePointer<std::tuple<int32_t &, int32_t &> const, std::tuple<int32_t &, int32_t &> const &>::value);

template<typename... Ts>
struct Overload : public Ts ...
{
  using Ts::operator()...;
};

template<typename... Ts>
constexpr auto overload(Ts &&...ts)
{
  return Overload<Ts...>{ts...};
}

template<typename T>
struct OooBinder;

struct Ooo;

template<typename T>
struct IdTraits
{
  constexpr static auto
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
  __attribute__((no_sanitize_address))
#endif
#endif
  equal(T const &lhs, T const &rhs)
  {
    return lhs == rhs;
  }
};

template<typename T>
class Id
{
private:
  class Block
  {
  public:
    ValueVariant<T> m_variant;
    int32_t m_depth;

    constexpr auto &variant()
    { return m_variant; }
    constexpr auto has_value() const
    {
      return std::visit(overload([](T const &) { return true; },
                                 [](T const *) { return true; },
                                 [](std::monostate const &) { return false; }),
                        m_variant);
    }

    constexpr decltype(auto) value() const
    {
      return std::visit(
          overload([](T const &v) -> T const & { return v; },
                   [](T const *p) -> T const & { return *p; },
                   [](std::monostate const &) -> T const & { throw std::logic_error("invalid state!"); }),
          m_variant);
    }

    constexpr decltype(auto) mutable_value()
    {
      return std::visit(
          overload([](T &v) -> T & { return v; },
                   [](T const *) -> T & { throw std::logic_error("Cannot get mutable_value for pointer type!"); },
                   [](std::monostate &) -> T & { throw std::logic_error("Invalid state!"); }),
          m_variant);
    }

    constexpr void reset(int32_t depth)
    {
      if (m_depth - depth >= 0) {
        m_variant = {};
        m_depth = depth;
      }
    }

    constexpr void confirm(int32_t depth)
    {
      if (m_depth > depth || m_depth == 0) {
        assert(depth == m_depth - 1 || depth == m_depth || m_depth == 0);
        m_depth = depth;
      }
    }
  };

  struct IdUtil
  {
    template<typename Tv>
    constexpr static auto bind_value(ValueVariant<T> &v, Tv &&value, std::false_type /* StorePointer */)
    {
      // for constexpr
      v = ValueVariant<T>{std::forward<Tv>(value)};
    }

    template<typename Tv>
    constexpr static auto bind_value(ValueVariant<T> &v, Tv &&value, std::true_type /* StorePointer */)
    {
      v = ValueVariant<T>{&value};
    }
  };

  using BlockVariant = std::variant<Block, Block *>;
  BlockVariant m_block = Block{};

  constexpr T const &internal_value() const
  {
    return block().value();
  }

public:
  constexpr Id() = default;

  constexpr Id(Id const &id)
  {
    m_block = BlockVariant{&id.block()};
  }

  // non-const to inform users not to mark id as const.
  template<typename Pattern>
  constexpr auto at(Pattern &&a_pattern)
  {
    return and_(a_pattern, *this);
  }

  // non-const to inform users not to mark id as const.
  constexpr auto at(Ooo const &)
  {
    return OooBinder<T>{*this};
  }

  constexpr Block &block() const
  {
    return std::visit(
        overload([](Block &v) -> Block & { return v; }, [](Block *p) -> Block & { return *p; }),
        // constexpr does not allow mutable, we use const_cast instead. Never declare id as const.
        const_cast<BlockVariant &>(m_block));
  }

  template<typename Tv>
  constexpr auto match_value(Tv &&v) const
  {
    if (has_value()) {
      return IdTraits<T>::equal(internal_value(), v);
    }

    IdUtil::bind_value(block().variant(), std::forward<Tv>(v), StorePointer<T, Tv>{});
    return true;
  }

  constexpr void reset(int32_t depth) const
  {
    return block().reset(depth);
  }

  constexpr void confirm(int32_t depth) const
  {
    return block().confirm(depth);
  }

  constexpr bool has_value() const
  {
    return block().has_value();
  }

  // non-const to inform users not to mark id as const.
  constexpr T const &value()
  {
    return block().value();
  }

  // non-const to inform users not to mark id as const.
  constexpr T const &operator*()
  {
    return value();
  }

  constexpr T &&move()
  {
    return std::move(block().mutable_value());
  }
};

template<typename T>
class PatternTraits<Id<T>>
{
public:
  template<typename>
  using AppResultTuple = std::tuple<>;

  constexpr static auto num_id_v = true;

  template<typename Tv, typename Tc>
  constexpr static auto match_pattern_impl(Tv &&value, Id<T> const &id_pat, int32_t /* depth */, Tc &)
  {
    return id_pat.match_value(std::forward<Tv>(value));
  }

  constexpr static void process_id_impl(Id<T> const &id_pat, int32_t depth, IdProcess id_process)
  {
    switch (id_process) {
      case IdProcess::kCancel:id_pat.reset(depth);
        break;

      case IdProcess::kConfirm:id_pat.confirm(depth);
        break;
    }
  }
};

template<typename... Ts>
class Ds
{
public:
  using Type = std::tuple<InternalPatternT<Ts>...>;

  constexpr explicit Ds(Ts const &...a_patterns) : m_patterns{a_patterns...}
  {
  }

  constexpr auto const &patterns() const
  {
    return m_patterns;
  }

private:
  Type m_patterns;
};

template<typename... Ts>
constexpr auto ds(Ts const &...a_patterns) -> Ds<Ts...>
{
  return Ds < Ts...>{ a_patterns... };
}

template<typename T>
class OooBinder
{
  Id<T> m_id;

public:
  OooBinder(Id<T> const &id) : m_id{id}
  {
  }

  decltype(auto) binder() const
  {
    return m_id;
  }
};

struct Ooo
{
  template<typename T>
  constexpr auto operator()(Id<T> id) const
  {
    return OooBinder<T>{id};
  }
};

constexpr Ooo ooo;

template<>
struct PatternTraits<Ooo>
{
  template<typename>
  using AppResultTuple = std::tuple<>;

  constexpr static auto num_id_v = false;

  template<typename Tv, typename Tc>
  constexpr static auto match_pattern_impl(Tv &&, Ooo, int32_t /*depth*/, Tc &)
  {
    return true;
  }

  constexpr static void process_id_impl(Ooo, int32_t /*depth*/, IdProcess)
  {
  }
};

template<typename T>
struct PatternTraits<OooBinder<T>>
{
  template<typename Tv>
  using AppResultTuple = typename PatternTraits<T>::template AppResultTuple<Tv>;

  constexpr static auto num_id_v = PatternTraits<T>::num_id_v;

  template<typename Tv, typename Tc>
  constexpr static auto match_pattern_impl(Tv &&value,
                                           OooBinder<T> const &ooo_binder_pat,
                                           int32_t depth,
                                           Tc &context)
  {
    return match_pattern(std::forward<Tv>(value), ooo_binder_pat.binder(), depth + 1, context);
  }

  constexpr static void process_id_impl(OooBinder<T> const &ooo_binder_pat, int32_t depth, IdProcess id_process)
  {
    process_id(ooo_binder_pat.binder(), depth, id_process);
  }
};

template<typename T>
struct IsOoo : public std::false_type
{
};

template<>
struct IsOoo<Ooo> : public std::true_type
{
};

template<typename T>
struct IsOooBinder : public std::false_type
{
};

template<typename T>
struct IsOooBinder<OooBinder<T>> : public std::true_type
{
};

template<typename T>
constexpr auto is_ooo_binder_v = IsOooBinder<std::decay_t<T>>::value;

template<typename T>
constexpr auto is_ooo_or_binder_v = IsOoo<std::decay_t<T>>::value || is_ooo_binder_v<T>;

template<typename... Ts>
constexpr auto num_ooo_or_binder_v = ((is_ooo_or_binder_v<Ts> ? 1 : 0) + ... + 0);

static_assert(num_ooo_or_binder_v<int32_t &, Ooo const &, char const *, Wildcard, Ooo const> == 2);

template<typename T, std::size_t... I>
constexpr size_t find_ooo_idx_impl(std::index_sequence<I...>)
{
  return ((is_ooo_or_binder_v<decltype(get<I>(std::declval<T>()))> ? I : 0) + ...);
}

template<typename T>
constexpr size_t find_ooo_idx()
{
  return find_ooo_idx_impl<T>(std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<T>>>{});
}

static_assert(is_ooo_or_binder_v<Ooo>);
static_assert(is_ooo_or_binder_v<OooBinder<int32_t>>);

static_assert(find_ooo_idx<std::tuple<int32_t, OooBinder<int32_t>, const char *>>() == 1);
static_assert(find_ooo_idx<std::tuple<int32_t, Ooo, const char *>>() == 1);

template<
    std::size_t ValueStartI,
    std::size_t PatternStartI,
    std::size_t... I,
    typename ValueTupleT,
    typename PatternTupleT,
    typename Tc
>
constexpr decltype(auto) match_pattern_multiple_impl(ValueTupleT &&a_value_tuple,
                                                     PatternTupleT &&a_pattern_tuple,
                                                     int32_t depth,
                                                     Tc &context,
                                                     std::index_sequence<I...>)
{
  auto const func = [&](auto &&value, auto &&a_pattern) {
    return match_pattern(std::forward<decltype(value)>(value), a_pattern, depth + 1, context);
  };

  static_cast<void>(func);
  return (func(std::get<I + ValueStartI>(std::forward<ValueTupleT>(a_value_tuple)),
               std::get<I + PatternStartI>(a_pattern_tuple)) && ...);
}

template<
    std::size_t ValueStartI,
    std::size_t PatternStartI,
    std::size_t N,
    typename ValueTupleT,
    typename PatternTupleT,
    typename Tc
>
constexpr decltype(auto) match_pattern_multiple(ValueTupleT &&a_value_tuple,
                                                PatternTupleT &&a_pattern_tuple,
                                                int32_t depth,
                                                Tc &context)
{
  return match_pattern_multiple_impl<ValueStartI, PatternStartI>(std::forward<ValueTupleT>(a_value_tuple),
                                                                 a_pattern_tuple,
                                                                 depth,
                                                                 context,
                                                                 std::make_index_sequence<N>{});
}

template<
    std::size_t PatternStartI,
    std::size_t... I,
    typename RangeBeginT,
    typename PatternTupleT,
    typename Tc
>
constexpr decltype(auto) match_pattern_range_impl(RangeBeginT &&a_range_begin,
                                                  PatternTupleT &&a_pattern_tuple,
                                                  int32_t depth,
                                                  Tc &context,
                                                  std::index_sequence<I...>)
{
  auto const func = [&](auto &&value, auto &&a_pattern) {
    return match_pattern(std::forward<decltype(value)>(value),
                         a_pattern,
                         depth + 1,
                         context);
  };

  static_cast<void>(func);
  // Fix Me, avoid call next from begin every time.
  return (func(*std::next(a_range_begin, static_cast<long>(I)), std::get<I + PatternStartI>(a_pattern_tuple)) && ...);
}

template<
    std::size_t PatternStartI,
    std::size_t N,
    typename ValueRangeBeginT,
    typename PatternTupleT,
    typename Tc
>
constexpr decltype(auto) match_pattern_range(ValueRangeBeginT &&a_value_range_begin,
                                             PatternTupleT &&a_pattern_tuple,
                                             int32_t depth,
                                             Tc &context)
{
  return match_pattern_range_impl<PatternStartI>(a_value_range_begin,
                                                 a_pattern_tuple,
                                                 depth,
                                                 context,
                                                 std::make_index_sequence<N>{});
}

template<std::size_t StartI, typename Is, typename T>
class IndexedTypes;

template<typename T, std::size_t StartI, std::size_t... I>
struct IndexedTypes<StartI, std::index_sequence<I...>, T>
{
  using type = std::tuple<std::decay_t<decltype(std::get<StartI + I>(std::declval<T>()))>...>;
};

template<std::size_t StartI, std::size_t EndI, typename T>
class SubTypes
{
  constexpr static auto tuple_size = std::tuple_size_v<std::remove_reference_t<T>>;
  static_assert((StartI <= EndI) && (EndI <= tuple_size));
public:
  using type = typename IndexedTypes<StartI, std::make_index_sequence<EndI - StartI>, T>::type;
};

template<std::size_t StartI, std::size_t EndI, typename T>
using SubTypesT = typename SubTypes<StartI, EndI, T>::type;

static_assert(
    std::is_same_v<
        std::tuple<std::nullptr_t>,
        SubTypesT<3, 4, std::tuple<char, bool, int32_t, std::nullptr_t>>>);

static_assert(
    std::is_same_v<
        std::tuple<char>,
        SubTypesT<0, 1, std::tuple<char, bool, int32_t, std::nullptr_t>>>);

static_assert(
    std::is_same_v<
        std::tuple<>,
        SubTypesT<1, 1, std::tuple<char, bool, int32_t, std::nullptr_t>>>);

static_assert(
    std::is_same_v<
        std::tuple<int32_t, std::nullptr_t>,
        SubTypesT<2, 4, std::tuple<char, bool, int32_t, std::nullptr_t>>>);

template<typename T>
struct IsArray : public std::false_type
{
};

template<typename T, size_t N>
struct IsArray<std::array<T, N>> : public std::true_type
{
};

template<typename T>
constexpr auto is_array_v = IsArray<std::decay_t<T>>::value;

template<typename T, typename = std::void_t<>>
struct IsTupleLike : std::false_type
{
};

template<typename T>
struct IsTupleLike<T, std::void_t<decltype(std::tuple_size<T>::value)>> : std::true_type
{
};

template<typename T>
constexpr auto is_tuplelike_v = IsTupleLike<std::decay_t<T>>::value;

static_assert(is_tuplelike_v<std::pair<int32_t, char>>);
static_assert(!is_tuplelike_v<bool>);

template<typename Tv, typename = std::void_t<>>
struct IsRange : std::false_type
{
};

template<typename Tv> //
requires requires { decltype(std::begin(std::declval<Tv>())){}; decltype(std::end(std::declval<Tv>())){}; }
struct IsRange<Tv> : std::true_type
{
};

template<typename T>
constexpr auto is_range_v = IsRange<std::decay_t<T>>::value;

static_assert(!is_range_v<std::pair<int32_t, char>>);
static_assert(is_range_v<const std::array<int32_t, 5>>);

template<typename... Ts>
class PatternTraits<Ds<Ts...>>
{
  constexpr static auto num_ooo_or_binder = num_ooo_or_binder_v<Ts...>;
  static_assert(num_ooo_or_binder == 0 || num_ooo_or_binder == 1);

public:
  template<typename P, typename Tv>
  struct PairPV;

  template<typename... Ps, typename... Vs>
  struct PairPV<std::tuple<Ps...>, std::tuple<Vs...>>
  {
    using type = decltype(std::tuple_cat(std::declval<typename PatternTraits<Ps>::template AppResultTuple<Vs>>()...));
  };

  template<std::size_t NumOoos, typename Tv>
  struct AppResultForTupleHelper;

  template<typename... Vs>
  struct AppResultForTupleHelper<0, std::tuple<Vs...>>
  {
    using type = decltype(std::tuple_cat(std::declval<typename PatternTraits<Ts>::template AppResultTuple<Vs>>()...));
  };

  template<typename... Vs>
  struct AppResultForTupleHelper<1, std::tuple<Vs...>>
  {
  private:
    constexpr static auto idx_ooo = find_ooo_idx<typename Ds<Ts...>::Type>();

    using Ps0 = SubTypesT<0, idx_ooo, std::tuple<Ts...>>;
    using Vs0 = SubTypesT<0, idx_ooo, std::tuple<Vs...>>;

    constexpr static auto is_binder = is_ooo_binder_v<std::tuple_element_t<idx_ooo, std::tuple<Ts...>>>;

    // <0, ...int32_t> to workaround compile failure for std::tuple<>.
    using ElemT = std::tuple_element_t<0, std::tuple<std::remove_reference_t<Vs>..., int32_t>>;

    constexpr static int64_t diff = static_cast<int64_t>(sizeof...(Vs) - sizeof...(Ts));
    constexpr static size_t clippedDiff = static_cast<size_t>(diff > 0 ? diff : 0);

    using OooResultTuple = typename std::conditional_t<
        is_binder,
        std::tuple<SubrangeT<std::array<ElemT, clippedDiff>>>,
        std::tuple<>
    >;

    using FirstHalfTuple = typename PairPV<Ps0, Vs0>::type;
    using Ps1 = SubTypesT<idx_ooo + 1, sizeof...(Ts), std::tuple<Ts...>>;

    constexpr static auto vs1_start = static_cast<size_t>(static_cast<int64_t>(idx_ooo) + 1 + diff);

    using Vs1 = SubTypesT<vs1_start, sizeof...(Vs), std::tuple<Vs...>>;
    using SecondHalfTuple = typename PairPV<Ps1, Vs1>::type;

  public:
    using type = decltype(std::tuple_cat(std::declval<FirstHalfTuple>(),
                                         std::declval<OooResultTuple>(),
                                         std::declval<SecondHalfTuple>()));
  };

  template<typename T>
  using AppResultForTuple = typename AppResultForTupleHelper<num_ooo_or_binder, decltype(drop<0>(std::declval<T>()))>::type;

  template<typename RangeT>
  using RangeTuple = std::conditional_t<
      num_ooo_or_binder == 1,
      std::tuple<SubrangeT<RangeT>>,
      std::tuple<>
  >;

  template<typename RangeT>
  using AppResultForRangeType = decltype(std::tuple_cat(
      std::declval<RangeTuple<RangeT>>(),
      std::declval<typename PatternTraits<Ts>::template AppResultTuple<decltype(*std::begin(std::declval<RangeT>()))>>() ...));

  template<typename Tv, typename = std::void_t<>>
  struct AppResultHelper;

  template<typename Tv>
  struct AppResultHelper<Tv, std::enable_if_t<is_tuplelike_v<Tv>>>
  {
    using type = AppResultForTuple<Tv>;
  };

  template<typename RangeT>
  struct AppResultHelper<RangeT, std::enable_if_t<!is_tuplelike_v<RangeT> && is_range_v<RangeT>>>
  {
    using type = AppResultForRangeType<RangeT>;
  };

  template<typename Tv>
  using AppResultTuple = typename AppResultHelper<Tv>::type;

  constexpr static auto num_id_v = (PatternTraits<Ts>::num_id_v + ... + 0);

  template<typename T, typename Tc>
  constexpr static auto match_pattern_impl(T &&a_value_tuple,
                                           Ds<Ts...> const &ds_pat,
                                           int32_t depth,
                                           Tc &context) -> std::enable_if_t<is_tuplelike_v<T>, bool>
  {
    if constexpr (num_ooo_or_binder == 0) {
      return std::apply(
          [&a_value_tuple, depth, &context](auto const &...a_patterns) {
            return apply_(
                [depth, &context, &a_patterns...](auto &&...values) constexpr {
                  static_assert(sizeof...(a_patterns) == sizeof...(values));
                  return (match_pattern(std::forward<decltype(values)>(values),
                                        a_patterns,
                                        depth + 1,
                                        context) && ...);
                },
                a_value_tuple);
          },
          ds_pat.patterns());
    } else if constexpr (num_ooo_or_binder == 1) {
      constexpr auto idx_ooo = find_ooo_idx<typename Ds<Ts...>::Type>();
      constexpr auto is_binder = is_ooo_binder_v<std::tuple_element_t<idx_ooo, std::tuple<Ts...>>>;
      constexpr auto is_array = is_array_v<T>;

      auto result = match_pattern_multiple<0, 0, idx_ooo>(std::forward<T>(a_value_tuple),
                                                          ds_pat.patterns(),
                                                          depth,
                                                          context);

      constexpr auto val_len = std::tuple_size_v<std::decay_t<T>>;
      constexpr auto pat_len = sizeof...(Ts);

      if constexpr (is_array) {
        if constexpr (is_binder) {
          auto const rangeSize = static_cast<long>(val_len - (pat_len - 1));

          context.emplace_back(make_subrange(&a_value_tuple[idx_ooo],
                                             &a_value_tuple[idx_ooo] + rangeSize));

          using type = decltype(make_subrange(&a_value_tuple[idx_ooo], &a_value_tuple[idx_ooo] + rangeSize));

          result = result && match_pattern(std::get<type>(context.back()),
                                           std::get<idx_ooo>(ds_pat.patterns()),
                                           depth,
                                           context);
        }
      } else {
        static_assert(!is_binder);
      }

      return result && match_pattern_multiple<val_len - pat_len + idx_ooo + 1, idx_ooo + 1, pat_len - idx_ooo - 1>(
          std::forward<T>(a_value_tuple),
          ds_pat.patterns(), depth, context);
    }
  }

  template<typename RangeT, typename Tc>
  requires (!is_tuplelike_v<RangeT> && is_range_v<RangeT>)
  constexpr static auto match_pattern_impl(RangeT &&value_range,
                                           Ds<Ts...> const &ds_pat,
                                           int32_t depth,
                                           Tc &context) -> bool
  {
    static_assert(num_ooo_or_binder == 0 || num_ooo_or_binder == 1);
    constexpr auto num_pat = sizeof...(Ts);

    if constexpr (num_ooo_or_binder == 0) {
      // size mismatch for dynamic array is not an error;
      if (value_range.size() != num_pat) {
        return false;
      }

      return match_pattern_range<0, num_pat>(std::begin(value_range),
                                             ds_pat.patterns(),
                                             depth,
                                             context);

    } else if constexpr (num_ooo_or_binder == 1) {
      if (value_range.size() < num_pat - 1) {
        return false;
      }

      constexpr auto idx_ooo = find_ooo_idx<typename Ds<Ts...>::Type>();
      constexpr auto is_binder = is_ooo_binder_v<std::tuple_element_t<idx_ooo, std::tuple<Ts...>>>;

      auto result = match_pattern_range<0, idx_ooo>(std::begin(value_range), ds_pat.patterns(), depth, context);
      auto const val_len = value_range.size();
      constexpr auto pat_len = sizeof...(Ts);
      auto const begin_ooo = std::next(std::begin(value_range), idx_ooo);

      if constexpr (is_binder) {
        auto const range_size = static_cast<long>(val_len - (pat_len - 1));
        auto const end = std::next(begin_ooo, range_size);
        context.emplace_back(make_subrange(begin_ooo, end));

        using type = decltype(make_subrange(begin_ooo, end));
        result = result && match_pattern(std::get<type>(context.back()),
                                         std::get<idx_ooo>(ds_pat.patterns()),
                                         depth,
                                         context);
      }

      auto const begin_after_ooo = std::next(begin_ooo, static_cast<long>(val_len - pat_len + 1));

      return result && match_pattern_range<idx_ooo + 1, pat_len - idx_ooo - 1>(begin_after_ooo,
                                                                               ds_pat.patterns(),
                                                                               depth,
                                                                               context);
    }
  }

  constexpr static void process_id_impl(Ds<Ts...> const &ds_pat, int32_t depth, IdProcess id_process)
  {
    return std::apply(
        [depth, id_process](auto &&...a_patterns) { return (process_id(a_patterns, depth, id_process), ...); },
        ds_pat.patterns());
  }

};

static_assert(
    std::is_same_v<
        typename PatternTraits<
            Ds<OooBinder<SubrangeT<const std::array<int32_t, 2>>>>
        >::AppResultTuple<const std::array<int32_t, 2>>,
        std::tuple<matchit::impl::Subrange<const int32_t *, const int32_t *>>
    >);

static_assert(
    std::is_same_v<
        typename PatternTraits<
            Ds<OooBinder<Subrange<int32_t *, int32_t *>>, matchit::impl::Id<int32_t>>
        >::AppResultTuple<const std::array<int32_t, 3>>,
        std::tuple<matchit::impl::Subrange<const int32_t *, const int32_t *>>>);

static_assert(
    std::is_same_v<
        typename PatternTraits<
            Ds<OooBinder<Subrange<int32_t *, int32_t *>>, matchit::impl::Id<int32_t>>
        >::AppResultTuple<std::array<int32_t, 3>>,
        std::tuple<matchit::impl::Subrange<int32_t *, int32_t *>>>);

template<typename T, typename P>
class PostCheck
{
public:
  constexpr explicit PostCheck(T const &a_pattern, P const &pred) : m_pattern{a_pattern}, m_pred{pred}
  {
  }

  constexpr bool check() const
  {
    return m_pred();
  }

  constexpr auto const &pattern() const
  {
    return m_pattern;
  }

private:
  T const m_pattern;
  P const m_pred;
};

template<typename T, typename P>
class PatternTraits<PostCheck<T, P>>
{
public:
  template<typename Tv>
  using AppResultTuple = typename PatternTraits<T>::template AppResultTuple<Tv>;

  template<typename Tv, typename Tc>
  constexpr static auto match_pattern_impl(Tv &&value,
                                           PostCheck<T, P> const &post_check,
                                           int32_t depth,
                                           Tc &context)
  {
    return match_pattern(std::forward<Tv>(value), post_check.pattern(), depth + 1, context) && post_check.check();
  }

  constexpr static void process_id_impl(PostCheck<T, P> const &post_check, int32_t depth, IdProcess id_process)
  {
    process_id(post_check.pattern(), depth, id_process);
  }
};

static_assert(std::is_same_v<
    PatternTraits<Wildcard>::template AppResultTuple<int32_t>,
    std::tuple<>>);

static_assert(std::is_same_v<
    PatternTraits<int32_t>::template AppResultTuple<int32_t>,
    std::tuple<>>);

constexpr auto x = [](auto &&t) { return t; };

static_assert(std::is_same_v<
    PatternTraits<App<decltype(x), Wildcard>>::template AppResultTuple<int32_t>,
    std::tuple<>>);

static_assert(std::is_same_v<
    PatternTraits<App<decltype(x), Wildcard>>::template AppResultTuple<std::array<int32_t, 3>>,
    std::tuple<std::array<int32_t, 3>>>);

static_assert(std::is_same_v<
    PatternTraits<And<App<decltype(x), Wildcard>>>::template AppResultTuple<int32_t>,
    std::tuple<>>);

static_assert(PatternTraits<And<App<decltype(x), Wildcard>>>::num_id_v == 0);
static_assert(PatternTraits<And<App<decltype(x), Id<int32_t>>>>::num_id_v == 1);
static_assert(PatternTraits<And<Id<int32_t>, Id<float>>>::num_id_v == 2);
static_assert(PatternTraits<Or<Id<int32_t>, Id<float>>>::num_id_v == 2);
static_assert(PatternTraits<Or<Wildcard, float>>::num_id_v == 0);

template<typename Tv, typename... Ts>
constexpr auto match_patterns(Tv &&value, Ts const &...a_patterns)
{
  using ReturnT = typename PatternPairsReturnType<Ts...>::ReturnT;

  using TupleT = decltype(std::tuple_cat(
      std::declval<typename PatternTraits<typename Ts::PatternT>::
      template AppResultTuple<Tv>>()...));

  // expression, has return value.
  if constexpr (!std::is_same_v<ReturnT, void>) {
    constexpr auto const func =
        [](auto const &a_pattern, auto &&value, ReturnT &result) constexpr -> bool {
          auto context = typename ContextTrait<TupleT>::ContextT{};

          if (a_pattern.match_value(std::forward<Tv>(value), context)) {
            result = a_pattern.execute();
            process_id(a_pattern, 0, IdProcess::kCancel);
            return true;
          }

          return false;
        };

    ReturnT result{};

    bool const matched = (func(a_patterns, value, result) || ...);
    if (!matched) {
      throw std::logic_error{"Error: no patterns got matched!"};
    }

    static_cast<void>(matched);
    return result;
  } else {
    // statement, no return value, mismatching all patterns is not an error.
    auto const func = [](auto const &a_pattern, auto &&value) -> bool {
      auto context = typename ContextTrait<TupleT>::ContextT{};

      if (a_pattern.match_value(std::forward<Tv>(value), context)) {
        a_pattern.execute();
        process_id(a_pattern, 0, IdProcess::kCancel);
        return true;
      }

      return false;
    };

    bool const matched = (func(a_patterns, value) || ...);
    static_cast<void>(matched);
  }
}

template<typename T>
constexpr auto cast = [](auto &&input) { return static_cast<T>(input); };

constexpr auto deref = [](auto &&x) -> decltype(*x) & { return *x; };
constexpr auto some = [](auto const pat) { return and_(app(cast<bool>, true), app(deref, pat)); };
constexpr auto none = app(cast<bool>, false);

template<typename T, typename VariantT, typename = std::void_t<>>
struct ViaGetIf : std::false_type
{
};

template<typename T, typename VariantT>
struct ViaGetIf<T, VariantT, std::void_t<decltype(std::get_if<T>(std::declval<VariantT const *>()))>> : std::true_type
{
};

template<typename T, typename VariantT>
constexpr auto via_get_if_v = ViaGetIf<T, VariantT>::value;

static_assert(via_get_if_v<int, std::variant<int, bool>>);

template<typename T>
struct AsPointer
{
  template<typename VariantT>
  requires(via_get_if_v<T, VariantT>)
  constexpr auto operator()(VariantT const &v) const
  {
    return std::get_if<T>(std::addressof(v));
  }

  // template to disable implicit cast to std::any
  template<typename A>
  requires(std::is_same_v<A, std::any>)
  constexpr auto operator()(A const &a) const
  {
    return std::any_cast<T>(std::addressof(a));
  }

  template<typename D>
  requires (!via_get_if_v<T, D> && std::is_base_of_v<T, D>)
  constexpr auto operator()(D const &d) const -> decltype(static_cast<T const *>(std::addressof(d)))
  {
    return static_cast<T const *>(std::addressof(d));
  }

  template<typename B>
  requires (!via_get_if_v<T, B> && std::is_base_of_v<B, T>)
  constexpr auto operator()(B const &b) const -> decltype(dynamic_cast<T const *>(std::addressof(b)))
  {
    return dynamic_cast<T const *>(std::addressof(b));
  }
};

template<typename T>
constexpr AsPointer<T> as_pointer;

template<typename T>
constexpr auto as = [](auto const pat) { return app(as_pointer<T>, some(pat)); };

template<typename Tv, typename T>
constexpr auto matched(Tv &&v, T &&a_pattern)
{
  return match(std::forward<Tv>(v))(
      pattern | std::forward<T>(a_pattern) = [] { return true; },
      pattern | _ = [] { return false; });
}

constexpr auto ds_via = [](auto ...members) {
  return [members...](auto ...pats) { return and_(app(members, pats)...); };
};

template<typename T>
constexpr auto as_ds_via = [](auto ...members) {
  return [members...](auto ...pats) {
    // FIXME, why the following line will cause segfault in at-Bindings.cpp
    // return as<T>(ds_via(members...)(pats...));
    return as<T>(and_(app(members, pats)...));
  };
};

} // namespace impl

using impl::match;
using impl::expr;
using impl::_;
using impl::and_;
using impl::app;
using impl::ds;
using impl::Id;
using impl::meet;
using impl::not_;
using impl::ooo;
using impl::or_;
using impl::pattern;
using impl::Subrange;
using impl::SubrangeT;
using impl::when;
using impl::as;
using impl::as_ds_via;
using impl::ds_via;
using impl::matched;
using impl::none;
using impl::some;

} // namespace matchit

#endif