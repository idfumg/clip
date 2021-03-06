/**
 * This file is part of the "clip" project
 *   Copyright (c) 2018 Paul Asmuth
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "bars.h"

#include "data.h"
#include "context.h"
#include "layout.h"
#include "graphics/path.h"
#include "graphics/brush.h"
#include "graphics/text.h"
#include "graphics/layout.h"
#include "scale.h"
#include "style.h"
#include "style_reader.h"
#include "color_reader.h"
#include "sexpr_conv.h"
#include "sexpr_util.h"

#include <numeric>

using namespace std::placeholders;
using std::bind;

namespace clip::plotgen {

static const double kDefaultBarSizePT = 10;
static const double kDefaultLabelPaddingHorizEM = 0.6;
static const double kDefaultLabelPaddingVertEM = 0.6;

struct PlotBarsConfig {
  PlotBarsConfig();
  Direction direction;
  DataBuffer x;
  DataBuffer xoffset;
  DataBuffer y;
  DataBuffer yoffset;
  ScaleConfig scale_x;
  ScaleConfig scale_y;
  StrokeStyle stroke_style;
  FillStyle fill_style;
  Number size;
  std::vector<Measure> sizes;
  Number offset;
  std::vector<Measure> offsets;
  std::vector<std::string> labels;
  FontInfo label_font;
  Measure label_padding;
  Number label_font_size;
  Color label_color;
};

PlotBarsConfig::PlotBarsConfig() :
    direction(Direction::VERTICAL) {}

ReturnCode bars_draw_horizontal(
    Context* ctx,
    PlotConfig* plot,
    PlotBarsConfig config) {
  const auto& clip = plot_get_clip(plot, layer_get(ctx));

  /* convert units */
  std::vector<double> xs;
  if (auto rc = scale_translatev(config.scale_x, config.x, &xs); !rc) {
    return rc;
  }

  std::vector<double> xoffsets;
  if (auto rc = scale_translatev(config.scale_x, config.xoffset, &xoffsets); !rc) {
    return rc;
  }

  std::vector<double> ys;
  if (auto rc = scale_translatev(config.scale_y, config.y, &ys); !rc) {
    return rc;
  }

  /* draw bars */
  auto x0 = std::clamp(scale_translate(config.scale_x, 0), 0.0, 1.0);
  for (size_t i = 0; i < xs.size(); ++i) {
    auto sy = clip.y + ys[i] * clip.h;
    auto sx1 = clip.x + (xoffsets.empty() ? x0 : xoffsets[i]) * clip.w;
    auto sx2 = clip.x + xs[i] * clip.w;

    auto size =
        config.size.value
            ? config.size.value
            : unit_from_pt(kDefaultBarSizePT, layer_get_dpi(ctx)).value;

    if (!config.sizes.empty()) {
      size = config.sizes[i % config.sizes.size()];
    }

    auto offset = config.offsets.empty()
        ? config.offset.value
        : config.offsets[i % config.offsets.size()];

    Path path;
    path.moveTo(sx1, sy + -offset - size * 0.5);
    path.lineTo(sx2, sy + -offset - size * 0.5);
    path.lineTo(sx2, sy + -offset + size * 0.5);
    path.lineTo(sx1, sy + -offset + size * 0.5);
    path.closePath();

    draw_path(ctx, path, config.stroke_style, config.fill_style);
  }

  /* draw labels */
  for (size_t i = 0; i < config.labels.size(); ++i) {
    const auto& text = config.labels[i];

    auto offset = config.offsets.empty()
        ? config.offset.value
        : config.offsets[i % config.offsets.size()];

    auto padding = measure_or(
        config.label_padding,
        from_em(kDefaultLabelPaddingHorizEM, config.label_font_size.value));

    Point p(
        clip.x + xs[i] * clip.w + padding,
        clip.y + -offset + ys[i] * clip.h);

    TextStyle style;
    style.font = config.label_font;
    style.color = config.label_color;
    style.font_size = config.label_font_size;

    auto ax = HAlign::LEFT;
    auto ay = VAlign::CENTER;
    if (auto rc = draw_text(ctx, text, p, ax, ay, style); rc != OK) {
      return rc;
    }
  }

  return OK;
}

