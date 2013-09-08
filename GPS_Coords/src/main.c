#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "http.h"
#include "itoa.h"



#define MY_UUID { 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0x04, 0x9F, 0x49, 0xC0, 0x99, 0xAD }
PBL_APP_INFO(HTTP_UUID,
             "GPS Coords.", "Benjamin von Deschwanden",
             1, 0, /* App version */
             RESOURCE_ID_MENU_ICON,
             APP_INFO_STANDARD_APP);

Window window;
TextLayer latLayer;
TextLayer lonLayer;



static int our_latitude, our_longitude;
static bool located;


void location(float latitude, float longitude, float altitude, float accuracy, void* context) {
	static char lat[10+7] = "Latitude: ";
	static char lon[11+7] = "Longitude: ";

	// Fix the floats
	our_latitude = latitude * 10000;
	our_longitude = longitude * 10000;
	located = true;

	memcpy(&lat[10], itoa(our_latitude), 7);
	text_layer_set_text(&latLayer, lat);

	memcpy(&lon[11], itoa(our_longitude), 7);
	text_layer_set_text(&lonLayer, lon);
}


void handle_init(AppContextRef ctx) {
	window_init(&window, "GPS Coordinates");
	window_stack_push(&window, true /* Animated */);
	
	text_layer_init(&latLayer, GRect(15, 25, 130, 75));
	layer_add_child(&window.layer, &latLayer.layer);
	text_layer_set_font(&latLayer,fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
	text_layer_init(&lonLayer, GRect(15, 75, 130, 120));
	layer_add_child(&window.layer, &lonLayer.layer);
	text_layer_set_font(&lonLayer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));

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
