#define _GNU_SOURCE 1
#include <time.h>
#include <e.h>
#include "weather.h"
#include "plugin.h"


#define WEATHER_URL "api.openweathermap.org/data/2.5/"
#define WEATHER_FORECAST_URL WEATHER_URL "forecast"
#define WEATHER_WEATHER_URL WEATHER_URL "weather"
#define WEATHER_CITY "?q=%s"
#define WEATHER_CITY_COUNTRY "?q=%s,%s"
#define WEATHER_ID "?id=%s"
#define WEATHER_PARAMS "&units=metric&mode=xml"
#define WEATHER_API_KEY "&APPID=" OPENWEATHERMAP_API_KEY

typedef enum Weather_Tag_
{
   WEATHER_TAG_NONE = 0,
   WEATHER_TAG_CITY = (1 << 0),
   WEATHER_TAG_COUNTRY = (1 << 1),
   WEATHER_TAG_LOCATION = (1 << 2)
} Weather_Tag;

static Weather_Tag _xml_tag = 0;
static Weather_Data *_current_wd = NULL;

static Eina_Bool
_location_attr(void *data, const char *key, const char *value)
{
   Weather *weather;

   weather = data;

   if (!strncmp(key, "altitude", strlen("altitude")))
     weather->altitude = atoi(value);
   else if (!strncmp(key, "latitude", strlen("latitude")))
     weather->latitude = atof(value);
   else if (!strncmp(key, "longitude", strlen("longitude")))
     weather->longitude = atof(value);

   return EINA_TRUE;

}

static Eina_Bool
_sun_attr(void *data, const char *key, const char *value)
{
   Weather *weather;

   weather = data;

   if (!strncmp(key, "rise", strlen("rise")))
     strptime(value, "%Y-%m-%dT%H:%M:%S", &weather->sunrise);
   if (!strncmp(key, "set", strlen("set")))
     strptime(value, "%Y-%m-%dT%H:%M:%S", &weather->sunset);

   return EINA_TRUE;
}

static Eina_Bool
_time_attr(void *data, const char *key, const char *value)
{
   Weather *weather;
   Weather_Data *wd = NULL;
   struct tm time_start;
   Eina_List *l;

   weather = data;

   if (!strncmp(key, "from", strlen("from")))
     {
        strptime(value, "%Y-%m-%dT%H:%M:%S", &time_start);
        EINA_LIST_FOREACH(weather->datas, l, wd)
          {
             if ((wd->time_start.tm_hour == time_start.tm_hour)
                 && (wd->time_start.tm_yday == time_start.tm_yday)
                 && (wd->time_start.tm_year == time_start.tm_year))
               break;
          }
        if (!wd)
          {
             wd = E_NEW(Weather_Data, 1);
             memcpy(&wd->time_start, &time_start, sizeof(struct tm));
             weather->datas = eina_list_append(weather->datas, wd);
          }
        _current_wd = wd;
        wd->updated = EINA_TRUE;
     }
   if (!strncmp(key, "to", strlen("to")))
     strptime(value, "%Y-%m-%dT%H:%M:%S", &_current_wd->time_end);


   return EINA_TRUE;
}

