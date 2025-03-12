#pragma once

#include <array>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

///********************************[Common block]********************************///

inline constexpr std::size_t kSmallSize = sizeof(void*) * 4;

struct bad_function_call : std::exception {
  bad_function_call() noexcept = default;
  bad_function_call(const bad_function_call& other) noexcept = default;
  bad_function_call& operator=(const bad_function_call& other) noexcept = default;

  const char* what() const noexcept override {
    return "Bad function call";
  }
};

///********************************[Concepts]********************************///

template <typename T, typename U>
concept not_same_as = !std::is_same_v<std::decay_t<T>, U>;

template <typename T>
concept isNothrowMoveConstructible = std::is_nothrow_move_constructible_v<T>;

template <typename T>
concept isSmallSize = sizeof(T) <= kSmallSize;

template <typename T>
concept isAligned = alignof(std::max_align_t) % alignof(T) == 0;

template <typename Function>
concept isSmall = isSmallSize<Function> && isAligned<Function> && isNothrowMoveConstructible<Function>;

///********************************[Models]********************************///

template <typename T>
struct concept_;

template <typename R, typename... Args>
struct concept_<R(Args...)> {
public:
  virtual void copy(std::byte*, std::byte*) const {}

  virtual void move(std::byte*, std::byte*) const noexcept {}

  virtual void destroy(std::byte*) const noexcept {}

  virtual R call(std::byte*, Args&&...) const = 0;
  virtual ~concept_() = default;
};

/// Model
template <typename U, typename V>
struct model {};

/// Regular model
template <typename F, typename R, typename... Args>
struct model<F, R(Args...)> final : concept_<R(Args...)> {
public:
  static F** _get_ppointer(std::byte* ptr) {
    return std::launder(reinterpret_cast<F**>(ptr));
  }

  static F* target(std::byte* ptr) {
    return *_get_ppointer(ptr);
  }

  template <typename U>
  static void construct(std::byte* ptr, U&& func) {
    std::construct_at(_get_ppointer(ptr), new F(std::forward<U>(func)));
  }

  void copy(std::byte* from_ptr, std::byte* to_ptr) const override {
    std::construct_at(_get_ppointer(to_ptr), new F(**_get_ppointer(from_ptr)));
  }

  void move(std::byte* from_ptr, std::byte* to_ptr) const noexcept override {
    std::construct_at(_get_ppointer(to_ptr), std::exchange(*_get_ppointer(from_ptr), nullptr));
    destroy(from_ptr);
  }

  void destroy(std::byte* ptr) const noexcept override {
    delete *_get_ppointer(ptr);
  }

  R call(std::byte* ptr, Args&&... args) const override {
    return std::invoke(**_get_ppointer(ptr), std::forward<Args>(args)...);
  }
};

/// Small model
template <isSmall F, typename R, typename... Args>
struct model<F, R(Args...)> final : concept_<R(Args...)> {
public:
  static F* _get_pointer(std::byte* ptr) {
    return std::launder(reinterpret_cast<F*>(ptr));
  }

  static F* target(std::byte* ptr) {
    return _get_pointer(ptr);
  }

  template <typename U>
  static void construct(std::byte* ptr, U&& func) {
    std::construct_at(_get_pointer(ptr), std::forward<U>(func));
  }

  void copy(std::byte* from_ptr, std::byte* to_ptr) const override {
    std::construct_at(_get_pointer(to_ptr), *_get_pointer(from_ptr));
  }

  void move(std::byte* from_ptr, std::byte* to_ptr) const noexcept override {
    std::construct_at(_get_pointer(to_ptr), std::move(*_get_pointer(from_ptr)));
    destroy(from_ptr);
  }

  void destroy(std::byte* ptr) const noexcept override {
    std::destroy_at(_get_pointer(ptr));
  }

  R call(std::byte* ptr, Args&&... args) const override {
    return std::invoke(*_get_pointer(ptr), std::forward<Args>(args)...);
  }
};

/// Empty model
template <typename R, typename... Args>
struct model<struct bad_call_tag, R(Args...)> final : concept_<R(Args...)> {
public:
  static bad_call_tag* _get_pointer(std::byte*) = delete;

  static bad_call_tag* target(std::byte*) = delete;

  R call(std::byte*, Args&&...) const override {
    throw bad_function_call();
  }
};
