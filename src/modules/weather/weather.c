#include "weather.h"
#include "plugin.h"

EINTERN Config *weather_config = NULL;
EINTERN Eina_List *weather_instances = NULL;


typedef enum _Weather_Gadget_Type
{
   WEATHER_GADGET_SMALL,
   WEATHER_GADGET_BIG
} Weather_Gadget_Type;

typedef struct _Wizard_Item
{
   E_Gadget_Wizard_End_Cb cb;
   void *data;
   int id;
} Wizard_Item;

typedef struct _Weather_State_Info
{
   Weather_State state;
   const char *sig_day_small;
   const char *sig_night_small;
   const char *sig_day_big;
} Weather_State_Info;

static void _weather_gadget_removed(void *data, Evas_Object *obj, void *event_info);
static Evas_Object *_weather_configure(Evas_Object *obj);

static Evas_Object *_weather_create(Evas_Object *parent, Instance *inst, E_Gadget_Site_Orient orient);
static void _weather_del(void *data, Evas *e, Evas_Object *obj, void *event);
static void _weather_mouse_down(void *data, Evas *e, Evas_Object *obj, void *event);
static void _weather_popup_new(Instance *inst);
static void _weather_popup_day_segment_changed(void *data, Evas_Object *obj, void *event);
static void _weather_popup_day_part_segment_changed(void *data, Evas_Object *obj, void *event);
static void _weather_popup_dismissed(void *data, Evas_Object *obj, void *event);
static void _weather_popup_del(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void _weather_update_object(Instance *inst, Evas_Object *obj, Weather_Data *wd, Eina_Bool big);
static Evas_Object *_weather_wizard(E_Gadget_Wizard_End_Cb cb, void *data, Evas_Object *site);
static void _weather_wizard_end(void *data, Evas *e, Evas_Object *obj, void *event);
static Config_Item *_conf_item_get(int *id);

static Weather_State_Info _signal_table[] =
{
     { WEATHER_STATE_THUNDERSTORM_RAIN_LIGHT, "thunderstorm", "night_thunderstorm", "right,day_heavyrain,sun,tstorm,rain" },
     { WEATHER_STATE_THUNDERSTORM_RAIN, "thunderstorm", "night_thunderstorm", "right,day_heavyrain,sun,tstorm,rain" },
     { WEATHER_STATE_THUNDERSTORM_RAIN_HEAVY, "thunderstorm", "night_thunderstorm", "right,day_heavyrain,sun,tstorm,rain" },
     { WEATHER_STATE_THUNDERSTORM_LIGHT, "thunderstorm", "night_thunderstorm", "right,day_heavyrain,sun,tstorm,rain" },
     { WEATHER_STATE_THUNDERSTORM, "thunderstorm", "night_thunderstorm", "right,day_heavyrain,sun,tstorm,rain" },
     { WEATHER_STATE_THUNDERSTORM_HEAVY, "thunderstorm", "night_thunderstorm", "right,day_heavyrain,sun,tstorm,rain" },
     { WEATHER_STATE_THUNDERSTORM_RAGGED, "thunderstorm", "night_thunderstorm", "right,day_heavyrain,sun,tstorm,rain" },
     { WEATHER_STATE_THUNDERSTORM_DRIZZLE_LIGHT, "thunderstorm", "night_thunderstorm", "right,day_heavyrain,sun,tstorm,rain" },
     { WEATHER_STATE_THUNDERSTORM_DRIZZLE, "thunderstorm", "night_thunderstorm", "right,day_heavyrain,sun,tstorm,rain" },
     { WEATHER_STATE_THUNDERSTORM_DRIZZLE_HEAVY, "thunderstorm", "night_thunderstorm", "right,day_heavyrain,sun,tstorm,rain" },
     { WEATHER_STATE_DRIZZLE_LIGHT, "rain", "night_rain", "right,day_rain,sun,rain,rain" },
     { WEATHER_STATE_DRIZZLE, "rain", "night_rain", "right,day_rain,sun,rain,rain" },
     { WEATHER_STATE_DRIZZLE_HEAVY, "rain", "night_rain", "right,day_rain,sun,rain,rain" },
     { WEATHER_STATE_DRIZZLE_RAIN_LIGHT, "rain", "night_rain", "right,day_rain,sun,rain,rain" },
     { WEATHER_STATE_DRIZZLE_RAIN, "rain", "night_rain", "right,day_rain,sun,rain,rain" },
     { WEATHER_STATE_DRIZZLE_RAIN_HEAVY, "rain", "night_rain", "right,day_rain,sun,rain,rain" },
     { WEATHER_STATE_DRIZZLE_RAIN_SHOWER, "rain", "night_rain", "right,day_rain,sun,rain,rain" },
     { WEATHER_STATE_DRIZZLE_RAIN_SHOWER_HEAVY, "rain", "night_rain", "right,day_rain,sun,rain,rain" },
     { WEATHER_STATE_DRIZZLE_SHOWER, "rain", "night_rain", "right,day_rain,sun,rain,rain" },
     { WEATHER_STATE_RAIN_LIGHT, "rain", "night_rain", "right,day_rain,sun,rain,rain" },
     { WEATHER_STATE_RAIN, "rain", "night_rain", "right,day_rain,sun,rain,rain" },
     { WEATHER_STATE_RAIN_HEAVY, "rain", "night_rain", "right,day_rain,sun,rain,rain" },
     { WEATHER_STATE_RAIN_VERY_HEAVY, "rain", "night_rain", "right,day_rain,sun,rain,rain" },
     { WEATHER_STATE_RAIN_FREEZING, "rain", "night_rain", "right,day_rain,sun,rain,rain" },
     { WEATHER_STATE_RAIN_SHOWER_LIGHT, "rain", "night_rain", "right,day_rain,sun,rain,rain" },
     { WEATHER_STATE_RAIN_SHOWER, "rain", "night_rain", "right,day_rain,sun,rain,rain" },
     { WEATHER_STATE_RAIN_SHOWER_HEAVY, "rain", "night_rain", "right,day_rain,sun,rain,rain" },
     { WEATHER_STATE_RAIN_RAGGED, "rain", "night_rain", "right,day_rain,sun,rain,rain" },
     { WEATHER_STATE_SNOW_LIGHT, "snow", "night_snow", "right,day_rain,sun,rain,snow" },
     { WEATHER_STATE_SNOW, "snow", "night_snow", "right,day_rain,sun,rain,snow" },
     { WEATHER_STATE_SNOW_HEAVY, "snow", "night_snow", "right,day_rain,sun,rain,snow" },
     { WEATHER_STATE_SLEET, "snow", "night_snow", "right,day_rain,sun,rain,rain_snow" },
     { WEATHER_STATE_SLEET_SHOWER, "snow", "night_snow", "right,day_rain,sun,rain,rain_snow" },
     { WEATHER_STATE_SNOW_RAIN_LIGHT, "snow", "night_snow", "right,day_rain,sun,rain,rain_snow" },
     { WEATHER_STATE_SNOW_RAIN, "snow", "night_snow", "right,day_rain,sun,rain,rain_snow" },
     { WEATHER_STATE_SNOW_SHOWER_LIGHT, "snow", "night_snow", "right,day_rain,sun,rain,rain_snow" },
     { WEATHER_STATE_SNOW_SHOWER, "snow", "night_snow", "right,day_rain,sun,rain,rain_snow" },
     { WEATHER_STATE_SNOW_SHOWER_HEAVY, "snow", "night_snow", "right,day_rain,sun,rain,rain_snow" },
     { WEATHER_STATE_MIST, "mist", "mist", "right,day_rain,sun,cloud,foggy" },
     { WEATHER_STATE_SMOKE, "mist", "mist", "right,day_rain,sun,cloud,foggy" },
     { WEATHER_STATE_HAZE, "mist", "mist", "right,day_rain,sun,cloud,foggy" },
     { WEATHER_STATE_DUST_WHIRLS, "mist", "mist", "right,day_clear,sun,isolated_cloud,windy" },
     { WEATHER_STATE_FOG, "mist", "mist", "right,day_rain,sun,cloud,foggy" },
     { WEATHER_STATE_DUST, "mist", "mist", "right,day_clear,sun,isolated_cloud,windy" },
     { WEATHER_STATE_VOLCANIC_ASH, "mist", "mist", "right,day_clear,sun,isolated_cloud,windy" },
     { WEATHER_STATE_SQUALLS, "mist", "mist", "right,day_clear,sun,isolated_cloud,windy" },
     { WEATHER_STATE_TORNADO, "rain", "night_rain", "right,day_clear,sun,isolated_cloud,windy" },
     { WEATHER_STATE_CLEAR_SKY, "clear", "night_clear", "right,day_clear,sun,nothing," },
     { WEATHER_STATE_FEW_CLOUDS, "few_clouds", "night_few_clouds", "right,day_clear,sun,isolated_cloud," },
     { WEATHER_STATE_SCATTERED_CLOUDS, "scattered_clouds", "night_scattered_clouds", "right,day_clear,sun,cloud," },
     { WEATHER_STATE_BROKEN_CLOUDS, "broken_clouds", "night_broken_clouds", "right,day_clear,sun,cloud," },
     { WEATHER_STATE_OVERCAST_CLOUDS, "broken_clouds", "night_broken_clouds", "right,day_clear,sun,cloud," },
     { WEATHER_STATE_TROPICAL_STORM, "thunderstorm", "night_thunderstorm", "right,day_heavyrain,sun,tstorm,rain" },
     { WEATHER_STATE_HURRICANE, "thunderstorm", "night_thunderstorm", "right,day_heavyrain,sun,tstorm,rain" },
     { WEATHER_STATE_COLD, "clear", "night_clear", "right,day_clear,sun,nothing," },
     { WEATHER_STATE_HOT, "clear", "night_clear", "right,day_clear,sun,nothing," },
     { WEATHER_STATE_WINDY, "clear", "night_clear", "right,day_clear,sun,isolated_cloud,windy" },
     { WEATHER_STATE_HAIL, "clear", "night_clear", "right,day_rain,sun,rain,snow" }
};



static Config_Item *
_conf_item_get(int *id)
{
    Config_Item *ci;
    Eina_List *l;

    if (*id > 0)
      {
         EINA_LIST_FOREACH(weather_config->items, l, ci)
           {
              if (ci->id == *id) return ci;
           }
      }

    ci = E_NEW(Config_Item, 1);
    if (!*id)
      ci->id = eina_list_count(weather_config->items) + 1;
    else
      ci->id = -1;

    if (ci->id < 1) return ci;
    weather_config->items = eina_list_append(weather_config->items, ci);
    e_config_save_queue();

    return ci;
}

static void
_weather_gadget_created(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst;
   Config_Item *ci;
   Eina_List *l;

   inst = data;
   fprintf(stderr, "Created lolollo\n");
   e_gadget_configure_cb_set(inst->obj, _weather_configure);
   evas_object_smart_callback_del_full(obj, "gadget_created", _weather_gadget_created, data);
   weather_plugin_weather_add(inst);
}

static void
_weather_gadget_removed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst;
   Config_Item *ci;
   Eina_List *l;

   inst = data;
   evas_object_smart_callback_del_full(obj, "gadget_created",
                                       _weather_gadget_created, inst);
   evas_object_smart_callback_del_full(obj, "gadget_removed",
                                       _weather_gadget_removed, inst);
                                       /*
   EINA_LIST_FOREACH(weather_config->items, l, ci)
     {
        if (ci->id == inst->cfg->id)
          {
              eina_stringshare_del(ci->geo_id);
              eina_stringshare_del(ci->city);
              eina_stringshare_del(ci->country);
              weather_config->items =
                 eina_list_remove_list(weather_config->items, l);
              break;
          }
     }
     * TODO  check if only one, add a refcount */

}

