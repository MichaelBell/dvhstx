#include "picographics.h"

// Class Methods
MP_DEFINE_CONST_FUN_OBJ_1(ModPicoGraphics_update_obj, ModPicoGraphics_update);

// Palette management
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ModPicoGraphics_update_pen_obj, 5, 5, ModPicoGraphics_update_pen);
MP_DEFINE_CONST_FUN_OBJ_2(ModPicoGraphics_reset_pen_obj, ModPicoGraphics_reset_pen);
MP_DEFINE_CONST_FUN_OBJ_KW(ModPicoGraphics_set_palette_obj, 2, ModPicoGraphics_set_palette);

// Pen
MP_DEFINE_CONST_FUN_OBJ_2(ModPicoGraphics_set_pen_obj, ModPicoGraphics_set_pen);
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ModPicoGraphics_create_pen_obj, 4, 4, ModPicoGraphics_create_pen);
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ModPicoGraphics_create_pen_hsv_obj, 4, 4, ModPicoGraphics_create_pen_hsv);
MP_DEFINE_CONST_FUN_OBJ_2(ModPicoGraphics_set_thickness_obj, ModPicoGraphics_set_thickness);

// Alpha Blending
MP_DEFINE_CONST_FUN_OBJ_2(ModPicoGraphics_set_bg_obj, ModPicoGraphics_set_bg);
MP_DEFINE_CONST_FUN_OBJ_2(ModPicoGraphics_set_blend_mode_obj, ModPicoGraphics_set_blend_mode);

// Depth
MP_DEFINE_CONST_FUN_OBJ_2(ModPicoGraphics_set_depth_obj, ModPicoGraphics_set_depth);

// Primitives
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ModPicoGraphics_set_clip_obj, 5, 5, ModPicoGraphics_set_clip);
MP_DEFINE_CONST_FUN_OBJ_1(ModPicoGraphics_remove_clip_obj, ModPicoGraphics_remove_clip);
MP_DEFINE_CONST_FUN_OBJ_1(ModPicoGraphics_clear_obj, ModPicoGraphics_clear);
MP_DEFINE_CONST_FUN_OBJ_3(ModPicoGraphics_pixel_obj, ModPicoGraphics_pixel);
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ModPicoGraphics_pixel_span_obj, 4, 4, ModPicoGraphics_pixel_span);
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ModPicoGraphics_rectangle_obj, 5, 5, ModPicoGraphics_rectangle);
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ModPicoGraphics_circle_obj, 4, 4, ModPicoGraphics_circle);
MP_DEFINE_CONST_FUN_OBJ_KW(ModPicoGraphics_character_obj, 1, ModPicoGraphics_character);
MP_DEFINE_CONST_FUN_OBJ_KW(ModPicoGraphics_text_obj, 1, ModPicoGraphics_text);
MP_DEFINE_CONST_FUN_OBJ_KW(ModPicoGraphics_measure_text_obj, 1, ModPicoGraphics_measure_text);
MP_DEFINE_CONST_FUN_OBJ_KW(ModPicoGraphics_polygon_obj, 2, ModPicoGraphics_polygon);
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ModPicoGraphics_triangle_obj, 7, 7, ModPicoGraphics_triangle);
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ModPicoGraphics_line_obj, 5, 6, ModPicoGraphics_line);

// Sprites
MP_DEFINE_CONST_FUN_OBJ_KW(ModPicoGraphics_load_sprite_obj, 2, ModPicoGraphics_load_sprite);
MP_DEFINE_CONST_FUN_OBJ_KW(ModPicoGraphics_display_sprite_obj, 5, ModPicoGraphics_display_sprite);
MP_DEFINE_CONST_FUN_OBJ_2(ModPicoGraphics_clear_sprite_obj, ModPicoGraphics_clear_sprite);

// Utility
MP_DEFINE_CONST_FUN_OBJ_1(ModPicoGraphics_get_bounds_obj, ModPicoGraphics_get_bounds);
MP_DEFINE_CONST_FUN_OBJ_2(ModPicoGraphics_set_font_obj, ModPicoGraphics_set_font);

MP_DEFINE_CONST_FUN_OBJ_1(ModPicoGraphics__del__obj, ModPicoGraphics__del__);

// Loop
MP_DEFINE_CONST_FUN_OBJ_3(ModPicoGraphics_loop_obj, ModPicoGraphics_loop);


