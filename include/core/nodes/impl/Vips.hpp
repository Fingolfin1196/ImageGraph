#pragma once

#include "../../../internal/Typing.hpp"
#include "../../Definitions.hpp"
#include "../TiledInputOutputNode.hpp"
#include <cstdlib>
#include <memory>
#include <vips/vips8>

namespace ImageGraph::nodes {
class UniqueVipsWrap {
  /**
   * Can ONLY be modified by the move constructor!
   */
  VipsImage* image_;

  UniqueVipsWrap(VipsImage* image) : image_{image} {}

public:
  static UniqueVipsWrap loadFromFile(const std::string& path) {
    return UniqueVipsWrap(vips_image_new_from_file(path.c_str(), nullptr));
  }

  UniqueVipsWrap(UniqueVipsWrap&& other) : image_{other.image_} { other.image_ = nullptr; }
  UniqueVipsWrap(const UniqueVipsWrap&) = delete;

  ~UniqueVipsWrap() {
    if (image_) g_object_unref(image_);
  }

  bool isValid() const { return image_; }

  const VipsImage& image() const { return *image_; }
  /**
   * WARNING This ONLY exists for compatibility reasons and should ONLY be used
   * when any new references are temporary and removed before continuing!
   */
  VipsImage& mutableImage() { return *image_; }

  int width() const { return vips_image_get_width(image_); }
  int height() const { return vips_image_get_height(image_); }
  int bands() const { return vips_image_get_bands(image_); }
  VipsBandFormat format() const { return vips_image_get_format(image_); }
};

#define __UNIQUE_VIPS_INIT(IMAGE, MEMORY_MODE, TYPE) \
  OutNode({std::size_t(IMAGE.width()), std::size_t(IMAGE.height())}, IMAGE.bands(), 0, MEMORY_MODE, typeid(TYPE))
#define __UNIQUE_VIPS_INIT_DEFAULT() __UNIQUE_VIPS_INIT(image, internal::MemoryMode::FULL_MEMORY, OutputType)

template<typename OutputType> struct VipsInputOutputNode : public TiledInputOutputNode<OutputType> {
  using dimensions_t = Node::dimensions_t;
  using rectangle_t = Node::rectangle_t;
  using input_index_t = Node::input_index_t;
  using duration_t = OutNode::duration_t;

protected:
  mutable UniqueVipsWrap vips_image_;

  OutNode::duration_t tileDuration(Node::rectangle_t) const override { return {}; }
  void updateTileDuration(OutNode::duration_t, Node::rectangle_t) const override {}
  std::size_t cacheSizeFromBytes(std::size_t) const override { return 0; }

  void setCacheSize(std::size_t) const override {}
  std::unique_ptr<OutNode::proto_cache_t> createProtoCache() const override { return nullptr; }

  rectangle_t rawInputRegion(input_index_t, rectangle_t) const final {
    throw std::invalid_argument("There are no inputs!");
  }

  std::ostream& print(std::ostream& stream) const override {
    stream << "[VipsOutputNode<" << internal::type_name<OutputType>() << ">(vips_image=";
    {
      char image_description[1024];
      VipsBuf buffer(VIPS_BUF_STATIC(image_description));
      vips_object_to_string((VipsObject*)(&vips_image_.image()), &buffer);
      stream << vips_buf_all(&buffer);
    }
    return stream << ") @ " << this << "]";
  }

public:
  const VipsImage& getVipsImage() const { return vips_image_.image(); }
  bool isValid() const { return vips_image_.isValid(); }

  void compute(std::tuple<>, Tile<OutputType>& output) const final {
    const auto left{output.left()}, top{output.top()}, width{output.width()}, height{output.height()};

    VipsRegion* region{vips_region_new(&vips_image_.mutableImage())};
    VipsRect rect{int(left), int(top), int(width), int(height)};
    vips_region_prepare(region, &rect);

    const std::size_t skip{VIPS_REGION_LSKIP(region) / sizeof(OutputType)}, line{width * output.channels()},
        copy_line{line * sizeof(OutputType)};
    OutputType *p{reinterpret_cast<OutputType*>(VIPS_REGION_ADDR(region, left, top))}, *q{output.data()};
    for (std::size_t y{0}; y < height; ++y) {
      std::memcpy(q, p, copy_line);
      p += skip, q += line;
    }
    g_object_unref(region);
  }

  VipsInputOutputNode(UniqueVipsWrap&& image) : __UNIQUE_VIPS_INIT_DEFAULT(), vips_image_{std::move(image)} {
    DEBUG_ASSERT_S(std::invalid_argument, vips_image_.format() == internal::band_format_v<OutputType>,
                   "The created VImage has type ", vips_image_.format(), ", which is different from the output type ",
                   internal::band_format_v<OutputType>, "!");
  }
};

template<typename OutputType> class LoadNode final : public VipsInputOutputNode<OutputType> {
  std::string path_;
  LoadNode(UniqueVipsWrap&& image, std::string&& path)
      : __UNIQUE_VIPS_INIT_DEFAULT(), VipsInputOutputNode<OutputType>(std::move(image)), path_{std::move(path)} {}

  std::ostream& print(std::ostream& stream) const final {
    return stream << "[LoadNode<" << internal::type_name<OutputType>() << ">(path=\"" << path_ << "\") @ " << this
                  << "]";
  }

public:
  LoadNode(std::string path) : LoadNode(UniqueVipsWrap::loadFromFile(path), std::move(path)) {}
};
} // namespace ImageGraph::nodes
