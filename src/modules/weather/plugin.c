#include <e.h>
#include "weather.h"
#include "plugin.h"
#include "plugin_openweathermap.h"

static Eina_Bool _gather_weather_all(void *data);

static Eina_List *plugins = NULL;
static Eina_List *weathers = NULL;
static Ecore_Timer *timer = NULL;
static Eina_Binbuf *buffer = NULL;
static Eina_List *handlers = NULL;
static Eina_List *_cur_weather = NULL;
static Eina_List *_cur_plugin = NULL;
static Weather *_cur_request = NULL;

static void
_gather_current_plugin(void)
{
   Weather_Plugin *wp;
   const char *uri;
   Ecore_Con_Url *url_con;
   Weather *w;

   wp = eina_list_data_get(_cur_plugin);
   w = (_cur_request ? _cur_request : eina_list_data_get(_cur_weather));
   if (!w || !wp) return;
   fprintf(stderr, "gather info for %s\n", w->city);
   uri = wp->func.uri_get(w);
   fprintf(stderr, "gather info from %s\n", uri);
   url_con = ecore_con_url_new(uri);
   ecore_con_url_get(url_con);
   eina_stringshare_del(uri);
}

static Eina_Bool
_gather_weather(void *data EINA_UNUSED)
{
   Weather *w;
   timer = NULL;
   _cur_plugin = plugins;
   w = (_cur_request ? _cur_request : eina_list_data_get(_cur_weather));
   if (!w)
     {
        fprintf(stderr, "No weather to gather !!!! !#@$!#@$!@#$!@ HUMMMMMPF\n");
        return ECORE_CALLBACK_CANCEL;
     }
   fprintf(stderr, "gather %x data %s %d\n", _cur_weather, w->city, w->translated);
   _gather_current_plugin();
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_gather_weather_all(void *data EINA_UNUSED)
{
   timer = NULL;
   fprintf(stderr, "Gather weather ALL data %d %d\n",
           eina_list_count(weathers), eina_list_count(plugins));
   _cur_weather = weathers;
   _gather_weather(NULL);

   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_url_complete(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   const unsigned char *buf;
   size_t size;
   Eina_List *l;
   Instance *inst;
   Weather_Plugin *wp;
   Weather *w;

   if (data != buffer) return ECORE_CALLBACK_PASS_ON;

   buf = eina_binbuf_string_get(buffer);
   size = eina_binbuf_length_get(buffer);

   wp = eina_list_data_get(_cur_plugin);
   w = (_cur_request ? _cur_request : eina_list_data_get(_cur_weather));
   if (!wp || !w) return ECORE_CALLBACK_DONE;
   fprintf(stderr, "gather complete %s %d\n", w->city, w->translated);
   wp->func.parse(w, buf, size);
   eina_binbuf_reset(buffer);

   EINA_LIST_FOREACH(weather_instances, l, inst)
     {
         if (inst->weather == w)
           weather_gadget_update(inst);
     }
   _cur_plugin = eina_list_next(_cur_plugin);
   if (!_cur_plugin)
     {
        if (_cur_request)
          _cur_request = NULL;
        _cur_weather = eina_list_next(_cur_weather);
        if (!_cur_weather)
          {
             if (timer)
               ecore_timer_del(timer);
             timer = ecore_timer_add(3600.0, _gather_weather_all, NULL);
          }
        else
          {
             if (timer)
               ecore_timer_del(timer);
             timer = ecore_timer_add(3.0, _gather_weather, NULL);
          }
     }
   else
     {
        _gather_current_plugin();
     }

   return ECORE_CALLBACK_DONE;
}

static Eina_Bool
_url_data(void *data, int type EINA_UNUSED, void *event)
{
   Ecore_Con_Event_Url_Data *ev;

   if (data != buffer) return ECORE_CALLBACK_PASS_ON;
   ev = event;

   if (ev->size > 0)
     {
        eina_binbuf_append_length(buffer, ev->data, ev->size);
     }

   return ECORE_CALLBACK_DONE;
}

EINTERN void
weather_plugin_request(Weather *w)
{
   _cur_plugin = plugins;
   _cur_request = w;
   _gather_weather(NULL);
}


EINTERN void
weather_plugin_init(void)
{
   Weather_Plugin *wp;

   buffer = eina_binbuf_new();
   E_LIST_HANDLER_APPEND(handlers, ECORE_CON_EVENT_URL_COMPLETE,
                         _url_complete, buffer);
   E_LIST_HANDLER_APPEND(handlers, ECORE_CON_EVENT_URL_DATA,
                         _url_data, buffer);

   wp = plugin_openweathermap_create();
   plugins = eina_list_append(plugins, wp);
}

EINTERN void
weather_plugin_shutdown(void)
{
   Weather_Plugin *wp;

   EINA_LIST_FREE(plugins, wp)
     {
        eina_stringshare_del(wp->name);
        free(wp);
     }

   E_FREE_LIST(handlers, ecore_event_handler_del);
   eina_binbuf_free(buffer);
}

EINTERN void
weather_plugin_weather_add(Instance *inst)
{
   Weather *wl;
   Eina_List *l;

   fprintf(stderr, "Weather add %s %s %s\n", inst->cfg->city, inst->cfg->country, inst->cfg->geo_id);
   if ((!inst->cfg->city) && (!inst->cfg->country) && (!inst->cfg->geo_id))
     return;
   EINA_LIST_FOREACH(weathers, l, wl)
     {
        if ((inst->cfg->geo_id) && (inst->cfg->geo_id == wl->geo_id))
          break;
        if ((inst->cfg->city && inst->cfg->country)
            && ((inst->cfg->city == wl->city)
                && (inst->cfg->country == wl->country)))
          break;
        if ((inst->cfg->city) && (inst->cfg->city == wl->city))
          break;
     }
   if (l)
     {
        fprintf(stderr, "found weather %s %s %s\n",
                wl->geo_id, wl->city, wl->country);
        inst->weather = wl;
        if (timer)
          {
             ecore_timer_del(timer);
             timer = NULL;
          }
        timer = ecore_timer_add(1.0, _gather_weather_all, NULL);

        return;
     }
   fprintf(stderr, "create a new weather %s %s %s\n",
           inst->cfg->geo_id, inst->cfg->city, inst->cfg->country);
   inst->weather = E_NEW(Weather, 1);
   inst->weather->geo_id = eina_stringshare_add(inst->cfg->geo_id);
   inst->weather->city = eina_stringshare_add(inst->cfg->city);
   inst->weather->country = eina_stringshare_add(inst->cfg->country);
   weathers = eina_list_append(weathers, inst->weather);
   if (timer)
     {
        ecore_timer_del(timer);
     }
   timer = ecore_timer_add(1.0, _gather_weather_all, NULL);
}

EINTERN void
weather_plugin_weather_remove(Instance *inst)
{
   Instance *inst_search;
   Eina_List *l;

   if (!inst->weather) return;
   EINA_LIST_FOREACH(weather_instances, l, inst_search)
     {
        if (inst_search->weather == inst->weather)
          break;
     }
   if (!l)
     {
        weather_free(inst->weather);
        weathers = eina_list_remove(weathers, inst->weather);
     }
}

