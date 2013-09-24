#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "http.h"
#include "itoa.h"

// APP INFOS :: BEGIN
#define MY_UUID { 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0x04, 0x9F, 0x49, 0xC0, 0x99, 0xAD }
#define APPVH 2
#define APPVL 1
PBL_APP_INFO(MY_UUID,
             "Groceries", "Benjamin von Deschwanden",
             APPVH, APPVL, /* App version */
             RESOURCE_ID_MENU_ICON,
             APP_INFO_STANDARD_APP);
// APP INFOS :: END

// HTTP Definitions :: BEGIN
#define LIST_HTTP_COOKIE 1949327671
#define response_script "http://192.168.1.45:80/Pebble/response2.php"  //"http://www.orchester-erstfeld.ch/Pebble/response2.php"

#define HTTP_KEY_CMD 1
#define HTTP_KEY_PARAM_1 2
#define HTTP_KEY_PARAM_2 3

#define HTTP_CMD_DELETE 0
#define HTTP_CMD_TOGGLE 1
#define HTTP_CMD_REQUEST_NUM_ITEMS 2
#define HTTP_CMD_REQUEST_ITEM 3
#define HTTP_CMD_REQUEST_NUM_LISTS 4
#define HTTP_CMD_REQUEST_LIST 5
#define HTTP_CMD_REQUEST_ID 6
#define HTTP_CMD_REQUEST_INFOS 7
#define HTTP_CMD_RESPONSE_ITEM 8
#define HTTP_CMD_RESPONSE_STATE 9
// HTTP Definitions :: END

#define MAX_LIST_ENTRIES (50)
#define MAX_ITEM_ENTRIES (50)
#define MAX_ITEM_TEXT_LENGTH (50)
//#define debbuging true


static Window lists_window, items_window;
static MenuLayer lists_menu_layer, items_menu_layer;
static TextLayer lists_infoLayer, items_infoLayer;
static char x_pebble_id[12];
static char infos[255] = "";
static bool info_locked = false;
static bool info_on = false;
static uint8_t counter;
static uint8_t num_lists;
static uint8_t num_items;
static uint8_t current_list = 0;


static void send_request(uint8_t request_code, uint8_t arg1, uint8_t arg2);

typedef enum {
    TodoListItemStateIncomplete = 0x00,
    TodoListItemStateComplete = 0x01,
} TodoListItemState;

typedef struct {
    TodoListItemState state;
    char text[MAX_ITEM_TEXT_LENGTH];
} TodoListItem;

static TodoListItem lists_menu_list[MAX_LIST_ENTRIES];
static TodoListItem items_menu_list[MAX_ITEM_ENTRIES];
static int lists_active_item_count = 0;
static int items_active_item_count = 0;

static void display_info(char* info_text) {
    if (!info_locked)
    {
        text_layer_set_background_color(&lists_infoLayer, GColorWhite);
        text_layer_set_text(&lists_infoLayer, info_text);

        text_layer_set_background_color(&items_infoLayer, GColorWhite);
        text_layer_set_text(&items_infoLayer, info_text);

        info_on = true;
    }
}

static void hide_info() {
    if (!info_locked)
    {
        text_layer_set_background_color(&lists_infoLayer, GColorClear);
        text_layer_set_text(&lists_infoLayer, "");

        text_layer_set_background_color(&items_infoLayer, GColorClear);
        text_layer_set_text(&items_infoLayer, "");

        info_on = false;
    }
}

static void lock_info() {
    info_locked = true;
}

static void unlock_info() {
    info_locked = false;
}

// LISTS MENU LAYER CALLBACK FUNCTIONS :: BEGIN

static TodoListItem* lists_get_todo_list_item_at_index(int index) {
    if (index < 0 || index >= MAX_LIST_ENTRIES) {
        return NULL;
    }

    return &lists_menu_list[index];
}

static void lists_todo_list_append(char *data, uint8_t state) {
    if (lists_active_item_count == MAX_LIST_ENTRIES) { 
        return;
    }

    lists_menu_list[lists_active_item_count].state = state;
    strcpy(lists_menu_list[lists_active_item_count].text, data);
    lists_active_item_count++;
}

static void lists_todo_list_delete(uint8_t list_idx) {
    if (lists_active_item_count < 1) {
        return;
    }

    lists_active_item_count--;

    memmove(&lists_menu_list[list_idx], &lists_menu_list[list_idx + 1],
        ((lists_active_item_count - list_idx) * sizeof(TodoListItem)));

    ////send_request(HTTP_CMD_DELETE, list_idx);
}

