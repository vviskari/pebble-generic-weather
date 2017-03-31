#include <pebble.h>
#include <pebble-events/pebble-events.h>

#include "include/pebble-generic-weather.h"

static GenericWeatherInfo *s_info;
static GenericWeatherForecast forecast[GENERIC_WEATHER_FORECAST_SIZE];
static int forecastSize=0;
static GenericWeatherCallback *s_callback;
static GenericWeatherStatus s_status;

static char s_api_key[33];
static GenericWeatherProvider s_provider;
static GenericWeatherCoordinates s_coordinates;
static bool s_feels_like;
static bool s_forecast;

static EventHandle s_event_handle;
static EventHandle s_drop_handle;

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "INBOX DROPPED %d", reason);
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *reply_tuple = dict_find(iter, MESSAGE_KEY_GW_REPLY);
  if(reply_tuple) {
    Tuple *desc_tuple = dict_find(iter, MESSAGE_KEY_GW_DESCRIPTION);
    strncpy(s_info->description, desc_tuple->value->cstring, GENERIC_WEATHER_BUFFER_SIZE);

    Tuple *name_tuple = dict_find(iter, MESSAGE_KEY_GW_NAME);
    strncpy(s_info->name, name_tuple->value->cstring, GENERIC_WEATHER_BUFFER_SIZE);

    Tuple *temp_tuple = dict_find(iter, MESSAGE_KEY_GW_TEMPK);
    s_info->temp_k = (int16_t)temp_tuple->value->int32;
    s_info->temp_c = s_info->temp_k - 273;
    s_info->temp_f = ((s_info->temp_c * 9) / 5) + 32;
    s_info->timestamp = time(NULL);

    Tuple *day_tuple = dict_find(iter, MESSAGE_KEY_GW_DAY);
    s_info->day = day_tuple->value->int16 == 1;

    Tuple *condition_tuple = dict_find(iter, MESSAGE_KEY_GW_CONDITIONCODE);
    s_info->condition = condition_tuple->value->int32;

    Tuple *sunrise_tuple = dict_find(iter, MESSAGE_KEY_GW_SUNRISE);
    s_info->timesunrise = sunrise_tuple->value->int32;

    Tuple *sunset_tuple = dict_find(iter, MESSAGE_KEY_GW_SUNSET);
    s_info->timesunset = sunset_tuple->value->int32;

    // Read forecast
    if (s_forecast) {
      for (int i = 0; i<GENERIC_WEATHER_FORECAST_SIZE; i++) {
        Tuple *f_cond_tuple = dict_find(iter, MESSAGE_KEY_GW_FORECAST_CONDITIONCODE + i);
        if (f_cond_tuple) {
          Tuple *f_time_tuple = dict_find(iter, MESSAGE_KEY_GW_FORECAST_TIME + i);
          Tuple *f_temp_tuple = dict_find(iter, MESSAGE_KEY_GW_FORECAST_TEMPK + i);
          int16_t temp_k = f_temp_tuple->value->int32;
          forecast[i] = (GenericWeatherForecast) {
            .condition = f_cond_tuple->value->int32,
            .timestamp = f_time_tuple->value->int32,
            .temp_k = temp_k,
            .temp_c = temp_k - 273,
            .temp_f = ((temp_k-273) * 9 / 5) + 32
          };
          forecastSize = i;
        } else {
          forecast[i] = (GenericWeatherForecast) {
            .condition = 0,
            .timestamp = 0,
            .temp_k = 0,
            .temp_c = 0,
            .temp_f = 0
          };
        }
      }
    }

    s_status = GenericWeatherStatusAvailable;
    s_callback(s_info, forecast, forecastSize, s_status);
  }

  Tuple *err_tuple = dict_find(iter, MESSAGE_KEY_GW_BADKEY);
  if(err_tuple) {
    s_status = GenericWeatherStatusBadKey;
    s_callback(s_info, forecast, 0, s_status);
  }

  err_tuple = dict_find(iter, MESSAGE_KEY_GW_LOCATIONUNAVAILABLE);
  if(err_tuple) {
    s_status = GenericWeatherStatusLocationUnavailable;
    s_callback(s_info, forecast, 0, s_status);
  }
}

static void fail_and_callback() {
  s_status = GenericWeatherStatusFailed;
  s_callback(s_info, forecast, 0, s_status);
}

