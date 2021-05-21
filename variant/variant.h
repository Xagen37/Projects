#pragma once
#include <exception>
#include <functional>
#include <new>
#include <type_traits>

template <class... Types> class variant;

// HELPERS
inline constexpr std::size_t variant_npos = -1;

class bad_variant_access : std::exception {
  const char *what() const noexcept override { return "bad variant access"; }
};

struct in_place_t {
  explicit in_place_t() = default;
};
inline constexpr in_place_t in_place{};
template <class T> struct in_place_type_t { explicit in_place_type_t() = default; };
template <class T> inline constexpr in_place_type_t<T> in_place_type{};
template <std::size_t Id> struct in_place_index_t { explicit in_place_index_t() = default; };
template <std::size_t Id> inline constexpr in_place_index_t<Id> in_place_index{};

template <class T> struct variant_size;
template <class T> struct variant_size<const T> : variant_size<T>::type {};
template <class T> struct variant_size<volatile T> : variant_size<T>::type {};
template <class T> struct variant_size<const volatile T> : variant_size<T>::type {};
template <class T> inline constexpr std::size_t variant_size_v = variant_size<T>::value;
template <class... Types>
struct variant_size<variant<Types...>> : std::integral_constant<std::size_t, sizeof...(Types)> {};

template <class T, std::size_t N> struct array_wrapper {
  constexpr const T &operator[](std::size_t id) const { return data[id]; }

  T data[N > 0 ? N : 1];
};

template <class Ids, std::size_t To_add> struct push_back;

template <std::size_t... Ids, std::size_t To_add> struct push_back<std::index_sequence<Ids...>, To_add> {
  using type = std::index_sequence<Ids..., To_add>;
};

template <class Ids, std::size_t To_add> using push_back_t = typename push_back<Ids, To_add>::type;

template <class...> struct list {};

template <class List> struct list_front;

template <template <class...> class List, class Head, class... Tail> struct list_front<List<Head, Tail...>> {
  using type = Head;
};

template <class T, class... Types> constexpr std::size_t list_find_index() {
  constexpr array_wrapper<bool, sizeof...(Types)> eqs = {std::is_same_v<T, Types>...};
  bool is_found = false;
  std::size_t result = variant_npos;
  for (std::size_t i = 0; i < sizeof...(Types); i++) {
    if (eqs[i]) {
      if (is_found) {
        return variant_npos;
      }
      result = i;
      is_found = true;
    }
  }
  return result;
}

template <std::size_t Id, class Seq> struct get_t_at { using type = void; };
template <std::size_t Id, class Head, class... Tail> struct get_t_at<Id, variant<Head, Tail...>> {
  using type = typename get_t_at<Id - 1, variant<Tail...>>::type;
};
template <class Head, class... Tail> struct get_t_at<0, variant<Head, Tail...>> { using type = Head; };

template <class T> struct is_spec_of_in_place : std::false_type {};
template <class T> struct is_spec_of_in_place<in_place_type_t<T>> : std::true_type {};
template <std::size_t Id> struct is_spec_of_in_place<in_place_index_t<Id>> : std::true_type {};
template <class T> static constexpr bool is_not_spec_of_in_place = !is_spec_of_in_place<std::decay_t<T>>::value;

template <std::size_t Id, class T> struct variant_alternative;
template <std::size_t Id, class... Types> struct variant_alternative<Id, variant<Types...>> {
  static_assert(Id < sizeof...(Types), "out of bounds in variant_alternative");

  using type = typename get_t_at<Id, variant<Types...>>::type;
};
template <std::size_t Id, class T> using variant_alternative_t = typename variant_alternative<Id, T>::type;
template <std::size_t Id, class T> struct variant_alternative<Id, const T> {
  using type = std::add_const_t<variant_alternative_t<Id, T>>;
};
template <std::size_t Id, class T> struct variant_alternative<Id, volatile T> {
  using type = std::add_volatile_t<variant_alternative_t<Id, T>>;
};
template <std::size_t Id, class T> struct variant_alternative<Id, const volatile T> {
  using type = std::add_cv_t<variant_alternative_t<Id, T>>;
};

// STORAGE

template <bool Is_triv_dest, class... Types> class storage {};

template <class... Types>
using autocheck_storage = storage<std::conjunction_v<std::is_trivially_destructible<Types>...>, Types...>;

template <class Head, class... Tail>
class storage<true, Head, Tail...> // trivially_destructible case
{
public:
  storage() noexcept {};

  template <class... Args>
  explicit constexpr storage(std::integral_constant<std::size_t, 0>,
                             Args &&... args) noexcept(std::is_nothrow_constructible_v<Head, Args...>)
      : head(std::forward<Args>(args)...) {}