static void lists_todo_list_insert(uint8_t list_idx, TodoListItem *item) {
    if (lists_active_item_count == MAX_LIST_ENTRIES) {
        return;
    }

    memmove(&lists_menu_list[list_idx + 1], &lists_menu_list[list_idx],
        ((lists_active_item_count - list_idx) * sizeof(TodoListItem)));
    lists_menu_list[list_idx] = *item;
    lists_active_item_count++;
}

static void lists_todo_list_move(uint8_t first_idx, uint8_t second_idx) {
    if (first_idx >= lists_active_item_count ||
        second_idx >= lists_active_item_count ||
        first_idx == second_idx) {
        return;
    }

    TodoListItem temp_item = lists_menu_list[first_idx];
    lists_todo_list_delete(first_idx);
    lists_todo_list_insert(second_idx, &temp_item);
}

static void lists_todo_list_toggle_state(uint8_t list_idx) {
    if (list_idx >= lists_active_item_count) {
        return;
    }
    lists_menu_list[list_idx].state ^= 0x01;
}

static int16_t lists_get_cell_height_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    return 44;
}

static void lists_draw_strikethrough_on_item(GContext* ctx, Layer* cell_layer, TodoListItem* item) {
    graphics_context_set_compositing_mode(ctx, GCompOpClear);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_fill_color(ctx, GColorBlack);
 
    GRect cell_bounds = cell_layer->bounds;

    static const int menu_cell_margin = 5;
    GSize text_cell_size = cell_bounds.size;
    text_cell_size.w -= 2 * menu_cell_margin;

    GRect text_cell_rect;
    text_cell_rect.origin = GPointZero;
    text_cell_rect.size = text_cell_size;

    GSize max_used_size = graphics_text_layout_get_max_used_size(ctx,
        item->text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), text_cell_rect,
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    GRect strike_through;
    strike_through.origin = cell_bounds.origin;
    strike_through.origin.x += menu_cell_margin;
    strike_through.origin.y += cell_bounds.size.h / 2;
    strike_through.size = (GSize) { max_used_size.w, 2 };

    // Stretch the strikethrough to be slightly wider than the text
    static const int pixel_nudge = 2;
    strike_through.origin.x -= pixel_nudge;
    strike_through.size.w += 2 * pixel_nudge;

    graphics_fill_rect(ctx, strike_through, 0, GCornerNone);
}

static void lists_draw_row_callback(GContext* ctx, Layer *cell_layer, MenuIndex *cell_index, void *data) {
    TodoListItem* item;
    const int index = cell_index->row;

    if ((item = lists_get_todo_list_item_at_index(index)) == NULL) {
        return;
    }

    menu_cell_basic_draw(ctx, cell_layer, item->text, NULL, NULL);
    if (item->state == TodoListItemStateComplete) {
        lists_draw_strikethrough_on_item(ctx, cell_layer, item);
    }
}

static uint16_t lists_get_num_rows_callback(struct MenuLayer *menu_layer, uint16_t section_index, void *data) {
    return lists_active_item_count;
}

// LISTS MENU LAYER CALLBACK FUNCTIONS :: END

// ITEMS MENU LAYER CALLBACK FUNCTIONS :: BEGIN

static TodoListItem* items_get_todo_list_item_at_index(int index) {
    if (index < 0 || index >= MAX_ITEM_ENTRIES) {
        return NULL;
    }

    return &items_menu_list[index];
}

static void items_todo_list_append(char *data, uint8_t state) {
    if (items_active_item_count == MAX_LIST_ENTRIES) { 
        return;
    }

    items_menu_list[items_active_item_count].state = state;
    strcpy(items_menu_list[items_active_item_count].text, data);
    items_active_item_count++;
}

static void items_todo_list_delete(uint8_t list_idx) {
    if (items_active_item_count < 1) {
        return;
    }

    items_active_item_count--;

    memmove(&items_menu_list[list_idx], &items_menu_list[list_idx + 1],
        ((items_active_item_count - list_idx) * sizeof(TodoListItem)));

    //send_request(HTTP_CMD_DELETE, current_list, list_idx);
}

