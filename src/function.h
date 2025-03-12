#pragma once
#include "descriptor.h"

template <typename F>
class function;

template <typename R, typename... Args>
class function<R(Args...)> {
public:
  using result_type = R;

public:
  function() noexcept
      : _descriptor(_init_descriptor<bad_call_tag>())
      , _storage({}) {}

  template <typename F>
    requires not_same_as<F, function>
  function(F&& func)
      : _descriptor(_init_descriptor<std::decay_t<F>>()) {
    model<std::decay_t<F>>::construct(_storage.data(), std::forward<F>(func));
  }

  function(const function& other)
      : _descriptor(other._descriptor) {
    _descriptor->copy(other._storage.data(), _storage.data());
  }

  function(function&& other) noexcept
      : _descriptor(std::exchange(other._descriptor, _init_descriptor<bad_call_tag>())) {
    _descriptor->move(other._storage.data(), _storage.data());
  }

  function& operator=(const function& other) {
    if (this != &other) {
      *this = std::move(function(other));
    }
    return *this;
  }

  function& operator=(function&& other) noexcept {
    if (this != &other) {
      _descriptor->destroy(_storage.data());
      _descriptor = std::exchange(other._descriptor, _init_descriptor<bad_call_tag>());
      _descriptor->move(other._storage.data(), _storage.data());
    }
    return *this;
  }

  ~function() {
    _descriptor->destroy(_storage.data());
  }

  explicit operator bool() const noexcept {
    return (_descriptor != _init_descriptor<bad_call_tag>());
  }

  result_type operator()(Args... args) const {
    return _descriptor->call(_storage.data(), std::forward<Args>(args)...);
  }

  template <typename T>
  T* target() noexcept {
    return const_cast<T*>(std::as_const(*this).template target<T>());
  }

  template <typename T>
  const T* target() const noexcept {
    if (_descriptor == _init_descriptor<T>()) {
      return model<T>::target(_storage.data());
    }
    return nullptr;
  }

private:
  template <typename F>
  using model = model<F, result_type(Args...)>;
  using descriptor = concept_<result_type(Args...)>;

private:
  descriptor* _descriptor;
  alignas(std::max_align_t) mutable std::array<std::byte, kSmallSize> _storage;

  template <typename Tag>
  static descriptor* _init_descriptor() {
    static model<Tag> initialized_model;
    return &initialized_model;
  }
};
