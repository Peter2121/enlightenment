#include "e.h"
#include "e_mod_main.h"

/* gadcon requirements */
static E_Gadcon_Client *_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style);
static void             _gc_shutdown(E_Gadcon_Client *gcc);
static void             _gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient __UNUSED__);
static const char      *_gc_label(const E_Gadcon_Client_Class *client_class __UNUSED__);
static Evas_Object     *_gc_icon(const E_Gadcon_Client_Class *client_class __UNUSED__, Evas *evas);
static const char      *_gc_id_new(const E_Gadcon_Client_Class *client_class __UNUSED__);

/* and actually define the gadcon class that this module provides (just 1) */
static const E_Gadcon_Client_Class _gadcon_class =
{
   GADCON_CLIENT_CLASS_VERSION,
   "pager",
   {
      _gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon, _gc_id_new, NULL,
      e_gadcon_site_is_not_toolbar
   },
   E_GADCON_CLIENT_STYLE_INSET
};

/* actual module specifics */
typedef struct _Instance    Instance;
typedef struct _Pager       Pager;
typedef struct _Pager_Desk  Pager_Desk;
typedef struct _Pager_Win   Pager_Win;
typedef struct _Pager_Popup Pager_Popup;

struct _Instance
{
   E_Gadcon_Client *gcc;
   Evas_Object     *o_pager; /* table */
   Pager           *pager;
};

struct _Pager
{
   Instance       *inst;
   E_Drop_Handler *drop_handler;
   Pager_Popup    *popup;
   Evas_Object    *o_table;
   E_Zone         *zone;
   int             xnum, ynum;
   Eina_List      *desks;
   Pager_Desk     *active_pd;
   unsigned char   dragging : 1;
   unsigned char   just_dragged : 1;
   Evas_Coord      dnd_x, dnd_y;
   Pager_Desk     *active_drop_pd;
   Eina_Bool invert : 1;
};

struct _Pager_Desk
{
   Pager       *pager;
   E_Desk      *desk;
   Eina_List   *wins;
   Evas_Object *o_desk;
   Evas_Object *o_layout;
   Evas_Object *o_bg;
   int          xpos, ypos, urgent;
   int          current : 1;
   struct
   {
      Pager        *from_pager;
      unsigned char in_pager : 1;
      unsigned char start : 1;
      int           x, y, dx, dy, button;
   } drag;
};

struct _Pager_Win
{
   E_Client     *client;
   Pager_Desk   *desk;
   Evas_Object  *o_window;
   Evas_Object  *o_icon;
   unsigned char skip_winlist : 1;
   struct
   {
      Pager        *from_pager;
      unsigned char start : 1;
      unsigned char in_pager : 1;
      unsigned char desktop  : 1;
      int           x, y, dx, dy, button;
   } drag;
};

struct _Pager_Popup
{
   Evas_Object  *popup;
   Pager        *pager;
   Evas_Object  *o_bg;
   Ecore_Timer  *timer;
   unsigned char urgent : 1;
};