static void items_todo_list_insert(uint8_t list_idx, TodoListItem *item) {
    if (items_active_item_count == MAX_LIST_ENTRIES) {
        return;
    }

    memmove(&items_menu_list[list_idx + 1], &items_menu_list[list_idx],
        ((items_active_item_count - list_idx) * sizeof(TodoListItem)));
    items_menu_list[list_idx] = *item;
    items_active_item_count++;
}

static void items_todo_list_move(uint8_t first_idx, uint8_t second_idx) {
    if (first_idx >= items_active_item_count ||
        second_idx >= items_active_item_count ||
        first_idx == second_idx) {
        return;
    }

    TodoListItem temp_item = items_menu_list[first_idx];
    items_todo_list_delete(first_idx);
    items_todo_list_insert(second_idx, &temp_item);
}

static void items_todo_list_toggle_state(uint8_t list_idx) {
    if (list_idx >= items_active_item_count) {
        return;
    }
    items_menu_list[list_idx].state ^= 0x01;
}

static int16_t items_get_cell_height_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    return 44;
}

static void items_draw_strikethrough_on_item(GContext* ctx, Layer* cell_layer, TodoListItem* item) {
    graphics_context_set_compositing_mode(ctx, GCompOpClear);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_fill_color(ctx, GColorBlack);
 
    GRect cell_bounds = cell_layer->bounds;

    static const int menu_cell_margin = 5;
    GSize text_cell_size = cell_bounds.size;
    text_cell_size.w -= 2 * menu_cell_margin;

    GRect text_cell_rect;
    text_cell_rect.origin = GPointZero;
    text_cell_rect.size = text_cell_size;

    GSize max_used_size = graphics_text_layout_get_max_used_size(ctx,
        item->text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), text_cell_rect,
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    GRect strike_through;
    strike_through.origin = cell_bounds.origin;
    strike_through.origin.x += menu_cell_margin;
    strike_through.origin.y += cell_bounds.size.h / 2;
    strike_through.size = (GSize) { max_used_size.w, 2 };

    // Stretch the strikethrough to be slightly wider than the text
    static const int pixel_nudge = 2;
    strike_through.origin.x -= pixel_nudge;
    strike_through.size.w += 2 * pixel_nudge;

    graphics_fill_rect(ctx, strike_through, 0, GCornerNone);
}

static void items_draw_row_callback(GContext* ctx, Layer *cell_layer, MenuIndex *cell_index, void *data) {
    TodoListItem* item;
    const int index = cell_index->row;

    if ((item = items_get_todo_list_item_at_index(index)) == NULL) {
        return;
    }

    menu_cell_basic_draw(ctx, cell_layer, item->text, NULL, NULL);
    if (item->state == TodoListItemStateComplete) {
        items_draw_strikethrough_on_item(ctx, cell_layer, item);
    }
}

static uint16_t items_get_num_rows_callback(struct MenuLayer *menu_layer, uint16_t section_index, void *data) {
    return items_active_item_count;
}

// ITEMS MENU LAYER CALLBACK FUNCTIONS :: END

// LISTS BUTTON CALLBACK FUNCTIONS :: BEGIN

static void lists_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {

    if (info_on)
    {
        unlock_info();
        hide_info();
        //display_info("LOADING...");
    } else {
        const int index = cell_index->row;

        window_stack_push(&items_window, true);
        //Send first request (INFOS):
        current_list = index;
        //send_request(HTTP_CMD_REQUEST_ITEM, current_list, 0);

        //scroll to top:
        while (menu_layer_get_selected_index(&items_menu_layer).row > 0) {
            menu_layer_set_selected_next(&items_menu_layer, true, MenuRowAlignTop, true);
        }

        //refresh menu layer:
        menu_layer_reload_data(&items_menu_layer);
    }
}

static void lists_select_long_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    const int index = cell_index->row;
    //todo_list_delete(index);

    //menu_layer_reload_data(&lists_menu_layer);
}

// LISTS BUTTON CALLBACK FUNCTIONS :: END

// ITEMS BUTTON CALLBACK FUNCTIONS :: BEGIN

static void items_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    const int index = menu_layer_get_selected_index(&items_menu_layer).row;

    items_todo_list_toggle_state(index);
    send_request(HTTP_CMD_TOGGLE, current_list, index);

    menu_layer_reload_data(&items_menu_layer);
}

static void items_select_long_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    const int index = menu_layer_get_selected_index(&items_menu_layer).row;

    items_todo_list_delete(index);
    send_request(HTTP_CMD_DELETE, current_list, index);

    menu_layer_reload_data(&items_menu_layer);
}

