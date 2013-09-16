#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "http.h"
#include "itoa.h"

#define MY_UUID { 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0x04, 0x9F, 0x49, 0xC0, 0x99, 0xAD }
PBL_APP_INFO(MY_UUID,
             "Groceries", "Benjamin von Deschwanden",
             1, 0, /* App version */
             RESOURCE_ID_MENU_ICON,
             APP_INFO_STANDARD_APP);

#define LIST_HTTP_COOKIE 1949327671
#define HTTP_KEY_CMD 1
#define HTTP_KEY_PARAM_1 2
#define HTTP_CMD_DELETE 0
#define HTTP_CMD_TOGGLE 1
#define HTTP_CMD_REQUEST_NUM_ITEMS 2
#define HTTP_CMD_REQUEST_ITEM 3
#define HTTP_CMD_REQUEST_ID 4

#define MAX_TODO_LIST_ITEMS (50)
#define MAX_ITEM_TEXT_LENGTH (20)

#define response_script "http://www.orchester-erstfeld.ch/Pebble/response2.php"


static Window s_window;
static MenuLayer s_menu_layer;
static TextLayer infoLayer;
static char x_pebble_id[12];
static uint8_t counter;
uint8_t num_items;

void request_num_items();
void request_id();
void request_item(uint8_t to_send);
void request_change_state(uint8_t to_toggle);
void request_deletion(uint8_t to_delete);


typedef enum {
    TodoListItemStateIncomplete = 0x00,
    TodoListItemStateComplete = 0x01,
} TodoListItemState;

typedef struct {
    TodoListItemState state;
    char text[MAX_ITEM_TEXT_LENGTH];
} TodoListItem;

static TodoListItem s_todo_list_items[MAX_TODO_LIST_ITEMS];
static int s_active_item_count = 0;

static TodoListItem* get_todo_list_item_at_index(int index) {
    if (index < 0 || index >= MAX_TODO_LIST_ITEMS) {
        return NULL;
    }

    return &s_todo_list_items[index];
}

static void todo_list_append(char *data, uint8_t state) {
    if (s_active_item_count == MAX_TODO_LIST_ITEMS) { 
        return;
    }

    s_todo_list_items[s_active_item_count].state = state;
    strcpy(s_todo_list_items[s_active_item_count].text, data);
    s_active_item_count++;
}

static void todo_list_delete(uint8_t list_idx) {
    if (s_active_item_count < 1) {
        return;
    }

    s_active_item_count--;

    memmove(&s_todo_list_items[list_idx], &s_todo_list_items[list_idx + 1],
        ((s_active_item_count - list_idx) * sizeof(TodoListItem)));

    request_deletion(list_idx);
}

static void todo_list_insert(uint8_t list_idx, TodoListItem *item) {
    if (s_active_item_count == MAX_TODO_LIST_ITEMS) {
        return;
    }

    memmove(&s_todo_list_items[list_idx + 1], &s_todo_list_items[list_idx],
        ((s_active_item_count - list_idx) * sizeof(TodoListItem)));
    s_todo_list_items[list_idx] = *item;
    s_active_item_count++;
}

static void todo_list_move(uint8_t first_idx, uint8_t second_idx) {
    if (first_idx >= s_active_item_count ||
        second_idx >= s_active_item_count ||
        first_idx == second_idx) {
        return;
    }

    TodoListItem temp_item = s_todo_list_items[first_idx];
    todo_list_delete(first_idx);
    todo_list_insert(second_idx, &temp_item);
}

static void todo_list_toggle_state(uint8_t list_idx) {
    if (list_idx >= s_active_item_count) {
        return;
    }
    s_todo_list_items[list_idx].state ^= 0x01;
}

static int16_t get_cell_height_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    return 44;
}

static void draw_strikethrough_on_item(GContext* ctx, Layer* cell_layer, TodoListItem* item) {
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

static void draw_row_callback(GContext* ctx, Layer *cell_layer, MenuIndex *cell_index, void *data) {
    TodoListItem* item;
    const int index = cell_index->row;

    if ((item = get_todo_list_item_at_index(index)) == NULL) {
        return;
    }

    menu_cell_basic_draw(ctx, cell_layer, item->text, NULL, NULL);
    if (item->state == TodoListItemStateComplete) {
        draw_strikethrough_on_item(ctx, cell_layer, item);
    }
}

static uint16_t get_num_rows_callback(struct MenuLayer *menu_layer, uint16_t section_index, void *data) {
    return s_active_item_count;
}

static void select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    const int index = cell_index->row;

    todo_list_toggle_state(index);
    request_change_state(index);

    menu_layer_reload_data(&s_menu_layer);
}

static void select_long_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    const int index = cell_index->row;

    todo_list_delete(index);

    menu_layer_reload_data(&s_menu_layer);
}

static void window_load(Window* window) {
    menu_layer_init(&s_menu_layer, window->layer.bounds);
    menu_layer_set_callbacks(&s_menu_layer, NULL, (MenuLayerCallbacks) {
        .get_cell_height = (MenuLayerGetCellHeightCallback) get_cell_height_callback,
        .draw_row = (MenuLayerDrawRowCallback) draw_row_callback,
        .get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback) get_num_rows_callback,
        .select_click = (MenuLayerSelectCallback) select_callback,
        .select_long_click = (MenuLayerSelectCallback) select_long_callback
    });
    menu_layer_set_click_config_onto_window(&s_menu_layer, window);
    layer_add_child(&window->layer, menu_layer_get_layer(&s_menu_layer));

    text_layer_init(&infoLayer, GRect(15, 30, 125, 100));
    text_layer_set_background_color(&infoLayer, GColorClear);
    layer_add_child(&window->layer, &infoLayer.layer);
    text_layer_set_text(&infoLayer, "LOADING...");

    //request_id();
    request_num_items();
}

