#ifndef WEATHER_PLUGIN_H
#define WEATHER_PLUGIN_H

typedef const char *(*Weather_Plugin_Uri) (Weather *wd);
typedef Eina_Bool (*Weather_Plugin_Parse) (Weather *wd, const unsigned char *parse, size_t size);

typedef enum _Weather_Plugin_Type
{
   WEATHER_PLUGIN_GEOLOC,
   WEATHER_PLUGIN_WEATHER_DAY,
   WEATHER_PLUGIN_WEATHER_HOUR,
} Weather_Plugin_Type;

typedef struct _Weather_Plugin
{
   const char *name;
   Weather_Plugin_Type type;
   Eina_Binbuf *buf;
   struct {
        Weather_Plugin_Uri uri_get;
        Weather_Plugin_Parse parse;
   } func;
} Weather_Plugin;

EINTERN void weather_plugin_init(void);
EINTERN void weather_plugin_shutdown(void);
EINTERN void weather_plugin_weather_add(Instance *inst);
EINTERN void weather_plugin_request(Weather *w);
EINTERN void weather_plugin_weather_remove(Instance *inst);



#endif /* WEATHER_PLUGIN_H */
