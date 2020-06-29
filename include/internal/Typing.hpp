#pragma once

#include "../core/Definitions.hpp"
#include "PCG.hpp"
#include <boost/core/typeinfo.hpp>
#include <boost/type_index.hpp>
#include <optional>
#include <random>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ImageGraph::internal {
template<typename T> static inline std::string type_name() {
  return boost::typeindex::type_id_with_cvr<T>().pretty_name();
}
static inline std::string demangled_name(const std::type_info& t) { return boost::core::demangled_name(t); }

template<typename... Types> class TypeList {
  template<typename T, typename... Ts> struct Container {};
  template<typename T> struct Container<T> { static inline constexpr bool value{false}; };
  template<typename T, typename T1, typename... Ts> struct Container<T, T1, Ts...> {
    static inline constexpr bool value{std::is_same_v<T, T1> or Container<T, Ts...>::value};
  };

  template<typename T1> struct Combinator {};
  template<typename... Ts> struct Combinator<TypeList<Ts...>> { using type = TypeList<Types..., Ts...>; };

  template<typename Select, typename TS> struct Selector {};
  template<typename Select> struct Selector<Select, TypeList<>> { using type = TypeList<>; };
  template<typename Select, typename T1, typename... Ts> struct Selector<Select, TypeList<T1, Ts...>> {
    using type =
        std::conditional_t<Select::template is_supported<T1>,
                           typename TypeList<T1>::template combined_t<typename Selector<Select, TypeList<Ts...>>::type>,
                           typename Selector<Select, TypeList<Ts...>>::type>;
  };

  template<typename Perform, typename TS> struct Performer {};
  template<typename Perform> struct Performer<Perform, TypeList<>> {
    template<typename Result, typename... Args>
    constexpr static inline std::optional<Result> perform(const std::type_info&, Args&&...) {
      return std::nullopt;
    }
  };
  template<typename Perform, typename T1, typename... Ts> struct Performer<Perform, TypeList<T1, Ts...>> {
    template<typename Result, typename... Args>
    constexpr static inline std::optional<Result> perform(const std::type_info& type, Args&&... args) {
      if (type == typeid(T1)) return Perform::template perform<T1>(std::forward<Args>(args)...);
      return Performer<Perform, TypeList<Ts...>>::template perform<Result, Args...>(type, std::forward<Args>(args)...);
    }
  };

  template<typename Transform, typename IndexSequence> struct Transformer {};
  template<typename Transform, std::size_t... Indices> struct Transformer<Transform, std::index_sequence<Indices...>> {
    using type = TypeList<typename Transform::template type<std::tuple_element_t<Indices, std::tuple<Types...>>>...>;
  };

  template<typename Perform, typename... Ts> struct ForEacher {};
  template<typename Perform> struct ForEacher<Perform> {
    template<typename... Args> constexpr static inline void perform(Args...) { return; }
  };
  template<typename Perform, typename T, typename... Ts> struct ForEacher<Perform, T, Ts...> {
    template<typename... Args> constexpr static inline void perform(Args... args) {
      Perform::template perform<T>(args...);
      ForEacher<Perform, Ts...>::perform(args...);
    }
  };

public:
  static inline constexpr std::size_t size{sizeof...(Types)};
  template<typename T> static inline constexpr bool contains{Container<T, Types...>::value};

  using tuple_t = std::tuple<Types...>;
  template<typename Transform> using transformed_t =
      typename Transformer<Transform, std::make_index_sequence<sizeof...(Types)>>::type;
  template<std::size_t Index> using at_t = std::tuple_element_t<Index, std::tuple<Types...>>;
  template<typename OtherTypeList> using combined_t = typename Combinator<OtherTypeList>::type;
  template<typename Select> using selected_t = typename Selector<Select, TypeList<Types...>>::type;

  template<typename Perform, typename Result, typename... Args>
  constexpr static inline std::optional<Result> perform(const std::type_info& type, Args&&... args) {
    return Performer<Perform, TypeList<Types...>>::template perform<Result, Args...>(type, std::forward<Args>(args)...);
  }
  template<typename Map, typename... Args> constexpr static inline auto map(Args&&... args) {
    return std::tuple(Map::template map<Types>(args...)...);
  }
  template<typename Perform, typename... Args> constexpr static inline void for_each(Args&&... args) {
    return ForEacher<Perform, Types...>::template perform<Args...>(std::forward<Args>(args)...);
  }
};

using default_numbers_t = TypeList<uint8_t, uint16_t, uint32_t, int8_t, int16_t, int32_t, float32_t, float64_t>;

