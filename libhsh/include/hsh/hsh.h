#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <type_traits>
#include <utility>

#include "bits/builtin_types.h"
#include "bits/common.h"
#include "bits/source_location.h"
#include "bits/vulkan.h"

namespace hsh {
struct offset2d {
  int32_t x, y;
  constexpr offset2d(int32_t x = {}, int32_t y = {}) noexcept : x(x), y(y) {}
#if HSH_ENABLE_VULKAN
  constexpr offset2d(vk::Offset2D off) noexcept : x(off.x), y(off.y) {}
  operator vk::Offset2D() const noexcept { return vk::Offset2D(x, y); }
#endif
};
struct offset2dF {
  double x, y;
  constexpr offset2dF(double x = {}, double y = {}) noexcept : x(x), y(y) {}
  constexpr offset2dF(offset2d off) noexcept
      : x(double(off.x)), y(double(off.y)) {}
  offset2dF operator+(const offset2dF &other) const noexcept {
    return {x + other.x, y + other.y};
  }
  offset2dF &operator+=(const offset2dF &other) noexcept {
    *this = *this + other;
    return *this;
  }
  offset2dF operator-(const offset2dF &other) const noexcept {
    return {x - other.x, y - other.y};
  }
  offset2dF &operator-=(const offset2dF &other) noexcept {
    *this = *this - other;
    return *this;
  }
};
struct extent2d {
  uint32_t w, h;
  constexpr extent2d(uint32_t w = {}, uint32_t h = {}) noexcept : w(w), h(h) {}
#if HSH_ENABLE_VULKAN
  constexpr extent2d(vk::Extent2D ext) noexcept : w(ext.width), h(ext.height) {}
  operator vk::Extent2D() const noexcept { return vk::Extent2D(w, h); }
#endif
};
struct rect2d {
  offset2d offset;
  extent2d extent;
  constexpr rect2d(offset2d offset = {}, extent2d extent = {}) noexcept
      : offset(offset), extent(extent) {}
#if HSH_ENABLE_VULKAN
  constexpr rect2d(vk::Rect2D rect) noexcept
      : offset(rect.offset), extent(rect.extent) {}
  operator vk::Rect2D() const noexcept { return vk::Rect2D(offset, extent); }
#endif
};
struct extent3d {
  uint32_t w, h, d;
  constexpr extent3d(uint32_t w, uint32_t h, uint32_t d = {}) noexcept
      : w(w), h(h), d(d) {}
  explicit constexpr extent3d(extent2d e2d) noexcept
      : w(e2d.w), h(e2d.h), d(1) {}
  explicit constexpr extent3d(uint32_t e1d) noexcept : w(e1d), h(1), d(1) {}
#if HSH_ENABLE_VULKAN
  constexpr extent3d(vk::Extent3D ext) noexcept
      : w(ext.width), h(ext.height), d(ext.depth) {}
  operator vk::Extent3D() const noexcept { return vk::Extent3D(w, h, d); }
#endif
};
struct viewport {
  float x;
  float y;
  float width;
  float height;
  float minDepth;
  float maxDepth;
  explicit viewport(float x = 0.f, float y = 0.f, float width = 0.f,
                    float height = 0.f, float minDepth = 0.f,
                    float maxDepth = 1.f)
      : x(x), y(y), width(width), height(height), minDepth(minDepth),
        maxDepth(maxDepth) {}
};
struct scissor : rect2d {
  using rect2d::rect2d;
};
} // namespace hsh

#include "bits/vulkan_impl.h"

#include "bits/select_target_traits.h"

#include "bits/common_resources.h"
#include "bits/vulkan_resources.h"

namespace hsh {

template <typename T, typename OwnerType = typename decltype(T::Binding)::Owner>
struct owner_base {
  OwnerType Owner;

  owner_base() noexcept = default;
  explicit owner_base(decltype(Owner) Owner) noexcept
      : Owner(std::move(Owner)) {}

