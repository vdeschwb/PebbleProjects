#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "http.h"
#include "itoa.h"



#define MY_UUID { 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0x04, 0x9F, 0x49, 0xC0, 0x99, 0xAD }
PBL_APP_INFO(HTTP_UUID,
             "GPS Coords. PasMoche", "Benjamin von Deschwanden",
             1, 0, /* App version */
             RESOURCE_ID_MENU_ICON,
             APP_INFO_STANDARD_APP);

Window window;
TextLayer latLayer;
TextLayer lonLayer;
TextLayer altLayer;



static bool located;


void location(float latitude, float longitude, float altitude, float alturacy, void* context) {
	static char degsign[] = "\u00B0";
	static char lat[30] = "Latitude:\n";
    	static char lon[30] = "Longitude:\n";
	static char alt[30] = "Altitude:\n";
    	int currpos=0;

	located = true;

	int lat_deg = latitude;
    	int lat_min = (latitude-lat_deg) * 60;
    	int lat_sec = ((latitude-lat_deg) * 60 - lat_min) * 60;

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

	text_layer_set_text(&latLayer, lat);


   	int lon_deg = longitude;
    	int lon_min = (longitude-lon_deg) * 60;
    	int lon_sec = ((longitude-lon_deg) * 60 - lon_min) * 60;


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

	text_layer_set_text(&lonLayer, lon);


	currpos = 10;
	memcpy(&alt[currpos], itoa((int) altitude), strlen(itoa((int) altitude)));
	currpos += strlen(itoa((int) altitude));
	memcpy(&alt[currpos], " m\0", strlen(" m\0"));
	text_layer_set_text(&altLayer, alt);
}


void select_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
text_layer_set_text(&latLayer, "");
text_layer_set_text(&altLayer, "");
text_layer_set_text(&lonLayer, "   UPDATING...");
http_location_request();
}


void config_provider(ClickConfig **config, Window *window) {
 // single click / repeat-on-hold config:
  config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) select_single_click_handler;
}


void handle_init(AppContextRef ctx) {
	window_init(&window, "GPS Coordinates");
	window_stack_push(&window, true /* Animated */);
	
	text_layer_init(&latLayer, GRect(15, 5, 130, 55));
	layer_add_child(&window.layer, &latLayer.layer);
	text_layer_set_font(&latLayer,fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));

	text_layer_init(&lonLayer, GRect(15, 55, 130, 105));
	layer_add_child(&window.layer, &lonLayer.layer);
	text_layer_set_font(&lonLayer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
	
	text_layer_init(&altLayer, GRect(15, 105, 130, 155));
	layer_add_child(&window.layer, &altLayer.layer);
	text_layer_set_font(&altLayer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));


window_set_click_config_provider(&window, (ClickConfigProvider) config_provider);

	http_set_app_id(39152173);

	http_register_callbacks((HTTPCallbacks){
		.location=location
	}, (void*)ctx);

	http_location_request();
}

void pbl_main(void *params) {
	PebbleAppHandlers handlers = {
		.init_handler = &handle_init,
		.messaging_info = {
			.buffer_sizes = {
				.inbound = 256,
				.outbound = 256,
			}
		},
	};
	app_event_loop(params, &handlers);
}
