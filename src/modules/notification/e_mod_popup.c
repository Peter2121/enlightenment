#include "e_mod_main.h"

/* Popup function protos */
static Popup_Data *_notification_popup_new(E_Notification_Notify *n, unsigned id);
static Popup_Data *_notification_popup_find(unsigned int id);

static int         _notification_popup_place(Popup_Data *popup,
                                             int         num);
static void        _notification_popup_refresh(Popup_Data *popup);
static void        _notification_popup_del(unsigned int                 id,
                                           E_Notification_Notify_Closed_Reason reason);
static void        _notification_popdown(Popup_Data                  *popup,
                                         E_Notification_Notify_Closed_Reason reason);

#define POPUP_GAP 10
#define POPUP_TO_EDGE 15
static int popups_displayed = 0;

/* Util function protos */
static void _notification_format_message(Popup_Data *popup);

static int next_pos = 0;

static Eina_Bool
_notification_timer_cb(Popup_Data *popup)
{
   _notification_popup_del(popup->id, E_NOTIFICATION_NOTIFY_CLOSED_REASON_EXPIRED);
   return EINA_FALSE;
}

int
notification_popup_notify(E_Notification_Notify *n,
                          unsigned int id)
{
   Popup_Data *popup = NULL;

   switch (n->urgency)
     {
      case E_NOTIFICATION_NOTIFY_URGENCY_LOW:
        if (!notification_cfg->show_low) return 0;
        break;
      case E_NOTIFICATION_NOTIFY_URGENCY_NORMAL:
        if (!notification_cfg->show_normal) return 0;
        break;
      case E_NOTIFICATION_NOTIFY_URGENCY_CRITICAL:
        if (!notification_cfg->show_critical) return 0;
        break;
      default:
        break;
     }
   if (notification_cfg->ignore_replacement)
     n->replaces_id = 0;

   if (n->replaces_id && (popup = _notification_popup_find(n->replaces_id)))
     {
        if (popup->notif)
          e_notification_notify_free(popup->notif);

        popup->notif = n;
        popup->id = id;
        _notification_popup_refresh(popup);
     }

   if (!popup)
     {
        popup = _notification_popup_new(n, id);
        if (!popup)
          {
             e_notification_notify_free(n);
             ERR("Error creating popup");
             return 0;
          }
        notification_cfg->popups = eina_list_append(notification_cfg->popups, popup);
        edje_object_signal_emit(popup->theme, "notification,new", "notification");
     }

   if (popup->timer)
     {
        ecore_timer_del(popup->timer);
        popup->timer = NULL;
     }

   if (n->timeout < 0 || notification_cfg->force_timeout)
      n->timeout = notification_cfg->timeout;
   else n->timeout = n->timeout / 1000.0;


   if (n->timeout > 0)
     popup->timer = ecore_timer_add(n->timeout, (Ecore_Task_Cb)_notification_timer_cb, popup);

   return 1;
}

void
notification_popup_shutdown(void)
{
   Popup_Data *popup;

   EINA_LIST_FREE(notification_cfg->popups, popup)
     _notification_popdown(popup, E_NOTIFICATION_NOTIFY_CLOSED_REASON_REQUESTED);
}

void
notification_popup_close(unsigned int id)
{
   _notification_popup_del(id, E_NOTIFICATION_NOTIFY_CLOSED_REASON_REQUESTED);
}

static void
_notification_theme_cb_deleted(Popup_Data *popup,
                               Evas_Object *obj __UNUSED__,
                               const char  *emission __UNUSED__,
                               const char  *source __UNUSED__)
{
   _notification_popup_refresh(popup);
   edje_object_signal_emit(popup->theme, "notification,new", "notification");
}

static void
_notification_theme_cb_close(Popup_Data *popup,
                             Evas_Object *obj __UNUSED__,
                             const char  *emission __UNUSED__,
                             const char  *source __UNUSED__)
{
   _notification_popup_del(popup->id, E_NOTIFICATION_NOTIFY_CLOSED_REASON_DISMISSED);
}