  owner_base(const owner_base &other) = delete;
  owner_base &operator=(const owner_base &other) = delete;
  owner_base(owner_base &&other) noexcept = default;
  owner_base &operator=(owner_base &&other) noexcept = default;

  operator bool() const noexcept { return Owner.IsValid(); }

  T get() const noexcept { return T(typename decltype(Owner)::Binding(Owner)); }
  operator T() const noexcept { return get(); }

  void reset() noexcept { Owner = decltype(Owner){}; }
};

template <typename T>
struct dynamic_owner_base
    : owner_base<T, typename decltype(T::Binding)::DynamicOwner> {
  using OwnerType = typename decltype(T::Binding)::DynamicOwner;
  using owner_base<T, OwnerType>::owner_base;
  using MappedType = typename T::MappedType;
  MappedType *map() noexcept {
    return reinterpret_cast<MappedType *>(
        owner_base<T, OwnerType>::Owner.Map());
  }
  void unmap() noexcept { owner_base<T, OwnerType>::Owner.Unmap(); }
  template <typename U,
            std::enable_if_t<
                std::conjunction_v<std::is_same<U, MappedType>,
                                   std::negation<std::is_void<MappedType>>>,
                int> = 0>
  void load(const U &obj) noexcept {
    auto *ptr = map();
    std::memcpy(ptr, &obj, sizeof(U));
    unmap();
  }
  void load(void *data, std::size_t size) noexcept {
    auto *ptr = map();
    std::memcpy(ptr, data, size);
    unmap();
  }
};

template <typename T> struct owner : owner_base<T> {
  using owner_base<T>::owner_base;
};

template <typename T> struct dynamic_owner : dynamic_owner_base<T> {
  using dynamic_owner_base<T>::dynamic_owner_base;
};

template <>
struct owner<uniform_buffer_typeless> : owner_base<uniform_buffer_typeless> {
  using owner_base<uniform_buffer_typeless>::owner_base;

  template <typename U>
  owner &operator=(owner<uniform_buffer<U>> &&other) noexcept {
    Owner = std::move(other.Owner);
    return *this;
  }
};

template <typename T>
struct dynamic_owner<uniform_buffer<T>>
    : dynamic_owner_base<uniform_buffer<T>> {
  using dynamic_owner_base<uniform_buffer<T>>::dynamic_owner_base;
  using MappedType = typename uniform_buffer<T>::MappedType;
  void load(const MappedType &obj) noexcept {
    auto *ptr = dynamic_owner_base<uniform_buffer<T>>::map();
    std::memcpy(ptr, &obj, sizeof(MappedType));
    dynamic_owner_base<uniform_buffer<T>>::unmap();
  }
};

template <>
struct dynamic_owner<uniform_buffer_typeless>
    : dynamic_owner_base<uniform_buffer_typeless> {
  using dynamic_owner_base<uniform_buffer_typeless>::dynamic_owner_base;

  template <typename U> void load(const U &obj) noexcept {
    auto *ptr = dynamic_owner_base<uniform_buffer_typeless>::map();
    std::memcpy(ptr, &obj, sizeof(U));
    dynamic_owner_base<uniform_buffer_typeless>::unmap();
  }

  template <typename U>
  dynamic_owner &operator=(dynamic_owner<uniform_buffer<U>> &&other) noexcept {
    Owner = std::move(other.Owner);
    return *this;
  }
};

template <>
struct owner<vertex_buffer_typeless> : owner_base<vertex_buffer_typeless> {
  using owner_base<vertex_buffer_typeless>::owner_base;

