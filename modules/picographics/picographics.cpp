#include "drivers/dvhstx/dvhstx.hpp"
#include "libraries/pico_graphics/pico_graphics_dvhstx.hpp"
#include "common/pimoroni_common.hpp"

#include "micropython/modules/util.hpp"

using namespace pimoroni;

extern "C" {
#include "picographics.h"
#include "pimoroni_i2c.h"
#include "py/stream.h"
#include "py/reader.h"
#include "py/objarray.h"
#include "extmod/vfs.h"

const std::string_view mp_obj_to_string_r(const mp_obj_t &obj) {
    if(mp_obj_is_str_or_bytes(obj)) {
        GET_STR_DATA_LEN(obj, str, str_len);
        return std::string_view((const char*)str, str_len);
    }
    mp_raise_TypeError("can't convert object to str implicitly");
}

void __printf_debug_flush() {
    for(auto i = 0u; i < 10; i++) {
        sleep_ms(2);
        mp_event_handle_nowait();
    }
}

int mp_vprintf(const mp_print_t *print, const char *fmt, va_list args);

void dvhstx_debug(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = mp_vprintf(&mp_plat_print, fmt, ap);
    va_end(ap);
    __printf_debug_flush();
    (void)ret;
}

static DVHSTX dv_display;

typedef struct _ModPicoGraphics_obj_t {
    mp_obj_base_t base;
    PicoGraphicsDVHSTX *graphics;
    DVHSTX *display;
} ModPicoGraphics_obj_t;

size_t get_required_buffer_size(PicoGraphicsPenType pen_type, uint width, uint height) {
    switch(pen_type) {
        //case PEN_RGB888:
            //return PicoGraphics_PenDVHSTX_RGB888::buffer_size(width, height);
        case PEN_RGB565:
            return PicoGraphics_PenDVHSTX_RGB565::buffer_size(width, height);
        case PEN_P8:
            return PicoGraphics_PenDVHSTX_P8::buffer_size(width, height);
        default:
            return 0;
    }
}

mp_obj_t ModPicoGraphics_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    ModPicoGraphics_obj_t *self = nullptr;

    enum { ARG_pen_type, ARG_width, ARG_height };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_pen_type, MP_ARG_INT, { .u_int = PEN_P8 } },
        { MP_QSTR_width, MP_ARG_INT, { .u_int = 320 } },
        { MP_QSTR_height, MP_ARG_INT, { .u_int = 240 } }
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    self = mp_obj_malloc_with_finaliser(ModPicoGraphics_obj_t, &ModPicoGraphics_type);
    self->base.type = &ModPicoGraphics_type;

    bool status = false;
    int width = args[ARG_width].u_int;
    int height = args[ARG_height].u_int;
    int pen_type = args[ARG_pen_type].u_int;

    dvhstx_debug("DVHSTX create display\n");

    // Create an instance of the graphics library and DV display driver
    switch((PicoGraphicsPenType)pen_type) {
        case PEN_RGB888:
            //self->graphics = m_new_class(PicoGraphics_PenDVHSTX_RGB888, width, height, dv_display);
            //status = dv_display.init(width, height, DVHSTX::MODE_RGB888);
            break;
        case PEN_RGB565:
            self->graphics = m_new_class(PicoGraphics_PenDVHSTX_RGB565, width, height, dv_display);
            status = dv_display.init(width, height, DVHSTX::MODE_RGB565);
            break;
        case PEN_P8:
            self->graphics = m_new_class(PicoGraphics_PenDVHSTX_P8, width, height, dv_display);
            status = dv_display.init(width, height, DVHSTX::MODE_PALETTE);
            break;
        default:
            break;
    }

    if (!status) {
        mp_raise_msg(&mp_type_RuntimeError, "PicoVision: Unsupported Mode!");
    }

    dvhstx_debug("DVHSTX created\n");

    self->display = &dv_display;

    // Clear each buffer
    for(auto x = 0u; x < 2u; x++){
        self->graphics->set_pen(0);
        self->graphics->clear();
        dv_display.flip_now();
    }

    return MP_OBJ_FROM_PTR(self);

}

mp_obj_t ModPicoGraphics__del__(mp_obj_t self_in) {
    //dv_display.reset();
    return mp_const_none;
}

mp_obj_t ModPicoGraphics_set_font(mp_obj_t self_in, mp_obj_t font) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);
    self->graphics->set_font(mp_obj_to_string_r(font));
    return mp_const_none;
}

mp_obj_t ModPicoGraphics_get_bounds(mp_obj_t self_in) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);
    mp_obj_t tuple[2] = {
        mp_obj_new_int(self->graphics->bounds.w),
        mp_obj_new_int(self->graphics->bounds.h)
    };
    return mp_obj_new_tuple(2, tuple);
}