static void
_notification_theme_cb_find(Popup_Data *popup,
                            Evas_Object *obj __UNUSED__,
                            const char  *emission __UNUSED__,
                            const char  *source __UNUSED__)
{
   Eina_List *l;
   E_Border *bd;

   if (!popup->app_name) return;

   EINA_LIST_FOREACH(e_border_client_list(), l, bd)
     {
        size_t len, test;

        len = strlen(popup->app_name);
        test = eina_strlen_bounded(bd->client.icccm.name, len + 1);

        /* We can't be sure that the app_name really match the application name.
         * Some plugin put their name instead. But this search gives some good
         * results.
         */
        if (strncasecmp(bd->client.icccm.name, popup->app_name, (test < len) ? test : len))
          continue;

        e_desk_show(bd->desk);
        e_border_show(bd);
        e_border_raise(bd);
        e_border_focus_set_with_pointer(bd);
        break;
     }
}

static void
_notification_popup_place_coords_get(int zw, int zh, int ow, int oh, int pos, int *x, int *y)
{
   /* XXX for now ignore placement requests */

   switch (notification_cfg->corner)
     {
      case CORNER_TL:
        *x = 15, *y = 15 + pos;
        break;
      case CORNER_TR:
        *x = zw - (ow + 15), *y = 15 + pos;
        break;
      case CORNER_BL:
        *x = 15, *y = (zh - oh) - (15 + pos);
        break;
      case CORNER_BR:
        *x = zw - (ow + 15), *y = (zh - oh) - (15 + pos);
        break;
     }
}