  template <typename U>
  owner &operator=(owner<vertex_buffer<U>> &&other) noexcept {
    Owner = std::move(other.Owner);
    return *this;
  }
};

template <typename T>
struct dynamic_owner<vertex_buffer<T>> : dynamic_owner_base<vertex_buffer<T>> {
  using dynamic_owner_base<vertex_buffer<T>>::dynamic_owner_base;
  using MappedType = typename vertex_buffer<T>::MappedType;
  void load(detail::ArrayProxy<MappedType> obj) noexcept {
    auto *ptr = dynamic_owner_base<vertex_buffer<T>>::map();
    std::memcpy(ptr, obj.data(), sizeof(MappedType) * obj.size());
    dynamic_owner_base<vertex_buffer<T>>::unmap();
  }
  template <std::size_t N>
  void load(const std::array<MappedType, N> &Arr) noexcept {
    load(detail::ArrayProxy<T>(Arr));
  }
};

template <>
struct dynamic_owner<vertex_buffer_typeless>
    : dynamic_owner_base<vertex_buffer_typeless> {
  using dynamic_owner_base<vertex_buffer_typeless>::dynamic_owner_base;

  template <typename U> void load(detail::ArrayProxy<U> obj) noexcept {
    auto *ptr = dynamic_owner_base<vertex_buffer_typeless>::map();
    std::memcpy(ptr, obj.data(), sizeof(U) * obj.size());
    dynamic_owner_base<vertex_buffer_typeless>::unmap();
  }

  template <typename U>
  dynamic_owner &operator=(dynamic_owner<vertex_buffer<U>> &&other) noexcept {
    Owner = std::move(other.Owner);
    return *this;
  }
};

template <>
struct owner<index_buffer_typeless> : owner_base<index_buffer_typeless> {
  using owner_base<index_buffer_typeless>::owner_base;