mp_obj_t ModPicoGraphics_update(mp_obj_t self_in) {
    (void)self_in;
    dv_display.flip_blocking();
    return mp_const_none;
}

mp_obj_t ModPicoGraphics_module_RGB332_to_RGB(mp_obj_t rgb332) {
    RGB c((RGB332)mp_obj_get_int(rgb332));
    mp_obj_t t[] = {
        mp_obj_new_int(c.r),
        mp_obj_new_int(c.g),
        mp_obj_new_int(c.b),
    };
    return mp_obj_new_tuple(3, t);
}

mp_obj_t ModPicoGraphics_module_RGB565_to_RGB(mp_obj_t rgb565) {
    RGB c((RGB565)mp_obj_get_int(rgb565));
    mp_obj_t t[] = {
        mp_obj_new_int(c.r),
        mp_obj_new_int(c.g),
        mp_obj_new_int(c.b),
    };
    return mp_obj_new_tuple(3, t);
}

mp_obj_t ModPicoGraphics_module_RGB_to_RGB332(mp_obj_t r, mp_obj_t g, mp_obj_t b) {
    return mp_obj_new_int(RGB(
        mp_obj_get_int(r),
        mp_obj_get_int(g),
        mp_obj_get_int(b)
    ).to_rgb332());
}

mp_obj_t ModPicoGraphics_module_RGB_to_RGB565(mp_obj_t r, mp_obj_t g, mp_obj_t b) {
    return mp_obj_new_int(RGB(
        mp_obj_get_int(r),
        mp_obj_get_int(g),
        mp_obj_get_int(b)
    ).to_rgb565());
}

mp_obj_t ModPicoGraphics_set_pen(mp_obj_t self_in, mp_obj_t pen) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->graphics->set_pen(mp_obj_get_int(pen));

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_set_bg(mp_obj_t self_in, mp_obj_t pen) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->graphics->set_bg(mp_obj_get_int(pen));

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_set_blend_mode(mp_obj_t self_in, mp_obj_t pen) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->graphics->set_blend_mode((BlendMode)mp_obj_get_int(pen));

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_set_depth(mp_obj_t self_in, mp_obj_t depth) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->graphics->set_depth(mp_obj_get_int(depth));

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_reset_pen(mp_obj_t self_in, mp_obj_t pen) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->graphics->reset_pen(mp_obj_get_int(pen));

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_update_pen(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_i, ARG_r, ARG_g, ARG_b };

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self], ModPicoGraphics_obj_t);

    self->graphics->update_pen(
        mp_obj_get_int(args[ARG_i]) & 0xff,
        mp_obj_get_int(args[ARG_r]) & 0xff,
        mp_obj_get_int(args[ARG_g]) & 0xff,
        mp_obj_get_int(args[ARG_b]) & 0xff
    );

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_create_pen(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_r, ARG_g, ARG_b };

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self], ModPicoGraphics_obj_t);

    int result = self->graphics->create_pen(
        mp_obj_get_int(args[ARG_r]) & 0xff,
        mp_obj_get_int(args[ARG_g]) & 0xff,
        mp_obj_get_int(args[ARG_b]) & 0xff
    );

    if (result == -1) mp_raise_ValueError("create_pen failed. No matching colour or space in palette!");

    return mp_obj_new_int(result);
}

mp_obj_t ModPicoGraphics_create_pen_hsv(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_h, ARG_s, ARG_v };

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self], ModPicoGraphics_obj_t);
    int result = self->graphics->create_pen_hsv(
        mp_obj_get_float(args[ARG_h]),
        mp_obj_get_float(args[ARG_s]),
        mp_obj_get_float(args[ARG_v])
    );

    if (result == -1) mp_raise_ValueError("create_pen failed. No matching colour or space in palette!");

    return mp_obj_new_int(result);
}

