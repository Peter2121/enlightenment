#include "weather.h"
#include "plugin.h"
#include <e.h>

static Weather *_tmp_weather = NULL;

static void
_config_label_add(Evas_Object *tb, const char *txt, int row)
{
   Evas_Object *o;

   o = elm_label_add(tb);
   E_ALIGN(o, 0, 0.5);
   elm_object_text_set(o, txt);
   evas_object_show(o);
   elm_table_pack(tb, o, 0, row, 1, 1);
}

static void
_config_close(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   weather_config->config_dialog = NULL;
   if (_tmp_weather)
     {
        eina_stringshare_del(_tmp_weather->city);
        eina_stringshare_del(_tmp_weather->country);
        E_FREE(_tmp_weather);
        _tmp_weather = NULL;
     }
}

static void
_config_weather_request(Config_Item *ci)
{
   if (!_tmp_weather)
     _tmp_weather = E_NEW(Weather, 1);
   eina_stringshare_replace(&_tmp_weather->city, ci->city);
   eina_stringshare_replace(&_tmp_weather->country, ci->country);
   weather_plugin_request(_tmp_weather);
}

static void
_config_weather_city_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   const char *city;
   Config_Item *ci;

   ci = data;

   city = elm_entry_entry_get(obj);
   eina_stringshare_replace(&ci->city, city);
   _config_weather_request(ci);
}

static void
_config_weather_country_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   const char *country;
   Config_Item *ci;

   ci = data;

   country = elm_entry_entry_get(obj);
   eina_stringshare_replace(&ci->country, country);
}


static void
_config_weather_temperature_unit_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Config_Item *ci;

   ci = data;

   ci->use_celcius = elm_check_state_get(obj);
}

static void
_config_weather_speed_unit_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Config_Item *ci;

   ci = data;

   ci->use_metric = elm_check_state_get(obj);
}

static void
_config_weather_24h_format_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Config_Item *ci;

   ci = data;

   ci->use_24h = elm_check_state_get(obj);
}

EINTERN Evas_Object *
config_weather(Config_Item *ci, E_Zone *zone)
{
   Evas_Object *popup, *tb, *fr, *o;
   int row = 0;

   if (!zone) zone = e_zone_current_get();

   popup = elm_popup_add(e_comp->elm);
   E_EXPAND(popup);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   elm_popup_allow_events_set(popup, 1);
   elm_popup_scrollable_set(popup, 1);

   tb = elm_table_add(popup);
   E_EXPAND(tb);
   evas_object_show(tb);
   elm_object_content_set(popup, tb);

   _config_label_add(tb, _("City name:"), row++);

   fr = elm_frame_add(tb);
   elm_object_style_set(fr, "pad_medium");
   E_FILL(fr);
   E_EXPAND(fr);
   elm_table_pack(tb, fr, 0, row++, 2, 1);
   evas_object_show(fr);

   o = elm_entry_add(tb);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_entry_scrollable_set(o, EINA_TRUE);
   elm_object_text_set(o, ci->city);
   E_FILL(o);
   evas_object_show(o);
   elm_object_focus_set(o, EINA_TRUE);
   evas_object_smart_callback_add(o, "changed,user", _config_weather_city_changed, ci);
   elm_object_part_text_set(o, "guide", _("Your city"));
   elm_object_content_set(fr, o);
   evas_object_show(o);

   _config_label_add(tb, _("Country name:"), row++);

   fr = elm_frame_add(tb);
   elm_object_style_set(fr, "pad_medium");
   E_FILL(fr);
   E_EXPAND(fr);
   elm_table_pack(tb, fr, 0, row++, 2, 1);
   evas_object_show(fr);

   o = elm_entry_add(tb);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_entry_scrollable_set(o, EINA_TRUE);
   elm_object_text_set(o, ci->country);
   E_FILL(o);
   evas_object_show(o);
   elm_object_focus_set(o, EINA_TRUE);
   evas_object_smart_callback_add(o, "changed,user", _config_weather_country_changed, ci);
   elm_object_part_text_set(o, "guide", _("Your country"));
   elm_object_content_set(fr, o);
   evas_object_show(o);


   _config_label_add(tb, _("Temperature unit:"), row);
   o = elm_check_add(tb);
   E_FILL(o);
   elm_object_style_set(o, "toggle");
   elm_object_part_text_set(o, "off", _("Farhenheit"));
   elm_object_part_text_set(o, "on", _("Celcius"));
   elm_table_pack(tb, o, 1, row++, 1, 1);
   elm_check_state_set(o, ci->use_celcius);
   evas_object_smart_callback_add(o, "changed", _config_weather_temperature_unit_changed, ci);
   evas_object_show(o);

   _config_label_add(tb, _("Speed unit:"), row);
   o = elm_check_add(tb);
   E_FILL(o);
   elm_object_style_set(o, "toggle");
   elm_object_part_text_set(o, "off", _("Imperial (mph)"));
   elm_object_part_text_set(o, "on", _("Metric (kmh)"));
   elm_table_pack(tb, o, 1, row++, 1, 1);
   elm_check_state_set(o, ci->use_metric);
   evas_object_smart_callback_add(o, "changed", _config_weather_speed_unit_changed, ci);
   evas_object_show(o);

   _config_label_add(tb, _("24 hour format:"), row);
   o = elm_check_add(tb);
   E_FILL(o);
   elm_object_style_set(o, "toggle");
   elm_object_part_text_set(o, "off", _("Disabled"));
   elm_object_part_text_set(o, "on", _("Enabled"));
   elm_table_pack(tb, o, 1, row++, 1, 1);
   elm_check_state_set(o, ci->use_24h);
   evas_object_smart_callback_add(o, "changed", _config_weather_24h_format_changed, ci);
   evas_object_show(o);


   popup = e_comp_object_util_add(popup, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   evas_object_move(popup, zone->x, zone->y);
   evas_object_resize(popup, zone->w / 4, zone->h / 3);
   e_comp_object_util_center(popup);
   evas_object_show(popup);
   e_comp_object_util_autoclose(popup, NULL, e_comp_object_util_autoclose_on_escape, NULL);
   evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL, _config_close, NULL);

   return weather_config->config_dialog = popup;
}


