#include "e.h"

#define DEFAULT_LAYER E_LAYER_CLIENT_ABOVE
#define E_BRYCE_TYPE 0xE31338

static unsigned int bryce_version = 2;

typedef struct Bryce
{
   E_Object *e_obj_inherit;
   Eina_Stringshare *name;

   Evas_Object *bryce;
   Evas_Object *events;
   Evas_Object *layout;
   Evas_Object *site;
   Evas_Object *scroller;
   Evas_Object *autohide_event;
   Eina_List *zone_obstacles;
   Eina_List *spacers;

   Evas_Object *parent; //comp_object is not an elm widget
   Eina_Stringshare *style;
   int size;
   int x, y;
   int last_w, last_h;
   int autohide_size;
   E_Layer layer;
   unsigned int zone;
   E_Gadget_Site_Orient orient;
   E_Gadget_Site_Anchor anchor;

   Ecore_Job *calc_job;
   Ecore_Timer *save_timer;
   Ecore_Timer *autohide_timer;
   unsigned int autohide_blocked;
   Eina_List *popups;
   void *event_info;
   unsigned int last_timestamp;

   E_Menu *menu;

   /* config: do not bitfield! */
   Eina_Bool autosize;
   Eina_Bool autohide;
   unsigned int version;

   Eina_Bool hidden E_BITFIELD;
   Eina_Bool animating E_BITFIELD;
   Eina_Bool mouse_in E_BITFIELD;
   Eina_Bool noshadow E_BITFIELD;
   Eina_Bool size_changed E_BITFIELD;
   Eina_Bool editing E_BITFIELD;
} Bryce;

typedef struct Bryces
{
   Eina_List *bryces;
} Bryces;

static E_Config_DD *edd_bryces;
static E_Config_DD *edd_bryce;
static Bryces *bryces;
static E_Action *resize_act;
static Eina_List *handlers;


static void _bryce_menu(Bryce *b, Evas_Object *g);

#define BRYCE_GET(obj) \
   Bryce *b; \
   b = evas_object_data_get((obj), "__bryce"); \
   if (!b) abort()

static void
_bryce_obstacle_del(void *obs)
{
   Bryce *b = e_object_data_get(obs);

   b->zone_obstacles = eina_list_remove(b->zone_obstacles, obs);
}

static void
_bryce_autohide_end(void *data, E_Efx_Map_Data *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED)
{
   Bryce *b = data;

   b->animating = 0;
}

static void
_bryce_autohide_coords(Bryce *b, int *x, int *y)
{
   int ox, oy, ow, oh;
   E_Gadget_Site_Anchor an;

   if (b->parent == e_comp->elm)
     {
        E_Zone *zone;
        
        zone = e_comp_zone_number_get(b->zone);
        ox = zone->x, oy = zone->y, ow = zone->w, oh = zone->h;
     }
   else
     evas_object_geometry_get(b->parent, &ox, &oy, &ow, &oh);
   an = e_gadget_site_anchor_get(b->site);

   if (b->orient == E_GADGET_SITE_ORIENT_HORIZONTAL)
     {
        *x = b->x;

        if (an & E_GADGET_SITE_ANCHOR_TOP)
          *y = oy - lround(e_scale * b->size) + lround(e_scale * b->autohide_size);
        if (an & E_GADGET_SITE_ANCHOR_BOTTOM)
          *y = oy + oh - lround(e_scale * b->autohide_size);
     }
   else if (b->orient == E_GADGET_SITE_ORIENT_VERTICAL)
     {
        *y = b->y;

        if (an & E_GADGET_SITE_ANCHOR_LEFT)
          *x = ox - lround(e_scale * b->size) + lround(e_scale * b->autohide_size);
        if (an & E_GADGET_SITE_ANCHOR_RIGHT)
          *x = ox + ow - lround(e_scale * b->autohide_size);
     }
}

static void
_bryce_position(Bryce *b, int w, int h, int *nx, int *ny)
{
   int ox, oy, ow, oh;
   int x, y;
   E_Gadget_Site_Anchor an;

   if (b->parent == e_comp->elm)
     {
        E_Zone *zone;
        
        zone = e_comp_zone_number_get(b->zone);
        ox = zone->x, oy = zone->y, ow = zone->w, oh = zone->h;
        if (starting)
          {
             E_Zone *zone2;

             zone2 = e_comp_object_util_zone_get(b->bryce);
             if (zone != zone2)
               evas_object_move(b->bryce, ox, oy);
          }
     }
   else
     evas_object_geometry_get(b->parent, &ox, &oy, &ow, &oh);
   x = ox + (ow - w) / 2;
   x = E_CLAMP(x, ox, ox + ow / 2);
   y = oy + (oh - h) / 2;
   y = E_CLAMP(y, oy, oy + oh / 2);
   an = e_gadget_site_anchor_get(b->site);
   if (an & E_GADGET_SITE_ANCHOR_LEFT)
     x = ox;
   if (an & E_GADGET_SITE_ANCHOR_TOP)
     y = oy;
   if (b->orient == E_GADGET_SITE_ORIENT_HORIZONTAL)
     {
        if (an & E_GADGET_SITE_ANCHOR_RIGHT)
          x = ox + ow - w;
        if (an & E_GADGET_SITE_ANCHOR_BOTTOM)
          y = oy + oh - lround(e_scale * b->size);
        if (!b->autosize)
          x = ox;
     }
   else if (b->orient == E_GADGET_SITE_ORIENT_VERTICAL)
     {
        if (an & E_GADGET_SITE_ANCHOR_RIGHT)
          x = ox + ow - lround(e_scale * b->size);
        if (an & E_GADGET_SITE_ANCHOR_BOTTOM)
          y = oy + oh - h;
        if (!b->autosize)
          y = oy;
     }
   b->x = x, b->y = y;
   if (b->animating)
     {
        if (b->hidden)
          {
             _bryce_autohide_coords(b, &x, &y);
             e_efx_move(b->bryce, E_EFX_EFFECT_SPEED_LINEAR, E_EFX_POINT(x, y), 0.5, _bryce_autohide_end, b);
          }
        else
          e_efx_move(b->bryce, E_EFX_EFFECT_SPEED_LINEAR, E_EFX_POINT(x, y), 0.5, _bryce_autohide_end, b);
     }
   else if (b->hidden)
     _bryce_autohide_coords(b, &x, &y);

   if (nx && ny)
     *nx = x, *ny = y;
   else
     evas_object_move(b->bryce, x, y);
}