mp_obj_t ModPicoGraphics_set_thickness(mp_obj_t self_in, mp_obj_t pen) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->graphics->set_thickness(mp_obj_get_int(pen));

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_set_palette(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    size_t num_tuples = n_args - 1;
    const mp_obj_t *tuples = pos_args + 1;

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(pos_args[0], ModPicoGraphics_obj_t);

    // Check if there is only one argument, which might be a list
    if(n_args == 2) {
        if(mp_obj_is_type(pos_args[1], &mp_type_list)) {
            mp_obj_list_t *points = MP_OBJ_TO_PTR2(pos_args[1], mp_obj_list_t);

            if(points->len <= 0) mp_raise_ValueError("set_palette(): cannot provide an empty list");

            num_tuples = points->len;
            tuples = points->items;
        }
        else {
            mp_raise_TypeError("set_palette(): can't convert object to list");
        }
    }

    for(size_t i = 0; i < num_tuples; i++) {
        mp_obj_t obj = tuples[i];
        if(!mp_obj_is_type(obj, &mp_type_tuple)) mp_raise_ValueError("set_palette(): can't convert object to tuple");

        mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR2(obj, mp_obj_tuple_t);

        if(tuple->len != 3) mp_raise_ValueError("set_palette(): tuple must contain R, G, B values");

        self->graphics->update_pen(
            i,
            mp_obj_get_int(tuple->items[0]),
            mp_obj_get_int(tuple->items[1]),
            mp_obj_get_int(tuple->items[2])
        );
    }

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_set_clip(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_x, ARG_y, ARG_w, ARG_h };

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self], ModPicoGraphics_obj_t);

    self->graphics->set_clip({
        mp_obj_get_int(args[ARG_x]),
        mp_obj_get_int(args[ARG_y]),
        mp_obj_get_int(args[ARG_w]),
        mp_obj_get_int(args[ARG_h])
    });

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_remove_clip(mp_obj_t self_in) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->graphics->remove_clip();

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_clear(mp_obj_t self_in) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->graphics->clear();

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_pixel(mp_obj_t self_in, mp_obj_t x, mp_obj_t y) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->graphics->pixel({
        mp_obj_get_int(x),
        mp_obj_get_int(y)
    });

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_pixel_span(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_x, ARG_y, ARG_l };

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self], ModPicoGraphics_obj_t);

    self->graphics->pixel_span({
        mp_obj_get_int(args[ARG_x]),
        mp_obj_get_int(args[ARG_y])
    },  mp_obj_get_int(args[ARG_l]));

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_rectangle(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_x, ARG_y, ARG_w, ARG_h };

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self], ModPicoGraphics_obj_t);

    self->graphics->rectangle({
        mp_obj_get_int(args[ARG_x]),
        mp_obj_get_int(args[ARG_y]),
        mp_obj_get_int(args[ARG_w]),
        mp_obj_get_int(args[ARG_h])
    });

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_circle(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_x, ARG_y, ARG_r };

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self], ModPicoGraphics_obj_t);

    self->graphics->circle({
        mp_obj_get_int(args[ARG_x]),
        mp_obj_get_int(args[ARG_y])
    },  mp_obj_get_int(args[ARG_r]));

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_character(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_self, ARG_char, ARG_x, ARG_y, ARG_scale };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_char, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_x, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_y, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_scale, MP_ARG_INT, {.u_int = 2} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self].u_obj, ModPicoGraphics_obj_t);

    int c = mp_obj_get_int(args[ARG_char].u_obj);
    int x = args[ARG_x].u_int;
    int y = args[ARG_y].u_int;
    int scale = args[ARG_scale].u_int;

    self->graphics->character((char)c, Point(x, y), scale);

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_text(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_self, ARG_text, ARG_x, ARG_y, ARG_wrap, ARG_scale, ARG_angle, ARG_spacing, ARG_fixed_width };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_text, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_x1, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_y1, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_wordwrap, MP_ARG_INT, {.u_int = __INT32_MAX__} },  // Sort-of a fudge, but wide-enough to avoid wrapping on any display size
        { MP_QSTR_scale, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_angle, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_spacing, MP_ARG_INT, {.u_int = 1} },
        { MP_QSTR_fixed_width, MP_ARG_OBJ, {.u_obj = mp_const_false} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self].u_obj, ModPicoGraphics_obj_t);

    mp_obj_t text_obj = args[ARG_text].u_obj;

    if(!mp_obj_is_str_or_bytes(text_obj)) mp_raise_TypeError("text: string required");

    GET_STR_DATA_LEN(text_obj, str, str_len);

    const std::string_view t((const char*)str, str_len);

    int x = args[ARG_x].u_int;
    int y = args[ARG_y].u_int;
    int wrap = args[ARG_wrap].u_int;
    float scale = args[ARG_scale].u_obj == mp_const_none ? 2.0f : mp_obj_get_float(args[ARG_scale].u_obj);
    int angle = args[ARG_angle].u_int;
    int letter_spacing = args[ARG_spacing].u_int;
    bool fixed_width = args[ARG_fixed_width].u_obj == mp_const_true;

    self->graphics->text(t, Point(x, y), wrap, scale, angle, letter_spacing, fixed_width);

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_measure_text(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_self, ARG_text, ARG_scale, ARG_spacing, ARG_fixed_width };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_text, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_scale, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_spacing, MP_ARG_INT, {.u_int = 1} },
        { MP_QSTR_fixed_width, MP_ARG_OBJ, {.u_obj = mp_const_false} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self].u_obj, ModPicoGraphics_obj_t);

    mp_obj_t text_obj = args[ARG_text].u_obj;

    if(!mp_obj_is_str_or_bytes(text_obj)) mp_raise_TypeError("text: string required");

    GET_STR_DATA_LEN(text_obj, str, str_len);

    const std::string_view t((const char*)str, str_len);

    float scale = args[ARG_scale].u_obj == mp_const_none ? 2.0f : mp_obj_get_float(args[ARG_scale].u_obj);
    int letter_spacing = args[ARG_spacing].u_int;
    bool fixed_width = args[ARG_fixed_width].u_obj == mp_const_true;

    int width = self->graphics->measure_text(t, scale, letter_spacing, fixed_width);

    return mp_obj_new_int(width);
}