  template <std::size_t Id, class... Args, class = std::enable_if_t<(Id > 0)>>
  explicit constexpr storage(std::integral_constant<std::size_t, Id>, Args &&... args) noexcept(
      std::is_nothrow_constructible_v<autocheck_storage<Tail...>, std::integral_constant<std::size_t, Id - 1>, Args...>)
      : tail(std::integral_constant<std::size_t, Id - 1>(), std::forward<Args>(args)...) {}

  constexpr Head &get_head() &noexcept { return head; }
  constexpr const Head &get_head() const &noexcept { return head; }
  constexpr Head &&get_head() &&noexcept { return std::move(head); }
  constexpr const Head &&get_head() const &&noexcept { return std::move(head); }

  constexpr autocheck_storage<Tail...> &get_tail() &noexcept { return tail; }
  constexpr const autocheck_storage<Tail...> &get_tail() const &noexcept { return tail; }
  constexpr autocheck_storage<Tail...> &&get_tail() &&noexcept { return std::move(tail); }
  constexpr const autocheck_storage<Tail...> &&get_tail() const &&noexcept { return std::move(tail); }

  static constexpr std::size_t size = sizeof...(Tail) + 1;

protected:
  union {
    Head head;
    autocheck_storage<Tail...> tail;
  };
};

template <class Head, class... Tail>
class storage<false, Head, Tail...> // non-trivially_destructible case
{
public:
  storage() noexcept {};

  template <class... Args>
  explicit constexpr storage(std::integral_constant<std::size_t, 0>,
                             Args &&... args) noexcept(std::is_nothrow_constructible_v<Head, Args...>)
      : head(std::forward<Args>(args)...) {}

  template <std::size_t Id, class... Args, class = std::enable_if_t<(Id > 0)>>
  explicit constexpr storage(std::integral_constant<std::size_t, Id>, Args &&... args) noexcept(
      std::is_nothrow_constructible_v<autocheck_storage<Tail...>, std::integral_constant<std::size_t, Id - 1>, Args...>)
      : tail(std::integral_constant<std::size_t, Id - 1>(), std::forward<Args>(args)...) {}

  ~storage() {}

  constexpr Head &get_head() &noexcept { return head; }
  constexpr const Head &get_head() const &noexcept { return head; }
  constexpr Head &&get_head() &&noexcept { return std::move(head); }
  constexpr const Head &&get_head() const &&noexcept { return std::move(head); }

  constexpr autocheck_storage<Tail...> &get_tail() &noexcept { return tail; }
  constexpr const autocheck_storage<Tail...> &get_tail() const &noexcept { return tail; }
  constexpr autocheck_storage<Tail...> &&get_tail() &&noexcept { return std::move(tail); }
  constexpr const autocheck_storage<Tail...> &&get_tail() const &&noexcept { return std::move(tail); }

  static constexpr std::size_t size = sizeof...(Tail) + 1;

protected:
  union {
    Head head;
    autocheck_storage<Tail...> tail;
  };
};

template <std::size_t Id, class Storage> constexpr decltype(auto) get_storage(Storage &&strg) noexcept {
  if constexpr (Id == 0) {
    return std::forward<Storage>(strg).get_head();
  } else {
    return get_storage<Id - 1>(std::forward<Storage>(strg).get_tail());
  }
}

// STORAGE - STORAGE_VISIT

template <std::size_t Id, class T> struct paired {
  static constexpr std::size_t id = Id;
  T val;
};

template <std::size_t Id, class Storage>
using v_paired_t = paired<Id, decltype(get_storage<Id>(std::declval<Storage>())) &&>;

template <class Func, class Storage>
using visit_storage_t = decltype(std::declval<Func>()(std::declval<v_paired_t<0, Storage>>()));

template <std::size_t Id, class Func, class Storage>
constexpr visit_storage_t<Func, Storage> dispatch_storage(Func &&f, Storage &&strg) {
  return std::forward<Func>(f)(v_paired_t<Id, Storage>{get_storage<Id>(std::forward<Storage>(strg))});
}

template <class Func, class Storage>
constexpr visit_storage_t<Func, Storage> visit_valueless_storage(Func &&f, Storage &&strg) {
  return std::forward<Func>(f)(paired<variant_npos, Storage &&>{std::forward<Storage>(strg)});
}

template <class Func, class Storage, class Id_seq = std::make_index_sequence<std::remove_reference_t<Storage>::size>>
struct visit_storage_table;

template <class Func, class Storage, std::size_t... Ids>
struct visit_storage_table<Func, Storage, std::index_sequence<Ids...>> {
  using dispatch_storage_t = visit_storage_t<Func, Storage> (*)(Func &&, Storage &&);
  static constexpr dispatch_storage_t vector[] = {&visit_valueless_storage<Func, Storage>,
                                                  &dispatch_storage<Ids, Func, Storage>...};
};