ReturnCode bars_draw_vertical(
    Context* ctx,
    PlotConfig* plot,
    PlotBarsConfig config) {
  const auto& clip = plot_get_clip(plot, layer_get(ctx));

  /* transform data */
  std::vector<double> xs;
  if (auto rc = scale_translatev(config.scale_x, config.x, &xs); !rc) {
    return rc;
  }

  std::vector<double> ys;
  if (auto rc = scale_translatev(config.scale_y, config.y, &ys); !rc) {
    return rc;
  }

  std::vector<double> yoffsets;
  if (auto rc = scale_translatev(config.scale_y, config.yoffset, &yoffsets); !rc) {
    return rc;
  }

  /* draw bars */
  auto y0 = std::clamp(scale_translate(config.scale_y, 0), 0.0, 1.0);
  for (size_t i = 0; i < xs.size(); ++i) {
    auto sx = clip.x + xs[i] * clip.w;
    auto sy1 = clip.y + (yoffsets.empty() ? y0 : yoffsets[i]) * clip.h;
    auto sy2 = clip.y + ys[i] * clip.h;

    auto size =
        config.size.value
            ? config.size.value
            : unit_from_pt(kDefaultBarSizePT, layer_get_dpi(ctx)).value;

    if (!config.sizes.empty()) {
      size = config.sizes[i % config.sizes.size()];
    }

    auto offset = config.offsets.empty()
        ? config.offset.value
        : config.offsets[i % config.offsets.size()];

    Path path;
    path.moveTo(sx + offset - size * 0.5, sy1);
    path.lineTo(sx + offset - size * 0.5, sy2);
    path.lineTo(sx + offset + size * 0.5, sy2);
    path.lineTo(sx + offset + size * 0.5, sy1);
    path.closePath();

    draw_path(ctx, path, config.stroke_style, config.fill_style);
  }

  /* draw labels */
  for (size_t i = 0; i < config.labels.size(); ++i) {
    const auto& text = config.labels[i];

    auto offset = config.offsets.empty()
        ? config.offset.value
        : config.offsets[i % config.offsets.size()];

    auto padding = measure_or(
        config.label_padding,
        from_em(kDefaultLabelPaddingVertEM, config.label_font_size.value));

    Point p(
        clip.x + offset + xs[i] * clip.w,
        clip.y + ys[i] * clip.h + padding);

    TextStyle style;
    style.font = config.label_font;
    style.color = config.label_color;
    style.font_size = config.label_font_size;

    auto ax = HAlign::CENTER;
    auto ay = VAlign::BOTTOM;
    if (auto rc = draw_text(ctx, text, p, ax, ay, style); rc != OK) {
      return rc;
    }
  }

  return OK;
}