static Evas_Object *
_weather_wizard(E_Gadget_Wizard_End_Cb cb, void *data, Evas_Object *site)
{
   Config_Item *ci;
   Wizard_Item *wi;
   int id = 0;
   Evas_Object *ns;

   wi = E_NEW(Wizard_Item, 1);
   wi->cb = cb;
   wi->data = data;
   ci = _conf_item_get(&id);
   wi->id = ci->id;
   ns = config_weather(ci, NULL);
   evas_object_event_callback_add(ns, EVAS_CALLBACK_DEL,
                                  _weather_wizard_end, wi);
   return ns;
}

static void
_weather_wizard_end(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Wizard_Item *wi;

   wi = data;

   wi->cb(wi->data, wi->id);
   free(wi);
}

static Evas_Object *
_weather_create(Evas_Object *parent, Instance *inst, E_Gadget_Site_Orient orient EINA_UNUSED)
{
   Evas_Object *o;
   char buf[PATH_MAX];


   o = elm_layout_add(parent);
   fprintf(stderr, "Created lolollo %s %s\n", inst->cfg->city, inst->cfg->country);
   inst->obj = o;
   evas_object_data_set(o, "weather_data__", inst);
   snprintf(buf, sizeof(buf), "%s/e-module-weather.edj", e_module_dir_get(weather_config->module));
   elm_layout_file_set(o, buf, "e/gadget/weather/small");
   evas_object_size_hint_aspect_set(o, EVAS_ASPECT_CONTROL_BOTH, 1.0, 1.0);
   evas_object_size_hint_min_set(o, 32, 32);
   evas_object_size_hint_max_set(o, 32, 32);

   evas_object_smart_callback_add(parent, "gadget_created",
                                  _weather_gadget_created, inst);
   evas_object_smart_callback_add(parent, "gadget_removed",
                                  _weather_gadget_removed, inst);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                  _weather_mouse_down, inst);
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _weather_del, inst);

   weather_instances = eina_list_append(weather_instances, inst);

   return o;
}