struct visit_storage_helper {
  template <class Func, class Storage>
  static constexpr visit_storage_t<Func, Storage> visit(std::size_t Id, Func &&f, Storage &&strg) {
    constexpr auto &vec = visit_storage_table<Func, Storage>::vector;
    return vec[Id](std::forward<Func>(f), std::forward<Storage>(strg));
  }
};

template <class Func, class Storage>
constexpr visit_storage_t<Func, Storage> visit_storage(std::size_t Id, Storage &&strg, Func &&f) {
  return visit_storage_helper::visit(Id + 1, std::forward<Func>(f), std::forward<Storage>(strg));
}

// VARIANT_BASES

template <class... Types> class variant_base : autocheck_storage<Types...> {
  using strg_t = autocheck_storage<Types...>;

protected:
  std::size_t id;

  void destroy() noexcept {
    if constexpr (!std::conjunction_v<std::is_trivially_destructible<Types>...>) {
      visit_storage(index(), get_storage(), [](auto paired_obj) {
        if constexpr (decltype(paired_obj)::id != variant_npos) {
          using T = std::decay_t<decltype(paired_obj.val)>;
          paired_obj.val.~T();
        }
      });
    }
  }

public:
  constexpr variant_base() noexcept : strg_t(), id(variant_npos) {}

  template <std::size_t Id, class... Args,
            class = std::enable_if_t<std::is_constructible_v<get_t_at<Id, variant<Types...>>>>>
  explicit constexpr variant_base(in_place_index_t<Id>, Args &&... args)
      : strg_t(std::integral_constant<std::size_t, Id>{}, std::forward<Args>(args)...), id(Id) {}

  void reset() noexcept {
    destroy();
    id = variant_npos;
  }

  constexpr strg_t &get_storage() &noexcept { return *this; }
  constexpr const strg_t &get_storage() const &noexcept { return *this; }
  constexpr strg_t &&get_storage() &&noexcept { return std::move(*this); }
  constexpr const strg_t &&get_storage() const &&noexcept { return std::move(*this); }

  constexpr std::size_t index() const noexcept { return id; }
  void set_id(const std::size_t new_id) noexcept { id = new_id; }
  constexpr bool valueless_by_exception() const noexcept { return id == variant_npos; }
};

template <class... Types> struct variant_base_destroy : variant_base<Types...> {
  using base = variant_base<Types...>;
  using base::base;

  ~variant_base_destroy() { this->destroy(); }

  variant_base_destroy() = default;
  variant_base_destroy(const variant_base_destroy &) = default;
  variant_base_destroy(variant_base_destroy &&) = default;
  variant_base_destroy &operator=(variant_base_destroy &&) = default;
  variant_base_destroy &operator=(const variant_base_destroy &) = default;
};

template <class... Types>
using what_base = std::conditional_t<std::conjunction_v<std::is_trivially_destructible<Types>...>,
                                     variant_base<Types...>, variant_base_destroy<Types...>>;

template <class... Types> struct construct_visitor {
  template <std::size_t Id, class T>
  void operator()(paired<Id, T> from) const noexcept(std::is_nothrow_constructible_v<T, T>) {
    if constexpr (Id != variant_npos) {
      auto &where = get_storage<Id>(to_construct.get_storage());
      void *mem = std::addressof(where);
      new (mem) std::decay_t<decltype(where)>(std::forward<T>(from.val));
      to_construct.set_id(Id);
    }
  }

  variant_base<Types...> &to_construct;
};

template <bool, class... Types> struct variant_copy_cons_base : what_base<Types...> {
  using base = what_base<Types...>;
  using base::base;
};

template <class... Types> struct variant_copy_cons_base<false, Types...> : what_base<Types...> {
  using base = what_base<Types...>;
  using base::base;

  variant_copy_cons_base(const variant_copy_cons_base &other) noexcept((std::is_nothrow_copy_constructible_v<Types> &&
                                                                        ...)) {
    visit_storage(other.index(), other.get_storage(), construct_visitor<Types...>{*this});
  }

  variant_copy_cons_base(variant_copy_cons_base &&) = default;
  variant_copy_cons_base &operator=(const variant_copy_cons_base &) = default;
  variant_copy_cons_base &operator=(variant_copy_cons_base &&) = default;
};

template <class... Types>
using what_copy_cons =
    std::conditional_t<std::conjunction_v<std::is_trivially_copy_constructible<Types>...>,
                       variant_copy_cons_base<true, Types...>,
                       std::conditional_t<std::conjunction_v<std::is_copy_constructible<Types>...>,
                                          variant_copy_cons_base<false, Types...>, what_base<Types...>>>;