  template <typename U>
  owner &operator=(owner<index_buffer<U>> &&other) noexcept {
    Owner = std::move(other.Owner);
    return *this;
  }
};

template <typename T>
struct dynamic_owner<index_buffer<T>> : dynamic_owner_base<index_buffer<T>> {
  using dynamic_owner_base<index_buffer<T>>::dynamic_owner_base;
  using MappedType = typename index_buffer<T>::MappedType;
  void load(detail::ArrayProxy<MappedType> obj) noexcept {
    auto *ptr = dynamic_owner_base<index_buffer<T>>::map();
    std::memcpy(ptr, obj.data(), sizeof(MappedType) * obj.size());
    dynamic_owner_base<index_buffer<T>>::unmap();
  }
  template <std::size_t N>
  void load(const std::array<MappedType, N> &Arr) noexcept {
    load(detail::ArrayProxy<T>(Arr));
  }
};

template <>
struct dynamic_owner<index_buffer_typeless>
    : dynamic_owner_base<index_buffer_typeless> {
  using dynamic_owner_base<index_buffer_typeless>::dynamic_owner_base;

  template <typename U> void load(detail::ArrayProxy<U> obj) noexcept {
    auto *ptr = dynamic_owner_base<index_buffer_typeless>::map();
    std::memcpy(ptr, obj.data(), sizeof(U) * obj.size());
    dynamic_owner_base<index_buffer_typeless>::unmap();
  }

  template <typename U>
  dynamic_owner &operator=(dynamic_owner<index_buffer<U>> &&other) noexcept {
    Owner = std::move(other.Owner);
    return *this;
  }
};

template <typename T, typename... Args>
inline owner<T> create_resource(const SourceLocation &location,
                                Args &&... args) noexcept {
  return owner<T>(detail::ActiveTargetTraits::CreateResource<T>(
      location, std::forward<Args>(args)...));
}

template <typename T, typename... Args>
inline dynamic_owner<T> create_dynamic_resource(const SourceLocation &location,
                                                Args &&... args) noexcept {
  return dynamic_owner<T>(detail::ActiveTargetTraits::CreateDynamicResource<T>(
      location, std::forward<Args>(args)...));
}

template <typename T, typename... Args>
inline owner<T> create_resource(Args &&... args) noexcept {
  return owner<T>(detail::ActiveTargetTraits::CreateResource<T>(
      SourceLocation::current(), std::forward<Args>(args)...));
}

template <typename T, typename CopyFunc>
inline owner<uniform_buffer<T>> create_uniform_buffer(
    CopyFunc copyFunc,
    const SourceLocation &location = SourceLocation::current()) noexcept {
  return create_resource<uniform_buffer<T>>(location, copyFunc);
}

template <typename T>
inline owner<uniform_buffer<T>> create_uniform_buffer(
    const T &data,
    const SourceLocation &location = SourceLocation::current()) noexcept {
  return create_resource<uniform_buffer<T>>(
      location,
      [&](void *buf, std::size_t size) { std::memcpy(buf, &data, sizeof(T)); });
}

template <typename T>
inline dynamic_owner<uniform_buffer<T>> create_dynamic_uniform_buffer(
    const SourceLocation &location = SourceLocation::current()) noexcept {
  return create_dynamic_resource<uniform_buffer<T>>(location);
}

template <typename T>
inline dynamic_owner<uniform_buffer<T>> create_dynamic_uniform_buffer(
    const T &data,
    const SourceLocation &location = SourceLocation::current()) noexcept {
  auto ret = create_dynamic_resource<uniform_buffer<T>>(location);
  ret.load(data);
  return ret;
}

template <typename T>
inline owner<vertex_buffer<T>> create_vertex_buffer(
    detail::ArrayProxy<T> data,
    const SourceLocation &location = SourceLocation::current()) noexcept {
  return create_resource<vertex_buffer<T>>(
      location, data.size(), [&](void *buf, std::size_t size) {
        std::memcpy(buf, data.data(), sizeof(T) * data.size());
      });
}

template <typename T, std::size_t N>
inline owner<vertex_buffer<T>> create_vertex_buffer(
    const std::array<T, N> &Arr,
    const SourceLocation &location = SourceLocation::current()) noexcept {
  return create_vertex_buffer(detail::ArrayProxy<T>(Arr), location);
}

template <typename T>
inline dynamic_owner<vertex_buffer<T>> create_dynamic_vertex_buffer(
    std::size_t Size,
    const SourceLocation &location = SourceLocation::current()) noexcept {
  return create_dynamic_resource<vertex_buffer<T>>(location, sizeof(T) * Size);
}

template <typename T>
inline dynamic_owner<vertex_buffer<T>> create_dynamic_vertex_buffer(
    detail::ArrayProxy<T> data,
    const SourceLocation &location = SourceLocation::current()) noexcept {
  auto ret = create_dynamic_resource<vertex_buffer<T>>(location, data.size());
  ret.load(data);
  return ret;
}

template <typename T, std::size_t N>
inline dynamic_owner<vertex_buffer<T>> create_dynamic_vertex_buffer(
    const std::array<T, N> &Arr,
    const SourceLocation &location = SourceLocation::current()) noexcept {
  auto ret = create_dynamic_resource<vertex_buffer<T>>(location, N);
  ret.load(Arr);
  return ret;
}

template <typename T>
inline owner<index_buffer<T>> create_index_buffer(
    detail::ArrayProxy<T> data,
    const SourceLocation &location = SourceLocation::current()) noexcept {
  return create_resource<index_buffer<T>>(
      location, data.size(), [&](void *buf, std::size_t size) {
        std::memcpy(buf, data.data(), sizeof(T) * data.size());
      });
}

template <typename T, std::size_t N>
inline owner<index_buffer<T>> create_index_buffer(
    const std::array<T, N> &Arr,
    const SourceLocation &location = SourceLocation::current()) noexcept {
  return create_index_buffer(detail::ArrayProxy<T>(Arr), location);
}

template <typename T>
inline dynamic_owner<index_buffer<T>> create_dynamic_index_buffer(
    std::size_t Size,
    const SourceLocation &location = SourceLocation::current()) noexcept {
  return create_dynamic_resource<index_buffer<T>>(location, sizeof(T) * Size);
}

template <typename T>
inline dynamic_owner<index_buffer<T>> create_dynamic_index_buffer(
    detail::ArrayProxy<T> data,
    const SourceLocation &location = SourceLocation::current()) noexcept {
  auto ret = create_dynamic_resource<index_buffer<T>>(location, data.size());
  ret.load(data);
  return ret;
}

template <typename T, std::size_t N>
inline dynamic_owner<index_buffer<T>> create_dynamic_index_buffer(
    const std::array<T, N> &Arr,
    const SourceLocation &location = SourceLocation::current()) noexcept {
  auto ret = create_dynamic_resource<index_buffer<T>>(location, N);
  ret.load(Arr);
  return ret;
}

template <typename CopyFunc>
inline owner<texture2d> create_texture2d(
    extent2d extent, Format format, uint32_t numMips, CopyFunc copyFunc,
    ColorSwizzle redSwizzle = CS_Identity,
    ColorSwizzle greenSwizzle = CS_Identity,
    ColorSwizzle blueSwizzle = CS_Identity,
    ColorSwizzle alphaSwizzle = CS_Identity,
    const SourceLocation &location = SourceLocation::current()) noexcept {
  return create_resource<texture2d>(location, extent, format, numMips, copyFunc,
                                    redSwizzle, greenSwizzle, blueSwizzle,
                                    alphaSwizzle);
}

inline dynamic_owner<texture2d> create_dynamic_texture2d(
    extent2d extent, Format format, uint32_t numMips,
    ColorSwizzle redSwizzle = CS_Identity,
    ColorSwizzle greenSwizzle = CS_Identity,
    ColorSwizzle blueSwizzle = CS_Identity,
    ColorSwizzle alphaSwizzle = CS_Identity,
    const SourceLocation &location = SourceLocation::current()) noexcept {
  return create_dynamic_resource<texture2d>(location, extent, format, numMips,
                                            redSwizzle, greenSwizzle,
                                            blueSwizzle, alphaSwizzle);
}

template <typename CopyFunc>
inline owner<texture2d_array> create_texture2d_array(
    extent2d extent, uint32_t numLayers, Format format, uint32_t numMips,
    CopyFunc copyFunc, ColorSwizzle redSwizzle = CS_Identity,
    ColorSwizzle greenSwizzle = CS_Identity,
    ColorSwizzle blueSwizzle = CS_Identity,
    ColorSwizzle alphaSwizzle = CS_Identity,
    const SourceLocation &location = SourceLocation::current()) noexcept {
  return create_resource<texture2d_array>(
      location, extent, numLayers, format, numMips, copyFunc, redSwizzle,
      greenSwizzle, blueSwizzle, alphaSwizzle);
}

inline dynamic_owner<texture2d_array> create_dynamic_texture2d_array(
    extent2d extent, uint32_t numLayers, Format format, uint32_t numMips,
    ColorSwizzle redSwizzle = CS_Identity,
    ColorSwizzle greenSwizzle = CS_Identity,
    ColorSwizzle blueSwizzle = CS_Identity,
    ColorSwizzle alphaSwizzle = CS_Identity,
    const SourceLocation &location = SourceLocation::current()) noexcept {
  return create_dynamic_resource<texture2d_array>(
      location, extent, numLayers, format, numMips, redSwizzle, greenSwizzle,
      blueSwizzle, alphaSwizzle);
}

template <> struct owner<render_texture2d> {
  typename decltype(render_texture2d::Binding)::Owner Owner;

  owner() noexcept = default;
  explicit owner(decltype(Owner) Owner) noexcept : Owner(std::move(Owner)) {}

  owner(const owner &other) = delete;
  owner &operator=(const owner &other) = delete;
  owner(owner &&other) noexcept = default;
  owner &operator=(owner &&other) noexcept = default;

  render_texture2d get_color(uint32_t idx) const noexcept {
    return {Owner.GetColor(idx)};
  }
  render_texture2d get_depth(uint32_t idx) const noexcept {
    return {Owner.GetDepth(idx)};
  }
  void attach() noexcept { Owner.Attach(); }
  void attach(const viewport &vp) noexcept { Owner.Attach(vp); }
  void attach(const scissor &s) noexcept { Owner.Attach(s); }
  void attach(const viewport &vp, const scissor &s) noexcept {
    Owner.Attach(vp, s);
  }
  void resolve_surface(surface surface, bool reattach = false) noexcept {
    Owner.ResolveSurface(surface.Binding, reattach);
  }
  void resolve_color_binding(uint32_t idx, rect2d region,
                             bool reattach = true) noexcept {
    Owner.ResolveColorBinding(idx, region, reattach);
  }
  void resolve_depth_binding(uint32_t idx, rect2d region,
                             bool reattach = true) noexcept {
    Owner.ResolveDepthBinding(idx, region, reattach);
  }

  void reset() noexcept { Owner = decltype(Owner){}; }
};

inline owner<render_texture2d> create_render_texture2d(
    surface Surf, uint32_t NumColorBindings = 0, uint32_t NumDepthBindings = 0,
    const SourceLocation &location = SourceLocation::current()) noexcept {
  return create_resource<render_texture2d>(location, Surf, NumColorBindings,
                                           NumDepthBindings);
}

template <> struct owner<surface> : owner_base<surface> {
  using owner_base<surface>::owner_base;
  bool acquire_next_image() noexcept {
    return owner_base<surface>::Owner.AcquireNextImage();
  }
  void attach_resize_lambda(
      std::function<void(const hsh::extent2d &)> &&Resize) noexcept {
    owner_base<surface>::Owner.AttachResizeLambda(std::move(Resize));
  }
  void attach_deleter_lambda(std::function<void()> &&Del) noexcept {
    owner_base<surface>::Owner.AttachDeleterLambda(std::move(Del));
  }
  void set_request_extent(const hsh::extent2d &Ext) noexcept {
    owner_base<surface>::Owner.SetRequestExtent(Ext);
  }
};

#if HSH_ENABLE_VULKAN
inline owner<surface> create_surface(
    vk::UniqueSurfaceKHR &&Surface,
    std::function<void(const hsh::extent2d &)> &&ResizeLambda = {},
    std::function<void()> &&DeleterLambda = {},
    const hsh::extent2d &RequestExtent = {},
    const SourceLocation &location = SourceLocation::current()) noexcept {
  return create_resource<surface>(location, std::move(Surface),
                                  std::move(ResizeLambda),
                                  std::move(DeleterLambda), RequestExtent);
}
#endif

class binding {
  detail::ActiveTargetTraits::PipelineBinding Data;
  bool UpdateDescriptors = true;

public:
  template <class Binder, typename... Args>
  binding &_bind(Args... args) noexcept {
    Binder::Bind(*this, args...);
    return *this;
  }
  template <typename Impl, typename... Res>
  void _rebind(Res... Resources) noexcept {
    Data.Rebind<Impl, Res...>(UpdateDescriptors, Resources...);
    UpdateDescriptors = false;
  }

  operator bool() const noexcept { return Data.IsValid(); }
  binding() noexcept = default;
  void draw(uint32_t start, uint32_t count) noexcept {
    Data.Draw(start, count);
  }
  void draw_indexed(uint32_t start, uint32_t count) noexcept {
    Data.DrawIndexed(start, count);
  }
  void update_descriptors() noexcept { UpdateDescriptors = true; }
  void reset() noexcept { Data = decltype(Data){}; }
};

template <typename Impl> class binding_impl : public binding {
protected:
  template <typename... Args>
  explicit binding_impl(Args... args) noexcept
      : binding(detail::ClassWrapper<Impl>(), args...) {}
};

inline void clear_attachments(bool color = true, bool depth = true) noexcept {
  detail::ActiveTargetTraits::ClearAttachments(color, depth);
}

#if __hsh__
#define HSH_VAR_STAGE(stage) [[hsh::stage]]
#else
#define HSH_VAR_STAGE(stage)
#endif

} // namespace hsh

#include "bits/profile.h"
