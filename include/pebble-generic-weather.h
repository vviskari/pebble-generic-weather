#pragma once

#include <pebble.h>

#define GENERIC_WEATHER_BUFFER_SIZE 32
#define GENERIC_WEATHER_FORECAST_SIZE 24

//! Possible statuses of the weather library
typedef enum {
  //! Weather library has not yet initiated a fetch
  GenericWeatherStatusNotYetFetched = 0,
  //! Bluetooth is disconnected
  GenericWeatherStatusBluetoothDisconnected,
  //! Weather data fetch is in progress
  GenericWeatherStatusPending,
  //! Weather fetch failed
  GenericWeatherStatusFailed,
  //! Weather fetched and available
  GenericWeatherStatusAvailable,
  //! API key was bad
  GenericWeatherStatusBadKey,
  //! Location not available
  GenericWeatherStatusLocationUnavailable
} GenericWeatherStatus;

//! Possible weather conditions
typedef enum {
  GenericWeatherConditionClearSky = 0,
  GenericWeatherConditionFewClouds,
  GenericWeatherConditionScatteredClouds,
  GenericWeatherConditionBrokenClouds,
  GenericWeatherConditionShowerRain,
  GenericWeatherConditionRain,
  GenericWeatherConditionThunderstorm,
  GenericWeatherConditionSnow,
  GenericWeatherConditionMist,
  GenericWeatherConditionWind,
  GenericWeatherConditionUnknown = 1000
} GenericWeatherConditionCode;

typedef struct {
  time_t timestamp;
  int16_t temp_k;
  int16_t temp_c;
  int16_t temp_f;
  GenericWeatherConditionCode condition;
} GenericWeatherForecast;

//! Struct containing weather data
typedef struct {
  //! Weather conditions string e.g: "Sky is clear"
  char description[GENERIC_WEATHER_BUFFER_SIZE];
  //! Name of the location from the weather feed
  char name[GENERIC_WEATHER_BUFFER_SIZE];
  //! Temperature in degrees Kelvin
  int16_t temp_k;
  int16_t temp_c;
  int16_t temp_f;
  //! day or night ?
  bool day;
  //! Condition code (see GenericWeatherConditionCode values)
  GenericWeatherConditionCode condition;
  //! Date that the data was received
  time_t timestamp;
  //! Sunrise time UTC
  time_t timesunrise;
  //! Sunset time UTC
  time_t timesunset;
  //! Latitude of the current location x 100000 (eg : 42.123456 -> 4212345)
  int32_t latitude; 
  //! Longitude of the current location x 100000 (eg : -12.354789 -> -1235478)
  int32_t longitude;
} GenericWeatherInfo;

typedef struct {
  GenericWeatherInfo *info;
  int forecastSize;
  GenericWeatherForecast *forecast;
} GenericWeatherPeekData;

//! Possible weather providers
typedef enum {
  //! OpenWeatherMap
  GenericWeatherProviderOpenWeatherMap      = 0,
  //! WeatherUnderground
  GenericWeatherProviderWeatherUnderground  = 1,
  //! Forecast.io
  GenericWeatherProviderForecastIo          = 2,

  //! Yahoo! Weather
  GenericWeatherProviderYahooWeather        = 3,
  
  GenericWeatherProviderUnknown             = 1000,
} GenericWeatherProvider;

//! Struct containing coordinates
typedef struct {
  //! Latitude of the coordinates x 100000 (eg : 42.123456 -> 4212345)
  int32_t latitude; 
  //! Longitude of the coordinates x 100000 (eg : -12.354789 -> -1235478)
  int32_t longitude;
} GenericWeatherCoordinates;

#define GENERIC_WEATHER_GPS_LOCATION (GenericWeatherCoordinates){.latitude=0xFFFFFFFF,.longitude=0xFFFFFFFF}

//! Callback for a weather fetch
//! @param info The struct containing the weather data
//! @param status The current GenericWeatherStatus, which may have changed.
typedef void(GenericWeatherCallback)(GenericWeatherInfo *info, GenericWeatherForecast *forecast, int forecastSize, GenericWeatherStatus status);

//! Initialize the weather library. The data is fetched after calling this, and should be accessed
//! and stored once the callback returns data, if it is successful.
void generic_weather_init();

//! Initialize the weather API key
//! @param api_key The API key for your weather provider.
void generic_weather_set_api_key(const char *api_key);

//! Initialize the weather provider
//! @param provider The selected weather provider (default is OWM)
void generic_weather_set_provider(GenericWeatherProvider provider);

//! Initialize the weather location if you don't want to use the GPS
//! @param coordinates The coordinates (default is GENERIC_WEATHER_GPS_LOCATION)
void generic_weather_set_location(const GenericWeatherCoordinates coordinates);

//! Use "feels like" temperature or not
//! @param feels_like The "feels like" setting (default is false)
void generic_weather_set_feels_like(const bool feels_like);

//! Fetch weather forecast data or not. Not all providers support this.
//! The resulting forecast might be an hourly or daily forecast data, depending on provider.
//! @param forecast (default is false)
void generic_weather_set_forecast(const bool forecast);

//! Important: This uses the AppMessage system. You should only use AppMessage yourself
//! either before calling this, or after you have obtained your weather data.
//! @param callback Callback to be called once the weather.
//! @return true if the fetch message to PebbleKit JS was successful, false otherwise.
bool generic_weather_fetch(GenericWeatherCallback *callback);

//! Deinitialize and free the backing GenericWeatherInfo.
void generic_weather_deinit();

//! Peek at the current state of the weather library. You should check the GenericWeatherStatus of the
//! returned GenericWeatherInfo before accessing data members.
//! @return GenericWeatherInfo object, internally allocated.
//! If NULL, generic_weather_init() has not been called.
GenericWeatherPeekData generic_weather_peek();

//! Save the current state of the weather library
//! @param key The key to write to.
void generic_weather_save(const uint32_t key);

//! Load the state of the weather library from persistent storage
//! @param key The key to read from.
void generic_weather_load(const uint32_t key);

void generic_weather_save_forecast(const uint32_t key);

void generic_weather_load_forecast(const uint32_t key);