template <bool, class... Types> struct variant_move_cons_base : what_copy_cons<Types...> {
  using base = what_copy_cons<Types...>;
  using base::base;
};

template <class... Types> struct variant_move_cons_base<false, Types...> : what_copy_cons<Types...> {
  using base = what_copy_cons<Types...>;
  using base::base;

  variant_move_cons_base(variant_move_cons_base &&other) noexcept((std::is_nothrow_move_constructible_v<Types> &&
                                                                   ...)) {
    visit_storage(other.index(), std::move(other).get_storage(), construct_visitor<Types...>{*this});
  }

  variant_move_cons_base(const variant_move_cons_base &) = default;
  variant_move_cons_base &operator=(const variant_move_cons_base &) = default;
  variant_move_cons_base &operator=(variant_move_cons_base &&) = default;
};

template <class... Types>
using what_move_cons =
    std::conditional_t<std::conjunction_v<std::is_trivially_move_constructible<Types>...>,
                       variant_move_cons_base<true, Types...>,
                       std::conditional_t<std::conjunction_v<std::is_move_constructible<Types>...>,
                                          variant_move_cons_base<false, Types...>, what_copy_cons<Types...>>>;

template <class... Types> struct assign_visitor {
  template <std::size_t Id, class T> void operator()(paired<Id, T> from) const {
    if constexpr (Id == variant_npos) {
      to_assign.reset();
    } else {
      auto &where = get_storage<Id>(to_assign.get_storage());
      if (to_assign.index() == Id) {
        where = std::forward<T &&>(from.val);
      } else {
        if constexpr (std::is_rvalue_reference_v<T>) {
          to_assign.reset();
          void *mem = const_cast<void *>(static_cast<const void *>(std::addressof(where)));
          new (mem) std::decay_t<decltype(where)>(std::forward<T>(from.val));
        } else {
          using check_type = std::remove_cv_t<std::remove_reference_t<T>>;
          if constexpr (std::is_nothrow_copy_constructible_v<check_type> ||
                        !std::is_nothrow_move_constructible_v<check_type>) {
            to_assign.reset();
            void *mem = const_cast<void *>(static_cast<const void *>(std::addressof(where)));
            new (mem) std::decay_t<decltype(where)>(from.val);
            // static_cast<variant<Types...>>(to_assign).emplace<Id>(from.val);
          } else {
            auto medium = from.val;
            to_assign.reset();
            void *mem = const_cast<void *>(static_cast<const void *>(std::addressof(where)));
            new (mem) std::decay_t<decltype(where)>(std::move(medium));
          }
        }

        to_assign.set_id(Id);
      }
    }
  }

  variant_base<Types...> &to_assign;
};

template <bool, class... Types> struct variant_copy_assign_base : what_move_cons<Types...> {
  using base = what_move_cons<Types...>;
  using base::base;
};

template <class... Types> struct variant_copy_assign_base<false, Types...> : what_move_cons<Types...> {
  using base = what_move_cons<Types...>;
  using base::base;

  variant_copy_assign_base &operator=(const variant_copy_assign_base &other) noexcept(
      ((std::is_nothrow_copy_constructible_v<Types> && std::is_nothrow_copy_assignable_v<Types>)&&...)) {
    visit_storage(other.index(), other.get_storage(), assign_visitor<Types...>{*this});

    return *this;
  }

  variant_copy_assign_base(const variant_copy_assign_base &) = default;
  variant_copy_assign_base(variant_copy_assign_base &&) = default;
  variant_copy_assign_base &operator=(variant_copy_assign_base &&) = default;
};

template <class... Types>
using what_copy_ass =
    std::conditional_t<std::conjunction_v<std::is_trivially_copy_constructible<Types>...> &&
                           std::conjunction_v<std::is_trivially_copy_assignable<Types>...> &&
                           std::conjunction_v<std::is_trivially_destructible<Types>...>,
                       variant_copy_assign_base<true, Types...>,
                       std::conditional_t<std::conjunction_v<std::is_copy_constructible<Types>...> &&
                                              std::conjunction_v<std::is_copy_assignable<Types>...>,
                                          variant_copy_assign_base<false, Types...>, what_move_cons<Types...>>>;

template <bool, class... Types> struct variant_move_assign_base : what_copy_ass<Types...> {
  using base = what_copy_ass<Types...>;
  using base::base;
};

