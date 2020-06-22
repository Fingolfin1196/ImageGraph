#pragma once

#include "../core/SizedArray.hpp"
#include "typing/NumberTraits.hpp"
#include <functional>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_vector.h>
#include <memory>
#include <stdexcept>
#include <vector>

namespace ImageGraph::internal {
namespace {
/**
 * The two immediate side-diagonals are 1.
 */
template<typename T, typename RandomAccessYIterator> requires is_floating_point_v<T> void
solveTridiagonalSystem(const RandomAccessYIterator& y, T* x, T* one_alpha, T* z, std::size_t N) {
  constexpr T _1{1}, _2{2}, _4{4}, p25{.25};

  // Update RHS
  one_alpha[0] = p25;
  z[0] = y[2] - _2 * y[1] + y[0];
  for (std::size_t i{1}; i < N; ++i) {
    one_alpha[i] = _1 / (_4 - one_alpha[i - 1]);
    z[i] = y[i + 2] - _2 * y[i + 1] + y[i] - z[i - 1] * one_alpha[i];
  }

  // Backsubstitution
  x[N - 1] = z[N - 1] * one_alpha[N - 1];
  if (N >= 2)
    for (std::size_t i{N - 2}, j{0}; j < N - 1; ++j, --i) x[i] = (z[i] - x[i + 1]) * one_alpha[i];
}

/**
 * This implementation is based on the script to Engineering Mathematics III,
 * assuming x = {0, 1, …, size} (of std::size_t), which implies h[i] = 1 for all i.
 * Here, M is set to 6M in comparison to the script.
 */
template<typename OutputType, typename RandomAccessYIterator> class CSpline {
  std::optional<RandomAccessYIterator> y_{};
  const std::size_t size_;
  SizedArray<OutputType> m_, one_alpha_, z_;

public:
  CSpline(std::size_t size) : size_{size}, m_(size), one_alpha_(size > 2 ? size - 2 : 0), z_(size > 2 ? size - 2 : 0) {
    DEBUG_PREVENT(std::invalid_argument, size == 0, "You cannot form a spline with no points!");
    DEBUG_PREVENT(std::invalid_argument, size == 1,
                  "A spline with only one point is not useful and leads to nastiness!");
  }

  void initialise(RandomAccessYIterator y) {
    constexpr OutputType _0{0}, _2{2}, p25{0.25};

    y_.emplace(std::move(y));
    const std::size_t max_index{size_ - 1}, sys_size{max_index - 1};

    m_[0] = _0, m_[max_index] = _0;

    if (sys_size == 1)
      m_[1] = p25 * (y[2] - _2 * y[1] + y[0]);
    else
      solveTridiagonalSystem<OutputType>(y, m_.data() + 1, one_alpha_.data(), z_.data(), sys_size);
  }

  OutputType derivative(OutputType x) {
    constexpr OutputType _3{3};
    const auto& y{*y_};

    // Evaluate
    const std::size_t index{std::clamp<size_t>(x, 0, size_ - 2)};
    const OutputType m_low{m_[index]}, m_high{m_[index + 1]}, xsub{x - index}, subx{1 - xsub};
    return _3 * (xsub * xsub * m_high - subx * subx * m_low) + y[index + 1] - y[index] - m_high + m_low;
  }
};
} // namespace

/**
 * This implementation assumes a regular grid with distance 1, where the coordinates are given as
 * std::size_t.
 */
template<typename InputType, typename OutputType, template<typename> typename TileType> class BicubicInterpolator {
  using convert_t = std::function<OutputType(InputType)>;

  const convert_t converter_;
  const TileType<InputType>& tile_;
  TileType<OutputType> zx_, zy_, zxy_;

  class TwoToOneInputRowIterator {
    const convert_t& converter_;
    const std::size_t j_, k_;
    const TileType<InputType>& tile_;

  public:
    TwoToOneInputRowIterator(std::size_t j, std::size_t k, const TileType<InputType>& tile, const convert_t& converter)
        : converter_{converter}, j_{j}, k_{k}, tile_{tile} {}

    OutputType operator[](std::size_t i) const { return converter_(tile_(i, j_, k_)); }
  };
  class TwoToOneInputColIterator {
    const convert_t& converter_;
    const std::size_t i_, k_;
    const TileType<InputType>& tile_;

  public:
    TwoToOneInputColIterator(std::size_t i, std::size_t k, const TileType<InputType>& tile, const convert_t& converter)
        : converter_{converter}, i_{i}, k_{k}, tile_{tile} {}

    OutputType operator[](std::size_t j) const { return converter_(tile_(i_, j, k_)); }
  };
  class TwoToOneOutputRowIterator {
    const std::size_t j_, k_;
    const TileType<OutputType>& tile_;

  public:
    TwoToOneOutputRowIterator(std::size_t j, std::size_t k, const TileType<OutputType>& tile)
        : j_{j}, k_{k}, tile_{tile} {}

    OutputType operator[](std::size_t i) const { return tile_(i, j_, k_); }
  };

public:
  BicubicInterpolator(const TileType<InputType>& tile, convert_t converter)
      : converter_{std::move(converter)}, tile_{tile}, zx_{tile.rectangle(), tile.channels()},
        zy_{tile.rectangle(), tile.channels()}, zxy_{tile.rectangle(), tile.channels()} {
    DEBUG_PREVENT(std::invalid_argument, tile.empty(), "I cannot interpolate on an empty rectangle!");
    DEBUG_PREVENT(std::invalid_argument, tile.width() == 1 or tile.height() == 1,
                  "A rectangle with width or hight 1 might make sense, but leads to nastiness!");

    const std::size_t width{tile.width()}, height{tile.height()}, channels{tile.channels()};

    {
      CSpline<OutputType, TwoToOneInputRowIterator> spline{width};
      for (std::size_t j{0}; j < height; ++j) {
        for (std::size_t k{0}; k < channels; ++k) {
          spline.initialise(TwoToOneInputRowIterator(j, k, tile_, converter_));
          for (std::size_t i{0}; i < width; ++i) zx_(i, j, k) = spline.derivative(i);
        }
      }
    }

    {
      CSpline<OutputType, TwoToOneInputColIterator> spline{height};
      for (std::size_t i{0}; i < width; ++i) {
        for (std::size_t k{0}; k < channels; ++k) {
          spline.initialise(TwoToOneInputColIterator(i, k, tile_, converter_));
          for (std::size_t j{0}; j < height; ++j) zy_(i, j, k) = spline.derivative(j);
        }
      }
    }

    {
      CSpline<OutputType, TwoToOneOutputRowIterator> spline{width};
      for (std::size_t j{0}; j < height; ++j) {
        for (std::size_t k{0}; k < channels; ++k) {
          spline.initialise(TwoToOneOutputRowIterator(j, k, zy_));
          for (std::size_t i{0}; i < width; ++i) zxy_(i, j, k) = spline.derivative(i);
        }
      }
    }
  }

  OutputType evaluate(OutputType x, OutputType y, std::size_t channel) const {
    constexpr OutputType _2{2}, _3{3}, _4{4}, _6{6}, _9{9};

    // First compute the indices into the data arrays where we are interpolating
    const std::size_t xi{std::clamp<std::size_t>(x, 0, tile_.width() - 2)},
        yi{std::clamp<std::size_t>(y, 0, tile_.height() - 2)};

    // Find the minimum and maximum values on the grid cell.
    const OutputType zminmin{converter_(tile_(xi, yi, channel))}, zminmax{converter_(tile_(xi, yi + 1, channel))},
        zmaxmin{converter_(tile_(xi + 1, yi, channel))}, zmaxmax{converter_(tile_(xi + 1, yi + 1, channel))};
    const OutputType zxminmin{zx_(xi, yi, channel)}, zxminmax{zx_(xi, yi + 1, channel)},
        zxmaxmin{zx_(xi + 1, yi, channel)}, zxmaxmax{zx_(xi + 1, yi + 1, channel)};
    const OutputType zyminmin{zy_(xi, yi, channel)}, zyminmax{zy_(xi, yi + 1, channel)},
        zymaxmin{zy_(xi + 1, yi, channel)}, zymaxmax{zy_(xi + 1, yi + 1, channel)};
    const OutputType zxyminmin{zxy_(xi, yi, channel)}, zxyminmax{zxy_(xi, yi + 1, channel)},
        zxymaxmin{zxy_(xi + 1, yi, channel)}, zxymaxmax{zxy_(xi + 1, yi + 1, channel)};
    const OutputType t{x - xi}, t2{t * t}, t3{t * t2}, u{y - yi}, u2{u * u}, u3{u * u2};

    return zminmin + zyminmin * u + (-_3 * zminmin + _3 * zminmax - _2 * zyminmin - zyminmax) * u2 +
           (_2 * zminmin - _2 * zminmax + zyminmin + zyminmax) * u3 + zxminmin * t + zxyminmin * t * u +
           (-_3 * zxminmin + _3 * zxminmax - _2 * zxyminmin - zxyminmax) * t * u2 +
           (_2 * zxminmin - _2 * zxminmax + zxyminmin + zxyminmax) * t * u3 +
           (-_3 * zminmin + _3 * zmaxmin - _2 * zxminmin - zxmaxmin) * t2 +
           (-_3 * zyminmin + _3 * zymaxmin - _2 * zxyminmin - zxymaxmin) * t2 * u +
           (_9 * zminmin - _9 * zmaxmin + _9 * zmaxmax - _9 * zminmax + _6 * zxminmin + _3 * zxmaxmin - _3 * zxmaxmax -
            _6 * zxminmax + _6 * zyminmin - _6 * zymaxmin - _3 * zymaxmax + _3 * zyminmax + _4 * zxyminmin +
            _2 * zxymaxmin + zxymaxmax + _2 * zxyminmax) *
               t2 * u2 +
           (-_6 * zminmin + _6 * zmaxmin - _6 * zmaxmax + _6 * zminmax - _4 * zxminmin - _2 * zxmaxmin + _2 * zxmaxmax +
            _4 * zxminmax - _3 * zyminmin + _3 * zymaxmin + _3 * zymaxmax - _3 * zyminmax - _2 * zxyminmin - zxymaxmin -
            zxymaxmax - _2 * zxyminmax) *
               t2 * u3 +
           (_2 * zminmin - _2 * zmaxmin + zxminmin + zxmaxmin) * t3 +
           (_2 * zyminmin - _2 * zymaxmin + zxyminmin + zxymaxmin) * t3 * u +
           (-_6 * zminmin + _6 * zmaxmin - _6 * zmaxmax + _6 * zminmax - _3 * zxminmin - _3 * zxmaxmin + _3 * zxmaxmax +
            _3 * zxminmax - _4 * zyminmin + _4 * zymaxmin + _2 * zymaxmax - _2 * zyminmax - _2 * zxyminmin -
            _2 * zxymaxmin - zxymaxmax - zxyminmax) *
               t3 * u2 +
           (_4 * zminmin - _4 * zmaxmin + _4 * zmaxmax - _4 * zminmax + _2 * zxminmin + _2 * zxmaxmin - _2 * zxmaxmax -
            _2 * zxminmax + _2 * zyminmin - _2 * zymaxmin - _2 * zymaxmax + _2 * zyminmax + zxyminmin + zxymaxmin +
            zxymaxmax + zxyminmax) *
               t3 * u3;
  }
};
} // namespace ImageGraph::internal