static Eina_Bool
_symbol_attr(void *data EINA_UNUSED, const char *key, const char *value)
{
   Weather_Data *wd;
   int type;

   wd = _current_wd;
   if (!wd) return EINA_FALSE;

   if (!strncmp(key, "name", strlen("name")))
     eina_stringshare_replace(&wd->state_name, value);
   if (!strncmp(key, "number", strlen("number")))
     {
        type = atoi(value);
        switch (type)
          {
           case 200: wd->state = WEATHER_STATE_THUNDERSTORM_RAIN_LIGHT; break;
           case 201: wd->state = WEATHER_STATE_THUNDERSTORM_RAIN; break;
           case 202: wd->state = WEATHER_STATE_THUNDERSTORM_RAIN_HEAVY; break;
           case 210: wd->state = WEATHER_STATE_THUNDERSTORM_LIGHT; break;
           case 211: wd->state = WEATHER_STATE_THUNDERSTORM; break;
           case 212: wd->state = WEATHER_STATE_THUNDERSTORM_HEAVY; break;
           case 221: wd->state = WEATHER_STATE_THUNDERSTORM_RAGGED; break;
           case 230: wd->state = WEATHER_STATE_THUNDERSTORM_DRIZZLE_LIGHT; break;
           case 231: wd->state = WEATHER_STATE_THUNDERSTORM_DRIZZLE; break;
           case 232: wd->state = WEATHER_STATE_THUNDERSTORM_DRIZZLE_HEAVY; break;
           case 300: wd->state = WEATHER_STATE_DRIZZLE_LIGHT; break;
           case 301: wd->state = WEATHER_STATE_DRIZZLE; break;
           case 302: wd->state = WEATHER_STATE_DRIZZLE_HEAVY; break;
           case 310: wd->state = WEATHER_STATE_DRIZZLE_RAIN_LIGHT; break;
           case 311: wd->state = WEATHER_STATE_DRIZZLE_RAIN; break;
           case 312: wd->state = WEATHER_STATE_DRIZZLE_RAIN_HEAVY; break;
           case 313: wd->state = WEATHER_STATE_DRIZZLE_RAIN_SHOWER; break;
           case 314: wd->state = WEATHER_STATE_DRIZZLE_RAIN_SHOWER_HEAVY; break;
           case 321: wd->state = WEATHER_STATE_DRIZZLE_SHOWER; break;
           case 500: wd->state = WEATHER_STATE_RAIN_LIGHT; break;
           case 501: wd->state = WEATHER_STATE_RAIN; break;
           case 502: wd->state = WEATHER_STATE_RAIN_HEAVY; break;
           case 503: wd->state = WEATHER_STATE_RAIN_VERY_HEAVY; break;
           case 504: wd->state = WEATHER_STATE_RAIN_EXTREME; break;
           case 511: wd->state = WEATHER_STATE_RAIN_FREEZING; break;
           case 520: wd->state = WEATHER_STATE_RAIN_SHOWER_LIGHT; break;
           case 521: wd->state = WEATHER_STATE_RAIN_SHOWER; break;
           case 522: wd->state = WEATHER_STATE_RAIN_SHOWER_HEAVY; break;
           case 531: wd->state = WEATHER_STATE_RAIN_RAGGED; break;
           case 600: wd->state = WEATHER_STATE_SNOW_LIGHT; break;
           case 601: wd->state = WEATHER_STATE_SNOW; break;
           case 602: wd->state = WEATHER_STATE_SNOW_HEAVY; break;
           case 611: wd->state = WEATHER_STATE_SLEET; break;
           case 612: wd->state = WEATHER_STATE_SLEET_SHOWER; break;
           case 615: wd->state = WEATHER_STATE_SNOW_RAIN_LIGHT; break;
           case 616: wd->state = WEATHER_STATE_SNOW_RAIN; break;
           case 620: wd->state = WEATHER_STATE_SNOW_SHOWER_LIGHT; break;
           case 621: wd->state = WEATHER_STATE_SNOW_SHOWER; break;
           case 622: wd->state = WEATHER_STATE_SNOW_SHOWER_HEAVY; break;
           case 701: wd->state = WEATHER_STATE_MIST; break;
           case 711: wd->state = WEATHER_STATE_SMOKE; break;
           case 721: wd->state = WEATHER_STATE_HAZE; break;
           case 731: wd->state = WEATHER_STATE_DUST_WHIRLS; break;
           case 741: wd->state = WEATHER_STATE_FOG; break;
           case 751: wd->state = WEATHER_STATE_SAND; break;
           case 761: wd->state = WEATHER_STATE_DUST; break;
           case 762: wd->state = WEATHER_STATE_VOLCANIC_ASH; break;
           case 771: wd->state = WEATHER_STATE_SQUALLS; break;
           case 900:
           case 781: wd->state = WEATHER_STATE_TORNADO; break;
           case 800: wd->state = WEATHER_STATE_CLEAR_SKY; break;
           case 801: wd->state = WEATHER_STATE_FEW_CLOUDS; break;
           case 802: wd->state = WEATHER_STATE_SCATTERED_CLOUDS; break;
           case 803: wd->state = WEATHER_STATE_BROKEN_CLOUDS; break;
           case 804: wd->state = WEATHER_STATE_OVERCAST_CLOUDS; break;
           case 901: wd->state = WEATHER_STATE_TROPICAL_STORM; break;
           case 902: wd->state = WEATHER_STATE_HURRICANE; break;
           case 903: wd->state = WEATHER_STATE_COLD; break;
           case 904: wd->state = WEATHER_STATE_HOT; break;
           case 905: wd->state = WEATHER_STATE_WINDY; break;
           case 906: wd->state = WEATHER_STATE_HAIL; break;
          }
     }

   return EINA_TRUE;
}