static void
_bryce_autosize(Bryce *b)
{
   int lw, lh, sw, sh, maxw, maxh, x, y, w, h;

   E_FREE_FUNC(b->calc_job, ecore_job_del);
   if (!b->autosize)
     {
        if (b->parent == e_comp->elm)
          {
             w = e_comp_zone_number_get(b->zone)->w;
             if (b->orient == E_GADGET_SITE_ORIENT_VERTICAL)
               e_zone_useful_geometry_get(e_comp_zone_number_get(b->zone), NULL, NULL, NULL, &h);
             else
               h = e_comp_zone_number_get(b->zone)->h;
          }
        else
          evas_object_geometry_get(b->parent, NULL, NULL, &w, &h);
        if (b->size_changed)
          elm_object_content_unset(b->scroller);
        _bryce_position(b, w, h, &x, &y);
        evas_object_move(b->bryce, x, y);
        if (b->orient == E_GADGET_SITE_ORIENT_HORIZONTAL)
          e_efx_resize(b->bryce, E_EFX_EFFECT_SPEED_LINEAR, E_EFX_POINT(x, y), w, lround(b->size * e_scale), 0.1, NULL, NULL);
        else if (b->orient == E_GADGET_SITE_ORIENT_VERTICAL)
          e_efx_resize(b->bryce, E_EFX_EFFECT_SPEED_LINEAR, E_EFX_POINT(x, y), lround(b->size * e_scale), h, 0.1, NULL, NULL);
        evas_object_smart_need_recalculate_set(b->site, 1);
        evas_object_size_hint_min_set(b->site, -1, -1);
        if (b->size_changed)
          elm_object_content_set(b->scroller, b->site);
        b->size_changed = 0;
        return;
     }
   if (b->parent == e_comp->elm) //screen-based bryce
     {
        E_Zone *zone;

        zone = e_comp_zone_number_get(b->zone);
        if (zone)
          {
             maxw = zone->w;
             if (b->orient == E_GADGET_SITE_ORIENT_VERTICAL)
               e_zone_useful_geometry_get(e_comp_zone_number_get(b->zone), NULL, NULL, NULL, &maxh);
             else
               maxh = zone->h;
          }
        else return;
     }
   else
     evas_object_geometry_get(b->parent, NULL, NULL, &maxw, &maxh);
   do
     {
        if (b->size_changed)
          {
             evas_object_geometry_get(b->bryce, NULL, NULL, &w, &h);
             elm_object_content_unset(b->scroller);
             if (b->orient == E_GADGET_SITE_ORIENT_HORIZONTAL)
               {
                  if (!h) h = 1;
                  evas_object_resize(b->bryce, w * lround(b->size * e_scale) / h, lround(b->size * e_scale));
                  evas_object_resize(b->site, w * lround(b->size * e_scale) / h, lround(b->size * e_scale));
               }
             else if (b->orient == E_GADGET_SITE_ORIENT_VERTICAL)
               {
                  if (!w) w = 1;
                  evas_object_resize(b->bryce, lround(b->size * e_scale), h * lround(b->size * e_scale) / w);
                  evas_object_resize(b->site, lround(b->size * e_scale), h * lround(b->size * e_scale) / w);
               }
             evas_object_smart_need_recalculate_set(b->site, 1);
             evas_object_size_hint_min_set(b->site, -1, -1);
             evas_object_smart_calculate(b->site);
             elm_object_content_set(b->scroller, b->site);
          }
        evas_object_size_hint_min_get(b->site, &sw, &sh);
        if ((!sw) && (!sh)) b->size_changed = 1;
     }
   while ((!sw) && (!sh));
   edje_object_size_min_calc(elm_layout_edje_get(b->layout), &lw, &lh);
   _bryce_position(b, lw + sw, lh + sh, &x, &y);
   if (b->orient == E_GADGET_SITE_ORIENT_HORIZONTAL)
     w = MIN(MAX(lw + sw, lround(b->size * e_scale)), maxw), h = lround(b->size * e_scale);
   else if (b->orient == E_GADGET_SITE_ORIENT_VERTICAL)
     w = lround(b->size * e_scale), h = MIN(MAX(lh + sh, lround(b->size * e_scale)), maxh);
   evas_object_move(b->bryce, x, y);
   e_efx_resize(b->bryce, E_EFX_EFFECT_SPEED_LINEAR, E_EFX_POINT(x, y), w, h, 0.1, NULL, NULL);
   b->size_changed = 0;
}

static Eina_Bool
_bryce_autohide_timeout(Bryce *b)
{
   int x = 0, y = 0;

   b->autohide_timer = NULL;
   b->hidden = b->animating = 1;
   _bryce_autohide_coords(b, &x, &y);
   e_efx_move(b->bryce, E_EFX_EFFECT_SPEED_LINEAR, E_EFX_POINT(x, y), 0.5, _bryce_autohide_end, b);
   return EINA_FALSE;
}

static void
_bryce_autohide_moveresize(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Bryce *b = data;
   int x, y, w, h;

   evas_object_geometry_get(obj, &x, &y, &w, &h);
   evas_object_geometry_set(b->autohide_event, x, y, w, h);
}

static void
_bryce_autohide_show(Bryce *b)
{
   E_FREE_FUNC(b->autohide_timer, ecore_timer_del);
   if (b->animating && (!b->hidden)) return;
   e_efx_move(b->bryce, E_EFX_EFFECT_SPEED_LINEAR, E_EFX_POINT(b->x, b->y), 0.5, _bryce_autohide_end, b);
   b->animating = 1;
   b->hidden = 0;
}

static void
_bryce_autohide_hide(Bryce *b)
{
   if (b->autohide_blocked) return;
   if (b->autohide_timer)
     ecore_timer_loop_reset(b->autohide_timer);
   else
     b->autohide_timer = ecore_timer_loop_add(1.0, (Ecore_Task_Cb)_bryce_autohide_timeout, b);
}

static void
_bryce_autohide_mouse_out(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Bryce *b = data;

   _bryce_autohide_hide(b);
   b->mouse_in = 0;
}

static void
_bryce_autohide_mouse_in(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Bryce *b = data;

   b->mouse_in = 1;
   _bryce_autohide_show(b);
}

static void
_bryce_autohide_setup(Bryce *b)
{
   int x, y, w, h;

   if (!b->autohide) return;
   b->autohide_event = evas_object_rectangle_add(evas_object_evas_get(b->bryce));
   evas_object_geometry_get(b->bryce, &x, &y, &w, &h);
   evas_object_geometry_set(b->autohide_event, x, y, w, h);
   evas_object_color_set(b->autohide_event, 0, 0, 0, 0);
   evas_object_repeat_events_set(b->autohide_event, 1);
   evas_object_layer_set(b->autohide_event, b->layer + 1);
   evas_object_show(b->autohide_event);
   evas_object_event_callback_add(b->autohide_event, EVAS_CALLBACK_MOUSE_IN, _bryce_autohide_mouse_in, b);
   evas_object_event_callback_add(b->autohide_event, EVAS_CALLBACK_MOUSE_OUT, _bryce_autohide_mouse_out, b);
   evas_object_event_callback_add(b->bryce, EVAS_CALLBACK_MOVE, _bryce_autohide_moveresize, b);
   evas_object_event_callback_add(b->bryce, EVAS_CALLBACK_RESIZE, _bryce_autohide_moveresize, b);
   ecore_evas_pointer_xy_get(e_comp->ee, &x, &y);
   if (!E_INSIDE(x, y, b->x, b->y, w, h))
     _bryce_autohide_hide(b);
}