static void items_back_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    current_list = 0;
    while (items_active_item_count > 0) {
        items_todo_list_delete(0);
    }

    unlock_info();
    hide_info();

    window_stack_remove(&items_window, true);
}

static void items_up_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    menu_layer_set_selected_next(&items_menu_layer, true, MenuRowAlignTop, true);
}

static void items_down_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    menu_layer_set_selected_next(&items_menu_layer, false, MenuRowAlignTop, true);
}

// ITEMS BUTTON CALLBACK FUNCTIONS :: END

static void lists_window_load(Window* window) {   
    //Send first request (INFOS):
    send_request(HTTP_CMD_REQUEST_INFOS, APPVH*10+APPVL, 0);
}

static void items_window_load(Window* window) {  
    //Send first request (first item):
    send_request(HTTP_CMD_REQUEST_NUM_ITEMS, current_list, 0);
}

static void failed(int32_t cookie, int http_status, void* context) {
    //todo_list_append("_failed_");
    //todo_list_append(itoa(http_status));
}

static void success(int32_t cookie, int http_status, DictionaryIterator* received, void* context) {
    //todo_list_append("_success_");
    if(cookie != LIST_HTTP_COOKIE) {/*todo_list_append("strange_suc");*/ return;}



    //Try to get Infos:
    Tuple* info_tuple = dict_find(received, HTTP_CMD_REQUEST_INFOS);
    if (info_tuple)
    {
        memcpy(&infos, info_tuple->value->cstring, 255);
        display_info(infos);
        lock_info();
        //Send second request (NUM_LISTS):
        send_request(HTTP_CMD_REQUEST_NUM_LISTS, 0, 0);
    }

    //Try to get the Pebble-ID:
    Tuple* id_tuple = dict_find(received, HTTP_CMD_REQUEST_ID);
    if (id_tuple)
    {
        memcpy(&x_pebble_id, id_tuple->value->cstring, 12);
        display_info(x_pebble_id);
    }

    //Try to get the number of lists:
    Tuple* num_lists_tuple = dict_find(received, HTTP_CMD_REQUEST_NUM_LISTS);
    if (num_lists_tuple)
    {
        num_lists = num_lists_tuple->value->int8;
        if (num_lists > 0) {
            //Send third request (first list name):
            counter = 0;
            send_request(HTTP_CMD_REQUEST_LIST, counter, 0);
            counter += 1;
        
        
        } else {
            unlock_info();
            display_info("No lists available.\n\nSee http://goo.gl/su3h4K for instructions.");
            lock_info();
        }
    }

    //Try to get a list name:
    Tuple* list_name_tuple = dict_find(received, HTTP_CMD_REQUEST_LIST);
    if (list_name_tuple)
    {
        lists_todo_list_append(list_name_tuple->value->cstring, 0);

        if (counter < (num_lists))
        {
            send_request(HTTP_CMD_REQUEST_LIST, counter, 0);
            counter+=1;
        } else {
            //todo_list_append("_counter_max_");
            if (num_lists > 0)
            {
                unlock_info();
                hide_info();
            }
            menu_layer_reload_data(&lists_menu_layer);
        }
    }


    //try to get number of items:
    Tuple* num_items_tuple = dict_find(received, HTTP_CMD_REQUEST_NUM_ITEMS);    
    if (num_items_tuple)
    {
        num_items = num_items_tuple->value->int8;
        if (num_items > 0) {
            counter = 1;
            send_request(HTTP_CMD_REQUEST_ITEM, current_list, counter);
            counter += 1;
        } else {
            items_todo_list_append("num_items==0",0);
                unlock_info();
                display_info("No items.\nSee http://goo.gl/su3h4K for instructions.");
                lock_info();
        }
    } 


    //Try to get an item:
    Tuple* data_tuple = dict_find(received, HTTP_CMD_RESPONSE_ITEM);
    Tuple* state_tuple = dict_find(received, HTTP_CMD_RESPONSE_STATE);
    if(data_tuple) {
        items_todo_list_append(data_tuple->value->cstring, state_tuple->value->int8);
        // todo_list_append(itoa(state_tuple->value->int8));      

        if (counter <= (num_items))
        {
            send_request(HTTP_CMD_REQUEST_ITEM, current_list, counter);
            counter+=1;
        } else {
            //todo_list_append("_counter_max_");
            if (num_items > 0)
            {
                unlock_info();
                hide_info();
            } else {

            }
        }
    }

    menu_layer_reload_data(&items_menu_layer);
}