mp_obj_t ModPicoGraphics_polygon(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    size_t num_tuples = n_args - 1;
    const mp_obj_t *tuples = pos_args + 1;

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(pos_args[0], ModPicoGraphics_obj_t);

    // Check if there is only one argument, which might be a list
    if(n_args == 2) {
        if(mp_obj_is_type(pos_args[1], &mp_type_list)) {
            mp_obj_list_t *points = MP_OBJ_TO_PTR2(pos_args[1], mp_obj_list_t);

            if(points->len <= 0) mp_raise_ValueError("poly(): cannot provide an empty list");

            num_tuples = points->len;
            tuples = points->items;
        }
        else {
            mp_raise_TypeError("poly(): can't convert object to list");
        }
    }

    if(num_tuples > 0) {
        std::vector<Point> points;
        for(size_t i = 0; i < num_tuples; i++) {
            mp_obj_t obj = tuples[i];
            if(!mp_obj_is_type(obj, &mp_type_tuple)) mp_raise_ValueError("poly(): can't convert object to tuple");

            mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR2(obj, mp_obj_tuple_t);

            if(tuple->len != 2) mp_raise_ValueError("poly(): tuple must only contain two numbers");

            points.push_back({
                mp_obj_get_int(tuple->items[0]),
                mp_obj_get_int(tuple->items[1])
            });
        }
        self->graphics->polygon(points);
    }

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_triangle(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_x1, ARG_y1, ARG_x2, ARG_y2, ARG_x3, ARG_y3 };

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self], ModPicoGraphics_obj_t);

    self->graphics->triangle(
        {mp_obj_get_int(args[ARG_x1]),
         mp_obj_get_int(args[ARG_y1])},
        {mp_obj_get_int(args[ARG_x2]),
         mp_obj_get_int(args[ARG_y2])},
        {mp_obj_get_int(args[ARG_x3]),
         mp_obj_get_int(args[ARG_y3])}
    );

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_line(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_x1, ARG_y1, ARG_x2, ARG_y2, ARG_thickness };

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self], ModPicoGraphics_obj_t);

    if(n_args == 5) {
        self->graphics->line(
            {mp_obj_get_int(args[ARG_x1]),
            mp_obj_get_int(args[ARG_y1])},
            {mp_obj_get_int(args[ARG_x2]),
            mp_obj_get_int(args[ARG_y2])}
        );
    }
    else if(n_args == 6) {
        self->graphics->thick_line(
            {mp_obj_get_int(args[ARG_x1]),
            mp_obj_get_int(args[ARG_y1])},
            {mp_obj_get_int(args[ARG_x2]),
            mp_obj_get_int(args[ARG_y2])},
            mp_obj_get_int(args[ARG_thickness])
        );
    }

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_loop(mp_obj_t self_in, mp_obj_t update, mp_obj_t render) {
    (void)self_in;
    /*
    TODO: Uh how do we typecheck a function?
    if(!mp_obj_is_type(update, &mp_type_function)) {
        mp_raise_TypeError("update(ticks_ms) must be a function.");
    }
    if(!mp_obj_is_type(render, &mp_type_function)) {
        mp_raise_TypeError("render(ticks_ms) must be a function.");
    }*/
    //ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);
    absolute_time_t t_start = get_absolute_time();
    mp_obj_t result;
    while(true) {
        uint32_t tick = absolute_time_diff_us(t_start, get_absolute_time()) / 1000;
        result = mp_call_function_1(update, mp_obj_new_int(tick));
        if (result == mp_const_false) break;
        dv_display.wait_for_flip();
        result = mp_call_function_1(render, mp_obj_new_int(tick));
        if (result == mp_const_false) break;
        dv_display.flip_async();
        #ifdef mp_event_handle_nowait
        mp_event_handle_nowait();
        #endif
    }
    return mp_const_none;
}

}