static void             _pager_desk_livethumb_setup(Pager_Desk *pd);
static void             _pager_cb_obj_moveresize(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__);
static void             _button_cb_mouse_down(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info);
static void             _pager_inst_cb_menu_configure(void *data __UNUSED__, E_Menu *m __UNUSED__, E_Menu_Item *mi __UNUSED__);
static void             _pager_inst_cb_menu_virtual_desktops_dialog(void *data, E_Menu *m __UNUSED__, E_Menu_Item *mi __UNUSED__);
static void             _pager_instance_drop_zone_recalc(Instance *inst);
static Eina_Bool        _pager_cb_event_bg_update(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_client_resize(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_client_move(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_client_add(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_client_remove(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_client_iconify(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_client_uniconify(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_client_stick(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_client_unstick(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_client_desk_set(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_client_stack(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_client_icon_change(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_client_urgent_change(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_client_focus_in(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_client_focus_out(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_client_property(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_zone_desk_count_set(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_desk_show(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_desk_name_change(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_cb_event_compositor_resize(void *data __UNUSED__, int type __UNUSED__, void *event);
static void             _pager_window_cb_mouse_down(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info);
static void             _pager_window_cb_mouse_up(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info);
static void             _pager_window_cb_mouse_move(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info);
static void            *_pager_window_cb_drag_convert(E_Drag *drag, const char *type);
static void             _pager_window_cb_drag_finished(E_Drag *drag, int dropped);
static void             _pager_drop_cb_enter(void *data, const char *type __UNUSED__, void *event_info);
static void             _pager_drop_cb_move(void *data, const char *type __UNUSED__, void *event_info);
static void             _pager_drop_cb_leave(void *data, const char *type __UNUSED__, void *event_info __UNUSED__);
static void             _pager_drop_cb_drop(void *data, const char *type, void *event_info);
static void             _pager_inst_cb_scroll(void *data);
static void             _pager_update_drop_position(Pager *p, Evas_Coord x, Evas_Coord y);
static void             _pager_desk_cb_mouse_down(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info);
static void             _pager_desk_cb_mouse_up(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info);
static void             _pager_desk_cb_mouse_move(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info);
static void             _pager_desk_cb_drag_finished(E_Drag *drag, int dropped);
static void             _pager_desk_cb_mouse_wheel(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info);
static Eina_Bool        _pager_popup_cb_timeout(void *data);
static Pager           *_pager_new(Evas *evas, E_Zone *zone, E_Gadcon *gc);
static void             _pager_free(Pager *p);
static void             _pager_fill(Pager *p, E_Gadcon *gc);
static void             _pager_empty(Pager *p);
static Pager_Desk      *_pager_desk_new(Pager *p, E_Desk *desk, int xpos, int ypos, Eina_Bool invert);
static void             _pager_desk_free(Pager_Desk *pd);
static Pager_Desk      *_pager_desk_at_coord(Pager *p, Evas_Coord x, Evas_Coord y);
static void             _pager_desk_select(Pager_Desk *pd);
static Pager_Desk      *_pager_desk_find(Pager *p, E_Desk *desk);
static void             _pager_desk_switch(Pager_Desk *pd1, Pager_Desk *pd2);
static Pager_Win       *_pager_window_new(Pager_Desk *pd, E_Client *client);
static void             _pager_window_free(Pager_Win *pw);
static void             _pager_window_move(Pager_Win *pw);
static Pager_Win       *_pager_window_find(Pager *p, E_Client *client);
static Pager_Win       *_pager_desk_window_find(Pager_Desk *pd, E_Client *client);
static Pager_Popup     *_pager_popup_new(E_Zone *zone, int keyaction);
static void             _pager_popup_free(Pager_Popup *pp);
static Pager_Popup     *_pager_popup_find(E_Zone *zone);

/* functions for pager popup on key actions */
static int              _pager_popup_show(void);
static void             _pager_popup_hide(int switch_desk);
static Eina_Bool        _pager_popup_cb_mouse_down(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_popup_cb_mouse_up(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_popup_cb_mouse_move(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_popup_cb_mouse_wheel(void *data __UNUSED__, int type __UNUSED__, void *event);
static void             _pager_popup_desk_switch(int x, int y);
static void             _pager_popup_modifiers_set(int mod);
static Eina_Bool        _pager_popup_cb_key_down(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool        _pager_popup_cb_key_up(void *data __UNUSED__, int type __UNUSED__, void *event);
static void             _pager_popup_cb_action_show(E_Object *obj __UNUSED__, const char *params __UNUSED__, Ecore_Event_Key *ev __UNUSED__);
static void             _pager_popup_cb_action_switch(E_Object *obj __UNUSED__, const char *params, Ecore_Event_Key *ev);

/* variables for pager popup on key actions */
static E_Action *act_popup_show = NULL;
static E_Action *act_popup_switch = NULL;
static Ecore_Window input_window = 0;
static Eina_List *handlers = NULL;
static Pager_Popup *act_popup = NULL; /* active popup */
static int hold_count = 0;
static int hold_mod = 0;
static E_Desk *current_desk = NULL;
static E_Config_DD *conf_edd = NULL;
static Eina_List *pagers = NULL;

Config *pager_config = NULL;

static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   Pager *p;
   Evas_Object *o;
   E_Gadcon_Client *gcc;
   Instance *inst;
   Evas_Coord x, y, w, h;
   const char *drop[] =
   {
      "enlightenment/pager_win", "enlightenment/border",
      "enlightenment/vdesktop"
   };

   inst = E_NEW(Instance, 1);

   p = _pager_new(gc->evas, gc->zone, gc);
   p->inst = inst;
   inst->pager = p;
   o = p->o_table;
   gcc = e_gadcon_client_new(gc, name, id, style, o);
   gcc->data = inst;

   inst->gcc = gcc;
   inst->o_pager = o;

   evas_object_geometry_get(o, &x, &y, &w, &h);
   p->drop_handler =
     e_drop_handler_add(E_OBJECT(inst->gcc), p,
                        _pager_drop_cb_enter, _pager_drop_cb_move,
                        _pager_drop_cb_leave, _pager_drop_cb_drop,
                        drop, 3, x, y, w, h);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOVE,
                                  _pager_cb_obj_moveresize, inst);
   evas_object_event_callback_add(o, EVAS_CALLBACK_RESIZE,
                                  _pager_cb_obj_moveresize, inst);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                  _button_cb_mouse_down, inst);
   pager_config->instances = eina_list_append(pager_config->instances, inst);
   return gcc;
}

static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
   Instance *inst;

   inst = gcc->data;
   pager_config->instances = eina_list_remove(pager_config->instances, inst);
   e_drop_handler_del(inst->pager->drop_handler);
   _pager_free(inst->pager);
   free(inst);
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient __UNUSED__)
{
   Instance *inst;
   int aspect_w, aspect_h;
   double aspect_ratio;

   inst = gcc->data;
   if (inst->pager->invert)
     {
        aspect_w = inst->pager->ynum * inst->pager->zone->w;
        aspect_h = inst->pager->xnum * inst->pager->zone->h;
     }
   else
     {
        aspect_w = inst->pager->xnum * inst->pager->zone->w;
        aspect_h = inst->pager->ynum * inst->pager->zone->h;
     }

   e_gadcon_client_aspect_set(gcc, aspect_w, aspect_h);
   aspect_ratio = (double)aspect_w / (double)aspect_h;

   if (aspect_ratio > 1.0)
     e_gadcon_client_min_size_set(gcc, 16 * aspect_ratio, 16);
   else
     e_gadcon_client_min_size_set(gcc, 16, 16 * aspect_ratio);
}

static const char *
_gc_label(const E_Gadcon_Client_Class *client_class __UNUSED__)
{
   return _("Pager");
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class __UNUSED__, Evas *evas)
{
   Evas_Object *o;
   char buf[PATH_MAX];

   o = edje_object_add(evas);
   snprintf(buf, sizeof(buf), "%s/e-module-pager-plain.edj",
            e_module_dir_get(pager_config->module));
   edje_object_file_set(o, buf, "icon");
   return o;
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class)
{
   static char buf[4096];

   snprintf(buf, sizeof(buf), "%s.%d", client_class->name,
            eina_list_count(pager_config->instances) + 1);
   return buf;
}

static Pager *
_pager_new(Evas *evas, E_Zone *zone, E_Gadcon *gc)
{
   Pager *p;

   p = E_NEW(Pager, 1);
   p->inst = NULL;
   p->popup = NULL;
   p->o_table = elm_table_add(e_win_evas_win_get(evas));
   elm_table_homogeneous_set(p->o_table, 1);
   p->zone = zone;
   _pager_fill(p, gc);
   pagers = eina_list_append(pagers, p);
   return p;
}

static void
_pager_free(Pager *p)
{
   _pager_empty(p);
   evas_object_del(p->o_table);
   pagers = eina_list_remove(pagers, p);
   free(p);
}

static void
_pager_fill(Pager *p, E_Gadcon *gc)
{
   int x, y;

   if (gc)
     {
        switch (gc->orient)
          {

             case E_GADCON_ORIENT_TOP:
             case E_GADCON_ORIENT_BOTTOM:
             case E_GADCON_ORIENT_CORNER_TL:
             case E_GADCON_ORIENT_CORNER_TR:
             case E_GADCON_ORIENT_CORNER_BL:
             case E_GADCON_ORIENT_CORNER_BR:
             case E_GADCON_ORIENT_HORIZ:
             case E_GADCON_ORIENT_FLOAT:
               p->invert = EINA_FALSE;
               break;
             case E_GADCON_ORIENT_VERT:
             case E_GADCON_ORIENT_LEFT:
             case E_GADCON_ORIENT_RIGHT:
             case E_GADCON_ORIENT_CORNER_LT:
             case E_GADCON_ORIENT_CORNER_RT:
             case E_GADCON_ORIENT_CORNER_LB:
             case E_GADCON_ORIENT_CORNER_RB:
             default:
               p->invert = EINA_TRUE;
          }
     }
   e_zone_desk_count_get(p->zone, &(p->xnum), &(p->ynum));
   if (p->ynum != 1) p->invert = EINA_FALSE;
   for (x = 0; x < p->xnum; x++)
     {
        for (y = 0; y < p->ynum; y++)
          {
             Pager_Desk *pd;
             E_Desk *desk;

             desk = e_desk_at_xy_get(p->zone, x, y);
             if (desk)
               {
                  pd = _pager_desk_new(p, desk, x, y, p->invert);
                  if (pd)
                    {
                       p->desks = eina_list_append(p->desks, pd);
                       if (desk == e_desk_current_get(desk->zone))
                         _pager_desk_select(pd);
                    }
               }
          }
     }
}

static void
_pager_empty(Pager *p)
{
   p->active_pd = NULL;
   while (p->desks)
     {
        _pager_desk_free(p->desks->data);
        p->desks = eina_list_remove_list(p->desks, p->desks);
     }
}

static void
_pager_desk_livethumb_setup(Pager_Desk *pd)
{
   const char *bgfile;
   Evas *e;
   Evas_Object *o;

   e = evas_object_evas_get(pd->pager->o_table);

   if (!pd->o_bg)
     {
        pd->o_bg = e_livethumb_add(e);
        e_livethumb_vsize_set(pd->o_bg, pd->desk->zone->w / 16, pd->desk->zone->h / 16);
//        edje_extern_object_aspect_set(pd->o_bg, EDJE_ASPECT_CONTROL_NEITHER,
//                                      pd->desk->zone->w / 16, pd->desk->zone->h / 16);
        edje_object_part_swallow(pd->o_desk, "e.background", pd->o_bg);
     }

   o = e_livethumb_thumb_get(pd->o_bg);
   if (!o) o = edje_object_add(e_livethumb_evas_get(pd->o_bg));
   bgfile = e_bg_file_get(pd->desk->zone->comp->num, pd->desk->zone->num, pd->desk->x, pd->desk->y);
   edje_object_file_set(o, bgfile, "e/desktop/background");
   e_livethumb_thumb_set(pd->o_bg, o);
   eina_stringshare_del(bgfile);
}

static Pager_Desk *
_pager_desk_new(Pager *p, E_Desk *desk, int xpos, int ypos, Eina_Bool invert)
{
   Pager_Desk *pd;
   Evas_Object *o, *evo;
   E_Client *ec;
   int w, h;
   Evas *e;

   if (!desk) return NULL;
   pd = E_NEW(Pager_Desk, 1);
   if (!pd) return NULL;

   pd->xpos = xpos;
   pd->ypos = ypos;
   pd->urgent = 0;
   pd->desk = desk;
   e_object_ref(E_OBJECT(desk));
   pd->pager = p;

   e = evas_object_evas_get(p->o_table);
   o = edje_object_add(e);
   pd->o_desk = o;
   e_theme_edje_object_set(o, "base/theme/modules/pager",
                           "e/modules/pager/desk");
   edje_object_part_text_set(o, "e.text.label", desk->name);
   if (pager_config->show_desk_names)
     edje_object_signal_emit(o, "e,name,show", "e");

   if (pager_config->disable_live_preview)
     edje_object_signal_emit(pd->o_desk, "e,preview,off", "e");
   else
     {
        _pager_desk_livethumb_setup(pd);
        edje_object_signal_emit(pd->o_desk, "e,preview,on", "e");
     }

   edje_object_size_min_calc(o, &w, &h);
   E_EXPAND(o);
   E_FILL(o);
   evas_object_size_hint_min_set(o, w, h);
   if (invert)
     elm_table_pack(p->o_table, o, ypos, xpos, 1, 1);
   else
     elm_table_pack(p->o_table, o, xpos, ypos, 1, 1);

   evo = (Evas_Object *)edje_object_part_object_get(o, "e.eventarea");
   if (!evo) evo = o;

   evas_object_event_callback_add(evo, EVAS_CALLBACK_MOUSE_DOWN,
                                  _pager_desk_cb_mouse_down, pd);
   evas_object_event_callback_add(evo, EVAS_CALLBACK_MOUSE_UP,
                                  _pager_desk_cb_mouse_up, pd);
   evas_object_event_callback_add(evo, EVAS_CALLBACK_MOUSE_MOVE,
                                  _pager_desk_cb_mouse_move, pd);
   evas_object_event_callback_add(evo, EVAS_CALLBACK_MOUSE_WHEEL,
                                  _pager_desk_cb_mouse_wheel, pd);
   evas_object_show(o);

   o = e_layout_add(e);
   pd->o_layout = o;

   e_layout_virtual_size_set(o, desk->zone->w, desk->zone->h);
   edje_object_part_swallow(pd->o_desk, "e.swallow.content", pd->o_layout);
   evas_object_show(o);

   E_CLIENT_FOREACH(desk->zone->comp, ec)
     {
        Pager_Win *pw;

        if (e_client_util_ignored_get(ec)) continue;
        if ((ec->new_client) || (ec->zone != desk->zone) ||
            ((ec->desk != desk) && (!ec->sticky)))
          continue;
        pw = _pager_window_new(pd, ec);
        if (pw) pd->wins = eina_list_append(pd->wins, pw);
     }
   return pd;
}

static void
_pager_desk_free(Pager_Desk *pd)
{
   Pager_Win *w;

   evas_object_del(pd->o_desk);
   evas_object_del(pd->o_bg);
   evas_object_del(pd->o_layout);
   EINA_LIST_FREE(pd->wins, w)
     _pager_window_free(w);
   e_object_unref(E_OBJECT(pd->desk));
   free(pd);
}

static Pager_Desk *
_pager_desk_at_coord(Pager *p, Evas_Coord x, Evas_Coord y)
{
   Eina_List *l;
   Pager_Desk *pd;

   EINA_LIST_FOREACH(p->desks, l, pd)
     {
        Evas_Coord dx, dy, dw, dh;

        evas_object_geometry_get(pd->o_desk, &dx, &dy, &dw, &dh);
        if (E_INSIDE(x, y, dx, dy, dw, dh)) return pd;
     }
   return NULL;
}

static void
_pager_desk_select(Pager_Desk *pd)
{
   if (pd->current) return;
   if (pd->pager->active_pd)
     {
        pd->pager->active_pd->current = 0;
        edje_object_signal_emit(pd->pager->active_pd->o_desk, "e,state,unselected", "e");
     }
   pd->current = 1;
   evas_object_raise(pd->o_desk);
   edje_object_signal_emit(pd->o_desk, "e,state,selected", "e");
   pd->pager->active_pd = pd;
}

static Pager_Desk *
_pager_desk_find(Pager *p, E_Desk *desk)
{
   Eina_List *l;
   Pager_Desk *pd;

   EINA_LIST_FOREACH(p->desks, l, pd)
     if (pd->desk == desk) return pd;

   return NULL;
}

static void
_pager_desk_switch(Pager_Desk *pd1, Pager_Desk *pd2)
{
   int c;
   E_Zone *zone1, *zone2;
   E_Desk *desk1, *desk2;
   Pager_Win *pw;
   Eina_List *l;

   if ((!pd1) || (!pd2) || (!pd1->desk) || (!pd2->desk)) return;
   if (pd1 == pd2) return;

   desk1 = pd1->desk;
   desk2 = pd2->desk;
   zone1 = pd1->desk->zone;
   zone2 = pd2->desk->zone;

   /* Move opened windows from on desk to the other */
   EINA_LIST_FOREACH(pd1->wins, l, pw)
     {
        if ((!pw) || (!pw->client) || (pw->client->iconic)) continue;
        pw->client->hidden = 0;
        e_client_desk_set(pw->client, desk2);
     }
   EINA_LIST_FOREACH(pd2->wins, l, pw)
     {
        if ((!pw) || (!pw->client) || (pw->client->iconic)) continue;
        pw->client->hidden = 0;
        e_client_desk_set(pw->client, desk1);
     }

   /* Modify desktop names in the config */
   for (l = e_config->desktop_names, c = 0; l && c < 2; l = l->next)
     {
        E_Config_Desktop_Name *tmp_dn;

        tmp_dn = l->data;
        if (!tmp_dn) continue;
        if ((tmp_dn->desk_x == desk1->x) &&
            (tmp_dn->desk_y == desk1->y) &&
            (tmp_dn->zone == (int)desk1->zone->num))
          {
             tmp_dn->desk_x = desk2->x;
             tmp_dn->desk_y = desk2->y;
             tmp_dn->zone = desk2->zone->num;
             c++;
          }
        else if ((tmp_dn->desk_x == desk2->x) &&
                 (tmp_dn->desk_y == desk2->y) &&
                 (tmp_dn->zone == (int)desk2->zone->num))
          {
             tmp_dn->desk_x = desk1->x;
             tmp_dn->desk_y = desk1->y;
             tmp_dn->zone = desk1->zone->num;
             c++;
          }
     }
   if (c > 0) e_config_save();
   e_desk_name_update();

   /* Modify desktop backgrounds in the config */
   for (l = e_config->desktop_backgrounds, c = 0; l && c < 2; l = l->next)
     {
        E_Config_Desktop_Background *tmp_db;

        tmp_db = l->data;
        if (!tmp_db) continue;
        if ((tmp_db->desk_x == desk1->x) &&
            (tmp_db->desk_y == desk1->y) &&
            (tmp_db->zone == (int)desk1->zone->num))
          {
             tmp_db->desk_x = desk2->x;
             tmp_db->desk_y = desk2->y;
             tmp_db->zone = desk2->zone->num;
             c++;
          }
        else if ((tmp_db->desk_x == desk2->x) &&
                 (tmp_db->desk_y == desk2->y) &&
                 (tmp_db->zone == (int)desk2->zone->num))
          {
             tmp_db->desk_x = desk1->x;
             tmp_db->desk_y = desk1->y;
             tmp_db->zone = desk1->zone->num;
             c++;
          }
     }
   if (c > 0) e_config_save();

   /* If the current desktop has been switched, force to update of the screen */
   if (desk2 == e_desk_current_get(zone2))
     {
        desk2->visible = 0;
        e_desk_show(desk2);
     }
   if (desk1 == e_desk_current_get(zone1))
     {
        desk1->visible = 0;
        e_desk_show(desk1);
     }
}

static Pager_Win *
_pager_window_new(Pager_Desk *pd, E_Client *client)
{
   Pager_Win *pw;
   Evas_Object *o;
   int visible;

   if (!client) return NULL;
   pw = E_NEW(Pager_Win, 1);
   if (!pw) return NULL;

   pw->client = client;
   e_object_ref(E_OBJECT(client));

   visible = ((!client->iconic) && (!client->netwm.state.skip_pager));
   pw->skip_winlist = client->netwm.state.skip_pager;
   pw->desk = pd;

   o = edje_object_add(evas_object_evas_get(pd->pager->o_table));
   pw->o_window = o;
   e_theme_edje_object_set(o, "base/theme/modules/pager",
                           "e/modules/pager/window");
   if (visible) evas_object_show(o);

   e_layout_pack(pd->o_layout, pw->o_window);
   e_layout_child_raise(pw->o_window);

   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                  _pager_window_cb_mouse_down, pw);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_UP,
                                  _pager_window_cb_mouse_up, pw);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_MOVE,
                                  _pager_window_cb_mouse_move, pw);

   o = e_client_icon_add(client, evas_object_evas_get(pd->pager->o_table));
   if (o)
     {
        pw->o_icon = o;
        evas_object_show(o);
        edje_object_part_swallow(pw->o_window, "e.swallow.icon", o);
     }

   if (client->urgent)
     {
        if (!(client->iconic))
          edje_object_signal_emit(pd->o_desk, "e,state,urgent", "e");
        edje_object_signal_emit(pw->o_window, "e,state,urgent", "e");
     }

   evas_object_show(o);

   _pager_window_move(pw);
   return pw;
}

static void
_pager_window_free(Pager_Win *pw)
{
   if ((pw->drag.from_pager) && (pw->desk->pager->dragging))
     pw->desk->pager->dragging = 0;
   if (pw->o_window) evas_object_del(pw->o_window);
   if (pw->o_icon) evas_object_del(pw->o_icon);
   e_object_unref(E_OBJECT(pw->client));
   free(pw);
}

static void
_pager_window_move(Pager_Win *pw)
{
   e_layout_child_move(pw->o_window,
                       pw->client->x - pw->client->zone->x,
                       pw->client->y - pw->client->zone->y);
   e_layout_child_resize(pw->o_window, pw->client->w, pw->client->h);
}

static Pager_Win *
_pager_window_find(Pager *p, E_Client *client)
{
   Eina_List *l;
   Pager_Desk *pd;

   EINA_LIST_FOREACH(p->desks, l, pd)
     {
        Pager_Win *pw;

        pw = _pager_desk_window_find(pd, client);
        if (pw) return pw;
     }
   return NULL;
}

static Pager_Win *
_pager_desk_window_find(Pager_Desk *pd, E_Client *client)
{
   Eina_List *l;
   Pager_Win *pw;

   EINA_LIST_FOREACH(pd->wins, l, pw)
     if (pw->client == client) return pw;

   return NULL;
}

static void
_pager_popup_cb_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Pager_Popup *pp = data;
   E_FREE_FUNC(pp->timer, ecore_timer_del);
   _pager_free(pp->pager);
   free(pp);
}

static Pager_Popup *
_pager_popup_new(E_Zone *zone, int keyaction)
{
   Pager_Popup *pp;
   Evas_Coord w, h, zx, zy, zw, zh;
   int x, y, height, width;
   E_Desk *desk;

   pp = E_NEW(Pager_Popup, 1);
   if (!pp) return NULL;

   /* Show popup */
   pp->pager = _pager_new(zone->comp->evas, zone, NULL);

   pp->pager->popup = pp;
   pp->urgent = 0;

   e_zone_desk_count_get(zone, &x, &y);

   if (keyaction)
     height = pager_config->popup_act_height * y;
   else
     height = pager_config->popup_height * y;

   width = height * (zone->w * x) / (zone->h * y);

   evas_object_move(pp->pager->o_table, 0, 0);
   evas_object_resize(pp->pager->o_table, width, height);

   pp->o_bg = edje_object_add(zone->comp->evas);
   evas_object_name_set(pp->o_bg, "pager_popup");
   e_theme_edje_object_set(pp->o_bg, "base/theme/modules/pager",
                           "e/modules/pager/popup");
   desk = e_desk_current_get(zone);
   if (desk)
     edje_object_part_text_set(pp->o_bg, "e.text.label", desk->name);

   evas_object_size_hint_min_set(pp->pager->o_table, width, height);
   edje_object_part_swallow(pp->o_bg, "e.swallow.content", pp->pager->o_table);
   edje_object_size_min_calc(pp->o_bg, &w, &h);

   pp->popup = e_comp_object_util_add(pp->o_bg, E_COMP_OBJECT_TYPE_POPUP);
   evas_object_layer_set(pp->popup, E_LAYER_CLIENT_POPUP);
   evas_object_pass_events_set(pp->popup, 1);
   e_zone_useful_geometry_get(zone, &zx, &zy, &zw, &zh);
   evas_object_geometry_set(pp->popup, zx, zy, w, h);
   e_comp_object_util_center(pp->popup);
   evas_object_event_callback_add(pp->popup, EVAS_CALLBACK_DEL, _pager_popup_cb_del, pp);
   evas_object_show(pp->popup);

   pp->timer = NULL;

   return pp;
}

static void
_pager_popup_free(Pager_Popup *pp)
{
   E_FREE_FUNC(pp->timer, ecore_timer_del);
   evas_object_hide(pp->popup);
   evas_object_del(pp->popup);
}

static Pager_Popup *
_pager_popup_find(E_Zone *zone)
{
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     if ((p->popup) && (p->zone == zone))
       return p->popup;

   return NULL;
}

static void
_pager_cb_obj_moveresize(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   Instance *inst;

   inst = data;
   _pager_instance_drop_zone_recalc(inst);
}

static void
_button_cb_mouse_down(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info)
{
   Instance *inst;
   Evas_Event_Mouse_Down *ev;
   E_Menu *m;
   E_Menu_Item *mi;
   int cx, cy;

   inst = data;
   ev = event_info;
   if (ev->button != 3) return;
   if (inst->gcc->menu) return;

   m = e_menu_new();
   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Settings"));
   e_util_menu_item_theme_icon_set(mi, "configure");
   e_menu_item_callback_set(mi, _pager_inst_cb_menu_configure, NULL);

   m = e_gadcon_client_util_menu_items_append(inst->gcc, m, 0);
   if (e_configure_registry_exists("screen/virtual_desktops"))
     {
        mi = e_menu_item_new_relative(m, NULL);
        e_menu_item_label_set(mi, _("Virtual Desktops Settings"));
        e_util_menu_item_theme_icon_set(mi, "preferences-desktop");
        e_menu_item_callback_set(mi, _pager_inst_cb_menu_virtual_desktops_dialog, inst);
     }

   e_gadcon_canvas_zone_geometry_get(inst->gcc->gadcon, &cx, &cy,
                                     NULL, NULL);
   e_menu_activate_mouse(m, e_util_zone_current_get(e_manager_current_get()),
                         cx + ev->output.x, cy + ev->output.y, 1, 1,
                         E_MENU_POP_DIRECTION_DOWN, ev->timestamp);
   evas_event_feed_mouse_up(inst->gcc->gadcon->evas, ev->button,
                            EVAS_BUTTON_NONE, ev->timestamp, NULL);
}

static void
_pager_inst_cb_menu_configure(void *data __UNUSED__, E_Menu *m __UNUSED__, E_Menu_Item *mi __UNUSED__)
{
   if (!pager_config) return;
   if (pager_config->config_dialog) return;
   /* FIXME: pass zone config item */
   _config_pager_module(NULL);
}

static E_Config_Dialog *
_pager_config_dialog(Evas_Object *parent __UNUSED__, const char *params __UNUSED__)
{
   if (!pager_config) return NULL;
   if (pager_config->config_dialog) return NULL;
   /* FIXME: pass zone config item */
   _config_pager_module(NULL);
   return pager_config->config_dialog;
}

static void
_pager_inst_cb_menu_virtual_desktops_dialog(void *data EINA_UNUSED, E_Menu *m __UNUSED__, E_Menu_Item *mi __UNUSED__)
{
   e_configure_registry_call("screen/virtual_desktops", NULL, NULL);
}

static void
_pager_instance_drop_zone_recalc(Instance *inst)
{
   Evas_Coord x, y, w, h;

   e_gadcon_client_viewport_geometry_get(inst->gcc, &x, &y, &w, &h);
   e_drop_handler_geometry_set(inst->pager->drop_handler, x, y, w, h);
}

EINTERN void
_pager_cb_config_updated(void)
{
   Pager *p;
   Pager_Desk *pd;
   Eina_List *l, *ll;
   if (!pager_config) return;
   EINA_LIST_FOREACH(pagers, l, p)
     EINA_LIST_FOREACH(p->desks, ll, pd)
       {
          if (pager_config->disable_live_preview)
            {
               if (pd->o_bg) evas_object_del(pd->o_bg);
               pd->o_bg = NULL;
               edje_object_signal_emit(pd->o_desk, "e,preview,off", "e");
            }
          else
            {
               _pager_desk_livethumb_setup(pd);
               edje_object_signal_emit(pd->o_desk, "e,preview,on", "e");
            }
          if (pd->current)
            edje_object_signal_emit(pd->o_desk, "e,state,selected", "e");
          else
            edje_object_signal_emit(pd->o_desk, "e,state,unselected", "e");
          if (pager_config->show_desk_names)
            edje_object_signal_emit(pd->o_desk, "e,name,show", "e");
          else
            edje_object_signal_emit(pd->o_desk, "e,name,hide", "e");
       }
}

static Eina_Bool
_pager_cb_event_client_resize(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client *ev = event;
   Eina_List *l, *l2;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        Pager_Desk *pd;

        if (p->zone != ev->ec->zone) continue;
        EINA_LIST_FOREACH(p->desks, l2, pd)
          {
             Pager_Win *pw;

             pw = _pager_desk_window_find(pd, ev->ec);
             if (pw) _pager_window_move(pw);
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_move(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client *ev = event;
   Eina_List *l;
   Pager_Win *pw;
   Pager_Desk *pd;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        Eina_List *l2;

        if (p->zone != ev->ec->zone) continue;
        EINA_LIST_FOREACH(p->desks, l2, pd)
          {
             pw = _pager_desk_window_find(pd, ev->ec);
             if (pw) _pager_window_move(pw);
          }
     }

   if ((act_popup) && (act_popup->pager->zone == ev->ec->zone))
     {
        EINA_LIST_FOREACH(act_popup->pager->desks, l, pd)
          {
             pw = _pager_desk_window_find(pd, ev->ec);
             if (pw) _pager_window_move(pw);
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_add(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client *ev = event;
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        Pager_Desk *pd;
        Pager_Win *pw;

        if ((p->zone != ev->ec->zone) ||
            (_pager_window_find(p, ev->ec)))
          continue;
        pd = _pager_desk_find(p, ev->ec->desk);
        if (!pd) continue;
        pw = _pager_window_new(pd, ev->ec);
        if (pw) pd->wins = eina_list_append(pd->wins, pw);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_remove(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client *ev = event;
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        Eina_List *l2;
        Pager_Desk *pd;

        if (p->zone != ev->ec->zone) continue;

        EINA_LIST_FOREACH(p->desks, l2, pd)
          {
             Pager_Win *pw;

             pw = _pager_desk_window_find(pd, ev->ec);
             if (!pw) continue;
             pd->wins = eina_list_remove(pd->wins, pw);
             _pager_window_free(pw);
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_iconify(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client *ev = event;
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        Eina_List *l2;
        Pager_Desk *pd;

        if (p->zone != ev->ec->zone) continue;

        EINA_LIST_FOREACH(p->desks, l2, pd)
          {
             Pager_Win *pw;

             pw = _pager_desk_window_find(pd, ev->ec);
             if (!pw) continue;
             if ((pw->drag.from_pager) && (pw->desk->pager->dragging))
               pw->desk->pager->dragging = 0;
             evas_object_hide(pw->o_window);
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_uniconify(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client *ev = event;
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        Eina_List *l2;
        Pager_Desk *pd;

        if (p->zone != ev->ec->zone) continue;

        EINA_LIST_FOREACH(p->desks, l2, pd)
          {
             Pager_Win *pw;

             pw = _pager_desk_window_find(pd, ev->ec);
             if ((pw) && (!pw->skip_winlist)) evas_object_show(pw->o_window);
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_stick(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client_Property *ev = event;
   Eina_List *l;
   Pager *p;

   if ((!(ev->property & E_CLIENT_PROPERTY_STICKY)) || (!ev->ec->sticky)) return ECORE_CALLBACK_RENEW;
   EINA_LIST_FOREACH(pagers, l, p)
     {
        Eina_List *l2;
        Pager_Desk *pd;
        Pager_Win *pw;

        if (p->zone != ev->ec->zone) continue;
        pw = _pager_window_find(p, ev->ec);
        if (!pw) continue;

        EINA_LIST_FOREACH(p->desks, l2, pd)
          if ((ev->ec->desk != pd->desk) && (!_pager_desk_window_find(pd, ev->ec)))
            {
               pw = _pager_window_new(pd, ev->ec);
               if (pw) pd->wins = eina_list_append(pd->wins, pw);
            }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_unstick(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client_Property *ev = event;
   Eina_List *l;
   Pager *p;

   if ((!(ev->property & E_CLIENT_PROPERTY_STICKY)) || (ev->ec->sticky)) return ECORE_CALLBACK_RENEW;
   EINA_LIST_FOREACH(pagers, l, p)
     {
        Pager_Desk *pd;
        Eina_List *l2;

        if (p->zone != ev->ec->zone) continue;

        EINA_LIST_FOREACH(p->desks, l2, pd)
          if (ev->ec->desk != pd->desk)
            {
               Pager_Win *pw;

               pw = _pager_desk_window_find(pd, ev->ec);
               if (pw)
                 {
                    pd->wins = eina_list_remove(pd->wins, pw);
                    _pager_window_free(pw);
                 }
            }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static void
_pager_window_desk_change(Pager *pager, E_Client *ec)
{
   Eina_List *l;
   Pager_Win *pw;
   Pager_Desk *pd;

   /* if this pager is not for the zone of the client */
   if (pager->zone != ec->zone)
     {
        /* look at all desks in the pager */
        EINA_LIST_FOREACH(pager->desks, l, pd)
          {
             /* find this client in this desk */
             pw = _pager_desk_window_find(pd, ec);
             if (!pw) continue;
             /* if it is found - remove it. it does not belong in this
              * pager as it probably moves zones */
             pd->wins = eina_list_remove(pd->wins, pw);
             _pager_window_free(pw);
          }
        return;
     }
   /* and this pager zone is for this client */
   /* see if the window is in this pager at all */
   pw = _pager_window_find(pager, ec);
   if (pw)
     {
        /* is it sticky */
        if (ec->sticky)
          {
             /* if its sticky and in this pager - its already everywhere, so abort
              * doing anything else */
             return;
          }
        /* move it to the right desk */
        /* find the pager desk of the target desk */
        pd = _pager_desk_find(pager, ec->desk);
        if (pd)
          {
             Pager_Win *pw2 = NULL;
             E_Client *ec_above;

             /* remove it from whatever desk it was on */
             pw->desk->wins = eina_list_remove(pw->desk->wins, pw);
             e_layout_unpack(pw->o_window);

             /* add it to the one its MEANT to be on */
             pw->desk = pd;
             pd->wins = eina_list_append(pd->wins, pw);
             e_layout_pack(pd->o_layout, pw->o_window);

             ec_above = e_util_desk_client_above(pw->client);
             if (ec_above)
               pw2 = _pager_desk_window_find(pd, ec_above);
             if (pw2)
               e_layout_child_lower_below(pw->o_window, pw2->o_window);
             else
               e_layout_child_raise(pw->o_window);

             _pager_window_move(pw);
          }
     }
   /* the client isn't in this pager at all - it must have moved zones */
   else
     {
        if (!ec->sticky)
          {
             /* find the pager desk it needs to go to */
             pd = _pager_desk_find(pager, ec->desk);
             if ((pd) && (!_pager_desk_window_find(pd, ec)))
               {
                  /* create it and add it */
                  pw = _pager_window_new(pd, ec);
                  if (pw)
                    {
                       Pager_Win *pw2 = NULL;
                       E_Client *ec_above;

                       pd->wins = eina_list_append(pd->wins, pw);
                       ec_above = e_util_desk_client_above(pw->client);
                       if (ec_above)
                         pw2 = _pager_desk_window_find(pd, ec_above);
                       if (pw2)
                         e_layout_child_lower_below(pw->o_window, pw2->o_window);
                       else
                         e_layout_child_raise(pw->o_window);
                       _pager_window_move(pw);
                    }
               }
          }
        else
          {
             /* go through all desks */
             EINA_LIST_FOREACH(pager->desks, l, pd)
               {
                  /* create it and add it */
                  if (_pager_desk_window_find(pd, ec)) continue;
                  pw = _pager_window_new(pd, ec);
                  if (pw)
                    {
                       Pager_Win *pw2 = NULL;
                       E_Client *ec_above;

                       pd->wins = eina_list_append(pd->wins, pw);
                       ec_above = e_util_desk_client_above(pw->client);
                       if (ec_above)
                         pw2 = _pager_desk_window_find(pd, ec_above);
                       if (pw2)
                         e_layout_child_lower_below(pw->o_window, pw2->o_window);
                       else
                         e_layout_child_raise(pw->o_window);
                       _pager_window_move(pw);
                    }
               }
          }
     }
}

static Eina_Bool
_pager_cb_event_client_desk_set(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client_Desk_Set *ev = event;
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     _pager_window_desk_change(p, ev->ec);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_stack(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client *ev = event;
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        Eina_List *l2;
        Pager_Desk *pd;

        if (p->zone != ev->ec->zone) continue;
        EINA_LIST_FOREACH(p->desks, l2, pd)
          {
             Pager_Win *pw, *pw2 = NULL;

             pw = _pager_desk_window_find(pd, ev->ec);
             if (pw)
               {
                  E_Client *ec;

                  ec = e_util_desk_client_below(ev->ec);
                  if (ec) pw2 = _pager_desk_window_find(pd, ec);
                  if (pw2)
                    {
                       e_layout_child_raise_above(pw->o_window, pw2->o_window);
                       continue;
                    }
                  ec = e_util_desk_client_above(ev->ec);
                  if (ec) pw2 = _pager_desk_window_find(pd, ec);
                  if (pw2)
                    {
                       e_layout_child_lower_below(pw->o_window, pw2->o_window);
                       continue;
                    }
               }
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_icon_change(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client_Property *ev = event;
   Eina_List *l;
   Pager *p;

   if (!(ev->property & E_CLIENT_PROPERTY_ICON)) return ECORE_CALLBACK_RENEW;
   EINA_LIST_FOREACH(pagers, l, p)
     {
        Eina_List *l2;
        Pager_Desk *pd;

        if (p->zone != ev->ec->zone) continue;
        EINA_LIST_FOREACH(p->desks, l2, pd)
          {
             Pager_Win *pw;

             pw = _pager_desk_window_find(pd, ev->ec);
             if (pw)
               {
                  Evas_Object *o;

                  if (pw->o_icon)
                    {
                       evas_object_del(pw->o_icon);
                       pw->o_icon = NULL;
                    }
                  o = e_client_icon_add(ev->ec,
                                        evas_object_evas_get(p->o_table));
                  if (o)
                    {
                       pw->o_icon = o;
                       evas_object_show(o);
                       edje_object_part_swallow(pw->o_window,
                                                "e.swallow.icon", o);
                    }
               }
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_urgent_change(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client_Property *ev = event;
   Eina_List *l, *l2;
   Pager_Popup *pp;
   E_Zone *zone;
   int urgent;
   Pager *p;
   Pager_Desk *pd;
   Pager_Win *pw;

   if (!(ev->property & E_CLIENT_PROPERTY_URGENCY)) return ECORE_CALLBACK_RENEW;
   zone = ev->ec->zone;
   urgent = ev->ec->urgent || ev->ec->icccm.urgent;

   if (pager_config->popup_urgent && (pager_config->popup_urgent_focus ||
                                      ((!pager_config->popup_urgent_focus) && (!ev->ec->focused) && (!ev->ec->want_focus))))
     {
        pp = _pager_popup_find(zone);

        if ((!pp) && (urgent) && !(ev->ec->iconic))
          {
             pp = _pager_popup_new(zone, 0);

             if ((pp) && (!pager_config->popup_urgent_stick))
               pp->timer = ecore_timer_add(pager_config->popup_urgent_speed,
                                           _pager_popup_cb_timeout, pp);
             if (pp) pp->urgent = 1;
          }
     }

   EINA_LIST_FOREACH(pagers, l, p)
     {
        if (p->zone != zone) continue;

        EINA_LIST_FOREACH(p->desks, l2, pd)
          {
             pw = _pager_desk_window_find(pd, ev->ec);
             if (pw)
               {
                  if (ev->ec->urgent)
                    {
                       if (!(ev->ec->iconic))
                         {
                            if ((pd->pager) && (pd->pager->inst) &&
                                (!pager_config->popup_urgent))
                              e_gadcon_urgent_show(pd->pager->inst->gcc->gadcon);
                            edje_object_signal_emit(pd->o_desk,
                                                    "e,state,urgent", "e");
                         }
                       edje_object_signal_emit(pw->o_window,
                                               "e,state,urgent", "e");
                    }
                  else
                    {
                       if (!(ev->ec->iconic))
                         edje_object_signal_emit(pd->o_desk,
                                                 "e,state,not_urgent", "e");
                       edje_object_signal_emit(pw->o_window,
                                               "e,state,not_urgent", "e");
                    }
               }
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_focus_in(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client *ev = event;
   Instance *inst;
   Eina_List *l, *l2;
   Pager_Popup *pp;
   Pager_Desk *pd;
   Pager_Win *pw;
   E_Zone *zone;

   zone = ev->ec->zone;

   EINA_LIST_FOREACH(pager_config->instances, l, inst)
     {
        if (inst->pager->zone != zone) continue;

        EINA_LIST_FOREACH(inst->pager->desks, l2, pd)
          {
             pw = _pager_desk_window_find(pd, ev->ec);
             if (pw)
               {
                  edje_object_signal_emit(pw->o_window,
                                          "e,state,focused", "e");
                  break;
               }
          }
     }

   pp = _pager_popup_find(zone);
   if (!pp) return ECORE_CALLBACK_PASS_ON;

   EINA_LIST_FOREACH(pp->pager->desks, l, pd)
     {
        pw = _pager_desk_window_find(pd, ev->ec);
        if (pw)
          {
             edje_object_signal_emit(pw->o_window,
                                     "e,state,focused", "e");
             break;
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_focus_out(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client *ev = event;
   Eina_List *l;
   Pager_Popup *pp;
   Pager_Desk *pd;
   Pager_Win *pw;
   Instance *inst;
   E_Zone *zone;

   zone = ev->ec->zone;

   EINA_LIST_FOREACH(pager_config->instances, l, inst)
     {
        Eina_List *l2;

        if (inst->pager->zone != zone) continue;

        EINA_LIST_FOREACH(inst->pager->desks, l2, pd)
          {
             pw = _pager_desk_window_find(pd, ev->ec);
             if (pw)
               {
                  edje_object_signal_emit(pw->o_window,
                                          "e,state,unfocused", "e");
                  break;
               }
          }
     }

   pp = _pager_popup_find(zone);
   if (!pp) return ECORE_CALLBACK_PASS_ON;

   EINA_LIST_FOREACH(pp->pager->desks, l, pd)
     {
        pw = _pager_desk_window_find(pd, ev->ec);
        if (pw)
          {
             edje_object_signal_emit(pw->o_window,
                                     "e,state,unfocused", "e");
             break;
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_property(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client_Property *ev = event;
   Eina_List *l, *l2;
   int found = 0;
   Pager *p;
   Pager_Win *pw;
   Pager_Desk *pd;

   if (!(ev->property & E_CLIENT_PROPERTY_NETWM_STATE)) return ECORE_CALLBACK_RENEW;
   EINA_LIST_FOREACH(pagers, l, p)
     {
        if (p->zone != ev->ec->zone) continue;

        EINA_LIST_FOREACH(p->desks, l2, pd)
          {
             pw = _pager_desk_window_find(pd, ev->ec);
             if (pw)
               {
                  found = 1;
                  if (ev->ec->netwm.state.skip_pager)
                    {
                       pd->wins = eina_list_remove(pd->wins, pw);
                       _pager_window_free(pw);
                    }
               }
          }
     }
   if (found) return ECORE_CALLBACK_PASS_ON;

   /* If we did not find this window in the pager, then add it because
    * the skip_pager state may have changed to 1 */
   EINA_LIST_FOREACH(pagers, l, p)
     {
        if ((p->zone != ev->ec->zone) ||
            (_pager_window_find(p, ev->ec)))
          continue;
        if (!ev->ec->sticky)
          {
             pd = _pager_desk_find(p, ev->ec->desk);
             if ((pd) && (!_pager_desk_window_find(pd, ev->ec)))
               {
                  pw = _pager_window_new(pd, ev->ec);
                  if (pw)
                    {
                       Pager_Win *pw2 = NULL;
                       E_Client *ec;

                       pd->wins = eina_list_append(pd->wins, pw);
                       ec = e_util_desk_client_above(pw->client);
                       if (ec)
                         pw2 = _pager_desk_window_find(pd, ec);
                       if (pw2)
                         e_layout_child_lower_below(pw->o_window, pw2->o_window);
                       else
                         e_layout_child_raise(pw->o_window);
                       _pager_window_move(pw);
                    }
               }
          }
        else
          {
             EINA_LIST_FOREACH(p->desks, l2, pd)
               {
                  if (_pager_desk_window_find(pd, ev->ec)) continue;
                  pw = _pager_window_new(pd, ev->ec);
                  if (pw)
                    {
                       Pager_Win *pw2 = NULL;
                       E_Client *ec;

                       pd->wins = eina_list_append(pd->wins, pw);
                       ec = e_util_desk_client_above(pw->client);
                       if (ec)
                         pw2 = _pager_desk_window_find(pd, ec);
                       if (pw2)
                         e_layout_child_lower_below(pw->o_window, pw2->o_window);
                       else
                         e_layout_child_raise(pw->o_window);
                       _pager_window_move(pw);
                    }
               }
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_zone_desk_count_set(void *data __UNUSED__, int type __UNUSED__, void *event __UNUSED__)
{
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        _pager_empty(p);
        _pager_fill(p, p->inst ? p->inst->gcc->gadcon : NULL);
        if (p->inst) _gc_orient(p->inst->gcc, p->inst->gcc->gadcon->orient);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_desk_show(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Desk_Show *ev = event;
   Eina_List *l;
   Pager *p;
   Pager_Popup *pp;
   Pager_Desk *pd;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        if (p->zone != ev->desk->zone) continue;
        pd = _pager_desk_find(p, ev->desk);
        if (pd) _pager_desk_select(pd);

        if (p->popup)
          edje_object_part_text_set(p->popup->o_bg, "e.text.label", ev->desk->name);
     }

   if ((pager_config->popup) && (!act_popup))
     {
        if ((pp = _pager_popup_find(ev->desk->zone)))
          evas_object_show(pp->popup);
        else
          pp = _pager_popup_new(ev->desk->zone, 0);
        if (pp->timer)
          ecore_timer_reset(pp->timer);
        else
          pp->timer = ecore_timer_add(pager_config->popup_speed,
                                      _pager_popup_cb_timeout, pp);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_desk_name_change(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Desk_Name_Change *ev = event;
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        Pager_Desk *pd;

        if (p->zone != ev->desk->zone) continue;
        pd = _pager_desk_find(p, ev->desk);
        if (pager_config->show_desk_names)
          {
             if (pd)
               edje_object_part_text_set(pd->o_desk, "e.text.label",
                                         ev->desk->name);
          }
        else
          {
             if (pd)
               edje_object_part_text_set(pd->o_desk, "e.text.label", "");
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_bg_update(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Bg_Update *ev = event;
   Eina_List *l, *ll;
   Pager *p;
   Pager_Desk *pd;
   E_Zone *zone;
   E_Desk *desk;

   if (ev->zone == -1)
     {
        EINA_LIST_FOREACH(pagers, l, p)
          EINA_LIST_FOREACH(p->desks, ll, pd)
            _pager_desk_livethumb_setup(pd);
        return ECORE_CALLBACK_RENEW;
     }
   zone = eina_list_nth(e_comp->zones, ev->zone);
   if (!zone) return ECORE_CALLBACK_RENEW;
   desk = e_desk_at_xy_get(zone, ev->desk_x, ev->desk_y);
   if (!desk) return ECORE_CALLBACK_RENEW;
   EINA_LIST_FOREACH(pagers, l, p)
     {
        pd = _pager_desk_find(p, desk);
        if (!pd) continue;
        _pager_desk_livethumb_setup(pd);
        continue;
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_pager_cb_event_compositor_resize(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Compositor_Resize *ev = event;
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        Eina_List *l2;
        Pager_Desk *pd;

        if (p->zone->comp != ev->comp) continue;

        EINA_LIST_FOREACH(p->desks, l2, pd)
          e_layout_virtual_size_set(pd->o_layout, pd->desk->zone->w,
                                    pd->desk->zone->h);

        if (p->inst) _gc_orient(p->inst->gcc, p->inst->gcc->gadcon->orient);
        /* TODO if (p->popup) */
     }

   return ECORE_CALLBACK_PASS_ON;
}

static void
_pager_window_cb_mouse_down(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info)
{
   Evas_Event_Mouse_Down *ev;
   Pager_Win *pw;

   ev = event_info;
   pw = data;

   if (!pw) return;
   if (pw->desk->pager->popup && !act_popup) return;
   if (!pw->desk->pager->popup && ev->button == 3) return;
   if (ev->button == (int)pager_config->btn_desk) return;
   if ((ev->button == (int)pager_config->btn_drag) ||
       (ev->button == (int)pager_config->btn_noplace))
     {
        Evas_Coord ox, oy;

        evas_object_geometry_get(pw->o_window, &ox, &oy, NULL, NULL);
        pw->drag.in_pager = 1;
        pw->drag.x = ev->canvas.x;
        pw->drag.y = ev->canvas.y;
        pw->drag.dx = ox - ev->canvas.x;
        pw->drag.dy = oy - ev->canvas.y;
        pw->drag.start = 1;
        pw->drag.button = ev->button;
     }
}

static void
_pager_window_cb_mouse_up(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info)
{
   Evas_Event_Mouse_Up *ev;
   Pager_Win *pw;
   Pager *p;

   ev = event_info;
   pw = data;
   if (!pw) return;

   p = pw->desk->pager;

   if (pw->desk->pager->popup && !act_popup) return;
   if (ev->button == (int)pager_config->btn_desk) return;
   if ((ev->button == (int)pager_config->btn_drag) ||
       (ev->button == (int)pager_config->btn_noplace))
     {
        if (!pw->drag.from_pager)
          {
             edje_object_signal_emit(pw->desk->o_desk, "e,action,drag,out", "e");
             e_comp_object_effect_unclip(pw->client->frame);
             if (!pw->drag.start) p->just_dragged = 1;
             pw->drag.in_pager = 0;
             pw->drag.start = 0;
             p->dragging = 0;
          }
     }
}

static void
_pager_window_cb_mouse_move(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info)
{
   Evas_Event_Mouse_Move *ev;
   Pager_Win *pw;
   E_Drag *drag;
   Evas_Object *o, *oo;
   Evas_Coord x, y, w, h;
   const char *drag_types[] =
   { "enlightenment/pager_win", "enlightenment/border" };
   Evas_Coord dx, dy;
   unsigned int resist = 0;
   Evas_Coord mx, my, vx, vy;
   Pager_Desk *pd;

   ev = event_info;
   pw = data;

   if (!pw) return;
   if (pw->client->lock_user_location) return;
   if ((pw->desk) && (pw->desk->pager))
     {
        if ((pw->desk->pager->popup) && (!act_popup)) return;
     }

   /* prevent drag for a few pixels */
   if (pw->drag.start)
     {
        dx = pw->drag.x - ev->cur.output.x;
        dy = pw->drag.y - ev->cur.output.y;
        if ((pw->desk) && (pw->desk->pager))
          resist = pager_config->drag_resist;

        if (((unsigned int)(dx * dx) + (unsigned int)(dy * dy)) <=
            (resist * resist)) return;

        if ((pw->desk) && (pw->desk->pager))
          pw->desk->pager->dragging = 1;
        pw->drag.start = 0;
        e_comp_object_effect_clip(pw->client->frame);
        edje_object_signal_emit(pw->desk->o_desk, "e,action,drag,in", "e");
        pw->desk->pager->active_drop_pd = pw->desk;
     }

   /* dragging this win around inside the pager */
   if (pw->drag.in_pager)
     {
        /* m for mouse */
        mx = ev->cur.canvas.x;
        my = ev->cur.canvas.y;

        /* find desk at pointer */
        pd = _pager_desk_at_coord(pw->desk->pager, mx, my);
        if (pd)
          {
             int zx, zy, zw, zh;

             e_zone_useful_geometry_get(pd->desk->zone, &zx, &zy, &zw, &zh);
             e_layout_coord_canvas_to_virtual(pd->o_layout,
                                              mx + pw->drag.dx,
                                              my + pw->drag.dy, &vx, &vy);
             if (pd != pw->desk)
               {
                  edje_object_signal_emit(pw->desk->o_desk, "e,action,drag,out", "e");
                  pw->client->hidden = 0;
                  e_client_desk_set(pw->client, pd->desk);
                  edje_object_signal_emit(pd->o_desk, "e,action,drag,in", "e");
                  pd->pager->active_drop_pd = pd;
               }
             mx = E_CLAMP(vx + zx, zx, zx + zw - pw->client->w);
             my = E_CLAMP(vy + zy, zy, zy + zh - pw->client->h);
             evas_object_move(pw->client->frame, mx, my);
          }
        else
          {
             evas_object_geometry_get(pw->o_window, &x, &y, &w, &h);
             evas_object_hide(pw->o_window);

             drag = e_drag_new(pw->desk->pager->zone->comp,
                               x, y, drag_types, 2, pw, -1,
                               _pager_window_cb_drag_convert,
                               _pager_window_cb_drag_finished);

             o = edje_object_add(drag->evas);
             e_theme_edje_object_set(o, "base/theme/modules/pager",
                                     "e/modules/pager/window");
             evas_object_show(o);

             oo = e_client_icon_add(pw->client, drag->evas);
             if (oo)
               {
                  evas_object_show(oo);
                  edje_object_part_swallow(o, "e.swallow.icon", oo);
               }

             e_drag_object_set(drag, o);
             e_drag_resize(drag, w, h);
             e_drag_start(drag, x - pw->drag.dx, y - pw->drag.dy);
             e_comp_object_util_del_list_append(drag->comp_object, oo);

             /* this prevents the desk from switching on drags */
             pw->drag.from_pager = pw->desk->pager;
             pw->drag.from_pager->dragging = 1;
             pw->drag.in_pager = 0;
          }
     }
}

static void *
_pager_window_cb_drag_convert(E_Drag *drag, const char *type)
{
   Pager_Win *pw;

   pw = drag->data;
   if (!strcmp(type, "enlightenment/pager_win")) return pw;
   if (!strcmp(type, "enlightenment/border")) return pw->client;
   return NULL;
}

static void
_pager_window_cb_drag_finished(E_Drag *drag, int dropped)
{
   Pager_Win *pw;
   E_Comp *comp;
   E_Zone *zone;
   E_Desk *desk;
   int x = 0, y = 0, dx, dy;

   pw = drag->data;
   if (!pw) return;
   evas_object_show(pw->o_window);
   if (!dropped)
     {
        int zx, zy, zw, zh;

        /* wasn't dropped (on pager). move it to position of mouse on screen */
        comp = e_util_comp_current_get();
        zone = e_zone_current_get(comp);
        desk = e_desk_current_get(zone);

        e_client_zone_set(pw->client, zone);
        if ((pw->client->desk != desk) && desk->visible)
          {
             pw->client->hidden = 0;
             e_client_desk_set(pw->client, desk);
          }

#ifndef HAVE_WAYLAND_ONLY
        ecore_x_pointer_last_xy_get(&x, &y);
#endif

        dx = (pw->client->w / 2);
        dy = (pw->client->h / 2);

        e_zone_useful_geometry_get(zone, &zx, &zy, &zw, &zh);

        /* offset so that center of window is on mouse, but keep within desk bounds */
        if (dx < x)
          {
             x -= dx;
             if ((pw->client->w < zw) &&
                 (x + pw->client->w > zx + zw))
               x -= x + pw->client->w - (zx + zw);
          }
        else x = 0;

        if (dy < y)
          {
             y -= dy;
             if ((pw->client->h < zh) &&
                 (y + pw->client->h > zy + zh))
               y -= y + pw->client->h - (zy + zh);
          }
        else y = 0;
        evas_object_move(pw->client->frame, x, y);

        if (!(pw->client->lock_user_stacking))
          evas_object_raise(pw->client->frame);
     }
   if (pw->desk->pager->active_drop_pd)
     {
        edje_object_signal_emit(pw->desk->pager->active_drop_pd->o_desk, "e,action,drag,out", "e");
        pw->desk->pager->active_drop_pd = NULL;
     }
   if (pw->drag.from_pager) pw->drag.from_pager->dragging = 0;
   pw->drag.from_pager = NULL;
   e_comp_object_effect_unclip(pw->client->frame);
   if (act_popup)
     {
        e_grabinput_get(input_window, 0, input_window);
        if (!hold_count) _pager_popup_hide(1);
     }
}

static void
_pager_inst_cb_scroll(void *data)
{
   Pager *p;

   p = data;
   _pager_update_drop_position(p, p->dnd_x, p->dnd_y);
}

static void
_pager_update_drop_position(Pager *p, Evas_Coord x, Evas_Coord y)
{
   Pager_Desk *pd;

   p->dnd_x = x;
   p->dnd_y = y;
   pd = _pager_desk_at_coord(p, x, y);
   if (pd == p->active_drop_pd) return;
   if (pd)
     edje_object_signal_emit(pd->o_desk, "e,action,drag,in", "e");
   if (p->active_drop_pd)
     edje_object_signal_emit(p->active_drop_pd->o_desk, "e,action,drag,out", "e");
   p->active_drop_pd = pd;
}

static void
_pager_drop_cb_enter(void *data, const char *type __UNUSED__, void *event_info EINA_UNUSED)
{
   Pager *p = data;

   /* FIXME this fixes a segv, but the case is not easy
    * reproduceable. this makes no sense either since
    * the same 'pager' is passed to e_drop_handler_add
    * and it works without this almost all the time.
    * so this must be an issue with e_dnd code... i guess */
   if (act_popup) p = act_popup->pager;

   if (p->inst)
     e_gadcon_client_autoscroll_cb_set(p->inst->gcc, _pager_inst_cb_scroll, p);
}

static void
_pager_drop_cb_move(void *data, const char *type __UNUSED__, void *event_info)
{
   E_Event_Dnd_Move *ev;
   Pager *p;

   ev = event_info;
   p = data;

   if (act_popup) p = act_popup->pager;

   _pager_update_drop_position(p, ev->x, ev->y);

   if (p->inst)
     e_gadcon_client_autoscroll_update(p->inst->gcc, ev->x, ev->y);
}

static void
_pager_drop_cb_leave(void *data, const char *type __UNUSED__, void *event_info __UNUSED__)
{
   Pager *p = data;

   if (act_popup) p = act_popup->pager;

   if (p->active_drop_pd)
     edje_object_signal_emit(p->active_drop_pd->o_desk, "e,action,drag,out", "e");
   p->active_drop_pd = NULL;

   if (p->inst) e_gadcon_client_autoscroll_cb_set(p->inst->gcc, NULL, NULL);
}

static void
_pager_drop_cb_drop(void *data, const char *type, void *event_info)
{
   E_Event_Dnd_Drop *ev;
   Pager_Desk *pd;
   Pager_Desk *pd2 = NULL;
   E_Client *ec = NULL;
   Eina_List *l;
   int dx = 0, dy = 0;
   Pager_Win *pw = NULL;
   Evas_Coord wx, wy, wx2, wy2;
   Evas_Coord nx, ny;
   Pager *p;

   ev = event_info;
   p = data;

   if (act_popup) p = act_popup->pager;

   pd = _pager_desk_at_coord(p, ev->x, ev->y);
   if (pd)
     {
        if (!strcmp(type, "enlightenment/pager_win"))
          {
             pw = (Pager_Win *)(ev->data);
             if (pw)
               {
                  ec = pw->client;
                  dx = pw->drag.dx;
                  dy = pw->drag.dy;
               }
          }
        else if (!strcmp(type, "enlightenment/border"))
          {
             ec = ev->data;
             e_layout_coord_virtual_to_canvas(pd->o_layout, ec->x, ec->y,
                                              &wx, &wy);
             e_layout_coord_virtual_to_canvas(pd->o_layout, ec->x + ec->w,
                                              ec->y + ec->h, &wx2, &wy2);
             dx = (wx - wx2) / 2;
             dy = (wy - wy2) / 2;
          }
        else if (!strcmp(type, "enlightenment/vdesktop"))
          {
             pd2 = ev->data;
             if (!pd2) return;
             _pager_desk_switch(pd, pd2);
          }
        else
          return;

        if (ec)
          {
             E_Maximize max = ec->maximized;
             E_Fullscreen fs = ec->fullscreen_policy;
             Eina_Bool fullscreen = ec->fullscreen;

             if (ec->iconic) e_client_uniconify(ec);
             if (ec->maximized)
               e_client_unmaximize(ec, E_MAXIMIZE_BOTH);
             if (fullscreen) e_client_unfullscreen(ec);
             if (pd->desk->visible)
               ec->hidden = 0;
             e_client_desk_set(ec, pd->desk);
             evas_object_raise(ec->frame);

             if ((!max) && (!fullscreen))
               {
                  int zx, zy, zw, zh, mx, my;

                  e_layout_coord_canvas_to_virtual(pd->o_layout,
                                                   ev->x + dx,
                                                   ev->y + dy,
                                                   &nx, &ny);
                  e_zone_useful_geometry_get(pd->desk->zone,
                                             &zx, &zy, &zw, &zh);

                  mx = E_CLAMP(nx + zx, zx, zx + zw - ec->w);
                  my = E_CLAMP(ny + zy, zy, zy + zh - ec->h);
                  evas_object_move(ec->frame, mx, my);
               }
             if (max) e_client_maximize(ec, max);
             if (fullscreen) e_client_fullscreen(ec, fs);
          }
     }

   EINA_LIST_FOREACH(p->desks, l, pd)
     {
        if (!p->active_drop_pd) break;
        if (pd == p->active_drop_pd)
          {
             edje_object_signal_emit(pd->o_desk, "e,action,drag,out", "e");
             p->active_drop_pd = NULL;
          }
     }

   if (p->inst) e_gadcon_client_autoscroll_cb_set(p->inst->gcc, NULL, NULL);
}

static void
_pager_desk_cb_mouse_down(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info)
{
   Evas_Event_Mouse_Down *ev;
   Pager_Desk *pd;
   Evas_Coord ox, oy;

   ev = event_info;
   pd = data;
   if (!pd) return;
   if ((!pd->pager->popup) && (ev->button == 3)) return;
   if (ev->button == (int)pager_config->btn_desk)
     {
        evas_object_geometry_get(pd->o_desk, &ox, &oy, NULL, NULL);
        pd->drag.start = 1;
        pd->drag.in_pager = 1;
        pd->drag.dx = ox - ev->canvas.x;
        pd->drag.dy = oy - ev->canvas.y;
        pd->drag.x = ev->canvas.x;
        pd->drag.y = ev->canvas.y;
        pd->drag.button = ev->button;
     }
   else
     {
        pd->drag.dx = pd->drag.dy = pd->drag.x = pd->drag.y = 0;
     }
   pd->pager->just_dragged = 0;
}

static void
_pager_desk_cb_mouse_up(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info)
{
   Evas_Event_Mouse_Up *ev;
   Pager_Desk *pd;
   Pager *p;

   ev = event_info;
   pd = data;

   if (!pd) return;
   p = pd->pager;

   /* FIXME: pd->pager->dragging is 0 when finishing a drag from desk to desk */
   if ((ev->button == 1) && (!pd->pager->dragging) &&
       (!pd->pager->just_dragged))
     {
        current_desk = pd->desk;
        e_desk_show(pd->desk);
        pd->drag.start = 0;
        pd->drag.in_pager = 0;
        p->active_drop_pd = NULL;
     }

   if ((p->popup) && (p->popup->urgent)) _pager_popup_free(p->popup);
}

static void
_pager_desk_cb_mouse_move(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info)
{
   Evas_Event_Mouse_Move *ev;
   Pager_Desk *pd;
   Evas_Coord dx, dy;
   unsigned int resist = 0;
   E_Drag *drag;
   Evas_Object *o, *oo, *o_icon;
   Evas_Coord x, y, w, h;
   const char *drag_types[] = { "enlightenment/vdesktop" };
   Pager_Win *pw;
   Eina_List *l;

   ev = event_info;

   pd = data;
   if (!pd) return;
   /* prevent drag for a few pixels */
   if (pd->drag.start)
     {
        dx = pd->drag.x - ev->cur.output.x;
        dy = pd->drag.y - ev->cur.output.y;
        if ((pd->pager) && (pd->pager->inst))
          resist = pager_config->drag_resist;

        if (((unsigned int)(dx * dx) + (unsigned int)(dy * dy)) <=
            (resist * resist)) return;

        if (pd->pager) pd->pager->dragging = 1;
        pd->drag.start = 0;
     }

   if (pd->drag.in_pager && pd->pager)
     {
        evas_object_geometry_get(pd->o_desk, &x, &y, &w, &h);
        drag = e_drag_new(pd->pager->zone->comp,
                          x, y, drag_types, 1, pd, -1,
                          NULL, _pager_desk_cb_drag_finished);

        /* redraw the desktop theme above */
        o = edje_object_add(drag->evas);
        e_theme_edje_object_set(o, "base/theme/modules/pager",
                                "e/modules/pager/desk");
        evas_object_show(o);
        e_drag_object_set(drag, o);

        /* and redraw is content */
        oo = e_layout_add(drag->evas);
        e_layout_virtual_size_set(oo, pd->pager->zone->w, pd->pager->zone->h);
        edje_object_part_swallow(o, "e.swallow.content", oo);
        e_comp_object_util_del_list_append(drag->comp_object, oo);
        evas_object_show(oo);

        EINA_LIST_FOREACH(pd->wins, l, pw)
          {
             int zx, zy;

             if ((!pw) || (pw->client->iconic)
                 || (pw->client->netwm.state.skip_pager))
               continue;

             o = edje_object_add(drag->evas);
             e_theme_edje_object_set(o, "base/theme/modules/pager",
                                     "e/modules/pager/window");
             e_layout_pack(oo, o);
             e_layout_child_raise(o);
             e_zone_useful_geometry_get(pw->desk->desk->zone,
                                        &zx, &zy, NULL, NULL);
             e_layout_child_move(o, pw->client->x - zx, pw->client->y - zy);
             e_layout_child_resize(o, pw->client->w, pw->client->h);
             e_comp_object_util_del_list_append(drag->comp_object, o);
             evas_object_show(o);

             if ((o_icon = e_client_icon_add(pw->client, drag->evas)))
               {
                  evas_object_show(o_icon);
                  edje_object_part_swallow(o, "e.swallow.icon", o_icon);
                  e_comp_object_util_del_list_append(drag->comp_object, o_icon);
               }
          }
        e_drag_resize(drag, w, h);
        e_drag_start(drag, x - pd->drag.dx, y - pd->drag.dy);

        pd->drag.from_pager = pd->pager;
        pd->drag.from_pager->dragging = 1;
        pd->drag.in_pager = 0;
     }
}

static void
_pager_desk_cb_drag_finished(E_Drag *drag, int dropped)
{
   Pager_Desk *pd;
   Pager_Desk *pd2 = NULL;
   Eina_List *l;
   E_Desk *desk;
   E_Zone *zone;
   Pager *p;

   pd = drag->data;
   if (!pd) return;
   if (!dropped)
     {
        /* wasn't dropped on pager, switch with current desktop */
        if (!pd->desk) return;
        zone = e_util_zone_current_get(e_manager_current_get());
        desk = e_desk_current_get(zone);
        EINA_LIST_FOREACH(pagers, l, p)
          {
             pd2 = _pager_desk_find(p, desk);
             if (pd2) break;
          }
        _pager_desk_switch(pd, pd2);
     }
   if (pd->drag.from_pager)
     {
        pd->drag.from_pager->dragging = 0;
        pd->drag.from_pager->just_dragged = 0;
     }
   if (pd->pager->active_drop_pd)
     {
        edje_object_signal_emit(pd->pager->active_drop_pd->o_desk, "e,action,drag,out", "e");
        pd->pager->active_drop_pd = NULL;
     }
   pd->drag.from_pager = NULL;

   if (act_popup)
     {
        e_grabinput_get(input_window, 0, input_window);
        if (!hold_count) _pager_popup_hide(1);
     }
}

static void
_pager_desk_cb_mouse_wheel(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info)
{
   Evas_Event_Mouse_Wheel *ev;
   Pager_Desk *pd;

   ev = event_info;
   pd = data;

   if (pd->pager->popup) return;

   if (pager_config->flip_desk)
     e_zone_desk_linear_flip_by(pd->desk->zone, ev->z);
}

static Eina_Bool
_pager_popup_cb_timeout(void *data)
{
   Pager_Popup *pp;

   pp = data;
   pp->timer = NULL;
   _pager_popup_free(pp);

#ifndef HAVE_WAYLAND_ONLY
   if (input_window)
     {
        ecore_x_window_free(input_window);
        e_grabinput_release(input_window, input_window);
        input_window = 0;
     }
#endif

   return ECORE_CALLBACK_CANCEL;
}

/************************************************************************/
/* popup-on-keyaction functions */
static int
_pager_popup_show(void)
{
   E_Zone *zone;
   int x, y, w, h;
   Pager_Popup *pp;
   //const char *drop[] =
   //{
      //"enlightenment/pager_win", "enlightenment/border",
      //"enlightenment/vdesktop"
   //};

   if ((act_popup) || (input_window)) return 0;

   zone = e_util_zone_current_get(e_manager_current_get());

   pp = _pager_popup_find(zone);
   if (pp) _pager_popup_free(pp);

#ifndef HAVE_WAYLAND_ONLY
   input_window = ecore_x_window_input_new(zone->comp->win, 0, 0, 1, 1);
   ecore_x_window_show(input_window);
   if (!e_grabinput_get(input_window, 0, input_window))
     {
        ecore_x_window_free(input_window);
        input_window = 0;
        return 0;
     }
#endif

   handlers = eina_list_append
       (handlers, ecore_event_handler_add
         (ECORE_EVENT_KEY_DOWN, _pager_popup_cb_key_down, NULL));
   handlers = eina_list_append
       (handlers, ecore_event_handler_add
         (ECORE_EVENT_KEY_UP, _pager_popup_cb_key_up, NULL));
   handlers = eina_list_append
       (handlers, ecore_event_handler_add
         (ECORE_EVENT_MOUSE_BUTTON_DOWN, _pager_popup_cb_mouse_down, NULL));
   handlers = eina_list_append
       (handlers, ecore_event_handler_add
         (ECORE_EVENT_MOUSE_BUTTON_UP, _pager_popup_cb_mouse_up, NULL));
   handlers = eina_list_append
       (handlers, ecore_event_handler_add
         (ECORE_EVENT_MOUSE_WHEEL, _pager_popup_cb_mouse_wheel, NULL));
   handlers = eina_list_append
       (handlers, ecore_event_handler_add
         (ECORE_EVENT_MOUSE_MOVE, _pager_popup_cb_mouse_move, NULL));

   act_popup = _pager_popup_new(zone, 1);

   evas_object_geometry_get(act_popup->pager->o_table, &x, &y, &w, &h);

   current_desk = e_desk_current_get(zone);

   return 1;
}

static void
_pager_popup_hide(int switch_desk)
{
   hold_count = 0;
   hold_mod = 0;
   while (handlers)
     {
        ecore_event_handler_del(handlers->data);
        handlers = eina_list_remove_list(handlers, handlers);
     }

   act_popup->timer = ecore_timer_add(0.1, _pager_popup_cb_timeout, act_popup);

   if ((switch_desk) && (current_desk)) e_desk_show(current_desk);

   act_popup = NULL;
}

static void
_pager_popup_modifiers_set(int mod)
{
   if (!act_popup) return;
   hold_mod = mod;
   hold_count = 0;
   if (hold_mod & ECORE_EVENT_MODIFIER_SHIFT) hold_count++;
   if (hold_mod & ECORE_EVENT_MODIFIER_CTRL) hold_count++;
   if (hold_mod & ECORE_EVENT_MODIFIER_ALT) hold_count++;
   if (hold_mod & ECORE_EVENT_MODIFIER_WIN) hold_count++;
}

static void
_pager_popup_desk_switch(int x, int y)
{
   int max_x, max_y, desk_x, desk_y;
   Pager_Desk *pd;
   Pager_Popup *pp = act_popup;

   e_zone_desk_count_get(pp->pager->zone, &max_x, &max_y);

   desk_x = current_desk->x + x;
   desk_y = current_desk->y + y;

   if (desk_x < 0)
     desk_x = max_x - 1;
   else if (desk_x >= max_x)
     desk_x = 0;

   if (desk_y < 0)
     desk_y = max_y - 1;
   else if (desk_y >= max_y)
     desk_y = 0;

   current_desk = e_desk_at_xy_get(pp->pager->zone, desk_x, desk_y);

   pd = _pager_desk_find(pp->pager, current_desk);
   if (pd) _pager_desk_select(pd);

   edje_object_part_text_set(pp->o_bg, "e.text.label", current_desk->name);
}

static void
_pager_popup_cb_action_show(E_Object *obj __UNUSED__, const char *params __UNUSED__, Ecore_Event_Key *ev __UNUSED__)
{
   if (_pager_popup_show())
     _pager_popup_modifiers_set(ev->modifiers);
}

static void
_pager_popup_cb_action_switch(E_Object *obj __UNUSED__, const char *params, Ecore_Event_Key *ev)
{
   int max_x, max_y, desk_x;
   int x = 0, y = 0;

   if (!act_popup)
     {
        if (_pager_popup_show())
          _pager_popup_modifiers_set(ev->modifiers);
        else
          return;
     }

   e_zone_desk_count_get(act_popup->pager->zone, &max_x, &max_y);
   desk_x = current_desk->x /* + x <=this is always 0 */;

   if (!strcmp(params, "left"))
     x = -1;
   else if (!strcmp(params, "right"))
     x = 1;
   else if (!strcmp(params, "up"))
     y = -1;
   else if (!strcmp(params, "down"))
     y = 1;
   else if (!strcmp(params, "next"))
     {
        x = 1;
        if (desk_x == max_x - 1)
          y = 1;
     }
   else if (!strcmp(params, "prev"))
     {
        x = -1;
        if (desk_x == 0)
          y = -1;
     }

   _pager_popup_desk_switch(x, y);
}

static Eina_Bool
_pager_popup_cb_mouse_down(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Event_Mouse_Button *ev;
   Pager_Popup *pp = act_popup;

   ev = event;
   if (ev->window != input_window) return ECORE_CALLBACK_PASS_ON;

   evas_event_feed_mouse_down(evas_object_evas_get(pp->popup), ev->buttons,
                              0, ev->timestamp, NULL);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_popup_cb_mouse_up(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Event_Mouse_Button *ev;
   Pager_Popup *pp = act_popup;

   ev = event;
   if (ev->window != input_window) return ECORE_CALLBACK_PASS_ON;

   evas_event_feed_mouse_up(evas_object_evas_get(pp->popup), ev->buttons,
                            0, ev->timestamp, NULL);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_popup_cb_mouse_move(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Event_Mouse_Move *ev;
   Pager_Popup *pp = act_popup;
   int x, y;

   ev = event;
   if (ev->window != input_window) return 1;

   evas_object_geometry_get(pp->popup, &x, &y, NULL, NULL);
   evas_event_feed_mouse_move(evas_object_evas_get(pp->popup),
                              ev->x - x + pp->pager->zone->x,
                              ev->y - y + pp->pager->zone->y,
                              ev->timestamp, NULL);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_popup_cb_mouse_wheel(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Event_Mouse_Wheel *ev = event;
   Pager_Popup *pp = act_popup;
   int max_x;

   e_zone_desk_count_get(pp->pager->zone, &max_x, NULL);

   if (current_desk->x + ev->z >= max_x)
     _pager_popup_desk_switch(1, 1);
   else if (current_desk->x + ev->z < 0)
     _pager_popup_desk_switch(-1, -1);
   else
     _pager_popup_desk_switch(ev->z, 0);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_popup_cb_key_down(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Event_Key *ev;

   ev = event;
   if (ev->window != input_window) return ECORE_CALLBACK_PASS_ON;
   if (!strcmp(ev->key, "Up"))
     _pager_popup_desk_switch(0, -1);
   else if (!strcmp(ev->key, "Down"))
     _pager_popup_desk_switch(0, 1);
   else if (!strcmp(ev->key, "Left"))
     _pager_popup_desk_switch(-1, 0);
   else if (!strcmp(ev->key, "Right"))
     _pager_popup_desk_switch(1, 0);
   else if (!strcmp(ev->key, "Escape"))
     _pager_popup_hide(0);
   else if ((!strcmp(ev->key, "Return")) || (!strcmp(ev->key, "KP_Enter")) ||
            (!strcmp(ev->key, "space")))
     {
        Pager_Popup *pp = act_popup;

        if (pp)
          {
             E_Desk *desk;
             
             desk = e_desk_at_xy_get(pp->pager->zone,
                                     current_desk->x, current_desk->y);
             if (desk) e_desk_show(desk);
          }
        _pager_popup_hide(0);
     }
   else
     {
        E_Config_Binding_Key *binding;
        Eina_List *l;

        EINA_LIST_FOREACH(e_bindings->key_bindings, l, binding)
          {
             E_Binding_Modifier mod = 0;

             if ((binding->action) && (strcmp(binding->action, "pager_switch")))
               continue;

             if (ev->modifiers & ECORE_EVENT_MODIFIER_SHIFT)
               mod |= E_BINDING_MODIFIER_SHIFT;
             if (ev->modifiers & ECORE_EVENT_MODIFIER_CTRL)
               mod |= E_BINDING_MODIFIER_CTRL;
             if (ev->modifiers & ECORE_EVENT_MODIFIER_ALT)
               mod |= E_BINDING_MODIFIER_ALT;
             if (ev->modifiers & ECORE_EVENT_MODIFIER_WIN)
               mod |= E_BINDING_MODIFIER_WIN;

             if (binding->key && (!strcmp(binding->key, ev->key)) &&
                 ((binding->modifiers == mod)))
               {
                  E_Action *act;

                  act = e_action_find(binding->action);

                  if (act)
                    {
                       if (act->func.go_key)
                         act->func.go_key(NULL, binding->params, ev);
                    }
               }
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_popup_cb_key_up(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Event_Key *ev;

   ev = event;
   if (!(act_popup)) return ECORE_CALLBACK_PASS_ON;

   if (hold_mod)
     {
        if ((hold_mod & ECORE_EVENT_MODIFIER_SHIFT) &&
            (!strcmp(ev->key, "Shift_L"))) hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_SHIFT) &&
                 (!strcmp(ev->key, "Shift_R")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_CTRL) &&
                 (!strcmp(ev->key, "Control_L")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_CTRL) &&
                 (!strcmp(ev->key, "Control_R")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_ALT) &&
                 (!strcmp(ev->key, "Alt_L")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_ALT) &&
                 (!strcmp(ev->key, "Alt_R")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_ALT) &&
                 (!strcmp(ev->key, "Meta_L")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_ALT) &&
                 (!strcmp(ev->key, "Meta_R")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_ALT) &&
                 (!strcmp(ev->key, "Super_L")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_ALT) &&
                 (!strcmp(ev->key, "Super_R")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_WIN) &&
                 (!strcmp(ev->key, "Super_L")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_WIN) &&
                 (!strcmp(ev->key, "Super_R")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_WIN) &&
                 (!strcmp(ev->key, "Mode_switch")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_WIN) &&
                 (!strcmp(ev->key, "Meta_L")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_WIN) &&
                 (!strcmp(ev->key, "Meta_R")))
          hold_count--;
        if ((hold_count <= 0) && (!act_popup->pager->dragging))
          {
             _pager_popup_hide(1);
             return ECORE_CALLBACK_PASS_ON;
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

/***************************************************************************/
/* module setup */
EAPI E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION, "Pager"
};

EAPI void *
e_modapi_init(E_Module *m)
{
   E_Module *p;

   p = e_module_find("pager");
   if (p && p->enabled)
     {
        e_util_dialog_show(_("Error"), _("Pager Plain module cannot be loaded at the same time as Pager!"));
        return NULL;
     }
   conf_edd = E_CONFIG_DD_NEW("Pager_Config", Config);
#undef T
#undef D
#define T Config
#define D conf_edd
   E_CONFIG_VAL(D, T, popup, UINT);
   E_CONFIG_VAL(D, T, popup_speed, DOUBLE);
   E_CONFIG_VAL(D, T, popup_urgent, UINT);
   E_CONFIG_VAL(D, T, popup_urgent_stick, UINT);
   E_CONFIG_VAL(D, T, popup_urgent_speed, DOUBLE);
   E_CONFIG_VAL(D, T, show_desk_names, UINT);
   E_CONFIG_VAL(D, T, popup_height, INT);
   E_CONFIG_VAL(D, T, popup_act_height, INT);
   E_CONFIG_VAL(D, T, drag_resist, UINT);
   E_CONFIG_VAL(D, T, btn_drag, UCHAR);
   E_CONFIG_VAL(D, T, btn_noplace, UCHAR);
   E_CONFIG_VAL(D, T, btn_desk, UCHAR);
   E_CONFIG_VAL(D, T, flip_desk, UCHAR);
   E_CONFIG_VAL(D, T, disable_live_preview, UCHAR);

   pager_config = e_config_domain_load("module.pager_plain", conf_edd);

   if (!pager_config)
     {
        pager_config = E_NEW(Config, 1);
        pager_config->popup = 1;
        pager_config->popup_speed = 1.0;
        pager_config->popup_urgent = 0;
        pager_config->popup_urgent_stick = 0;
        pager_config->popup_urgent_speed = 1.5;
        pager_config->show_desk_names = 0;
        pager_config->popup_height = 60;
        pager_config->popup_act_height = 60;
        pager_config->drag_resist = 3;
        pager_config->btn_drag = 1;
        pager_config->btn_noplace = 2;
        pager_config->btn_desk = 2;
        pager_config->flip_desk = 0;
        pager_config->disable_live_preview = 1;
     }
   E_CONFIG_LIMIT(pager_config->popup, 0, 1);
   E_CONFIG_LIMIT(pager_config->popup_speed, 0.1, 10.0);
   E_CONFIG_LIMIT(pager_config->popup_urgent, 0, 1);
   E_CONFIG_LIMIT(pager_config->popup_urgent_stick, 0, 1);
   E_CONFIG_LIMIT(pager_config->popup_urgent_speed, 0.1, 10.0);
   E_CONFIG_LIMIT(pager_config->show_desk_names, 0, 1);
   E_CONFIG_LIMIT(pager_config->popup_height, 20, 200);
   E_CONFIG_LIMIT(pager_config->popup_act_height, 20, 200);
   E_CONFIG_LIMIT(pager_config->drag_resist, 0, 50);
   E_CONFIG_LIMIT(pager_config->flip_desk, 0, 1);
   E_CONFIG_LIMIT(pager_config->btn_drag, 0, 32);
   E_CONFIG_LIMIT(pager_config->btn_noplace, 0, 32);
   E_CONFIG_LIMIT(pager_config->btn_desk, 0, 32);
   E_CONFIG_LIMIT(pager_config->disable_live_preview, 0, 1);

   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_CLIENT_RESIZE, _pager_cb_event_client_resize, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_CLIENT_MOVE, _pager_cb_event_client_move, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_CLIENT_ADD, _pager_cb_event_client_add, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_CLIENT_REMOVE, _pager_cb_event_client_remove, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_CLIENT_ICONIFY, _pager_cb_event_client_iconify, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_CLIENT_UNICONIFY, _pager_cb_event_client_uniconify, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_CLIENT_PROPERTY, _pager_cb_event_client_stick, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_CLIENT_PROPERTY, _pager_cb_event_client_unstick, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_CLIENT_DESK_SET, _pager_cb_event_client_desk_set, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_CLIENT_STACK, _pager_cb_event_client_stack, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_CLIENT_PROPERTY, _pager_cb_event_client_icon_change, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_CLIENT_PROPERTY, _pager_cb_event_client_urgent_change, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_CLIENT_FOCUS_IN, _pager_cb_event_client_focus_in, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_CLIENT_FOCUS_OUT, _pager_cb_event_client_focus_out, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_CLIENT_PROPERTY, _pager_cb_event_client_property, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_ZONE_DESK_COUNT_SET, _pager_cb_event_zone_desk_count_set, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_DESK_SHOW, _pager_cb_event_desk_show, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_DESK_NAME_CHANGE, _pager_cb_event_desk_name_change, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_COMPOSITOR_RESIZE, _pager_cb_event_compositor_resize, NULL);
   E_LIST_HANDLER_APPEND(pager_config->handlers, E_EVENT_BG_UPDATE, _pager_cb_event_bg_update, NULL);

   pager_config->module = m;

   e_gadcon_provider_register(&_gadcon_class);

   e_configure_registry_item_add("extensions/pager", 40, _("Pager"), NULL,
                                 "preferences-pager", _pager_config_dialog);

   act_popup_show = e_action_add("pager_show");
   if (act_popup_show)
     {
        act_popup_show->func.go_key = _pager_popup_cb_action_show;
        e_action_predef_name_set(N_("Pager"), N_("Show Pager Popup"),
                                 "pager_show", "<none>", NULL, 0);
     }
   act_popup_switch = e_action_add("pager_switch");
   if (act_popup_switch)
     {
        act_popup_switch->func.go_key = _pager_popup_cb_action_switch;
        e_action_predef_name_set(N_("Pager"), N_("Popup Desk Right"),
                                 "pager_switch", "right", NULL, 0);
        e_action_predef_name_set(N_("Pager"), N_("Popup Desk Left"),
                                 "pager_switch", "left", NULL, 0);
        e_action_predef_name_set(N_("Pager"), N_("Popup Desk Up"),
                                 "pager_switch", "up", NULL, 0);
        e_action_predef_name_set(N_("Pager"), N_("Popup Desk Down"),
                                 "pager_switch", "down", NULL, 0);
        e_action_predef_name_set(N_("Pager"), N_("Popup Desk Next"),
                                 "pager_switch", "next", NULL, 0);
        e_action_predef_name_set(N_("Pager"), N_("Popup Desk Previous"),
                                 "pager_switch", "prev", NULL, 0);
     }

   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m __UNUSED__)
{
   e_gadcon_provider_unregister(&_gadcon_class);

   if (pager_config->config_dialog)
     e_object_del(E_OBJECT(pager_config->config_dialog));
   E_FREE_LIST(pager_config->handlers, ecore_event_handler_del);

   e_configure_registry_item_del("extensions/pager");

   e_action_del("pager_show");
   e_action_del("pager_switch");

   e_action_predef_name_del("Pager", "Popup Desk Right");
   e_action_predef_name_del("Pager", "Popup Desk Left");
   e_action_predef_name_del("Pager", "Popup Desk Up");
   e_action_predef_name_del("Pager", "Popup Desk Down");
   e_action_predef_name_del("Pager", "Popup Desk Next");
   e_action_predef_name_del("Pager", "Popup Desk Previous");

   E_FREE(pager_config);
   E_CONFIG_DD_FREE(conf_edd);
   return 1;
}

EAPI int
e_modapi_save(E_Module *m __UNUSED__)
{
   e_config_domain_save("module.pager_plain", conf_edd, pager_config);
   return 1;
}
