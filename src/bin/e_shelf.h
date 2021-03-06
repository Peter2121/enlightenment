#ifdef E_TYPEDEFS

typedef struct _E_Shelf E_Shelf;

#else
#ifndef E_SHELF_H
#define E_SHELF_H

#define E_SHELF_TYPE 0xE0b0101e
#define E_SHELF_DUMMY_TYPE 0xE0b0102e

struct _E_Shelf
{
   E_Object             e_obj_inherit;
   int                  id;
   int                  x, y, w, h;
   E_Layer              layer;
   Evas_Object         *comp_object;
   E_Zone              *zone;
   Evas_Object         *o_base;
   Ecore_Evas          *ee;
   Evas                *evas;
   E_Gadcon            *gadcon;
   const char          *name;
   const char          *style;
   E_Config_Shelf      *cfg;
   int                  size;
   E_Config_Dialog     *config_dialog;
   E_Entry_Dialog      *rename_dialog;
   E_Menu              *menu;
   Ecore_Timer         *hide_timer;
   Ecore_Animator      *hide_animator;
   int                  hide_step;
   int                  hidden_state_size;
   int                  hide_origin;
   int                  interrupted;
   float                instant_delay;
   Ecore_Timer         *instant_timer;
   Ecore_Timer         *autohide_timer;
   Ecore_Timer         *module_init_end_timer;
   Eina_List           *handlers;
   Ecore_Event_Handler *autohide;
   Eina_List           *zone_obstacles;
   unsigned char        fit_along   E_BITFIELD;
   unsigned char        fit_size    E_BITFIELD;
   unsigned char        hidden      E_BITFIELD;
   unsigned char        toggle      E_BITFIELD;
   unsigned char        edge        E_BITFIELD;
   unsigned char        urgent_show E_BITFIELD;
   unsigned char        dummy E_BITFIELD;
   Eina_Bool            cfg_delete E_BITFIELD;
   unsigned int         locked;
   Eina_Bool hiding E_BITFIELD;
};

typedef struct E_Event_Shelf
{
   E_Shelf *shelf;
} E_Event_Shelf;

typedef struct E_Event_Shelf E_Event_Shelf_Rename;
typedef struct E_Event_Shelf E_Event_Shelf_Add;
typedef struct E_Event_Shelf E_Event_Shelf_Del;

E_API extern int E_EVENT_SHELF_RENAME;
E_API extern int E_EVENT_SHELF_ADD;
E_API extern int E_EVENT_SHELF_DEL;

EINTERN int              e_shelf_init(void);
EINTERN int              e_shelf_shutdown(void);
E_API void             e_shelf_config_update(void);
E_API E_Entry_Dialog *e_shelf_new_dialog(E_Zone *zone);
E_API Eina_List       *e_shelf_list(void);
E_API Eina_List       *e_shelf_list_all(void); // includes dummy shelves
E_API E_Shelf         *e_shelf_zone_new(E_Zone *zone, const char *name, const char *style, E_Layer layer, int id);
E_API E_Shelf         *e_shelf_zone_dummy_new(E_Zone *zone, Evas_Object *obj, int id);
E_API void             e_shelf_zone_move_resize_handle(E_Zone *zone);
E_API void             e_shelf_populate(E_Shelf *es);
E_API void             e_shelf_show(E_Shelf *es);
E_API void             e_shelf_hide(E_Shelf *es);
E_API void             e_shelf_locked_set(E_Shelf *es, int lock);
E_API void             e_shelf_toggle(E_Shelf *es, int show);
E_API void             e_shelf_urgent_show(E_Shelf *es);
E_API void             e_shelf_move(E_Shelf *es, int x, int y);
E_API void             e_shelf_resize(E_Shelf *es, int w, int h);
E_API void             e_shelf_move_resize(E_Shelf *es, int x, int y, int w, int h);
E_API void             e_shelf_save(E_Shelf *es);
E_API void             e_shelf_unsave(E_Shelf *es);
E_API void             e_shelf_orient(E_Shelf *es, E_Gadcon_Orient orient);
E_API const char      *e_shelf_orient_string_get(E_Shelf *es);
E_API void             e_shelf_position_calc(E_Shelf *es);
E_API void             e_shelf_style_set(E_Shelf *es, const char *style);
E_API E_Shelf         *e_shelf_config_new(E_Zone *zone, E_Config_Shelf *cf_es);
E_API void             e_shelf_name_set(E_Shelf *es, const char *name);
E_API void             e_shelf_rename_dialog(E_Shelf *es);
E_API void             e_shelf_autohide_set(E_Shelf *es, int autohide_type);
E_API Eina_Bool       e_shelf_desk_visible(const E_Shelf *es, const E_Desk *desk);
E_API void e_shelf_obstacles_update(E_Shelf *es);
#endif
#endif
