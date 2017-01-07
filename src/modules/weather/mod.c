#include <e.h>
#include "weather.h"
#include "plugin.h"

static E_Config_DD *conf_edd = NULL;
static E_Config_DD *conf_item_edd = NULL;

EINTERN void
weather_init(E_Module *m)
{
   conf_item_edd = E_CONFIG_DD_NEW("Config_Item", Config_Item);
#undef T
#undef D
#define T Config_Item
#define D conf_item_edd
   E_CONFIG_VAL(D, T, id, INT);
   E_CONFIG_VAL(D, T, city, STR);
   E_CONFIG_VAL(D, T, country, STR);
   E_CONFIG_VAL(D, T, geo_id, STR);
   E_CONFIG_VAL(D, T, use_celcius, UCHAR);
   E_CONFIG_VAL(D, T, use_metric, UCHAR);
   E_CONFIG_VAL(D, T, use_24h, UCHAR);

   conf_edd = E_CONFIG_DD_NEW("Config", Config);
#undef T
#undef D
#define T Config
#define D conf_edd
   E_CONFIG_LIST(D, T, items, conf_item_edd);

   weather_config = e_config_domain_load("module.weather", conf_edd);

   if (!weather_config)
     weather_config = E_NEW(Config, 1);
   weather_plugin_init();

   weather_config->module = m;

   e_gadget_type_add("Weather small", weather_small_create, weather_small_wizard);
   e_gadget_type_add("Weather big", weather_big_create, weather_big_wizard);

}

EINTERN void
weather_shutdown(void)
{
   weather_plugin_shutdown();
   if (weather_config)
     {
        Config_Item *ci;
#if 0
        if (weather_config->config_dialog)
          {
             evas_object_hide(time_config->config_dialog);
             evas_object_del(weather_config->config_dialog);
          }
#endif

        EINA_LIST_FREE(weather_config->items, ci)
          {
             eina_stringshare_del(ci->city);
             eina_stringshare_del(ci->country);
             eina_stringshare_del(ci->geo_id);
             free(ci);
          }

        E_FREE(weather_config);
     }
   E_CONFIG_DD_FREE(conf_edd);
   E_CONFIG_DD_FREE(conf_item_edd);

   e_gadget_type_del("Weather small");
   e_gadget_type_del("Weather big");
}

/* module setup */
E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Weather"
};

E_API void *
e_modapi_init(E_Module *m)
{
   if (!E_EFL_VERSION_MINIMUM(1, 17, 99)) return NULL;
   weather_init(m);
   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   weather_shutdown();
   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.weather", conf_edd, weather_config);
   return 1;
}