template <class... Types> struct variant_move_assign_base<false, Types...> : what_copy_ass<Types...> {
  using base = what_copy_ass<Types...>;
  using base::base;

  variant_move_assign_base &operator=(variant_move_assign_base &&other) noexcept(
      ((std::is_nothrow_move_constructible_v<Types> && std::is_nothrow_move_assignable_v<Types>)&&...)) {
    visit_storage(other.index(), std::move(other).get_storage(), assign_visitor<Types...>{*this});

    return *this;
  }

  variant_move_assign_base(const variant_move_assign_base &) = default;
  variant_move_assign_base(variant_move_assign_base &&) = default;
  variant_move_assign_base &operator=(const variant_move_assign_base &) = default;
};

template <class... Types>
using what_move_ass =
    std::conditional_t<std::conjunction_v<std::is_trivially_move_constructible<Types>...> &&
                           std::conjunction_v<std::is_trivially_move_assignable<Types>...> &&
                           std::conjunction_v<std::is_trivially_destructible<Types>...>,
                       variant_move_assign_base<true, Types...>,
                       std::conditional_t<std::conjunction_v<std::is_move_constructible<Types>...> &&
                                              std::conjunction_v<std::is_move_assignable<Types>...>,
                                          variant_move_assign_base<false, Types...>, what_copy_ass<Types...>>>;

// GET

template <std::size_t Id, class... Types> constexpr decltype(auto) get(variant<Types...> &v) {
  static_assert(Id < sizeof...(Types), "out of bounds in get");
  if (v.index() == Id) {
    return get_storage<Id>(v.get_storage());
  }

  throw bad_variant_access();
}

template <std::size_t Id, class... Types> constexpr decltype(auto) get(const variant<Types...> &v) {
  static_assert(Id < sizeof...(Types), "out of bounds in get");
  if (v.index() == Id) {
    return get_storage<Id>(v.get_storage());
  }

  throw bad_variant_access();
}

template <std::size_t Id, class... Types> constexpr decltype(auto) get(variant<Types...> &&v) {
  static_assert(Id < sizeof...(Types), "out of bounds in get");
  if (v.index() == Id) {
    return get_storage<Id>(std::move(v).get_storage());
  }

  throw bad_variant_access();
}

template <std::size_t Id, class... Types> constexpr decltype(auto) get(const variant<Types...> &&v) {
  static_assert(Id < sizeof...(Types), "out of bounds in get");
  if (v.index() == Id) {
    return get_storage<Id>(std::move(v).get_storage());
  }

  throw bad_variant_access();
}

template <class T, class... Types> constexpr T &get(variant<Types...> &v) {
  constexpr std::size_t id = list_find_index<T, Types...>();
  static_assert(id < sizeof...(Types), "out of bounds in get");

  return get<id>(v);
}

template <class T, class... Types> constexpr const T &get(const variant<Types...> &v) {
  constexpr std::size_t id = list_find_index<T, Types...>();
  static_assert(id < sizeof...(Types), "out of bounds in get");

  return get<id>(v);
}

template <class T, class... Types> constexpr T &&get(variant<Types...> &&v) {
  constexpr std::size_t id = list_find_index<T, Types...>();
  static_assert(id < sizeof...(Types), "out of bounds in get");

  return get<id>(std::move(v));
}

template <class T, class... Types> constexpr const T &&get(const variant<Types...> &&v) {
  constexpr std::size_t id = list_find_index<T, Types...>();
  static_assert(id < sizeof...(Types), "out of bounds in get");

  return get<id>(std::move(v));
}

// VARIANT

template <class T> struct Arr { T val[1]; };

template <std::size_t Id, class T, class U, bool is_bool = std::is_same_v<std::remove_cv_t<U>, bool>, class = void>
struct build_imaginary_f {
  void Im_fun();
};

template <std::size_t Id, class T, class U>
struct build_imaginary_f<Id, T, U, false, std::void_t<decltype(Arr<U>{{std::declval<T>()}})>> {
  static std::integral_constant<std::size_t, Id> Im_fun(U);
};

template <std::size_t Id, class T, class U>
struct build_imaginary_f<Id, T, U, true,
                         std::enable_if_t<std::is_same_v<bool, std::remove_cv_t<std::remove_reference_t<T>>>>> {
  static std::integral_constant<std::size_t, Id> Im_fun(U);
};

template <class T, class Variant, class = std::make_index_sequence<variant_size_v<Variant>>> struct build_fs;

template <class T, class... Types, std::size_t... Ids>
struct build_fs<T, variant<Types...>, std::index_sequence<Ids...>> : build_imaginary_f<Ids, T, Types>... {
  using build_imaginary_f<Ids, T, Types>::Im_fun...;
};

template <class T, class Variant> using Im_fun_t = decltype(build_fs<T, Variant>::Im_fun(std::declval<T>()));

template <class T, class Variant, class = void>
struct accepted_index : std::integral_constant<std::size_t, variant_npos> {};