static Popup_Data *
_notification_popup_new(E_Notification_Notify *n, unsigned id)
{
   E_Container *con;
   Popup_Data *popup;
   char buf[PATH_MAX];
   Eina_List *l;
   int pos = next_pos;
   E_Manager *man;
   E_Zone *zone = NULL;

   switch (notification_cfg->dual_screen)
     {
      case POPUP_DISPLAY_POLICY_FIRST:
        man = eina_list_data_get(e_manager_list());
        con = eina_list_data_get(man->containers);
        zone = eina_list_data_get(con->zones);
        break;
      case POPUP_DISPLAY_POLICY_CURRENT:
      case POPUP_DISPLAY_POLICY_ALL:
        zone = e_util_zone_current_get(e_manager_current_get());
        break;
      case POPUP_DISPLAY_POLICY_MULTI:
        if ((notification_cfg->corner == CORNER_BR) ||
            (notification_cfg->corner == CORNER_TR))
          zone = eina_list_last_data_get(e_util_container_current_get()->zones);
        else
          zone = eina_list_data_get(e_util_container_current_get()->zones);
        break;
     }

   /* prevent popups if they would go offscreen
    * FIXME: this can be improved...
    */
   if (next_pos + 30 >= zone->h) return NULL;
   popup = E_NEW(Popup_Data, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(popup, NULL);
   popup->notif = n;
   popup->id = id;
   /* Create the popup window */
   popup->win = e_popup_new(zone, 0, 0, 0, 0);
   e_popup_name_set(popup->win, "_e_popup_notification");
   popup->e = popup->win->evas;

   /* Setup the theme */
   snprintf(buf, sizeof(buf), "%s/e-module-notification.edj",
            notification_mod->dir);
   popup->theme = edje_object_add(popup->e);
   evas_object_name_set(popup->theme, "notification");

   if (!e_theme_edje_object_set(popup->theme,
                                "base/theme/modules/notification",
                                "e/modules/notification/main"))
     if (!e_theme_edje_object_set(popup->theme,
                                  "base/theme/modules/notification",
                                  "modules/notification/main"))
       edje_object_file_set(popup->theme, buf, "modules/notification/main");

   e_popup_content_set(popup->win, popup->theme);

   evas_object_show(popup->theme);
   edje_object_signal_callback_add
     (popup->theme, "notification,deleted", "theme",
     (Edje_Signal_Cb)_notification_theme_cb_deleted, popup);
   edje_object_signal_callback_add
     (popup->theme, "notification,close", "theme",
     (Edje_Signal_Cb)_notification_theme_cb_close, popup);
   edje_object_signal_callback_add
     (popup->theme, "notification,find", "theme",
     (Edje_Signal_Cb)_notification_theme_cb_find, popup);

   _notification_popup_refresh(popup);
   next_pos = _notification_popup_place(popup, next_pos);
   e_popup_show(popup->win);
   if (notification_cfg->dual_screen == POPUP_DISPLAY_POLICY_ALL)
     {
        EINA_LIST_FOREACH(popup->win->zone->container->zones, l, zone)
          {
             Evas_Object *o;
             int x, y;

             if (zone == popup->win->zone) continue;
             o = e_comp_win_image_mirror_add(popup->win->cw);
             evas_object_name_set(o, "notification_mirror");
             evas_object_data_set(o, "zone", zone);
             evas_object_resize(o, popup->win->w, popup->win->h);
             _notification_popup_place_coords_get(zone->w, zone->h, popup->win->w, popup->win->h, pos, &x, &y);
             evas_object_move(o, zone->x + x, zone->y + y);
             evas_object_show(o);
             popup->mirrors = eina_list_append(popup->mirrors, o);
          }
     }
   popups_displayed++;

   return popup;
}

static int
_notification_popup_place(Popup_Data *popup,
                          int         pos)
{
   int x, y;
   Eina_List *l;
   Evas_Object *o;

   _notification_popup_place_coords_get(popup->win->zone->w, popup->win->zone->h, popup->win->w, popup->win->h, pos, &x, &y);
   e_popup_move(popup->win, x, y);
   EINA_LIST_FOREACH(popup->mirrors, l, o)
     {
        E_Zone *zone = evas_object_data_get(o, "zone");
        _notification_popup_place_coords_get(zone->w, zone->h, popup->win->w, popup->win->h, pos, &x, &y);
        evas_object_move(o, zone->x + x, zone->y + y);
     }
   return pos + popup->win->h + 10;
}

static void
_notification_popup_refresh(Popup_Data *popup)
{
   const char *icon_path;
   const char *app_icon_max;
   int w, h, width = 80, height = 80;

   if (!popup) return;

   popup->app_name = popup->notif->app_name;

   if (popup->app_icon)
     {
        e_popup_object_remove(popup->win, popup->app_icon);
        evas_object_del(popup->app_icon);
        popup->app_icon = NULL;
     }

   app_icon_max = edje_object_data_get(popup->theme, "app_icon_max");
   if (app_icon_max)
     {
        char *endptr;

        errno = 0;
        width = strtol(app_icon_max, &endptr, 10);
        if (errno || (width < 1) || (endptr == app_icon_max))
          {
             width = 80;
             height = 80;
          }
        else
          {
             endptr++;
             if (endptr)
               {
                  height = strtol(endptr, NULL, 10);
                  if (errno || (height < 1)) height = 80;
               }
             else height = 80;
          }
     }

   /* Check if the app specify an icon either by a path or by a hint */
   if (!popup->notif->icon.raw.data)
     {
        icon_path = popup->notif->icon.icon_path;
        if ((!icon_path) || (!icon_path[0]))
          icon_path = popup->notif->icon.icon;
        if (icon_path)
          {
             if (!strncmp(icon_path, "file://", 7)) icon_path += 7;
             if (!ecore_file_exists(icon_path))
               {
                  const char *new_path;
                  unsigned int size;

                  size = e_util_icon_size_normalize(width * e_scale);
                  new_path = efreet_icon_path_find(e_config->icon_theme, 
                                                   icon_path, size);
                  if (new_path)
                    icon_path = new_path;
                  else
                    {
                       Evas_Object *o = e_icon_add(popup->e);
                       if (!e_util_icon_theme_set(o, icon_path))
                         evas_object_del(o);
                       else
                         {
                            popup->app_icon = o;
                            w = width;
                            h = height;
                         }
                    }
               }

             if (!popup->app_icon)
               {
                  popup->app_icon = e_icon_add(popup->e);
                  if (!e_icon_file_set(popup->app_icon, icon_path))
                    {
                       evas_object_del(popup->app_icon);
                       popup->app_icon = NULL;
                    }
                  else e_icon_size_get(popup->app_icon, &w, &h);
               }
          }
     }
   else
     {
        popup->app_icon = e_notification_notify_raw_image_get(popup->notif,
                                                              popup->e);
        evas_object_image_filled_set(popup->app_icon, EINA_TRUE);
        evas_object_image_alpha_set(popup->app_icon, EINA_TRUE);
        evas_object_image_size_get(popup->app_icon, &w, &h);
     }

   if (!popup->app_icon)
     {
        char buf[PATH_MAX];

        snprintf(buf, sizeof(buf), "%s/e-module-notification.edj", 
                 notification_mod->dir);
        popup->app_icon = edje_object_add(popup->e);
        if (!e_theme_edje_object_set(popup->app_icon, 
                                     "base/theme/modules/notification",
                                     "e/modules/notification/logo"))
          if (!e_theme_edje_object_set(popup->app_icon, 
                                       "base/theme/modules/notification",
                                       "modules/notification/logo"))
            edje_object_file_set(popup->app_icon, buf, 
                                 "modules/notification/logo");
        w = width;
        h = height;
     }

   e_popup_object_add(popup->win, popup->app_icon);
   if ((w > width) || (h > height))
     {
        int v;
        v = w > h ? w : h;
        h = h * height / v;
        w = w * width / v;
     }
   edje_extern_object_min_size_set(popup->app_icon, w, h);
   edje_extern_object_max_size_set(popup->app_icon, w, h);

   edje_object_part_swallow(popup->theme, "notification.swallow.app_icon", 
                            popup->app_icon);
   edje_object_signal_emit(popup->theme, "notification,icon", "notification");

   /* Fill up the event message */
   _notification_format_message(popup);

   /* Compute the new size of the popup */
   edje_object_calc_force(popup->theme);
   edje_object_size_min_calc(popup->theme, &w, &h);
   w = MIN(w, popup->win->zone->w / 2);
   h = MIN(h, popup->win->zone->h / 2);
   e_popup_resize(popup->win, w, h);
}

static Popup_Data *
_notification_popup_find(unsigned int id)
{
   Eina_List *l;
   Popup_Data *popup;

   if (!id) return NULL;
   EINA_LIST_FOREACH(notification_cfg->popups, l, popup)
     if (popup->id == id)
       return popup;
   return NULL;
}

static void
_notification_reshuffle_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Popup_Data *popup;
   Eina_List *l, *l2;
   int pos = 0;

   EINA_LIST_FOREACH_SAFE(notification_cfg->popups, l, l2, popup)
     {
        if (popup->theme == obj)
          {
             popup->pending = 0;
             _notification_popdown(popup, 0);
             notification_cfg->popups = eina_list_remove_list(notification_cfg->popups, l);
          }
        else
          pos = _notification_popup_place(popup, pos);
     }
   next_pos = pos;
}