static Eina_Bool
_precipitation_attr(void *data EINA_UNUSED, const char *key, const char *value)
{
   Weather_Data *wd;

   wd = _current_wd;
   if (!wd) return EINA_FALSE;

   if (!strncmp(key, "type", strlen("type")))
     eina_stringshare_replace(&wd->precipitation.type, value);
   if (!strncmp(key, "value", strlen("value")))
     wd->precipitation.value = atof(value);

   return EINA_TRUE;
}

static Eina_Bool
_wind_direction_attr(void *data EINA_UNUSED, const char *key, const char *value)
{
   Weather_Data *wd;

   wd = _current_wd;
   if (!wd) return EINA_FALSE;

   if (!strncmp(key, "deg", strlen("deg")))
     wd->wind.direction_value = atof(value);
   else if (!strncmp(key, "code", strlen("code")))
     eina_stringshare_replace(&wd->wind.direction, value);
   else if (!strncmp(key, "name", strlen("name")))
     eina_stringshare_replace(&wd->wind.direction_name, value);

   return EINA_TRUE;
}

static Eina_Bool
_wind_speed_attr(void *data EINA_UNUSED, const char *key, const char *value)
{
   Weather_Data *wd;

   wd = _current_wd;
   if (!wd) return EINA_FALSE;

   if (!strncmp(key, "mps", strlen("mps")))
     wd->wind.speed = atof(value);
   else if (!strncmp(key, "name", strlen("name")))
     eina_stringshare_replace(&wd->wind.name, value);

   return EINA_TRUE;
}

static Eina_Bool
_temperature_attr(void *data EINA_UNUSED, const char *key, const char *value)
{
   Weather_Data *wd;

   wd = _current_wd;
   if (!wd) return EINA_FALSE;

   if (!strncmp(key, "unit", strlen("unit")))
     eina_stringshare_replace(&wd->temperature.unit, value);
   else if (!strncmp(key, "value", strlen("value")))
     wd->temperature.value = atof(value);
   else if (!strncmp(key, "min", strlen("min")))
     wd->temperature.value_min = atof(value);
   else if (!strncmp(key, "max", strlen("max")))
     wd->temperature.value_max = atof(value);

   return EINA_TRUE;
}

static Eina_Bool
_pressure_attr(void *data EINA_UNUSED, const char *key, const char *value)
{
   Weather_Data *wd;

   wd = _current_wd;
   if (!wd) return EINA_FALSE;

   if (!strncmp(key, "unit", strlen("unit")))
     eina_stringshare_replace(&wd->pressure.unit, value);
   else if (!strncmp(key, "value", strlen("value")))
     wd->pressure.value = atof(value);

   return EINA_TRUE;
}

static Eina_Bool
_humidity_attr(void *data EINA_UNUSED, const char *key, const char *value)
{
   Weather_Data *wd;

   wd = _current_wd;
   if (!wd) return EINA_FALSE;

   if (!strncmp(key, "unit", strlen("unit")))
     eina_stringshare_replace(&wd->humidity.unit, value);
   else if (!strncmp(key, "value", strlen("value")))
     wd->humidity.value = atof(value);

   return EINA_TRUE;
}

static Eina_Bool
_clouds_attr(void *data EINA_UNUSED, const char *key, const char *value)
{
   Weather_Data *wd;

   wd = _current_wd;
   if (!wd) return EINA_FALSE;

   if (!strncmp(key, "unit", strlen("unit")))
     eina_stringshare_replace(&wd->cloud.unit, value);
   if (!strncmp(key, "all", strlen("all")))
     wd->cloud.value = atoi(value);
   else if (!strncmp(key, "value", strlen("value")))
     eina_stringshare_replace(&wd->cloud.name, value);

   return EINA_TRUE;
}