template <class T, class Variant>
struct accepted_index<T, Variant, std::void_t<Im_fun_t<T, Variant>>> : Im_fun_t<T, Variant> {};

template <class T, class Variant> using accepted_type = variant_alternative_t<accepted_index<T, Variant>{}, Variant>;

template <class... Types> class variant : public what_move_ass<Types...> {
  static_assert(sizeof...(Types) > 0, "Empty variants are ill-formed");
  static_assert(!std::disjunction_v<std::is_void<Types>..., std::is_array<Types>..., std::is_reference<Types>...>,
                "A variant is not permitted to hold references, arrays, or the type void");

  using base = what_move_ass<Types...>;

  template <std::size_t Id, class... Args>
  variant_alternative_t<Id, variant<Types...>> &emplace_if_valueless(Args &&... args) noexcept(
      std::is_nothrow_constructible_v<typename get_t_at<Id, variant>::type, Args...>) {
    auto &where = get_storage<Id>(this->get_storage());
    void *mem = const_cast<void *>(static_cast<const void *>(std::addressof(where)));
    new (mem) std::decay_t<decltype(where)>(std::forward<Args &&>(args)...);
    this->set_id(Id);
    return where;
  }

  void emplace_from_other(variant &&other) noexcept(std::conjunction_v<std::is_nothrow_move_constructible<Types>...>) {
    this->reset();
    visit_storage(other.index(), other.get_storage(), [this](auto paired_obj) {
      constexpr std::size_t id = decltype(paired_obj)::id;
      if constexpr (id != variant_npos) {
        this->emplace_if_valueless<id>(std::move(paired_obj.val));
      }
    });
  }

public:
  template <class Head = typename list_front<list<Types...>>::type,
            class = std::enable_if_t<std::is_default_constructible_v<Head>>>
  constexpr variant() noexcept(std::is_nothrow_default_constructible_v<Head>) : base(in_place_index<0>) {}

  template <class T, class... Args, std::size_t Id = list_find_index<T, Types...>(),
            class = std::enable_if_t<Id != variant_npos && std::is_constructible_v<T, Args...>>>
  constexpr explicit variant(in_place_type_t<T>, Args &&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
      : base(in_place_index<Id>, std::forward<Args>(args)...) {}

  template <std::size_t Id, class... Args, class = std::enable_if_t<(Id < sizeof...(Types))>,
            class = std::enable_if_t<std::is_constructible_v<typename get_t_at<Id, variant<Types...>>::type, Args...>>>
  constexpr explicit variant(in_place_index_t<Id>, Args &&... args) noexcept(
      std::is_nothrow_constructible_v<typename get_t_at<Id, variant<Types...>>::type, Args...>)
      : base(in_place_index<Id>, std::forward<Args>(args)...) {}

  template <class T, std::size_t Id = accepted_index<T, variant<Types...>>::value,
            class = std::enable_if_t<(sizeof...(Types) > 0) && !std::is_same_v<std::decay_t<T>, variant> &&
                                     is_not_spec_of_in_place<T> && Id != variant_npos>,
            class = std::enable_if_t<std::is_constructible_v<accepted_type<T, variant<Types...>>, T>>>
  constexpr variant(T &&t) noexcept(std::is_nothrow_constructible_v<accepted_type<T, variant<Types...>>, T>)
      : variant(in_place_index<accepted_index<T, variant<Types...>>::value>, std::forward<T>(t)) {}

  template <class T, std::size_t Id = accepted_index<T, variant<Types...>>::value,
            class = std::enable_if_t<!std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, variant>>,
            class = std::enable_if_t<Id != variant_npos>, class Accepted = accepted_type<T, variant<Types...>>,
            class = std::enable_if_t<std::is_constructible_v<Accepted, T> && std::is_assignable_v<Accepted &, T>>>
  variant &operator=(T &&t) noexcept(
      std::is_nothrow_assignable_v<Accepted &, T> &&std::is_nothrow_constructible_v<Accepted, T>) {
    constexpr auto id = Id;
    if (this->index() == id) {
      auto &where = get_storage<id>(this->get_storage());
      where = std::forward<T>(t);
    } else {
      using target_t = Accepted;
      if constexpr (std::is_nothrow_constructible_v<target_t, T> || !std::is_nothrow_move_constructible_v<target_t>) {
        this->emplace<id>(std::forward<T>(t));
      } else {
        *this = variant(std::forward<T>(t));
      }
    }
    return *this;
  }

  template <class T, class... Args, std::size_t Id = list_find_index<T, Types...>(),
            class = std::enable_if_t<Id != variant_npos && std::is_constructible_v<T, Args...>>>
  T &emplace(Args &&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
    this->reset();
    return emplace_if_valueless<Id>(std::forward<Args &&>(args)...);
  }

  template <std::size_t Id, class... Args,
            class = std::enable_if_t<std::is_constructible_v<variant_alternative_t<Id, variant<Types...>>, Args...>>>
  variant_alternative_t<Id, variant<Types...>> &emplace(Args &&... args) noexcept(
      std::is_nothrow_constructible_v<variant_alternative_t<Id, variant<Types...>>, Args...>) {
    this->reset();
    return emplace_if_valueless<Id>(std::forward<Args &&>(args)...);
  }

  void swap(variant &other) noexcept(
      std::conjunction_v<std::is_nothrow_move_constructible<Types>..., std::is_nothrow_swappable<Types>...>) {
    if (this->index() == other.index()) {
      visit_storage(this->index(), other.get_storage(), [this](auto paired_obj) {
        constexpr std::size_t id = decltype(paired_obj)::id;
        if constexpr (id != variant_npos) {
          using std::swap;
          swap(get_storage<id>(this->get_storage()), paired_obj.val);
        }
      });
    } else {
      variant medium = std::move(other);
      other.emplace_from_other(std::move(*this));
      this->emplace_from_other(std::move(medium));
    }
  }
};

// NON_MEMBER_FUNCTION - HOLDS_ALTERNATIVE

template <class T, class... Types> constexpr bool holds_alternative(const variant<Types...> &v) noexcept {
  constexpr std::size_t id = list_find_index<T, Types...>();
  static_assert(id != variant_npos, "The call is ill-formed if T does not appear exactly once in Types...");
  return v.index() == id;
}

// NON_MEMBER_FUNCTION - GET_IF

template <std::size_t Id, class... Types>
constexpr std::add_pointer_t<variant_alternative_t<Id, variant<Types...>>> get_if(variant<Types...> *pv) noexcept {
  static_assert(Id < sizeof...(Types), "out of bound in get_if");

  return !pv ? nullptr : (pv->index() != Id ? nullptr : std::addressof(get_storage<Id>(pv->get_storage())));
}

template <class T, class... Types> constexpr std::add_pointer_t<T> get_if(variant<Types...> *pv) noexcept {
  constexpr std::size_t id = list_find_index<T, Types...>();
  static_assert(id != variant_npos, "The call is ill-formed if T is not a unique element of Types...");

  return get_if<id>(pv);
}
template <class T, class... Types> constexpr std::add_pointer_t<const T> get_if(const variant<Types...> *pv) noexcept {
  constexpr std::size_t id = list_find_index<T, Types...>();
  static_assert(id != variant_npos, "The call is ill-formed if T is not a unique element of Types...");

  return get_if<id>(pv);
}

// NON_MEMBER_FUNCTION - RELATIONAL

template <class Func, class... Types> struct compare_visitor {
  template <std::size_t Id, class T> constexpr bool operator()(paired<Id, const T &> rhs) const {
    if constexpr (Id != variant_npos) {
      return Func{}(get_storage<Id>(lhs.get_storage()), rhs.val);
    } else {
      return Func{}(19, 19);
    }
  }

  const variant_base<Types...> &lhs;
};

template <class... Types>
inline constexpr int valueless_helper(const variant<Types...> &first, const variant<Types...> &second) {
  if (first.valueless_by_exception()) {
    return -1;
  } else if (second.valueless_by_exception()) {
    return 1;
  } else {
    return 0;
  }
}

template <class... Types>
inline constexpr int index_helper(const variant<Types...> &first, const variant<Types...> &second) {
  if (first.index() != second.index()) {
    return (first.index() < second.index()) ? 1 : -1;
  } else {
    return 0;
  }
}

template <bool Is_less, class... Types>
constexpr bool strict(const variant<Types...> &first, const variant<Types...> &second) {
  using VISITOR =
      std::conditional_t<Is_less, compare_visitor<std::less<>, Types...>, compare_visitor<std::greater<>, Types...>>;
  int check_res = valueless_helper(first, second);
  if (!check_res) {
    check_res = index_helper(second, first);
    if (!check_res) {
      return visit_storage(first.index(), second.get_storage(), VISITOR{first});
    } else {
      return check_res + 1;
    }
  } else {
    return check_res + 1;
  }
}

template <bool Is_less_or_equal, class... Types>
constexpr bool not_strict(const variant<Types...> &first, const variant<Types...> &second) {
  using VISITOR = std::conditional_t<Is_less_or_equal, compare_visitor<std::less_equal<>, Types...>,
                                     compare_visitor<std::greater_equal<>, Types...>>;
  int check_res = -valueless_helper(first, second);
  if (!check_res) {
    check_res = index_helper(first, second);
    if (!check_res) {
      return visit_storage(first.index(), second.get_storage(), VISITOR{first});
    } else {
      return check_res + 1;
    }
  } else {
    return check_res + 1;
  }
}

template <class... Types> constexpr bool operator==(const variant<Types...> &v, const variant<Types...> &w) {
  using VISITOR = compare_visitor<std::equal_to<>, Types...>;
  return index_helper(v, w) ? false
                            : v.valueless_by_exception() || visit_storage(v.index(), w.get_storage(), VISITOR{v});
}

template <class... Types> constexpr bool operator!=(const variant<Types...> &v, const variant<Types...> &w) {
  using VISITOR = compare_visitor<std::not_equal_to<>, Types...>;
  return index_helper(v, w) ? true
                            : !v.valueless_by_exception() && visit_storage(v.index(), w.get_storage(), VISITOR{v});
}

template <class... Types> constexpr bool operator<(const variant<Types...> &v, const variant<Types...> &w) {
  return strict<true>(w, v);
}

template <class... Types> constexpr bool operator>(const variant<Types...> &v, const variant<Types...> &w) {
  return strict<false>(v, w);
}

template <class... Types> constexpr bool operator<=(const variant<Types...> &v, const variant<Types...> &w) {
  return not_strict<true>(v, w);
}

template <class... Types> constexpr bool operator>=(const variant<Types...> &v, const variant<Types...> &w) {
  return not_strict<false>(w, v);
}

// NON_MEMBER_FUNCTION - VISIT

template <class T> constexpr decltype(auto) at(const T &elem) noexcept { return elem; }

template <class T, std::size_t N, class... Ids>
constexpr decltype(auto) at(const array_wrapper<T, N> &elems, std::size_t i, Ids... ids) noexcept {
  return at(elems[i], ids...);
}

template <class F, class... Funcs>
constexpr array_wrapper<std::decay_t<F>, sizeof...(Funcs) + 1> make_fun_array(F &&f, Funcs &&... funcs) {
  return {{std::forward<F>(f), std::forward<Funcs>(funcs)...}};
}

template <std::size_t... Ids> struct dispatcher {
  template <typename Func, typename... Variants> struct helper {
    static constexpr decltype(auto) dispatch(Func f, Variants... vars) {
      return f(get_storage<Ids>(std::forward<Variants>(vars).get_storage())...);
    }
  };
};

template <class Func, class... Variants, std::size_t... Ids> constexpr auto make_dispatch(std::index_sequence<Ids...>) {
  return &dispatcher<Ids...>::template helper<Func, Variants...>::dispatch;
}

template <class Func, class... Variants, class Ids> constexpr auto make_fmatrix_helper(Ids ids) {
  return make_dispatch<Func, Variants...>(ids);
}

template <class Func, class... Variants, class Ids_first, std::size_t... Ids_second, class... Lists>
constexpr auto make_fmatrix_helper(Ids_first, std::index_sequence<Ids_second...>, Lists... lists) {
  return make_fun_array(make_fmatrix_helper<Func, Variants...>(push_back_t<Ids_first, Ids_second>{}, lists...)...);
}

template <class Func, class... Variants> constexpr auto make_fmatrix() {
  return make_fmatrix_helper<Func, Variants...>(std::index_sequence<>{},
                                                std::make_index_sequence<variant_size_v<std::decay_t<Variants>>>{}...);
}

template <class Func, class... Variants> struct fmatrix {
  static constexpr auto value = make_fmatrix<Func, Variants...>();
};

template <class Visitor, class... Variants>
static constexpr decltype(auto) visit_alternative(Visitor &&visitor, Variants &&... vars) {
  return at(fmatrix<Visitor &&, decltype(std::forward<Variants>(vars))...>::value,
            vars.index()...)(std::forward<Visitor>(visitor), std::forward<Variants>(vars)...);
}

template <class Visitor> struct value_visitor {
  Visitor &&visitor;

  template <class... Alters> constexpr decltype(auto) operator()(Alters &&... alters) const {
    return visitor(std::forward<Alters>(alters)...);
  }
};

template <class Visitor> inline static constexpr auto make_value_visitor(Visitor &&visitor) {
  return value_visitor<Visitor>{std::forward<Visitor>(visitor)};
}

template <class Visitor, class... Variants>
inline static constexpr decltype(auto) visit_value(Visitor &&visitor, Variants &&... vars) {
  return visit_alternative(make_value_visitor(std::forward<Visitor>(visitor)), std::forward<Variants>(vars)...);
}

template <class Func, class... Variants> constexpr decltype(auto) visit(Func &&visitor, Variants &&... vars) {
  if ((vars.valueless_by_exception() || ...)) {
    throw bad_variant_access();
  }

  return visit_value(std::forward<Func>(visitor), std::forward<Variants>(vars)...);
}