void failed(int32_t cookie, int http_status, void* context) {
    //todo_list_append("_failed_");
    //todo_list_append(itoa(http_status));
}

void success(int32_t cookie, int http_status, DictionaryIterator* received, void* context) {
    //todo_list_append("_success_");
    if(cookie != LIST_HTTP_COOKIE) {/*todo_list_append("strange_suc");*/ return;}
    
    Tuple* id_tuple = dict_find(received, 4);
    if (id_tuple)
    {
        memcpy(&x_pebble_id, id_tuple->value->cstring, 12);
        text_layer_set_text(&infoLayer,x_pebble_id);
    }

    Tuple* num_items_tuple = dict_find(received, 1);
    if (num_items_tuple)
    {
        //todo_list_append("num_items:");
        num_items = num_items_tuple->value->int8;
        //todo_list_append(itoa(num_items));
        if (num_items > 0) {
            counter = 2;
        } else {
            text_layer_set_text(&infoLayer, "No items.\nSee http://goo.gl/su3h4K for instructions.");
        }
    } else {
  
        //todo_list_append("item:");
       Tuple* data_tuple = dict_find(received, 2);
       Tuple* state_tuple = dict_find(received, 3);
        if(data_tuple) {
            todo_list_append(data_tuple->value->cstring, state_tuple->value->int8);
           // todo_list_append(itoa(state_tuple->value->int8));      
        }
    }

        if (counter <= (num_items + 1))
        {
            request_item(counter);
            counter+=1;
        } else {
            //todo_list_append("_counter_max_");
            if (num_items > 0)
            {
                text_layer_set_text(&infoLayer, "");
            }
            menu_layer_reload_data(&s_menu_layer);
        }
}

void reconnect(void* context) {
    request_num_items();
}

void handle_init(AppContextRef ctx) {
    window_init(&s_window, "Weather Watch");
    window_set_window_handlers(&s_window, (WindowHandlers) {
        .load = window_load,
    });
    window_stack_push(&s_window, true /* Animated */);
    window_set_fullscreen(&s_window, false);

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


void request_num_items() {
    //todo_list_append("_request_num_items_");
    // Build the HTTP request
    DictionaryIterator *body;
    HTTPResult result = http_out_get(response_script, LIST_HTTP_COOKIE, &body);
    if(result != HTTP_OK) {
       //todo_list_append(itoa(result));
        return;
    }
    dict_write_int8(body, HTTP_KEY_CMD, HTTP_CMD_REQUEST_NUM_ITEMS);
    // Send it.
    if(http_out_send() != HTTP_OK) {
       // todo_list_append(itoa(result));
        return;
    }  
}

void request_item(uint8_t to_send) {
    //todo_list_append("_request_item_");
    DictionaryIterator *body;
    HTTPResult result = http_out_get(response_script, LIST_HTTP_COOKIE, &body);
    if(result != HTTP_OK) {
       //todo_list_append(itoa(result));
        return;
    }
    dict_write_int8(body, HTTP_KEY_CMD, HTTP_CMD_REQUEST_ITEM);
    dict_write_int8(body, HTTP_KEY_PARAM_1, to_send);
    // Send it.
    if(http_out_send() != HTTP_OK) {
        //todo_list_append(itoa(result));
        return;
    }
}


void request_change_state(uint8_t to_toggle) {
    //todo_list_append("_request_item_");
    DictionaryIterator *body;
    HTTPResult result = http_out_get(response_script, LIST_HTTP_COOKIE, &body);
    if(result != HTTP_OK) {
       //todo_list_append(itoa(result));
        return;
    }
    dict_write_int8(body, HTTP_KEY_CMD, HTTP_CMD_TOGGLE);
    dict_write_int8(body, HTTP_KEY_PARAM_1, to_toggle);
    // Send it.
    if(http_out_send() != HTTP_OK) {
        //todo_list_append(itoa(result));
        return;
    }
}

void request_deletion(uint8_t to_delete) {
    //todo_list_append("_request_item_");
    DictionaryIterator *body;
    HTTPResult result = http_out_get(response_script, LIST_HTTP_COOKIE, &body);
    if(result != HTTP_OK) {
       //todo_list_append(itoa(result));
        return;
    }
    dict_write_int8(body, HTTP_KEY_CMD, HTTP_CMD_DELETE);
    dict_write_int8(body, HTTP_KEY_PARAM_1, to_delete);
    // Send it.
    if(http_out_send() != HTTP_OK) {
        //todo_list_append(itoa(result));
        return;
    }
}


void request_id() {
    DictionaryIterator *body;
    HTTPResult result = http_out_get(response_script, LIST_HTTP_COOKIE, &body);
    if(result != HTTP_OK) {
        return;
    }
    dict_write_int8(body, HTTP_KEY_CMD, HTTP_CMD_REQUEST_ID);
    dict_write_int8(body, HTTP_KEY_PARAM_1, 0);
    // Send it.
    if(http_out_send() != HTTP_OK) {
        //todo_list_append(itoa(result));
        return;
    }
}