ReturnCode bars_configure(
    Context* ctx,
    PlotConfig* plot,
    PlotBarsConfig* c,
    const Expr* expr) {
  const auto& layer = *layer_get(ctx);

  /* set defaults from environment */
  c->scale_x = plot->scale_x;
  c->scale_y = plot->scale_y;
  c->stroke_style.color = layer_get(ctx)->foreground_color;
  c->stroke_style.line_width = Number(0);
  c->fill_style.color = layer_get(ctx)->foreground_color;
  c->label_font = layer_get_font(ctx);
  c->label_font_size = layer_get_font_size(ctx);

  /* parse properties */
  auto config_rc = expr_walk_map(expr, {
    {"data-x", std::bind(&data_load_simple, _1, &c->x)},
    {"data-y", std::bind(&data_load_simple, _1, &c->y)},
    {"data-x-high", std::bind(&data_load_simple, _1, &c->x)},
    {"data-y-high", std::bind(&data_load_simple, _1, &c->y)},
    {"data-x-low", std::bind(&data_load_simple, _1, &c->xoffset)},
    {"data-y-low", std::bind(&data_load_simple, _1, &c->yoffset)},
    {"width", std::bind(&expr_to_size, _1, layer, &c->size)},
    {"widths", std::bind(&data_load, _1, &c->sizes)},
    {"offset", std::bind(&expr_to_size, _1, layer, &c->offset)},
    {"offsets", std::bind(&data_load, _1, &c->offsets)},
    {"stroke-color", std::bind(&color_read, ctx, _1, &c->stroke_style.color)},
    {"stroke-width", std::bind(&expr_to_size, _1, layer, &c->stroke_style.line_width)},
    {"stroke-style", std::bind(&stroke_style_read, ctx, _1, &c->stroke_style)},
    {"fill", std::bind(&fill_style_read, ctx, _1, &c->fill_style)},
    {"limit-x", std::bind(&expr_to_float64_opt_pair, _1, &c->scale_x.min, &c->scale_x.max)},
    {"limit-x-min", std::bind(&expr_to_float64_opt, _1, &c->scale_x.min)},
    {"limit-x-max", std::bind(&expr_to_float64_opt, _1, &c->scale_x.max)},
    {"limit-y", std::bind(&expr_to_float64_opt_pair, _1, &c->scale_y.min, &c->scale_y.max)},
    {"limit-y-min", std::bind(&expr_to_float64_opt, _1, &c->scale_y.min)},
    {"limit-y-max", std::bind(&expr_to_float64_opt, _1, &c->scale_y.max)},
    {"scale-x", std::bind(&scale_configure_kind, _1, &c->scale_x)},
    {"scale-y", std::bind(&scale_configure_kind, _1, &c->scale_y)},
    {"scale-x-padding", std::bind(&expr_to_float64, _1, &c->scale_x.padding)},
    {"scale-y-padding", std::bind(&expr_to_float64, _1, &c->scale_y.padding)},
    {
      "color",
      expr_calln_fn({
        std::bind(&color_read, ctx, _1, &c->stroke_style.color),
        std::bind(&fill_style_read_solid, ctx, _1, &c->fill_style),
      })
    },
    {
      "direction",
      expr_to_enum_fn<Direction>(&c->direction, {
        { "horizontal", Direction::HORIZONTAL },
        { "vertical", Direction::VERTICAL },
      })
    },
    {"labels", std::bind(&data_load_strings, _1, &c->labels)},
    {"label-font-size", std::bind(&expr_to_font_size, _1, layer, &c->label_font_size)},
    {"label-color", std::bind(&color_read, ctx, _1, &c->label_color)},
    {"label-padding", std::bind(&measure_read, _1, &c->label_padding)},
  });

  if (!config_rc) {
    return config_rc;
  }

  /* check configuraton */
  if (databuf_len(c->x) != databuf_len(c->y)) {
    return error(ERROR, "The length of the 'data-x' and 'data-y' lists must be equal");
  }

  if (databuf_len(c->xoffset) != 0 &&
      databuf_len(c->x) != databuf_len(c->xoffset)) {
    return error(ERROR, "the length of the 'data-x' and 'data-x-low' properties must be equal");
  }

  if (databuf_len(c->yoffset) != 0 &&
      databuf_len(c->y) != databuf_len(c->yoffset)) {
    return error(ERROR, "the length of the 'data-y' and 'data-y-low' properties must be equal");
  }

  return OK;
}

ReturnCode bars_draw(
    Context* ctx,
    PlotConfig* plot,
    const Expr* expr) {
  PlotBarsConfig conf;

  if (auto rc = bars_configure(ctx, plot, &conf, expr); !rc) {
    return rc;
  }

  switch (conf.direction) {
    case Direction::HORIZONTAL:
      return bars_draw_horizontal(ctx, plot, conf);
    case Direction::VERTICAL:
      return bars_draw_vertical(ctx, plot, conf);
    default:
      return ERROR;
  }
}

ReturnCode bars_autorange(
    Context* ctx,
    PlotConfig* plot,
    const Expr* expr) {
  PlotBarsConfig conf;
  if (auto rc = bars_configure(ctx, plot, &conf, expr); !rc) {
    return rc;
  }

  if (auto rc = scale_fit(&plot->scale_x, conf.x); !rc) {
    return rc;
  }

  if (auto rc = scale_fit(&plot->scale_x, conf.xoffset); !rc) {
    return rc;
  }

  if (auto rc = scale_fit(&plot->scale_y, conf.y); !rc) {
    return rc;
  }

  if (auto rc = scale_fit(&plot->scale_y, conf.yoffset); !rc) {
    return rc;
  }

  return OK;
}

} // namespace clip::plotgen

