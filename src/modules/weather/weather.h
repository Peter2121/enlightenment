#ifndef WEATHER_H
#define WEATHER_H

#include "e.h"

E_API extern E_Module_Api e_modapi;

E_API void *e_modapi_init     (E_Module *m);
E_API int   e_modapi_shutdown (E_Module *m);
E_API int   e_modapi_save     (E_Module *m);

typedef struct _Config Config;
typedef struct _Config_Item Config_Item;
typedef struct _Instance Instance;

typedef enum _Weather_State
{
   WEATHER_STATE_CLEAR_SKY,
   WEATHER_STATE_FEW_CLOUDS,
   WEATHER_STATE_SCATTERED_CLOUDS,
   WEATHER_STATE_BROKEN_CLOUDS,
   WEATHER_STATE_OVERCAST_CLOUDS,
   WEATHER_STATE_THUNDERSTORM_LIGHT,
   WEATHER_STATE_THUNDERSTORM,
   WEATHER_STATE_THUNDERSTORM_RAIN_LIGHT,
   WEATHER_STATE_THUNDERSTORM_RAIN,
   WEATHER_STATE_THUNDERSTORM_RAIN_HEAVY,
   WEATHER_STATE_THUNDERSTORM_HEAVY,
   WEATHER_STATE_THUNDERSTORM_RAGGED,
   WEATHER_STATE_THUNDERSTORM_DRIZZLE_LIGHT,
   WEATHER_STATE_THUNDERSTORM_DRIZZLE,
   WEATHER_STATE_THUNDERSTORM_DRIZZLE_HEAVY,
   WEATHER_STATE_DRIZZLE_LIGHT,
   WEATHER_STATE_DRIZZLE,
   WEATHER_STATE_DRIZZLE_HEAVY,
   WEATHER_STATE_DRIZZLE_RAIN_LIGHT,
   WEATHER_STATE_DRIZZLE_RAIN,
   WEATHER_STATE_DRIZZLE_RAIN_HEAVY,
   WEATHER_STATE_DRIZZLE_RAIN_SHOWER,
   WEATHER_STATE_DRIZZLE_RAIN_SHOWER_HEAVY,
   WEATHER_STATE_DRIZZLE_SHOWER,
   WEATHER_STATE_RAIN_LIGHT,
   WEATHER_STATE_RAIN,
   WEATHER_STATE_RAIN_HEAVY,
   WEATHER_STATE_RAIN_VERY_HEAVY,
   WEATHER_STATE_RAIN_EXTREME,
   WEATHER_STATE_RAIN_FREEZING,
   WEATHER_STATE_RAIN_SHOWER_LIGHT,
   WEATHER_STATE_RAIN_SHOWER,
   WEATHER_STATE_RAIN_SHOWER_HEAVY,
   WEATHER_STATE_RAIN_RAGGED,
   WEATHER_STATE_SNOW_LIGHT,
   WEATHER_STATE_SNOW,
   WEATHER_STATE_SNOW_HEAVY,
   WEATHER_STATE_SLEET,
   WEATHER_STATE_SLEET_SHOWER,
   WEATHER_STATE_SNOW_RAIN_LIGHT,
   WEATHER_STATE_SNOW_RAIN,
   WEATHER_STATE_SNOW_SHOWER_LIGHT,
   WEATHER_STATE_SNOW_SHOWER,
   WEATHER_STATE_SNOW_SHOWER_HEAVY,
   WEATHER_STATE_MIST,
   WEATHER_STATE_SMOKE,
   WEATHER_STATE_HAZE,
   WEATHER_STATE_DUST_WHIRLS,
   WEATHER_STATE_FOG,
   WEATHER_STATE_SAND,
   WEATHER_STATE_DUST,
   WEATHER_STATE_VOLCANIC_ASH,
   WEATHER_STATE_SQUALLS,
   WEATHER_STATE_TORNADO,
   WEATHER_STATE_TROPICAL_STORM,
   WEATHER_STATE_HURRICANE,
   WEATHER_STATE_COLD,
   WEATHER_STATE_HOT,
   WEATHER_STATE_WINDY,
   WEATHER_STATE_HAIL,
} Weather_State;

struct _Config
{
  Eina_List *items;

  E_Module *module;
  Evas_Object *config_dialog;
};

struct _Config_Item
{
  int id;
  const char *geo_id;
  const char *city;
  const char *country;
  unsigned char use_celcius;
  unsigned char use_metric;
  unsigned char use_24h;
};


typedef struct Weather_
{
   const char *city;
   const char *country;
   const char *geo_id;
   int altitude;
   double latitude;
   double longitude;
   struct tm sunrise;
   struct tm sunset;
   Eina_List *datas;
//   Config_Item *cfg;
   Eina_Bool translated;
   Eina_Bool plugin_error;
} Weather;

typedef struct Weather_Data_
{
   struct tm time_start;
   struct tm time_end;
   Weather_State state;
   const char *state_name;
   struct
     {
        double value;
        double value_min;
        double value_max;
        const char *unit;
     } temperature;
   struct
     {
        const char *type;
        double value;
     } precipitation;
   struct
     {
        const char *name;
        const char *direction_name;
        const char *direction;
        double direction_value;
        double speed;
     } wind;
   struct
     {
        const char *name;
        int value;
        const char *unit;
     } cloud;
   struct
     {
        int value;
        const char *unit;
     } humidity;
   struct
     {
        double value;
        const char *unit;
     } pressure;
   Eina_Bool updated;
} Weather_Data;

struct _Instance
{
   Evas_Object     *obj;
   Evas_Object  *popup;
   Evas_Object  *popup_segment;
   Evas_Object  *popup_weather;
   Config_Item     *cfg;
   Weather *weather;
};


EINTERN Evas_Object *config_weather(Config_Item *, E_Zone*);

EINTERN void weather_init(E_Module *m);
EINTERN void weather_shutdown(void);

EINTERN Evas_Object *weather_small_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient);
EINTERN Evas_Object *weather_big_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient);
EINTERN Evas_Object *weather_small_wizard(E_Gadget_Wizard_End_Cb cb, void *data, Evas_Object *site);
EINTERN Evas_Object *weather_big_wizard(E_Gadget_Wizard_End_Cb cb, void *data, Evas_Object *site);
EINTERN void weather_gadget_update(Instance *inst);

EINTERN void weather_free(Weather *weather);
EINTERN void weather_data_free(Weather_Data *wd);

extern Config *weather_config;
extern Eina_List *weather_instances;
extern Eina_List *weather_list;

#endif