static void
_bryce_style(Evas_Object *site, Eina_Stringshare *name, Evas_Object *g)
{
   Evas_Object *ly, *prev;
   char buf[1024];

   BRYCE_GET(site);

   ly = elm_layout_add(b->site);
   snprintf(buf, sizeof(buf), "e/bryce/%s/%s", b->style ?: "default", name ?: "plain");
   if (!e_theme_edje_object_set(ly, NULL, buf))
     {
        evas_object_del(ly);
        return;
     }
   if (b->orient == E_GADGET_SITE_ORIENT_HORIZONTAL)
     elm_layout_signal_emit(ly, "e,state,orient,horizontal", "e");
   else
     elm_layout_signal_emit(ly, "e,state,orient,vertical", "e");
   edje_object_message_signal_process(elm_layout_edje_get(ly));
   prev = e_gadget_util_layout_style_init(g, ly);
   elm_object_part_content_set(ly, "e.swallow.content", g);
   evas_object_smart_callback_call(g, "gadget_reparent", ly);
   evas_object_del(prev);
}

static void
_bryce_site_hints(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Bryce *b = data;
   int w, h;

   evas_object_size_hint_min_get(obj, &w, &h);
   if ((w < 0) || (h < 0)) return;
   if (b->autosize && (!b->calc_job))
     b->calc_job = ecore_job_add((Ecore_Cb)_bryce_autosize, b);
}

static E_Comp_Object_Type
_bryce_shadow_type(const Bryce *b)
{
   if ((b->layer == E_LAYER_DESKTOP_TOP) || b->noshadow)
     return E_COMP_OBJECT_TYPE_NONE;
   return E_COMP_OBJECT_TYPE_POPUP;
}

static void
_bryce_restack(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Bryce *b = data;
   E_Layer layer;

   layer = evas_object_layer_get(obj);
   if (layer > DEFAULT_LAYER) return;
   b->layer = layer;
   if ((!b->noshadow) && (layer != b->layer))
     e_comp_object_util_type_set(b->bryce, _bryce_shadow_type(b));
}

static Eina_Bool
_bryce_moveresize_save(void *data)
{
   Bryce *b = data;
   int w, h;
   int size;

   b->save_timer = NULL;
   evas_object_geometry_get(b->bryce, NULL, NULL, &w, &h);
   if (b->orient == E_GADGET_SITE_ORIENT_HORIZONTAL)
     size = h;
   else
     size = w;
   size = lround(size / e_scale);
   if (b->size == size) return EINA_FALSE;
   e_config_save_queue();
   b->size = size;
   return EINA_FALSE;
}

static void
_bryce_zone_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Bryce *b = data;
   e_object_del(E_OBJECT(b->e_obj_inherit));
}

static void
_bryce_zone_moveresize(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Bryce *b = data;
   if (!b->calc_job)
     b->calc_job = ecore_job_add((Ecore_Cb)_bryce_autosize, b);
}

static void
_bryce_zone_setup(Bryce *b)
{
   Evas_Object *zone_clip;

   zone_clip = evas_object_clip_get(b->bryce);
   if (zone_clip)
     {
        evas_object_event_callback_del_full(zone_clip, EVAS_CALLBACK_DEL, _bryce_zone_del, b);
        evas_object_event_callback_del_full(zone_clip, EVAS_CALLBACK_MOVE, _bryce_zone_moveresize, b);
        evas_object_event_callback_del_full(zone_clip, EVAS_CALLBACK_RESIZE, _bryce_zone_moveresize, b);
     }
   zone_clip = e_comp_zone_number_get(b->zone)->bg_clip_object;
   evas_object_clip_set(b->bryce, zone_clip);
   evas_object_event_callback_add(zone_clip, EVAS_CALLBACK_DEL, _bryce_zone_del, b);
   evas_object_event_callback_add(zone_clip, EVAS_CALLBACK_MOVE, _bryce_zone_moveresize, b);
   evas_object_event_callback_add(zone_clip, EVAS_CALLBACK_RESIZE, _bryce_zone_moveresize, b);
}

static void
_bryce_rename(Bryce *b, int num)
{
   char buf[1024], buf2[1024], *name, *p;

   name = strdup(b->name);
   if (b->version >= 2)
     {
        p = strrchr(name, '_');
        p[0] = 0;
     }
   snprintf(buf, sizeof(buf), "__bryce%s", name);
   snprintf(buf2, sizeof(buf2), "__bryce%s_%d", name, num);
   e_gadget_site_rename(buf, buf2);
   if (b->version >= 2)
     {
        snprintf(buf, sizeof(buf), "%s_%u", name, num);
        eina_stringshare_replace(&b->name, buf);
     }
   free(name);
}