namespace __detail {
template<typename T, std::size_t I> struct ConstantTupler {
  template<typename... Args> using type = typename ConstantTupler<T, I - 1>::template type<T, Args...>;
};
template<typename T> struct ConstantTupler<T, 0> { template<typename... Args> using type = std::tuple<Args...>; };
} // namespace __detail

template<typename T, std::size_t size> using ConstantTuple =
    typename __detail::ConstantTupler<T, size>::template type<>;

template<typename... TypeLists> class TypeListList {
public:
  static inline constexpr std::size_t size{sizeof...(TypeLists)};
  template<std::size_t Index> using at_t = std::tuple_element_t<Index, std::tuple<TypeLists...>>;
  using types_t = ConstantTuple<const std::type_info&, sizeof...(TypeLists)>;

private:
  template<typename Perform> struct Performer {
    template<typename TLL, typename STL> struct TypeParter {};
    template<typename T, typename... Ts, typename... TLs, typename... STs>
    struct TypeParter<TypeListList<TypeList<T, Ts...>, TLs...>, TypeList<STs...>> {
      static inline constexpr std::size_t main_index{sizeof...(STs)};

      template<typename Result, typename... Args>
      constexpr static inline std::optional<Result> perform(types_t types, Args&&... args) {
        if (std::get<main_index>(types) == typeid(T))
          return TypeParter<TypeListList<TLs...>, TypeList<STs..., T>>::template perform<Result, Args...>(
              types, std::forward<Args>(args)...);
        else
          return TypeParter<TypeListList<TypeList<Ts...>, TLs...>, TypeList<STs...>>::template perform<Result, Args...>(
              types, std::forward<Args>(args)...);
      }
    };
    template<typename... TLs, typename... STs> struct TypeParter<TypeListList<TypeList<>, TLs...>, TypeList<STs...>> {
      template<typename Result, typename... Args>
      constexpr static inline std::optional<Result> perform(types_t, Args&&...) {
        return std::nullopt;
      }
    };
    template<typename... STs> struct TypeParter<TypeListList<>, TypeList<STs...>> {
      template<typename Result, typename... Args>
      constexpr static inline std::optional<Result> perform(types_t, Args&&... args) {
        return Perform::template perform<STs...>(std::forward<Args>(args)...);
      }
    };
  };

public:
  template<typename Perform, typename Result, typename... Args>
  constexpr static inline std::optional<Result> perform(types_t types, Args&&... args) {
    return Performer<Perform>::template TypeParter<TypeListList<TypeLists...>, TypeList<>>::template perform<Result,
                                                                                                             Args...>(
        std::move(types), std::forward<Args>(args)...);
  }
};

struct node_tag {};

/**
 * A node trait is expected to have at least the following members:
 * * template<typename Out, typename... In> struct are_types_supported { static inline constexpr bool value; };
 */
template<typename Node> requires std::is_base_of_v<node_tag, Node> struct node_traits {};

namespace {
template<typename Base, size_t Index, typename... Types> struct dynamic_get {
  constexpr static inline Base* get(const std::tuple<Types*...>& tuple, size_t index) {
    if (index == Index) return dynamic_cast<Base*>(std::get<Index>(tuple));
    return dynamic_get<Base, Index - 1, Types...>::get(tuple, index);
  }
};
template<typename Base, typename... Types> struct dynamic_get<Base, 0, Types...> {
  constexpr static inline Base* get(const std::tuple<Types*...>& tuple, size_t index) {
    if (index == 0) return dynamic_cast<Base*>(std::get<0>(tuple));
    throw std::invalid_argument("The index is not in range!");
  }
};
} // namespace
template<typename Base, typename... Types>
constexpr static inline Base* dynamic_get_v(const std::tuple<Types*...>& tuple, size_t index) {
  if constexpr (sizeof...(Types) > 0)
    return dynamic_get<Base, sizeof...(Types) - 1, Types...>::get(tuple, index);
  else
    throw std::invalid_argument("The index is not in range!");
}

template<typename T> struct pcg_fast_generator {};
template<> struct pcg_fast_generator<float32_t> : public pcg32_fast {
  pcg_fast_generator() : pcg32_fast(pcg_extras::seed_seq_from<std::random_device>()) {}
};
template<> struct pcg_fast_generator<float64_t> : public pcg64_fast {
  pcg_fast_generator() : pcg64_fast(pcg_extras::seed_seq_from<std::random_device>()) {}
};
} // namespace ImageGraph::internal