static const mp_rom_map_elem_t ModPicoGraphics_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_pixel), MP_ROM_PTR(&ModPicoGraphics_pixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_pen), MP_ROM_PTR(&ModPicoGraphics_set_pen_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_thickness), MP_ROM_PTR(&ModPicoGraphics_set_thickness_obj) },
    { MP_ROM_QSTR(MP_QSTR_clear), MP_ROM_PTR(&ModPicoGraphics_clear_obj) },

    { MP_ROM_QSTR(MP_QSTR_set_bg), MP_ROM_PTR(&ModPicoGraphics_set_bg_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_blend_mode), MP_ROM_PTR(&ModPicoGraphics_set_blend_mode_obj) },

    { MP_ROM_QSTR(MP_QSTR_set_depth), MP_ROM_PTR(&ModPicoGraphics_set_depth_obj) },

    { MP_ROM_QSTR(MP_QSTR_update), MP_ROM_PTR(&ModPicoGraphics_update_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_clip), MP_ROM_PTR(&ModPicoGraphics_set_clip_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove_clip), MP_ROM_PTR(&ModPicoGraphics_remove_clip_obj) },
    { MP_ROM_QSTR(MP_QSTR_pixel_span), MP_ROM_PTR(&ModPicoGraphics_pixel_span_obj) },
    { MP_ROM_QSTR(MP_QSTR_rectangle), MP_ROM_PTR(&ModPicoGraphics_rectangle_obj) },
    { MP_ROM_QSTR(MP_QSTR_circle), MP_ROM_PTR(&ModPicoGraphics_circle_obj) },
    { MP_ROM_QSTR(MP_QSTR_character), MP_ROM_PTR(&ModPicoGraphics_character_obj) },
    { MP_ROM_QSTR(MP_QSTR_text), MP_ROM_PTR(&ModPicoGraphics_text_obj) },
    { MP_ROM_QSTR(MP_QSTR_measure_text), MP_ROM_PTR(&ModPicoGraphics_measure_text_obj) },
    { MP_ROM_QSTR(MP_QSTR_polygon), MP_ROM_PTR(&ModPicoGraphics_polygon_obj) },
    { MP_ROM_QSTR(MP_QSTR_triangle), MP_ROM_PTR(&ModPicoGraphics_triangle_obj) },
    { MP_ROM_QSTR(MP_QSTR_line), MP_ROM_PTR(&ModPicoGraphics_line_obj) },

    { MP_ROM_QSTR(MP_QSTR_create_pen), MP_ROM_PTR(&ModPicoGraphics_create_pen_obj) },
    { MP_ROM_QSTR(MP_QSTR_create_pen_hsv), MP_ROM_PTR(&ModPicoGraphics_create_pen_hsv_obj) },
    { MP_ROM_QSTR(MP_QSTR_update_pen), MP_ROM_PTR(&ModPicoGraphics_update_pen_obj) },
    { MP_ROM_QSTR(MP_QSTR_reset_pen), MP_ROM_PTR(&ModPicoGraphics_reset_pen_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_palette), MP_ROM_PTR(&ModPicoGraphics_set_palette_obj) },

    { MP_ROM_QSTR(MP_QSTR_get_bounds), MP_ROM_PTR(&ModPicoGraphics_get_bounds_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_font), MP_ROM_PTR(&ModPicoGraphics_set_font_obj) },

//    { MP_ROM_QSTR(MP_QSTR_loop), MP_ROM_PTR(&ModPicoGraphics_loop_obj) },

    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&ModPicoGraphics__del__obj) },
};
static MP_DEFINE_CONST_DICT(ModPicoGraphics_locals_dict, ModPicoGraphics_locals_dict_table);

/***** Class Definition *****/
#ifdef MP_DEFINE_CONST_OBJ_TYPE
MP_DEFINE_CONST_OBJ_TYPE(
    ModPicoGraphics_type,
    MP_QSTR_dvhstx,
    MP_TYPE_FLAG_NONE,
    make_new, ModPicoGraphics_make_new,
    locals_dict, (mp_obj_dict_t*)&ModPicoGraphics_locals_dict
);
#else
const mp_obj_type_t ModPicoGraphics_type = {
    { &mp_type_type },
    .name = MP_QSTR_dvhstx,
    .make_new = ModPicoGraphics_make_new,
    .locals_dict = (mp_obj_dict_t*)&ModPicoGraphics_locals_dict,
};
#endif

/***** Module Globals *****/
static const mp_map_elem_t picographics_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_dvhstx) },
    { MP_ROM_QSTR(MP_QSTR_DVHSTX), (mp_obj_t)&ModPicoGraphics_type },

    { MP_ROM_QSTR(MP_QSTR_PEN_RGB888), MP_ROM_INT(PEN_RGB888) },
    { MP_ROM_QSTR(MP_QSTR_PEN_RGB565), MP_ROM_INT(PEN_RGB565) },
    { MP_ROM_QSTR(MP_QSTR_PEN_P8), MP_ROM_INT(PEN_P8) },

    { MP_ROM_QSTR(MP_QSTR_BLEND_TARGET), MP_ROM_INT(0) },
    { MP_ROM_QSTR(MP_QSTR_BLEND_FIXED), MP_ROM_INT(1) },

    { MP_ROM_QSTR(MP_QSTR_SPRITE_OVERWRITE), MP_ROM_INT(0) },
    { MP_ROM_QSTR(MP_QSTR_SPRITE_UNDER), MP_ROM_INT(1) },
    { MP_ROM_QSTR(MP_QSTR_SPRITE_OVER), MP_ROM_INT(2) },
    { MP_ROM_QSTR(MP_QSTR_SPRITE_BLEND_UNDER), MP_ROM_INT(3) },
    { MP_ROM_QSTR(MP_QSTR_SPRITE_BLEND_OVER), MP_ROM_INT(4) },

#if SUPPORT_WIDE_MODES
    { MP_ROM_QSTR(MP_QSTR_WIDESCREEN), MP_ROM_TRUE },
#else
    { MP_ROM_QSTR(MP_QSTR_WIDESCREEN), MP_ROM_FALSE },
#endif
};
static MP_DEFINE_CONST_DICT(mp_module_picographics_globals, picographics_globals_table);

/***** Module Definition *****/
const mp_obj_module_t picographics_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_picographics_globals,
};

#if MICROPY_VERSION <= 70144
MP_REGISTER_MODULE(MP_QSTR_dvhstx, picographics_user_cmodule, MODULE_PICOGRAPHICS_ENABLED);
#else
MP_REGISTER_MODULE(MP_QSTR_dvhstx, picographics_user_cmodule);
#endif