static void
_bryce_moveresize(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Bryce *b = data;
   int x, y, w, h;
   E_Zone *zone;
   int size;

   evas_object_geometry_get(obj, &x, &y, &w, &h);
   evas_object_geometry_set(b->events, x, y, w, h);
   if (b->autohide)
     {
        E_FREE_LIST(b->zone_obstacles, e_object_del);
        return;
     }
   if (b->orient == E_GADGET_SITE_ORIENT_HORIZONTAL)
     size = h;
   else
     size = w;
   if (((b->orient == E_GADGET_SITE_ORIENT_VERTICAL) && (w != b->last_w)) ||
       ((b->orient == E_GADGET_SITE_ORIENT_HORIZONTAL) && (h != b->last_h)))
     {
        evas_object_smart_need_recalculate_set(b->site, 1);
        evas_object_size_hint_min_set(b->site, -1, -1);
     }
   b->last_w = w, b->last_h = h;

   if (size && (b->size != size))
     {
        if (b->save_timer)
          ecore_timer_loop_reset(b->save_timer);
        else
          b->save_timer = ecore_timer_loop_add(0.5, _bryce_moveresize_save, b);
     }

   if (starting) return;
   zone = e_comp_object_util_zone_get(obj);
   if (zone)
     {
        Eina_Bool vertical = b->orient == E_GADGET_SITE_ORIENT_VERTICAL;
        if (b->zone_obstacles)
          {
             Eina_List *l;
             E_Zone_Obstacle *obs;

             EINA_LIST_FOREACH(b->zone_obstacles, l, obs)
               e_zone_obstacle_modify(obs, &(Eina_Rectangle){b->x, b->y, w, h}, vertical);
          }
        else
          {
             void *obs;

             obs = e_zone_obstacle_add(e_comp_object_util_zone_get(obj), NULL,
                    &(Eina_Rectangle){b->x, b->y, w, h}, vertical);
             e_object_data_set(obs, b);
             E_OBJECT_DEL_SET(obs, _bryce_obstacle_del);
             b->zone_obstacles = eina_list_append(b->zone_obstacles, obs);
          }
     }
   else
     {
        /* determine "closest" zone:
         * calculate size of rect between bryce and zone
         * smallest rect = closest zone
         */
        Eina_List *l;
        E_Zone *lz;
        size = 0;

        E_FREE_LIST(b->zone_obstacles, e_object_del);
        EINA_LIST_FOREACH(e_comp->zones, l, lz)
          {
             int cw, ch;

             if (x < lz->x)
               cw = lz->x + lz->w - x;
             else
               cw = x + w - lz->x;
             if (y < lz->y)
               ch = lz->y + lz->h - y;
             else
               ch = y + h - lz->y;
             if (size >= cw * ch) continue;
             size = cw * ch;
             zone = lz;
          }
     }
   if (!zone) return;
   if (b->zone == zone->num) return;
   e_config_save_queue();
   _bryce_rename(b, zone->num);
   b->zone = zone->num;
   _bryce_zone_setup(b);
   _bryce_autosize(b);
}

static void
_bryce_mouse_wheel(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Bryce *b = data;
   Evas_Event_Mouse_Wheel *ev = event_info;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (e_bindings_wheel_evas_event_handle(E_BINDING_CONTEXT_ANY, b->e_obj_inherit, ev))
     ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
}

static void
_bryce_popup_hide(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Bryce *b = data;

   b->autohide_blocked--;
   b->popups = eina_list_remove(b->popups, obj);
   if (!b->autohide) return;
   if (!b->mouse_in)
     _bryce_autohide_hide(b);
}

static void
_bryce_popup(Bryce *b, Evas_Object *popup)
{
   evas_object_event_callback_add(popup, EVAS_CALLBACK_HIDE, _bryce_popup_hide, b);
   b->autohide_blocked++;
   b->popups = eina_list_append(b->popups, popup);
   if (b->autohide)
     _bryce_autohide_show(b);
}

static void
_bryce_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Bryce *b = data;
   Evas_Object *p;
   Evas_Object *zone_clip;
   void *obs;

   if (b->menu)
     {
        e_menu_deactivate(b->menu);
        e_object_del(E_OBJECT(b->menu));
        b->menu = NULL;
     }
   EINA_LIST_FREE(b->zone_obstacles, obs)
     {
        E_OBJECT_DEL_SET(obs, NULL);
        e_object_del(obs);
     }
   evas_object_del(b->autohide_event);
   E_FREE_FUNC(b->events, evas_object_del);
   E_FREE_FUNC(b->calc_job, ecore_job_del);
   E_FREE_FUNC(b->autohide_timer, ecore_timer_del);
   ecore_timer_del(b->save_timer);
   EINA_LIST_FREE(b->popups, p)
     evas_object_event_callback_del(p, EVAS_CALLBACK_HIDE, _bryce_popup_hide);
   zone_clip = evas_object_clip_get(b->bryce);
   if (zone_clip)
     {
        evas_object_event_callback_del_full(zone_clip, EVAS_CALLBACK_DEL, _bryce_zone_del, b);
        evas_object_event_callback_del_full(zone_clip, EVAS_CALLBACK_MOVE, _bryce_zone_moveresize, b);
        evas_object_event_callback_del_full(zone_clip, EVAS_CALLBACK_RESIZE, _bryce_zone_moveresize, b);
     }
   E_FREE(b->e_obj_inherit);
   b->bryce = NULL;
   b->site = NULL;
}

static void
_bryce_object_free(E_Object *eobj)
{
   Bryce *b = e_object_data_get(eobj);
   evas_object_hide(b->bryce);
   evas_object_del(b->bryce);
}

static void
_bryce_style_menu(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Bryce *b = data;
   char buf[1024];

   snprintf(buf, sizeof(buf), "e/bryce/%s", b->style ?: "default");
   e_object_data_set(event_info, e_theme_collection_items_find(NULL, buf));
}

static void
_bryce_gadgets_menu_close(void *data, Evas_Object *obj)
{
   Bryce *b = data;

   b->autohide_blocked--;
   b->editing = 0;
   evas_object_layer_set(b->bryce, b->layer);
   evas_object_hide(obj);
   evas_object_del(obj);
   if (b->autohide && (!b->mouse_in))
     _bryce_autohide_hide(b);
}

static void
_bryce_gadgets_menu(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   Bryce *b = data;
   Evas_Object *comp_object;

   b->autohide_blocked++;
   b->editing = 1;
   comp_object = e_gadget_site_edit(b->site);
   evas_object_layer_set(b->bryce, E_LAYER_POPUP);
   e_comp_object_util_autoclose(comp_object, _bryce_gadgets_menu_close, e_comp_object_util_autoclose_on_escape, b);
}

static void
_bryce_autosize_menu(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   Bryce *b = data;

   e_bryce_autosize_set(b->bryce, !b->autosize);
}

static void
_bryce_autohide_menu(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   Bryce *b = data;

   e_bryce_autohide_set(b->bryce, !b->autohide);
}

static void
_bryce_remove_menu(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   Bryce *b = data;
   bryces->bryces = eina_list_remove(bryces->bryces, data);
   e_gadget_site_del(b->site);
   eina_stringshare_del(b->name);
   eina_stringshare_del(b->style);
   evas_object_hide(b->bryce);
   evas_object_del(b->bryce);
   e_config_save_queue();
}

static void
_bryce_wizard_menu(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   Bryce *b = data;
   Evas_Object *editor;

   editor = e_bryce_edit(b->bryce);
   _bryce_popup(b, editor);
}

static void
_bryce_gadget_settings_menu(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   Evas_Object *g = data;
   e_gadget_configure(g);
}

static void
_bryce_gadget_remove_menu(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   Evas_Object *g = data;
   e_gadget_del(g);
   e_config_save_queue();
}

static void
_bryce_menu_post_cb(void *data, E_Menu *m)
{
   Bryce *b = data;
   if (b->menu != m) return;
   b->menu = NULL;
}

