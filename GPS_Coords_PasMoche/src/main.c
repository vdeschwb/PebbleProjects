#include "pebble_os.h"
#include <math.h>
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "http.h"
#include "itoa.h"

//Add configuration modes here
//The numbers have to be contiguous and END has to be at the end
enum conf_mode {
    AUTOUPDATE = 0,
    REPRESENTATION = 1,
    ABOUT = 2,
    END = 3
};

//Prototype for a config_handler
//Config handlers get called whenever you change a certain configuration
typedef void (*config_handler) (void);

//Prototype of a configuration struct
//title   : The string displayed as the configuration title
//value   : The current value of the config.
//handler : This function gets called whenever the config. is changed. Can be NULL or a proper config_handler
//strings : An array of strings, which are used as display names for the menu items 
typedef struct {
    char* title;
    int value;
    config_handler handler;
    char* strings[];
} configuration;

//Prototypes for several functions:
void set_autoupdate_handler (void);

//Configurations:
static configuration auto_update = {"Auto-Update:", 5, (config_handler) set_autoupdate_handler, {"Off", "1 hour", "30 min", "10 min", "1 min", "30 sec", "10 sec", "5 sec", "1 sec", ":END:"}};
static configuration representation = {"Representation:", 0, NULL, {"ddd mm ss", "ddd.dddd", ":END:"}};
static configuration about = {"About:", 0, NULL, {"by SirTate", "2013", ":END:"}};
//Global configuration
//Add configurations here
static configuration* global_configuration[] = {&auto_update, &representation, &about};
//Set default config. mode:
static enum conf_mode configuration_mode = AUTOUPDATE;

//Set default AUTOUPDATE_TIME:
static uint32_t AUTOUPDATE_TIME = 60;


static AppTimerHandle current_timer;
static AppContextRef current_app_ctx;

static float our_lat;
static float our_lon;
static float our_alt;

#define MY_UUID { 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0x04, 0x9F, 0x49, 0xC0, 0x99, 0xAD }

PBL_APP_INFO(HTTP_UUID,
    "GPS Coords. PasMoche", "Benjamin von Deschwanden",
    2, 1, /* App version */
    RESOURCE_ID_MENU_ICON,
    APP_INFO_STANDARD_APP);

Window mainWindow, settingsWindow;
TextLayer latLayer, lonLayer, altLayer, setTitleLayer, setValueLayer;


void update_screen(void) {
    static char degsign[] = "\u00B0";
    static char lat[30] = "Latitude:\n";
    static char lon[30] = "Longitude:\n";
    static char alt[30] = "Altitude:\n";
    int currpos=0;


    if ((*global_configuration[REPRESENTATION]).value == 0) {
        int lat_deg = our_lat;
        int lat_min = (fabs(our_lat) - fabs(lat_deg)) * 60;
        int lat_sec = ((fabs(our_lat) - fabs(lat_deg)) * 60 - lat_min) * 60;

        currpos = 10;
        memcpy(&lat[currpos], itoa(lat_deg), strlen(itoa(lat_deg)));
        currpos += strlen(itoa(lat_deg));
        memcpy(&lat[currpos], degsign, strlen(degsign));
        currpos += strlen(degsign);
        memcpy(&lat[currpos], itoa(lat_min), strlen(itoa(lat_min)));
        currpos += strlen(itoa(lat_min));
        memcpy(&lat[currpos], "'", 1);
        currpos += 1;
        memcpy(&lat[currpos], itoa(lat_sec), strlen(itoa(lat_sec)));
        currpos += strlen(itoa(lat_sec));
        memcpy(&lat[currpos], "''\0", strlen("''\0"));



        int lon_deg = our_lon;
        int lon_min = (fabs(our_lon) - fabs(lon_deg)) * 60;
        int lon_sec = ((fabs(our_lon) - fabs(lon_deg)) * 60 - lon_min) * 60;

        currpos = 11;
        memcpy(&lon[currpos], itoa(lon_deg), strlen(itoa(lon_deg)));
        currpos += strlen(itoa(lon_deg));
        memcpy(&lon[currpos], degsign, strlen(degsign));
        currpos += strlen(degsign);
        memcpy(&lon[currpos], itoa(lon_min), strlen(itoa(lon_min)));
        currpos += strlen(itoa(lon_min));
        memcpy(&lon[currpos], "'", 1);
        currpos += 1;
        memcpy(&lon[currpos], itoa(lon_sec), strlen(itoa(lon_sec)));
        currpos += strlen(itoa(lon_sec));
        memcpy(&lon[currpos], "''\0", strlen("''\0"));
    } else {
        currpos = 10;
        snprintf(&lat[currpos], 30, "%.4f", our_lat);

        currpos = 11;
        snprintf(&lon[currpos], 30, "%.4f", our_lon);
    }

    text_layer_set_text(&latLayer, lat);
    text_layer_set_text(&lonLayer, lon);


    currpos = 10;
    memcpy(&alt[currpos], itoa((int) our_alt), strlen(itoa((int) our_alt)));
    currpos += strlen(itoa((int) our_alt));
    memcpy(&alt[currpos], " m\0", strlen(" m\0"));
    text_layer_set_text(&altLayer, alt);
}