static Eina_Bool
_tag_cb(void *data, Eina_Simple_XML_Type type, const char *content, unsigned int offset EINA_UNUSED, unsigned int length)
{
   char buf[4096];
   const char *tags;
   Weather *weather;

   weather = data;

   if (type == EINA_SIMPLE_XML_OPEN)
     {
        if (!strncmp(content, "name", strlen("name")))
          _xml_tag = WEATHER_TAG_CITY;
        else if (!strncmp(content, "country", strlen("country")))
          _xml_tag = WEATHER_TAG_COUNTRY;
        else if (!strncmp(content, "location", strlen("location")))
          {
             if (weather->plugin_error) weather->plugin_error = EINA_FALSE;
             tags = eina_simple_xml_tag_attributes_find(content, length);
             eina_simple_xml_attributes_parse(tags,
                                              length - (tags - content),
                                              _location_attr, weather);
          }
        else if (!strncmp(content, "sun", strlen("sun")))
          {
             tags = eina_simple_xml_tag_attributes_find(content, length);
             eina_simple_xml_attributes_parse(tags,
                                              length - (tags - content),
                                              _sun_attr, weather);
          }
        else if (!strncmp(content, "timezone", strlen("timezone")))
          {
          }
        else if (!strncmp(content, "time", strlen("time")))
          {
             tags = eina_simple_xml_tag_attributes_find(content, length);
             eina_simple_xml_attributes_parse(tags,
                                              length - (tags - content),
                                              _time_attr, weather);
          }
        else if (!strncmp(content, "symbol", strlen("symbol")))
          {
             tags = eina_simple_xml_tag_attributes_find(content, length);
             eina_simple_xml_attributes_parse(tags,
                                              length - (tags - content),
                                              _symbol_attr, weather);
          }
        else if (!strncmp(content, "precipitation", strlen("precipitation")))
          {
             tags = eina_simple_xml_tag_attributes_find(content, length);
             eina_simple_xml_attributes_parse(tags,
                                              length - (tags - content),
                                              _precipitation_attr, weather);
          }
        else if (!strncmp(content, "windDirection", strlen("windDirection")))
          {
             tags = eina_simple_xml_tag_attributes_find(content, length);
             eina_simple_xml_attributes_parse(tags,
                                              length - (tags - content),
                                              _wind_direction_attr, weather);
          }
        else if (!strncmp(content, "windSpeed", strlen("windSpeed")))
          {
             tags = eina_simple_xml_tag_attributes_find(content, length);
             eina_simple_xml_attributes_parse(tags,
                                              length - (tags - content),
                                              _wind_speed_attr, weather);
          }
        else if (!strncmp(content, "temperature", strlen("temperature")))
          {
             tags = eina_simple_xml_tag_attributes_find(content, length);
             eina_simple_xml_attributes_parse(tags,
                                              length - (tags - content),
                                              _temperature_attr, weather);
          }
        else if (!strncmp(content, "pressure", strlen("pressure")))
          {
             tags = eina_simple_xml_tag_attributes_find(content, length);
             eina_simple_xml_attributes_parse(tags,
                                              length - (tags - content),
                                              _pressure_attr, weather);
          }
        else if (!strncmp(content, "humidity", strlen("humidity")))
          {
             tags = eina_simple_xml_tag_attributes_find(content, length);
             eina_simple_xml_attributes_parse(tags,
                                              length - (tags - content),
                                              _humidity_attr, weather);
          }
        else if (!strncmp(content, "clouds", strlen("clouds")))
          {
             tags = eina_simple_xml_tag_attributes_find(content, length);
             eina_simple_xml_attributes_parse(tags,
                                              length - (tags - content),
                                              _clouds_attr, weather);
          }
     }
   else if (type == EINA_SIMPLE_XML_DATA)
     {
        snprintf(buf, length + 1, "%s", content);
        //        fprintf(stderr, "Type %s\n", buf);
        switch (_xml_tag)
          {
           case WEATHER_TAG_CITY:
              if (!weather->city)
                eina_stringshare_replace(&weather->city, buf);
              break;
           case WEATHER_TAG_COUNTRY:
              if (!weather->country)
                eina_stringshare_replace(&weather->country, buf);
              break;
           case WEATHER_TAG_LOCATION:
           case WEATHER_TAG_NONE:
              break;
          }
        _xml_tag = WEATHER_TAG_NONE;
     }

   return EINA_TRUE;
}