static Evas_Object *
_weather_configure(Evas_Object *obj)
{
   Instance *inst = evas_object_data_get(obj, "weather_data__");
   return config_weather(inst->cfg, e_comp_object_util_zone_get(obj));
}

static void
_weather_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Instance *inst;

   inst = data;

   weather_instances = eina_list_remove(weather_instances, inst);
   weather_plugin_weather_remove(inst);
   free(inst);
}

static void
_weather_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Instance *inst;
   Evas_Event_Mouse_Down *ev;

   inst = data;
   ev = event;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
   if (ev->button == 1)
     {
        if (inst->popup) elm_ctxpopup_dismiss(inst->popup);
        else _weather_popup_new(inst);
     }
}

static void
_weather_popup_new(Instance *inst)
{
   Evas_Object *o;
   Evas_Object *vbx;
   Evas_Object *seg;
   Eina_List *l;
   Weather_Data *wd;
   char buf[4096];
   int last_day = -1;
   Elm_Object_Item *it;
   time_t tt;
   const struct tm *now;

   tt = time(NULL);
   now = localtime(&tt);

   if (inst->popup) return;
   if (!inst->weather) return;
   inst->popup = elm_ctxpopup_add(inst->obj);
   elm_object_style_set(inst->popup, "noblock");

   evas_object_smart_callback_add(inst->popup, "dismissed", _weather_popup_dismissed, inst);
   evas_object_event_callback_add(inst->popup, EVAS_CALLBACK_DEL, _weather_popup_del, inst);

   vbx = elm_box_add(inst->popup);
   elm_box_horizontal_set(vbx, EINA_FALSE);
   seg = elm_segment_control_add(vbx);
   evas_object_size_hint_weight_set(seg, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(seg, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_min_set(seg, 320, 0);
   EINA_LIST_FOREACH(inst->weather->datas, l, wd)
     {
        if (last_day != wd->time_start.tm_mday)
          {
             last_day = wd->time_start.tm_mday;
             if (now->tm_mday == wd->time_start.tm_mday)
               snprintf(buf, sizeof(buf), _("Today"));
             else
               strftime(buf, sizeof(buf), "%A", &wd->time_start);
             it = elm_segment_control_item_add(seg, NULL, buf);
             elm_object_item_data_set(it, wd);
          }
     }
   evas_object_smart_callback_add(seg, "changed",
                                  _weather_popup_day_segment_changed, inst);
   it = elm_segment_control_item_get(seg, 0);
   elm_box_pack_end(vbx, seg);
   evas_object_show(seg);

   seg = elm_segment_control_add(vbx);
   inst->popup_segment = seg;
   evas_object_size_hint_weight_set(seg, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(seg, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(vbx, seg);
   evas_object_smart_callback_add(seg, "changed",
                                  _weather_popup_day_part_segment_changed, inst);
   evas_object_show(seg);

   o = elm_layout_add(vbx);
   inst->popup_weather = o;
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   snprintf(buf, sizeof(buf), "%s/e-module-weather.edj",
            e_module_dir_get(weather_config->module));
   elm_layout_file_set(o, buf, "e/gadget/weather/big");
   elm_box_pack_end(vbx, o);
   evas_object_show(o);
   evas_object_show(vbx);
   evas_object_size_hint_min_set(inst->popup, 0, 360);
   elm_object_content_set(inst->popup, vbx);
   e_gadget_util_ctxpopup_place(inst->obj, inst->popup, NULL);
   evas_object_show(inst->popup);

   if (it)
     elm_segment_control_item_selected_set(it, EINA_TRUE);
}


static void
_weather_format_time(char *buf, size_t size, const struct tm *tm, Eina_Bool use_24h)
{
   if (!use_24h)
     {
        if (tm->tm_hour > 12)
          {
             snprintf(buf, size, "%d PM", (tm->tm_hour - 12));
          }
        else
          {
             if (tm->tm_hour != 0)
               snprintf(buf, size, "%d AM", tm->tm_hour);
             else
               snprintf(buf, size, "12 PM");
          }
     }
   else
     strftime(buf, size, "%H h", tm);
}

static void
_weather_popup_day_segment_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event)
{
   Elm_Object_Item *it;
   Instance *inst;
   Weather_Data *wd, *it_wd;
   Eina_List *l;
   int idx;
   char buf[4096];

   it = event;
   inst = data;
   it_wd = elm_object_item_data_get(it);

   idx = elm_segment_control_item_count_get(inst->popup_segment);
   for (--idx; idx >= 0; --idx)
     {
        elm_segment_control_item_del_at(inst->popup_segment, idx);
     }


   EINA_LIST_FOREACH(inst->weather->datas, l, wd)
     {
        if ((((wd->time_start.tm_mday == it_wd->time_start.tm_mday)
              && (wd->time_start.tm_hour != 0)))
            || ((wd->time_start.tm_mday == (it_wd->time_start.tm_mday + 1)
                 && (wd->time_start.tm_hour == 0))))
          {
             _weather_format_time(buf, sizeof(buf),
                                  &wd->time_start, inst->cfg->use_24h);
             it = elm_segment_control_item_add(inst->popup_segment, NULL, buf);
             elm_object_item_data_set(it, wd);
          }
     }
   it = elm_segment_control_item_get(inst->popup_segment, 0);
   if (it)
     elm_segment_control_item_selected_set(it, EINA_TRUE);
}

static void
_weather_popup_day_part_segment_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event)
{
   Elm_Object_Item *it;
   Instance *inst;
   Weather_Data *wd;

   it = event;
   inst = data;
   wd = elm_object_item_data_get(it);

   _weather_update_object(inst, inst->popup_weather, wd, EINA_TRUE);
}

static void
_weather_popup_dismissed(void *data EINA_UNUSED, Evas_Object *obj, void *event EINA_UNUSED)
{
   evas_object_del(obj);
}


static void
_weather_popup_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst;

   inst = data;

   inst->popup = NULL;
   inst->popup_segment = NULL;
   inst->popup_weather = NULL;
}

static double
_weather_convert_celcius_to_farhenheit(double celcius)
{
   return ((celcius * 9.) / 5.) + 32.;
}


static void
_weather_update_object(Instance *inst, Evas_Object *obj, Weather_Data *wd, Eina_Bool big)
{
   Eina_Bool night = EINA_FALSE;
   const char *condition = "";
   char buf[4096];
   char buf_time[4096];
   char buf_time2[4096];
   unsigned int i;
   time_t tt;
   const struct tm *now;


   if (wd->time_start.tm_hour <= inst->weather->sunrise.tm_hour)
     {
        night = EINA_TRUE;
     }
   else if (wd->time_start.tm_hour >= inst->weather->sunset.tm_hour)
     {
        night = EINA_TRUE;
     }



   if (!inst->weather->translated)
     {
        switch (wd->state)
          {
           case WEATHER_STATE_THUNDERSTORM_RAIN_LIGHT:
              condition = _("thunderstorm with light rain");
              break;
           case WEATHER_STATE_THUNDERSTORM_RAIN:
              condition = _("thunderstorm with rain");
              break;
           case WEATHER_STATE_THUNDERSTORM_RAIN_HEAVY:
              condition = _("thunderstorm with heavy rain");
              break;
           case WEATHER_STATE_THUNDERSTORM_LIGHT:
              condition = _("light thunderstorm");
              break;
           case WEATHER_STATE_THUNDERSTORM:
              condition = _("Thunderstorm");
              break;
           case WEATHER_STATE_THUNDERSTORM_HEAVY:
              condition = _("Heavy thunderstorm");
              break;
           case WEATHER_STATE_THUNDERSTORM_RAGGED:
              condition = _("Ragged thunderstorm");
              break;
           case WEATHER_STATE_THUNDERSTORM_DRIZZLE_LIGHT:
              condition = _("Thunderstorm with light drizzle");
              break;
           case WEATHER_STATE_THUNDERSTORM_DRIZZLE:
              condition = _("Thunderstorm with drizzle");
              break;
           case WEATHER_STATE_THUNDERSTORM_DRIZZLE_HEAVY:
              condition = _("Thunderstorm with heavy drizzle");
              break;
           case WEATHER_STATE_DRIZZLE_LIGHT:
              condition = _("Light drizzle");
              break;
           case WEATHER_STATE_DRIZZLE:
              condition = _("Drizzle");
              break;
           case WEATHER_STATE_DRIZZLE_HEAVY:
              condition = _("Heavy drizzle");
              break;
           case WEATHER_STATE_DRIZZLE_RAIN_LIGHT:
              condition = _("Drizzle with light rain");
              break;
           case WEATHER_STATE_DRIZZLE_RAIN:
              condition = _("Drizzle with rain");
              break;
           case WEATHER_STATE_DRIZZLE_RAIN_HEAVY:
              condition = _("Drizzle with heavy rain");
              break;
           case WEATHER_STATE_DRIZZLE_RAIN_SHOWER:
              condition = _("Drizzle with shower rain");
              break;
           case WEATHER_STATE_DRIZZLE_RAIN_SHOWER_HEAVY:
              condition = _("Drizzle with heavy shower rain");
              break;
           case WEATHER_STATE_DRIZZLE_SHOWER:
              condition = _("Shower drizzle");
              break;
           case WEATHER_STATE_RAIN_LIGHT:
              condition = _("Light rain");
              break;
           case WEATHER_STATE_RAIN:
              condition = _("Rain");
              break;
           case WEATHER_STATE_RAIN_HEAVY:
              condition = _("Heavy rain");
              break;
           case WEATHER_STATE_RAIN_VERY_HEAVY:
              condition = _("Very heavy rain");
              break;
           case WEATHER_STATE_RAIN_EXTREME:
              condition = _("Extreme rain");
              break;
           case WEATHER_STATE_RAIN_FREEZING:
              condition = _("Freezing rain");
              break;
           case WEATHER_STATE_RAIN_SHOWER_LIGHT:
              condition = _("Light shower rain");
              break;
           case WEATHER_STATE_RAIN_SHOWER:
              condition = _("Shower rain");
              break;
           case WEATHER_STATE_RAIN_SHOWER_HEAVY:
              condition = _("Heavy shower rain");
              break;
           case WEATHER_STATE_RAIN_RAGGED:
              condition = _("Ragged rain");
              break;
           case WEATHER_STATE_SNOW_LIGHT:
              condition = _("Light snow");
              break;
           case WEATHER_STATE_SNOW:
              condition = _("Snow");
              break;
           case WEATHER_STATE_SNOW_HEAVY:
              condition = _("Heavy snow");
              break;
           case WEATHER_STATE_SLEET:
              condition = _("Sleet");
              break;
           case WEATHER_STATE_SLEET_SHOWER:
              condition = _("Shower sleet");
              break;
           case WEATHER_STATE_SNOW_RAIN_LIGHT:
              condition = _("Snow with light rain");
              break;
           case WEATHER_STATE_SNOW_RAIN:
              condition = _("Snow with rain");
              break;
           case WEATHER_STATE_SNOW_SHOWER_LIGHT:
              condition = _("Light shower snow");
              break;
           case WEATHER_STATE_SNOW_SHOWER:
              condition = _("Shower snow");
              break;
           case WEATHER_STATE_SNOW_SHOWER_HEAVY:
              condition = _("Heavy shower snow");
              break;
           case WEATHER_STATE_MIST:
              condition = _("Mist");
              break;
           case WEATHER_STATE_SMOKE:
              condition = _("Smoke");
              break;
           case WEATHER_STATE_HAZE:
              condition = _("Haze");
              break;
           case WEATHER_STATE_DUST_WHIRLS:
              condition = _("Sand, dust whirls");
              break;
           case WEATHER_STATE_FOG:
              condition = _("Fog");
              break;
           case WEATHER_STATE_SAND:
              condition = _("Sand");
              break;
           case WEATHER_STATE_DUST:
              condition = _("Dust");
              break;
           case WEATHER_STATE_VOLCANIC_ASH:
              condition = _("Volcanic ash");
              break;
           case WEATHER_STATE_SQUALLS:
              condition = _("Squalls");
              break;
           case WEATHER_STATE_TORNADO:
              condition = _("Tornado");
              break;
           case WEATHER_STATE_CLEAR_SKY:
              condition = _("Clear");
              break;
           case WEATHER_STATE_FEW_CLOUDS:
              condition = _("Few clouds");
              break;
           case WEATHER_STATE_SCATTERED_CLOUDS:
              condition = _("Scattered clouds");
              break;
           case WEATHER_STATE_BROKEN_CLOUDS:
              condition = _("Broken clouds");
              break;
           case WEATHER_STATE_OVERCAST_CLOUDS:
              condition = _("Overcast clouds");
              break;
           case WEATHER_STATE_TROPICAL_STORM:
              condition = _("Tropical storm");
              break;
           case WEATHER_STATE_HURRICANE:
              condition = _("Hurricane");
              break;
           case WEATHER_STATE_COLD:
              condition = _("Cold");
              break;
           case WEATHER_STATE_HOT:
              condition = _("Hot");
              break;
           case WEATHER_STATE_WINDY:
              condition = _("Windy");
              break;
           case WEATHER_STATE_HAIL:
              condition = _("Hail");
              break;
          }
        elm_object_part_text_set(obj, "text.condition", condition);
     }
   else
     elm_object_part_text_set(obj, "text.condition", wd->state_name);

   if (inst->cfg->use_celcius)
     snprintf(buf, sizeof(buf), "%01.1f °C", wd->temperature.value);
   else
     snprintf(buf, sizeof(buf), "%01.1f °F",
              _weather_convert_celcius_to_farhenheit(wd->temperature.value));
   elm_object_part_text_set(obj, "text.temp", buf);

   if (big)
     {
        _weather_format_time(buf_time, sizeof(buf_time),
                             &wd->time_start, inst->cfg->use_24h);
        _weather_format_time(buf_time2, sizeof(buf_time2),
                             &wd->time_end, inst->cfg->use_24h);
        snprintf(buf, sizeof(buf), "%s - %s", buf_time, buf_time2);
        elm_object_part_text_set(obj, "text.date", buf);
        snprintf(buf, sizeof(buf), "%s (%s)",
                 inst->weather->city, inst->weather->country);
        elm_object_part_text_set(obj, "text.city", buf);
        tt = time(NULL);
        now = localtime(&tt);
        if (now->tm_mday == wd->time_start.tm_mday)
          {
             strftime(buf_time, sizeof(buf_time), "%X", &inst->weather->sunrise);
             snprintf(buf, sizeof(buf), _("sunrise %s"), buf_time);
             elm_object_part_text_set(obj, "text.sunrise", buf);
             strftime(buf_time, sizeof(buf_time), "%X", &inst->weather->sunset);
             snprintf(buf, sizeof(buf), _("Sunset %s"), buf_time);
             elm_object_part_text_set(obj, "text.sunrise", buf);
          }
        if (inst->cfg->use_celcius)
          snprintf(buf, sizeof(buf), "%01.1f °C", wd->temperature.value_min);
        else
          snprintf(buf, sizeof(buf), "%01.1f °F",
                   _weather_convert_celcius_to_farhenheit(wd->temperature.value_min));
        elm_object_part_text_set(obj, "text.temp_min", buf);
        if (inst->cfg->use_celcius)
          snprintf(buf, sizeof(buf), "%01.1f °C", wd->temperature.value_max);
        else
          snprintf(buf, sizeof(buf), "%01.1f °F",
                   _weather_convert_celcius_to_farhenheit(wd->temperature.value_max));
        elm_object_part_text_set(obj, "text.temp_max", buf);
        snprintf(buf, sizeof(buf), _("Wind : %s"), wd->wind.name);
        elm_object_part_text_set(obj, "text.wind", buf);
        if (inst->cfg->use_metric)
          snprintf(buf, sizeof(buf), _("Speed : %01.1f Kmh"), (wd->wind.speed * 3.6));
        else
          snprintf(buf, sizeof(buf), _("Speed : %01.1f Mph"), (wd->wind.speed * 2.2369));
        elm_object_part_text_set(obj, "text.wind_speed", buf);
        snprintf(buf, sizeof(buf), _("Direction : %s"), wd->wind.direction);
        elm_object_part_text_set(obj, "text.wind_direction", buf);
        snprintf(buf, sizeof(buf), _("Pressure : %01.1f %s"), wd->pressure.value, wd->pressure.unit);
        elm_object_part_text_set(obj, "text.pressure", buf);
        snprintf(buf, sizeof(buf), _("Humidity : %01d %s"), wd->humidity.value, wd->humidity.unit);
        elm_object_part_text_set(obj, "text.humidity", buf);
        snprintf(buf, sizeof(buf), _("Precipitation : %01.2f mm"), wd->precipitation.value);
        elm_object_part_text_set(obj, "text.precipitation", buf);
        snprintf(buf, sizeof(buf), _("Cloud : %01d %s"), wd->cloud.value, wd->cloud.unit);
        elm_object_part_text_set(obj, "text.cloud", buf);
     }

   for (i = 0; i < (sizeof(_signal_table) / sizeof(_signal_table[0])); ++i)
     {
        if (_signal_table[i].state == wd->state)
          {
             if (night)
               {
                  if (big)
                    elm_object_signal_emit(obj, _signal_table[i].sig_day_big, "");
                  else
                    elm_object_signal_emit(obj, _signal_table[i].sig_night_small, "");
               }
             else
               {
                  if (big)
                    elm_object_signal_emit(obj, _signal_table[i].sig_day_big, "");
                  else
                    elm_object_signal_emit(obj, _signal_table[i].sig_day_small, "");
               }
             break;
          }
     }

}

EINTERN void
weather_gadget_update(Instance *inst)
{
   Eina_Bool cfg_update = EINA_FALSE;
   Weather_Data *wd;

   if (inst->cfg->city != inst->weather->city)
     {
        eina_stringshare_replace(&inst->cfg->city, inst->weather->city);
        cfg_update = EINA_TRUE;
     }
   if (inst->cfg->country != inst->weather->country)
     {
        eina_stringshare_replace(&inst->cfg->country, inst->weather->country);
        cfg_update = EINA_TRUE;
     }
   if (inst->cfg->geo_id != inst->weather->geo_id)
     {
        eina_stringshare_replace(&inst->cfg->geo_id, inst->weather->geo_id);
        cfg_update = EINA_TRUE;
     }
   if (cfg_update) e_config_save_queue();
   if (inst->obj)
     {
        wd = eina_list_data_get(inst->weather->datas);
        if (!wd) return;
        fprintf(stderr, "updating gadget\n");
        _weather_update_object(inst, inst->obj, wd, EINA_FALSE);
     }
}

EINTERN Evas_Object *
weather_small_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient)
{
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->cfg = _conf_item_get(id);
   return _weather_create(parent, inst, orient);
}