void show_config(void) {
    text_layer_set_text(&setTitleLayer, (*global_configuration[configuration_mode]).title);
    text_layer_set_text(&setValueLayer, (*global_configuration[configuration_mode]).strings[(*global_configuration[configuration_mode]).value]);
}

void set_timer() {
    current_timer = app_timer_send_event(current_app_ctx, AUTOUPDATE_TIME * 1000, 42);
}

void set_autoupdate_handler (void) {
    static uint32_t autoupdate_time_values[] = {0, 3600, 1800, 600, 60, 30, 10, 5, 1};
    AUTOUPDATE_TIME = autoupdate_time_values[(*global_configuration[AUTOUPDATE]).value];

    app_timer_cancel_event(current_app_ctx, current_timer);
    if (AUTOUPDATE_TIME != 0) {
        set_timer();
    }
}

void apply_config(void) {
    if ((*global_configuration[configuration_mode]).handler != NULL) {
        (*global_configuration[configuration_mode]).handler();
    }
    show_config();
}

void location(float latitude, float longitude, float altitude, float accuracy, void* context) {
    our_lat = latitude;
    our_lon = longitude;
    our_alt = altitude;

    update_screen();
}

void select_long_click_handler(ClickRecognizerRef recognizer, Window *window) {
    if (window_stack_get_top_window() == &mainWindow) {
        window_stack_push(&settingsWindow, true);
        show_config();
        return;
    }
    
    configuration_mode++;
    if (configuration_mode == END) {configuration_mode = 0;}
    show_config();
}


void up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
    (*global_configuration[configuration_mode]).value -= 1;
    if ((*global_configuration[configuration_mode]).value < 0) {(*global_configuration[configuration_mode]).value = 0;}
    apply_config();
}
void select_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
    text_layer_set_text(&latLayer, "");
    text_layer_set_text(&altLayer, "");
    text_layer_set_text(&lonLayer, " UPDATING...");
    http_location_request();
}


void down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
    (*global_configuration[configuration_mode]).value += 1;
    if (strcmp((*global_configuration[configuration_mode]).strings[(*global_configuration[configuration_mode]).value], ":END:") == 0) {(*global_configuration[configuration_mode]).value -= 1;}
    apply_config();
}


void config_provider(ClickConfig **config, Window *window) {
    // single click / repeat-on-hold config:
    config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) select_single_click_handler;
    config[BUTTON_ID_UP]->click.handler = (ClickHandler) up_single_click_handler;
    config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) down_single_click_handler;
    
    // long click config:
    config[BUTTON_ID_SELECT]->long_click.handler = (ClickHandler) select_long_click_handler;
    config[BUTTON_ID_UP]->click.repeat_interval_ms = 500;
    config[BUTTON_ID_DOWN]->click.repeat_interval_ms = 500;
    config[BUTTON_ID_SELECT]->long_click.delay_ms = 700;
}

void handle_timer(AppContextRef app_ctx, AppTimerHandle handle, uint32_t cookie) {
    text_layer_set_text(&lonLayer, " UPDATING...");
    http_location_request();
    if(cookie) {
        set_timer();
    }
}


void handle_init(AppContextRef ctx) {

    //Main Window init :: BEGIN
    window_init(&mainWindow, "Status");
    window_stack_push(&mainWindow, true /* Animated */);
    
    text_layer_init(&latLayer, GRect(15, 5, 130, 55));
    layer_add_child(&mainWindow.layer, &latLayer.layer);
    text_layer_set_font(&latLayer,fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));

    text_layer_init(&lonLayer, GRect(15, 55, 130, 105));
    layer_add_child(&mainWindow.layer, &lonLayer.layer);
    text_layer_set_font(&lonLayer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
    
    text_layer_init(&altLayer, GRect(15, 105, 130, 155));
    layer_add_child(&mainWindow.layer, &altLayer.layer);
    text_layer_set_font(&altLayer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));

    window_set_click_config_provider(&mainWindow, (ClickConfigProvider) config_provider);
    //Main Window init :: END


    //Settings Window init :: BEGIN
    window_init(&settingsWindow, "Settings");
    
    text_layer_init(&setTitleLayer, GRect(5, 25, 130, 55));
    layer_add_child(&settingsWindow.layer, &setTitleLayer.layer);
    text_layer_set_font(&setTitleLayer,fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));

    text_layer_init(&setValueLayer, GRect(15, 60, 130, 85));
    layer_add_child(&settingsWindow.layer, &setValueLayer.layer);
    text_layer_set_font(&setValueLayer,fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));

    window_set_click_config_provider(&settingsWindow, (ClickConfigProvider) config_provider);
    //Settings Window init :: END

    http_set_app_id(39152173);

    current_app_ctx = ctx;

    http_register_callbacks((HTTPCallbacks){
        .location=location
    }, (void*)ctx);

    http_location_request();
    set_timer();
}

void pbl_main(void *params) {
    PebbleAppHandlers handlers = {
        .init_handler = &handle_init,
        .timer_handler = handle_timer,
        .messaging_info = {
            .buffer_sizes = {
                .inbound = 256,
                .outbound = 256,
            }
        },
    };
    app_event_loop(params, &handlers);
}