static bool fetch() {
  DictionaryIterator *out;
  AppMessageResult result = app_message_outbox_begin(&out);
  if(result != APP_MSG_OK) {
    fail_and_callback();
    return false;
  }

  dict_write_uint8(out, MESSAGE_KEY_GW_REQUEST, 1);

  if(strlen(s_api_key) > 0)
    dict_write_cstring(out, MESSAGE_KEY_GW_APIKEY, s_api_key);

  if(s_provider != GenericWeatherProviderUnknown)
    dict_write_int32(out, MESSAGE_KEY_GW_PROVIDER, s_provider);

  if(s_coordinates.latitude != (int32_t)0xFFFFFFFF && s_coordinates.longitude != (int32_t)0xFFFFFFFF) {
    dict_write_int32(out, MESSAGE_KEY_GW_LATITUDE, s_coordinates.latitude);
    dict_write_int32(out, MESSAGE_KEY_GW_LONGITUDE, s_coordinates.longitude);
  }

  if(s_feels_like){
    dict_write_int8(out, MESSAGE_KEY_GW_FEELS_LIKE, s_feels_like);
  }

  if(s_forecast){
    dict_write_int8(out, MESSAGE_KEY_GW_FORECAST, s_forecast);
  }

  result = app_message_outbox_send();
  if(result != APP_MSG_OK) {
    fail_and_callback();
    return false;
  }

  s_status = GenericWeatherStatusPending;
  s_callback(s_info, forecast, 0, s_status);
  return true;
}

void generic_weather_init() {
  if(s_info) {
    free(s_info);
  }

  s_info = (GenericWeatherInfo*)malloc(sizeof(GenericWeatherInfo));
  s_api_key[0] = 0;
  s_provider = GenericWeatherProviderUnknown;
  s_coordinates = GENERIC_WEATHER_GPS_LOCATION;
  s_status = GenericWeatherStatusNotYetFetched;
  s_forecast = false;
  events_app_message_request_inbox_size(1024);
  events_app_message_request_outbox_size(128);
  s_event_handle = events_app_message_register_inbox_received(inbox_received_handler, NULL);
  s_drop_handle = events_app_message_register_inbox_dropped(inbox_dropped_handler, NULL);
}

void generic_weather_set_api_key(const char *api_key){
  if(!api_key) {
    s_api_key[0] = 0;
  }
  else {
    strncpy(s_api_key, api_key, sizeof(s_api_key));
  }
}

void generic_weather_set_provider(GenericWeatherProvider provider){
  s_provider = provider;
}

void generic_weather_set_location(const GenericWeatherCoordinates coordinates){
  s_coordinates = coordinates;
}

void generic_weather_set_feels_like(const bool feels_like) {
  s_feels_like = feels_like;
}

void generic_weather_set_forecast(const bool forecast){
  s_forecast = forecast;
}

bool generic_weather_fetch(GenericWeatherCallback *callback) {
  if(!s_info) {
    return false;
  }

  if(!callback) {
    return false;
  }

  s_callback = callback;

  if(!bluetooth_connection_service_peek()) {
    s_status = GenericWeatherStatusBluetoothDisconnected;
    s_callback(s_info, forecast, 0, s_status);
    return false;
  }

  return fetch();
}

void generic_weather_deinit() {
  if(s_info) {
    free(s_info);
    s_info = NULL;
    s_callback = NULL;
    events_app_message_unsubscribe(s_event_handle);
    events_app_message_unsubscribe(s_drop_handle);    
  }
}

GenericWeatherPeekData generic_weather_peek() {
  if(!s_info) {
    return (GenericWeatherPeekData) {.info = NULL, .forecastSize = 0, .forecast = NULL};
  }
  return (GenericWeatherPeekData) {
    .info = s_info,
    .forecastSize = forecastSize,
    .forecast = forecast
  };
}

void generic_weather_save(const uint32_t key){
  if(!s_info) {
    return;
  }

  persist_write_data(key, s_info, sizeof(GenericWeatherInfo));
}

void generic_weather_load(const uint32_t key){
  if(!s_info) {
    return;
  }

  if(persist_exists(key)){
    persist_read_data(key, s_info, sizeof(GenericWeatherInfo));
  }
}

void generic_weather_save_forecast(const uint32_t key){
  if(!s_forecast) {
    return;
  }
  for (int i = 0; i < GENERIC_WEATHER_FORECAST_SIZE; i++) {
    persist_write_data(key+i, forecast+i, sizeof(GenericWeatherForecast));
  }
  persist_write_int(key+GENERIC_WEATHER_FORECAST_SIZE, forecastSize);
}

void generic_weather_load_forecast(const uint32_t key) {
  if(!s_forecast) {
    return;
  }

  if(persist_exists(key)) {
    for (int i = 0; i < GENERIC_WEATHER_FORECAST_SIZE; i++) {
      persist_read_data(key+i, forecast+i, sizeof(GenericWeatherForecast));
    }
    forecastSize = persist_read_int(key+GENERIC_WEATHER_FORECAST_SIZE);
  }
}