static Eina_Bool
_plugin_openweathermap_parse(Weather *weather, const unsigned char *buf, size_t size)
{
   Weather_Data *wd;
   Eina_List *l, *ll;

   printf("%s\n", buf);

   weather->plugin_error = EINA_TRUE;
   EINA_LIST_FOREACH(weather->datas, l, wd)
     {
        wd->updated = EINA_FALSE;
     }
   eina_simple_xml_parse((const char *)buf, size, EINA_TRUE, _tag_cb, weather);
   if (!weather->plugin_error)
     {
        fprintf(stderr, "Openweather error %s\n", weather->city);
        EINA_LIST_FOREACH_SAFE(weather->datas, l, ll, wd)
          {
             if (!wd->updated)
               weather->datas = eina_list_remove_list(weather->datas, l);
          }
     }



#if 0
   fprintf(stderr, "City %s\nCountry %s\n", weather->city, weather->country);
   fprintf(stderr, "Location alt %d geo %.5f %.5f\n", weather->altitude, weather->latitude, weather->longitude);
   fprintf(stderr, "Sun %s %s\n", weather->sunrise, weather->sunset);
   EINA_LIST_FOREACH(weather->datas, l, wd)
     {
        fprintf(stderr, "Time %02d:%02d:%02d ", wd->time_start.tm_hour, wd->time_start.tm_min, wd->time_start.tm_sec);
//        fprintf(stderr, "Value %s\n", wd->state);
        fprintf(stderr, "\t%f min %f max %f unit %s\n", wd->temperature.value, wd->temperature.value_min, wd->temperature.value_max, wd->temperature.unit);
        fprintf(stderr, "\t%s %f\n", wd->precipitation.type, wd->precipitation.value);
        fprintf(stderr, "\t%s %s %s %f %f\n", wd->wind.name, wd->wind.direction_name, wd->wind.direction, wd->wind.speed);
        fprintf(stderr, "\t%s %d %s\n", wd->cloud.name, wd->cloud.value, wd->cloud.unit);
        fprintf(stderr, "\t%d %s\n", wd->humidity.value, wd->humidity.unit);
        fprintf(stderr, "\t%f %s\n", wd->pressure.value, wd->pressure.unit);
     }
#endif
   return weather->plugin_error;
}

static const char *
_plugin_openweathermap_uri_get(Weather *w)
{
   char buf[4096];
   char buf_lang[1024];
   const char *lang;
   E_Locale_Parts *lang_part = NULL;

   lang = e_intl_language_get();
   if (lang)
     {
        lang_part = e_intl_locale_parts_get(lang);
     }

   if (lang_part)
     {
        if ((!strcmp(lang_part->lang, "en"))
            || (!strcmp(lang_part->lang, "ru"))
            || (!strcmp(lang_part->lang, "it"))
            || (!strcmp(lang_part->lang, "es"))
            || (!strcmp(lang_part->lang, "uk"))
            || (!strcmp(lang_part->lang, "de"))
            || (!strcmp(lang_part->lang, "pt"))
            || (!strcmp(lang_part->lang, "ro"))
            || (!strcmp(lang_part->lang, "pl"))
            || (!strcmp(lang_part->lang, "fi"))
            || (!strcmp(lang_part->lang, "nl"))
            || (!strcmp(lang_part->lang, "fr"))
            || (!strcmp(lang_part->lang, "bg"))
            || (!strcmp(lang_part->lang, "sv"))
            || (!strcmp(lang_part->lang, "zh"))
            || (!strcmp(lang_part->lang, "tr"))
            || (!strcmp(lang_part->lang, "hr"))
            || (!strcmp(lang_part->lang, "ca")))
          {
             snprintf(buf_lang, sizeof(buf_lang), "&lang=%s", lang_part->lang);
             w->translated = EINA_TRUE;
          }
        else
          *buf_lang = 0;
     }
   else
     *buf_lang = 0;

   if (w->geo_id)
     snprintf(buf, sizeof(buf),
              WEATHER_FORECAST_URL WEATHER_ID
              "%s"
              WEATHER_PARAMS WEATHER_API_KEY,
              w->geo_id, buf_lang);
   else if (w->country && w->city)
     snprintf(buf, sizeof(buf),
              WEATHER_FORECAST_URL WEATHER_CITY_COUNTRY
              "%s"
              WEATHER_PARAMS WEATHER_API_KEY,
              w->city, w->country, buf_lang);
   else if (w->city)
     snprintf(buf, sizeof(buf),
              WEATHER_FORECAST_URL WEATHER_CITY
              "%s"
              WEATHER_PARAMS WEATHER_API_KEY,
              w->city, buf_lang);
   else
     *buf = 0;

   if (lang_part) e_intl_locale_parts_free(lang_part);

   return eina_stringshare_add(buf);
}


EINTERN Weather_Plugin *
plugin_openweathermap_create(void)
{
   Weather_Plugin *wp;

   wp = E_NEW(Weather_Plugin, 1);
   wp->name = eina_stringshare_add("Openweathermap");
   wp->type = WEATHER_PLUGIN_WEATHER_HOUR;
   wp->func.uri_get = _plugin_openweathermap_uri_get;
   wp->func.parse = _plugin_openweathermap_parse;

   return wp;
}