static void
_notification_popup_del(unsigned int                 id,
                        E_Notification_Notify_Closed_Reason reason)
{
   Popup_Data *popup;
   Eina_List *l;

   EINA_LIST_FOREACH(notification_cfg->popups, l, popup)
     {
        if (popup->id == id)
          {
             popup->pending = 1;
             evas_object_event_callback_add(popup->theme, EVAS_CALLBACK_DEL, _notification_reshuffle_cb, NULL);
             _notification_popdown(popup, reason);
             break;
          }
     }
}

static void
_notification_popdown(Popup_Data                  *popup,
                      E_Notification_Notify_Closed_Reason reason)
{
   E_FREE_FUNC(popup->timer, ecore_timer_del);
   popup->mirrors = eina_list_free(popup->mirrors);
   e_object_del(E_OBJECT(popup->win));
   if (popup->notif)
     {
        e_notification_notify_close(popup->notif, reason);
        e_notification_notify_free(popup->notif);
     }
   popup->notif = NULL;
   if (popup->pending) return;
   popups_displayed--;
   free(popup);
}

static void
_notification_format_message(Popup_Data *popup)
{
   Evas_Object *o = popup->theme;
   Eina_Strbuf *buf = eina_strbuf_new();
   edje_object_part_text_set(o, "notification.text.title",
                             popup->notif->sumary);
   /* FIXME: Filter to only include allowed markup? */
   /* We need to replace \n with <br>. FIXME: We need to handle all the
   * newline kinds, and paragraph separator. ATM this will suffice. */
   eina_strbuf_append(buf, popup->notif->body);
   eina_strbuf_replace_all(buf, "\n", "<br/>");
   edje_object_part_text_set(o, "notification.textblock.message",
                             eina_strbuf_string_get(buf));
   eina_strbuf_free(buf);
}