static void reconnect(void* context) {
    send_request(HTTP_CMD_REQUEST_INFOS, 0, 0);
}

void items_config_provider(ClickConfig **config, Window *window) {
    // single click / repeat-on-hold config:
    config[BUTTON_ID_BACK]->click.handler = (ClickHandler) items_back_callback;
    config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) items_down_callback;
    config[BUTTON_ID_UP]->click.handler = (ClickHandler) items_up_callback;
    config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) items_select_callback;

    config[BUTTON_ID_SELECT]->long_click.handler = (ClickHandler) items_select_long_callback;
    config[BUTTON_ID_SELECT]->long_click.delay_ms = 700;
}

static void handle_init(AppContextRef ctx) {
    // Lists Window init :: BEGIN
    window_init(&lists_window, "Lists");
    window_set_window_handlers(&lists_window, (WindowHandlers) {
        .load = lists_window_load,
    });
    window_stack_push(&lists_window, true);
    window_set_fullscreen(&lists_window, false);

    menu_layer_init(&lists_menu_layer, GRect(0, 0, 144, 153));
    menu_layer_set_callbacks(&lists_menu_layer, NULL, (MenuLayerCallbacks) {
        .get_cell_height = (MenuLayerGetCellHeightCallback) lists_get_cell_height_callback,
        .draw_row = (MenuLayerDrawRowCallback) lists_draw_row_callback,
        .get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback) lists_get_num_rows_callback,
        .select_click = (MenuLayerSelectCallback) lists_select_callback,
        .select_long_click = (MenuLayerSelectCallback) lists_select_long_callback
    });
    menu_layer_set_click_config_onto_window(&lists_menu_layer, &lists_window);
    layer_add_child(&lists_window.layer, menu_layer_get_layer(&lists_menu_layer));

    text_layer_init(&lists_infoLayer, GRect(0, 0, 144, 153));
    layer_add_child(&lists_window.layer, &lists_infoLayer.layer);
    display_info("LOADING...");
    // Lists Window init :: END


    // Items Window init :: BEGIN
    window_init(&items_window, "Items");
    window_set_window_handlers(&items_window, (WindowHandlers) {
        .load = items_window_load,
    });
    window_set_fullscreen(&items_window, false);

    menu_layer_init(&items_menu_layer, GRect(0, 0, 144, 153));
    menu_layer_set_callbacks(&items_menu_layer, NULL, (MenuLayerCallbacks) {
        .get_cell_height = (MenuLayerGetCellHeightCallback) items_get_cell_height_callback,
        .draw_row = (MenuLayerDrawRowCallback) items_draw_row_callback,
        .get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback) items_get_num_rows_callback
    });
    layer_add_child(&items_window.layer, menu_layer_get_layer(&items_menu_layer));

    text_layer_init(&items_infoLayer, GRect(0, 0, 144, 153));
    layer_add_child(&items_window.layer, &items_infoLayer.layer);
    display_info("LOADING...");

    window_set_click_config_provider(&items_window, (ClickConfigProvider) items_config_provider);
    items_window.overrides_back_button = true;
    // Items Window init :: END

    http_set_app_id(114548647);

    http_register_callbacks((HTTPCallbacks){
        .failure=failed,
        .success=success,
        .reconnect=reconnect
    }, (void*)ctx);
}


void pbl_main(void *params) {
    PebbleAppHandlers handlers = {
        .init_handler = &handle_init,
        .messaging_info = {
            .buffer_sizes = {
                .inbound = 124,
                .outbound = 256,
            }
        }
    };
    app_event_loop(params, &handlers);
}

static void send_request(uint8_t request_code, uint8_t arg1, uint8_t arg2) {
    DictionaryIterator *body;
    HTTPResult result = http_out_get(response_script, LIST_HTTP_COOKIE, &body);
    if(result != HTTP_OK) {
        return;
    }

    dict_write_int8(body, HTTP_KEY_CMD, request_code);
    dict_write_int8(body, HTTP_KEY_PARAM_1, arg1);
    dict_write_int8(body, HTTP_KEY_PARAM_2, arg2);

    // Send it.
    if(http_out_send() != HTTP_OK) {
        return;
    }
}