static void
_bryce_menu_populate(Bryce *b, E_Menu *m, Evas_Object *g)
{
   E_Menu_Item *mi;
   const char *s = NULL;

   if (g) s = e_gadget_type_get(g);
   if (s) e_menu_title_set(m, e_gadget_type_get(g));
   else e_menu_title_set(m, _("Gadget Bar"));

   if (g)
     {
        mi = e_menu_item_new(m);
        e_menu_item_label_set(mi, _("Gadget Settings"));
        e_util_menu_item_theme_icon_set(mi, "preferences-system");
        e_menu_item_callback_set(mi, _bryce_gadget_settings_menu, g);

        mi = e_menu_item_new(m);
        e_menu_item_label_set(mi, _("Remove Gadget"));
        e_util_menu_item_theme_icon_set(mi, "list-remove");
        e_menu_item_callback_set(mi, _bryce_gadget_remove_menu, g);

        e_gadget_menu_populate(g, m);

        mi = e_menu_item_new(m);
        e_menu_item_separator_set(mi, 1);
     }

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Autosize"));
   e_menu_item_check_set(mi, 1);
   e_menu_item_toggle_set(mi, b->autosize);
   e_menu_item_callback_set(mi, _bryce_autosize_menu, b);

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Autohide"));
   e_menu_item_check_set(mi, 1);
   e_menu_item_toggle_set(mi, b->autohide);
   e_menu_item_callback_set(mi, _bryce_autohide_menu, b);

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Wizard"));
   e_menu_item_callback_set(mi, _bryce_wizard_menu, b);

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Select Gadgets"));
   e_menu_item_callback_set(mi, _bryce_gadgets_menu, b);

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Remove Gadget Bar"));
   e_util_menu_item_theme_icon_set(mi, "list-remove");
   e_menu_item_callback_set(mi, _bryce_remove_menu, b);

   if (b->menu)
     {
        e_menu_deactivate(b->menu);
        e_object_del(E_OBJECT(b->menu));
     }
   b->menu = m;
   e_menu_post_deactivate_callback_set(m, _bryce_menu_post_cb, b);
}

static void
_bryce_owner_menu(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Bryce *b = data;
   E_Menu_Item *mi = event_info;
   E_Menu *subm;

   e_menu_item_label_set(mi, _("Gadget Bar"));

   subm = e_menu_new();
   e_menu_hold_mode_set(subm, EINA_FALSE);
   e_menu_item_submenu_set(mi, subm);
   e_object_unref(E_OBJECT(subm));

   _bryce_menu_populate(b, subm, NULL);
}

static void
_bryce_gadget_popup(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   _bryce_popup(data, event_info);
}

static void
_bryce_gadget_locked(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Bryce *b = data;

   b->autohide_blocked++;
   if (b->autohide)
     _bryce_autohide_show(b);
}

static void
_bryce_gadget_unlocked(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Bryce *b = data;

   b->autohide_blocked--;
   if (b->autohide && (!b->mouse_in))
     _bryce_autohide_hide(b);
}

static void
_bryce_site_anchor(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Bryce *b = data;

   e_bryce_orient(b->bryce, e_gadget_site_orient_get(obj), e_gadget_site_anchor_get(obj));
}

static void
_bryce_gadget_size_request(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Coord_Size *size = event_info;
   Bryce *b = data;

   edje_object_part_geometry_get(elm_layout_edje_get(b->layout), "e.swallow.content", NULL, NULL, &size->w, &size->h);
}

static void
_bryce_context(Evas_Object *site, Evas_Object *g, unsigned int timestamp)
{
   Bryce *b = evas_object_data_get(site, "__bryce");
   if (b)
     {
        b->last_timestamp = timestamp;
        _bryce_menu(b, g);
     }
}