EINTERN Evas_Object *
weather_big_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient)
{
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->cfg = _conf_item_get(id);
   return _weather_create(parent, inst, orient);
}

EINTERN Evas_Object *
weather_small_wizard(E_Gadget_Wizard_End_Cb cb, void *data, Evas_Object *site)
{
   return _weather_wizard(cb, data, site);
}


EINTERN Evas_Object *
weather_big_wizard(E_Gadget_Wizard_End_Cb cb, void *data, Evas_Object *site)
{
   return _weather_wizard(cb, data, site);
}

EINTERN void
weather_data_free(Weather_Data *wd)
{
   eina_stringshare_del(wd->state_name);
   eina_stringshare_del(wd->temperature.unit);
   eina_stringshare_del(wd->precipitation.type);
   eina_stringshare_del(wd->wind.name);
   eina_stringshare_del(wd->wind.direction_name);
   eina_stringshare_del(wd->wind.direction);
   eina_stringshare_del(wd->cloud.name);
   eina_stringshare_del(wd->cloud.unit);
   eina_stringshare_del(wd->humidity.unit);
   eina_stringshare_del(wd->pressure.unit);
   free(wd);
}

EINTERN void
weather_free(Weather *weather)
{
   Weather_Data *wd;

   eina_stringshare_del(weather->city);
   eina_stringshare_del(weather->country);
   eina_stringshare_del(weather->geo_id);
   EINA_LIST_FREE(weather->datas, wd)
      weather_data_free(wd);
   free(weather);
}