static void
_bryce_orient(Bryce *b)
{
   char buf[1024];

   evas_object_del(b->site);

   snprintf(buf, sizeof(buf), "__bryce%s", b->name);
   b->site = e_gadget_site_add(b->orient, buf);
   E_EXPAND(b->site);
   E_FILL(b->site);
   evas_object_data_set(b->site, "__bryce", b);
   elm_object_content_set(b->scroller, b->site);
   e_gadget_site_owner_setup(b->site, b->anchor, _bryce_style, _bryce_context);
   if (b->orient == E_GADGET_SITE_ORIENT_HORIZONTAL)
     {
        elm_layout_signal_emit(b->layout, "e,state,orient,horizontal", "e");
        elm_layout_signal_emit(b->scroller, "e,state,orient,horizontal", "e");
        elm_scroller_policy_set(b->scroller, ELM_SCROLLER_POLICY_AUTO, ELM_SCROLLER_POLICY_OFF);
     }
   else
     {
        elm_layout_signal_emit(b->layout, "e,state,orient,vertical", "e");
        elm_layout_signal_emit(b->scroller, "e,state,orient,vertical", "e");
        elm_scroller_policy_set(b->scroller, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
     }
   evas_object_event_callback_add(b->site, EVAS_CALLBACK_CHANGED_SIZE_HINTS, _bryce_site_hints, b);
   evas_object_smart_callback_add(b->site, "gadget_site_anchor", _bryce_site_anchor, b);
   evas_object_smart_callback_add(b->site, "gadget_site_style_menu", _bryce_style_menu, b);
   evas_object_smart_callback_add(b->site, "gadget_site_owner_menu", _bryce_owner_menu, b);
   evas_object_smart_callback_add(b->site, "gadget_site_popup", _bryce_gadget_popup, b);
   evas_object_smart_callback_add(b->site, "gadget_site_locked", _bryce_gadget_locked, b);
   evas_object_smart_callback_add(b->site, "gadget_site_unlocked", _bryce_gadget_unlocked, b);
   evas_object_smart_callback_add(b->site, "gadget_site_parent_size_request", _bryce_gadget_size_request, b);
}

static void
_bryce_style_apply(Bryce *b)
{
   char buf[1024];
   Eina_Bool noshadow;

   snprintf(buf, sizeof(buf), "e/bryce/%s/base", b->style ?: "default");
   e_theme_edje_object_set(b->layout, NULL, buf);
   noshadow = b->noshadow;
   b->noshadow = !!elm_layout_data_get(b->layout, "noshadow");
   if (b->bryce && (noshadow != b->noshadow))
     e_comp_object_util_type_set(b->bryce, _bryce_shadow_type(b));
}

static void
_bryce_create(Bryce *b, Evas_Object *parent)
{
   Evas_Object *ly, *bryce, *scr;

   b->e_obj_inherit = E_OBJECT_ALLOC(E_Object, E_BRYCE_TYPE, _bryce_object_free);
   b->events = evas_object_rectangle_add(e_comp->evas);
   evas_object_repeat_events_set(b->events, EINA_TRUE);
   evas_object_color_set(b->events, 0, 0, 0, 0);
   evas_object_name_set(b->events, "b->events");
   evas_object_show(b->events);
   e_object_data_set(b->e_obj_inherit, b);
   b->layout = ly = elm_layout_add(parent);
   _bryce_style_apply(b);

   b->scroller = scr = elm_scroller_add(ly);
   elm_scroller_bounce_set(scr, 0, 0);
   elm_object_style_set(scr, "bryce");
   _bryce_orient(b);
   elm_object_part_content_set(ly, "e.swallow.content", scr);
   evas_object_show(ly);
   b->bryce = bryce = e_comp_object_util_add(ly, _bryce_shadow_type(b));
//   evas_object_repeat_events_set(evas_object_smart_parent_get(ly), 1);
   evas_object_smart_member_add(b->events, bryce);
   evas_object_raise(b->events);
   evas_object_data_set(bryce, "comp_skip", (void*)1);
   evas_object_layer_set(bryce, b->layer);
   evas_object_lower(bryce);

   b->parent = parent;
   {
      const char *str;

      str = elm_layout_data_get(ly, "hidden_state_size");
      if (str && str[0])
        b->autohide_size = strtol(str, NULL, 10);
   }
   evas_object_data_set(bryce, "__bryce", b);
   evas_object_event_callback_add(bryce, EVAS_CALLBACK_DEL, _bryce_del, b);
   evas_object_event_callback_add(bryce, EVAS_CALLBACK_RESTACK, _bryce_restack, b);
   evas_object_event_callback_add(bryce, EVAS_CALLBACK_MOVE, _bryce_moveresize, b);
   evas_object_event_callback_add(bryce, EVAS_CALLBACK_RESIZE, _bryce_moveresize, b);
   evas_object_event_callback_add(b->events, EVAS_CALLBACK_MOUSE_WHEEL, _bryce_mouse_wheel, b);

  _bryce_zone_setup(b);
   _bryce_autohide_setup(b);
   _bryce_autosize(b);
}

static Eina_Bool
_bryce_act_resize(E_Object *obj, const char *params, E_Binding_Event_Wheel *ev)
{
   Bryce *b;
   int size, step = 4;
   char buf[64];

   if (obj->type != E_BRYCE_TYPE) return EINA_FALSE;
   if (params && params[0])
     {
        step = strtol(params, NULL, 10);
        step = MAX(step, 4);
     }
   b = e_object_data_get(obj);
   size = b->size;
   if (ev->z < 0)//up
     b->size += step;
   else
     b->size -= step;
   b->size = E_CLAMP(b->size, 20, 256);
   if (dblequal(e_scale, 1.0))
     snprintf(buf, sizeof(buf), "%dpx", b->size);
   else
     snprintf(buf, sizeof(buf), "%dpx (%ldpx scaled)", b->size, lround(b->size * e_scale));
   elm_object_part_text_set(b->layout, "e.text", buf);
   elm_object_signal_emit(b->layout, "e,action,resize", "e");
   e_config_save_queue();
   if (size == b->size) return EINA_TRUE;
   b->size_changed = 1;
   if (!b->calc_job)
     b->calc_job = ecore_job_add((Ecore_Cb)_bryce_autosize, b);
   return EINA_TRUE;
}

static void
_bryce_menu(Bryce *b, Evas_Object *g)
{
   E_Menu *m;
   int x = 0, y = 0, w = 1, h = 1;

   m = e_menu_new();
   e_menu_hold_mode_set(m, EINA_FALSE);
   _bryce_menu_populate(b, m, g);
   if (g)
     evas_object_geometry_get(g, &x, &y, &w, &h);
   else
     evas_pointer_canvas_xy_get(e_comp->evas, &x, &y);
   e_menu_activate_mouse(m, e_zone_current_get(), x, y, w, h,
                         E_MENU_POP_DIRECTION_AUTO, b->last_timestamp);
   _bryce_popup(b, m->comp_object);
}

static Eina_Bool
_bryce_zone_useful_geometry_changed(void *d EINA_UNUSED, int t EINA_UNUSED, E_Event_Zone_Move_Resize *ev)
{
   Eina_List *l;
   Bryce *b;

   EINA_LIST_FOREACH(bryces->bryces, l, b)
     {
        if (b->bryce && (b->zone == ev->zone->num) &&
            (b->orient == E_GADGET_SITE_ORIENT_VERTICAL))
          _bryce_autosize(b);
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_bryce_comp_resize(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Eina_List *l;
   Bryce *b;

   EINA_LIST_FOREACH(bryces->bryces, l, b)
     {
        if (e_comp_zone_number_get(b->zone))
          {
             if (b->bryce) continue;
             _bryce_create(b, e_comp->elm);
             evas_object_show(b->bryce);
          }
     }
}

E_API Evas_Object *
e_bryce_add(Evas_Object *parent, const char *name, E_Gadget_Site_Orient orient, E_Gadget_Site_Anchor an)
{
   Bryce *b;
   const char *z;

   b = E_NEW(Bryce, 1);
   b->size = 48;
   b->name = eina_stringshare_add(name);
   z = strrchr(name, '_');
   b->zone = strtoul(z + 1, NULL, 10);
   b->anchor = an;
   b->orient = orient;
   b->layer = DEFAULT_LAYER;
   b->version = bryce_version;
   _bryce_create(b, parent);
   bryces->bryces = eina_list_append(bryces->bryces, b);
   e_config_save_queue();
   return b->bryce;
}

E_API void
e_bryce_orient(Evas_Object *bryce, E_Gadget_Site_Orient orient, E_Gadget_Site_Anchor an)
{
   const char *loc = NULL, *loc2 = NULL;
   char buf[1024], buf2[1024];

   BRYCE_GET(bryce);
   if ((b->orient == orient) && (b->anchor == an)) return;
   if (an & E_GADGET_SITE_ANCHOR_TOP)
     loc = "top";
   else if (an & E_GADGET_SITE_ANCHOR_BOTTOM)
     loc = "bottom";
   else if (an & E_GADGET_SITE_ANCHOR_LEFT)
     loc = "left";
   else if (an & E_GADGET_SITE_ANCHOR_RIGHT)
     loc = "right";
   if (an & E_GADGET_SITE_ANCHOR_RIGHT)
     loc2 = "right";
   else if (an & E_GADGET_SITE_ANCHOR_LEFT)
     loc2 = "left";
   else if (an & E_GADGET_SITE_ANCHOR_TOP)
     loc2 = "top";
   else if (an & E_GADGET_SITE_ANCHOR_BOTTOM)
     loc2 = "bottom";

   snprintf(buf, sizeof(buf), "__bryce%s", b->name);
   snprintf(buf2, sizeof(buf2), "__brycebryce_%s_%s_%d", loc, loc2, b->zone);
   e_gadget_site_rename(buf, buf2);
   b->orient = orient;
   b->anchor = an;
   snprintf(buf2, sizeof(buf2), "bryce_%s_%s_%d", loc, loc2, b->zone);
   eina_stringshare_replace(&b->name, buf2);
   _bryce_orient(b);
   _bryce_autosize(b);
}

E_API Evas_Object *
e_bryce_site_get(Evas_Object *bryce)
{
   BRYCE_GET(bryce);

   return b->site;
}

E_API Eina_Bool
e_bryce_autosize_get(Evas_Object *bryce)
{
   BRYCE_GET(bryce);
   return b->autosize;
}

E_API void
e_bryce_autosize_set(Evas_Object *bryce, Eina_Bool set)
{
   BRYCE_GET(bryce);
   set = !!set;

   if (b->autosize == set) return;
   b->autosize = set;
   if (set)
     {
        while (b->spacers)
          e_gadget_del(eina_list_data_get(b->spacers));
     }
   e_config_save_queue();
   _bryce_autosize(b);
}

E_API Eina_Bool
e_bryce_autohide_get(Evas_Object *bryce)
{
   BRYCE_GET(bryce);
   return b->autohide;
}

E_API void
e_bryce_autohide_set(Evas_Object *bryce, Eina_Bool set)
{
   BRYCE_GET(bryce);
   set = !!set;

   if (b->autohide == set) return;
   b->autohide = set;

   if (set)
     _bryce_autohide_setup(b);
   else
     {
        E_FREE_FUNC(b->autohide_event, evas_object_del);
        evas_object_event_callback_del_full(bryce, EVAS_CALLBACK_MOVE, _bryce_autohide_moveresize, b);
        evas_object_event_callback_del_full(bryce, EVAS_CALLBACK_RESIZE, _bryce_autohide_moveresize, b);
        if (!b->hidden) return;
        e_efx_move(b->bryce, E_EFX_EFFECT_SPEED_LINEAR, E_EFX_POINT(b->x, b->y), 0.5, _bryce_autohide_end, b);
        b->animating = 1;
        b->hidden = 0;
     }
   e_config_save_queue();
}

E_API Eina_List *
e_bryce_list(Evas_Object *parent)
{
   Eina_List *l, *ret = NULL;
   Bryce *b;

   if (!parent) parent = e_comp->elm;
   EINA_LIST_FOREACH(bryces->bryces, l, b)
     {
        if (!b->bryce) continue;
        if (parent == b->parent)
          ret = eina_list_append(ret, b->bryce);
     }
   return ret;
}

E_API Eina_Bool
e_bryce_exists(Evas_Object *parent, Evas_Object *bryce, E_Gadget_Site_Orient orient, E_Gadget_Site_Anchor an)
{
   Eina_List *l;
   Bryce *b;
   int zone = -1;

   if (!parent) parent = e_comp->elm;
   if (parent == e_comp->elm)
     {
        E_Shelf *es;
        E_Zone *z;

        z = e_zone_current_get();
        zone = z->num;
        /* FIXME: remove shelf block once shelves are dead */
        l = e_shelf_list_all();
        EINA_LIST_FREE(l, es)
          {
             if (es->zone != z) continue;
             switch (es->cfg->orient)
               {
#define ORIENT_CHECK(ORIENT, ANCHOR1, ANCHOR2) \
  if ((orient == E_GADGET_SITE_ORIENT_##ORIENT) && \
    ((an == (E_GADGET_SITE_ANCHOR_##ANCHOR1)) || \
      ((an & E_GADGET_SITE_ANCHOR_##ANCHOR2) && (!es->cfg->fit_along)))) \
        return EINA_TRUE; \
  break
                default: break;
                case E_GADCON_ORIENT_LEFT:
                  ORIENT_CHECK(VERTICAL, LEFT, LEFT);
                case E_GADCON_ORIENT_RIGHT:
                  ORIENT_CHECK(VERTICAL, RIGHT, RIGHT);
                case E_GADCON_ORIENT_TOP:
                  ORIENT_CHECK(HORIZONTAL, TOP, TOP);
                case E_GADCON_ORIENT_BOTTOM:
                  ORIENT_CHECK(HORIZONTAL, BOTTOM, BOTTOM);
                case E_GADCON_ORIENT_CORNER_TL:
                  ORIENT_CHECK(HORIZONTAL, TOP | E_GADGET_SITE_ANCHOR_LEFT, TOP);
                case E_GADCON_ORIENT_CORNER_TR:
                  ORIENT_CHECK(HORIZONTAL, TOP | E_GADGET_SITE_ANCHOR_RIGHT, TOP);
                case E_GADCON_ORIENT_CORNER_BL:
                  ORIENT_CHECK(HORIZONTAL, BOTTOM | E_GADGET_SITE_ANCHOR_LEFT, BOTTOM);
                case E_GADCON_ORIENT_CORNER_BR:
                  ORIENT_CHECK(HORIZONTAL, BOTTOM | E_GADGET_SITE_ANCHOR_RIGHT, BOTTOM);
                case E_GADCON_ORIENT_CORNER_LT:
                  ORIENT_CHECK(VERTICAL, LEFT | E_GADGET_SITE_ANCHOR_TOP, LEFT);
                case E_GADCON_ORIENT_CORNER_RT:
                  ORIENT_CHECK(VERTICAL, RIGHT | E_GADGET_SITE_ANCHOR_TOP, RIGHT);
                case E_GADCON_ORIENT_CORNER_LB:
                  ORIENT_CHECK(VERTICAL, LEFT | E_GADGET_SITE_ANCHOR_BOTTOM, LEFT);
                case E_GADCON_ORIENT_CORNER_RB:
                  ORIENT_CHECK(VERTICAL, RIGHT | E_GADGET_SITE_ANCHOR_BOTTOM, RIGHT);
#undef ORIENT_CHECK
               }
          }
          /* end FIXME */
     }
   EINA_LIST_FOREACH(bryces->bryces, l, b)
     {
        if (!b->bryce) continue;
        if (b->bryce == bryce) return EINA_FALSE;
        if (parent != b->parent) continue;
        if (b->orient != orient) continue;
        if ((zone >= 0) && ((int)b->zone != zone)) continue;
        if ((b->anchor & an) == an) return EINA_TRUE;
        if (b->autosize) continue;
        if (b->orient == E_GADGET_SITE_ORIENT_HORIZONTAL)
          {
             if ((b->anchor & E_GADGET_SITE_ANCHOR_BOTTOM) && (an & E_GADGET_SITE_ANCHOR_BOTTOM))
               return EINA_TRUE;
             if ((b->anchor & E_GADGET_SITE_ANCHOR_TOP) && (an & E_GADGET_SITE_ANCHOR_TOP))
               return EINA_TRUE;
          }
        else
          {
             if ((b->anchor & E_GADGET_SITE_ANCHOR_LEFT) && (an & E_GADGET_SITE_ANCHOR_LEFT))
               return EINA_TRUE;
             if ((b->anchor & E_GADGET_SITE_ANCHOR_RIGHT) && (an & E_GADGET_SITE_ANCHOR_RIGHT))
               return EINA_TRUE;
          }
     }
   return EINA_FALSE;
}

E_API void
e_bryce_style_set(Evas_Object *bryce, const char *style)
{
   BRYCE_GET(bryce);

   eina_stringshare_replace(&b->style, style);
   _bryce_style_apply(b);
   e_config_save_queue();
   evas_object_smart_callback_call(b->site, "gadget_site_style", NULL);
}

/* FIXME */
EINTERN void
e_bryce_save(void)
{
   e_config_domain_save("e_bryces", edd_bryces, bryces);
}

static void
bryce_spacer_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Bryce *b = data;

   b->spacers = eina_list_remove(b->spacers, obj);
}

static Evas_Object *
bryce_spacer_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient)
{
   Bryce *b;
   Eina_List *l;
   Evas_Object *r;

   if (!orient) return NULL;

   /* only allow on bryces */
   EINA_LIST_FOREACH(bryces->bryces, l, b)
     {
        if (b->editing || (parent == b->site)) break;
     }
   if (!b) return NULL;
   if (b->autosize) return NULL;
   r = evas_object_rectangle_add(e_comp->evas);
   evas_object_event_callback_add(r, EVAS_CALLBACK_DEL, bryce_spacer_del, b);
   b->spacers = eina_list_append(b->spacers, r);
   E_EXPAND(r);
   E_FILL(r);
   /* FIXME: this should be better for demo gadgets... */
   if (*id < 0)
     {
        evas_object_size_hint_aspect_set(r, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
        evas_object_color_set(r, 255, 0, 0, 255);
     }
   else
     evas_object_color_set(r, 0, 0, 0, 0);
   return r;
}

EINTERN void
e_bryce_init(void)
{
   resize_act = e_action_add("bryce_resize");
   e_action_predef_name_set(_("Bryces"), _("Resize Gadget Bar"), "bryce_resize", NULL, "syntax: step, example: 4", 1);
   resize_act->func.go_wheel = _bryce_act_resize;

   edd_bryce = E_CONFIG_DD_NEW("Bryce", Bryce);
   E_CONFIG_VAL(edd_bryce, Bryce, name, STR);
   E_CONFIG_VAL(edd_bryce, Bryce, style, STR);
   E_CONFIG_VAL(edd_bryce, Bryce, zone, UINT);
   E_CONFIG_VAL(edd_bryce, Bryce, size, INT);
   E_CONFIG_VAL(edd_bryce, Bryce, layer, UINT);
   E_CONFIG_VAL(edd_bryce, Bryce, autosize, UCHAR);
   E_CONFIG_VAL(edd_bryce, Bryce, autohide, UCHAR);
   E_CONFIG_VAL(edd_bryce, Bryce, orient, UINT);
   E_CONFIG_VAL(edd_bryce, Bryce, anchor, UINT);
   E_CONFIG_VAL(edd_bryce, Bryce, version, UINT);

   edd_bryces = E_CONFIG_DD_NEW("Bryces", Bryces);
   E_CONFIG_LIST(edd_bryces, Bryces, bryces, edd_bryce);
   bryces = e_config_domain_load("e_bryces", edd_bryces);

   if (bryces)
     {
        Eina_List *l;
        Bryce *b;

        EINA_LIST_FOREACH(bryces->bryces, l, b)
          {
             if (b->version < 2)
               {
                  /* I broke this the first time by forgetting the __bryce prefix :(
                   */
                  _bryce_rename(b, b->zone);
                  if (b->version < 1)
                    {
                       char buf[1024];

                       snprintf(buf, sizeof(buf), "%s_%u", b->name, b->zone);
                       eina_stringshare_replace(&b->name, buf);
                    }
               }
             b->version = bryce_version;
             if (!e_comp_zone_number_get(b->zone)) continue;
             b->layer = E_CLAMP(b->layer, E_LAYER_DESKTOP_TOP, E_LAYER_CLIENT_ABOVE);
             _bryce_create(b, e_comp->elm);
             evas_object_show(b->bryce);
          }
     }
   else
     bryces = E_NEW(Bryces, 1);

   evas_object_event_callback_add(e_comp->canvas->resize_object, EVAS_CALLBACK_RESIZE, _bryce_comp_resize, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_ZONE_USEFUL_GEOMETRY_CHANGED, _bryce_zone_useful_geometry_changed, NULL);
   e_gadget_type_add("Spacer Bar", bryce_spacer_create, NULL);
}

EINTERN void
e_bryce_shutdown(void)
{
   Bryce *b;

   E_CONFIG_DD_FREE(edd_bryce);
   E_CONFIG_DD_FREE(edd_bryces);
   EINA_LIST_FREE(bryces->bryces, b)
     {
        E_Zone *zone;
        void *obs;

        EINA_LIST_FREE(b->zone_obstacles, obs)
          {
             E_OBJECT_DEL_SET(obs, NULL);
             e_object_del(obs);
          }
        evas_object_event_callback_del(b->bryce, EVAS_CALLBACK_DEL, _bryce_del);
        EINA_LIST_FREE(b->popups, obs)
          evas_object_event_callback_del(obs, EVAS_CALLBACK_HIDE, _bryce_popup_hide);
        evas_object_hide(b->bryce);
        evas_object_del(b->bryce);
        evas_object_del(b->autohide_event);
        eina_stringshare_del(b->name);
        eina_stringshare_del(b->style);
        ecore_job_del(b->calc_job);
        ecore_timer_del(b->save_timer);
        ecore_timer_del(b->autohide_timer);
        zone = e_comp_zone_number_get(b->zone);
        if (zone)
          {
             evas_object_event_callback_del(zone->bg_clip_object, EVAS_CALLBACK_DEL, _bryce_zone_del);
             evas_object_event_callback_del(zone->bg_clip_object, EVAS_CALLBACK_MOVE, _bryce_zone_moveresize);
             evas_object_event_callback_del(zone->bg_clip_object, EVAS_CALLBACK_RESIZE, _bryce_zone_moveresize);
          }
        free(b->e_obj_inherit);
        free(b);
     }
   E_FREE_LIST(handlers, ecore_event_handler_del);
   E_FREE(bryces);
   e_gadget_type_del("Spacer Bar");
}
