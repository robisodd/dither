// Reference and table from:
//   http://www.efg2.com/Lab/Library/ImageProcessing/DHALF.TXT

#include <pebble.h>

// Global variables for the demo part
static Window *main_window;
static Layer *dither_layer, *text_layer;
uint8_t color[3] = {128, 128, 128};  // Start colors in the middle
enum {red, green, blue};             // for color[] above
enum {gray};                         // for color[] above, but just uses "red" channel
uint8_t grayscale = false;           // Allows 2-bit grayscale on color displays (ignored on B&W)
uint8_t cursor = 0;                  // Position for RGB caret
uint8_t cursormax = PBL_IF_COLOR_ELSE(3, 1); // 3 cursor positions for color, just the 1 for B&W


// ------------------------------------------------------------------------------------------------------------------------------------------------ //
//  Dither Function
// ------------------------------------------------------------------------ //
static const uint8_t dither_table[8][8] = {
  { 0, 32,  8, 40,  2, 34, 10, 42}, /* 8x8 Bayer ordered dithering */
  {48, 16, 56, 24, 50, 18, 58, 26}, /* pattern. Each input pixel */
  {12, 44,  4, 36, 14, 46,  6, 38}, /* is scaled to the 0..63 range */
  {60, 28, 52, 20, 62, 30, 54, 22}, /* before looking in this table */
  { 3, 35, 11, 43,  1, 33,  9, 41}, /* to determine the action. */
  {51, 19, 59, 27, 49, 17, 57, 25},
  {15, 47,  7, 39, 13, 45,  5, 37},
  {63, 31, 55, 23, 61, 29, 53, 21}
};

static void fill_rect_dithered(GContext *ctx, GRect rect, uint8_t r, uint8_t g, uint8_t b) {
  GBitmap* framebuffer = graphics_capture_frame_buffer(ctx);  // Capture the framebuffer
  if(framebuffer) {                                           // if successfully captured the framebuffer
    int16_t h = gbitmap_get_bounds(framebuffer).size.h;                      // Get visible height
    if(rect.origin.y<0) {rect.size.h += rect.origin.y; rect.origin.y = 0;}                   // If top    of rect is visible
    if(rect.origin.y + rect.size.h > h) rect.size.h = h - rect.origin.y;                     // If bottom of rect is visible
    for(int y = rect.origin.y; y < rect.origin.y+rect.size.h; y++) {         // Iterate over all y of visible part of rect

      GBitmapDataRowInfo info = gbitmap_get_data_row_info(framebuffer, y);   // Get visible width
      if(info.min_x < rect.origin.x) info.min_x = rect.origin.x;                             // If left   of rect is visible
      if(info.max_x > rect.origin.x + rect.size.w) info.max_x = rect.origin.x + rect.size.w; // If right  of rect is visible
      for(int x = info.min_x; x <= info.max_x; x++) {                        // Iterate over all x of visible part of rect

        // Draw the dithers!
        uint8_t d = dither_table[x&7][y&7];
        #if defined(PBL_BW) && defined(PBL_RECT)
          // intensity = Average RGB into a single 0-255 "brightness" value:
          uint8_t intensity = (r+r+r+b+g+g+g+g) >> 3;
          info.data[x/8] &= ~(1 << (x%8));                                // Color Pixel Black
          info.data[x/8] |= (((intensity+5)/4 - 1) > d ? 1 : 0) << (x%8); // Color Pixel White if dither says to (intensity 0-64)
        #elif defined(PBL_COLOR)
          uint8_t r2 = r/85; if ((64*(r%85))/85 > d) r2++;
          uint8_t g2 = g/85; if ((64*(g%85))/85 > d) g2++;
          uint8_t b2 = b/85; if ((64*(b%85))/85 > d) b2++;
          info.data[x] = 0b11000000 + (r2<<4) + (g2<<2) + b2;
        #endif
        
      }
    }
    graphics_release_frame_buffer(ctx, framebuffer);  // Release the Kraken! err, framebuffer.
  }
}


// ------------------------------------------------------------------------------------------------------------------------------------------------ //
//  Layer Update Functions
// ------------------------------------------------------------------------ //
static void dither_layer_update(Layer *layer, GContext *ctx) {
  GRect frame = layer_get_frame(layer);
  #if(PBL_COLOR)
  if(!grayscale)
    fill_rect_dithered(ctx, frame, color[red], color[green], color[blue]);    // Fill the layer
  else
  #endif
    fill_rect_dithered(ctx, frame, color[gray], color[gray], color[gray]);    // Use channel 0 (normally red) for all 3 colors
}

// ------------------------------------------------------------------------------------------------------------------------------------------------ //

static void text_layer_update(Layer *layer, GContext *ctx) {
  char text[50];  //Buffer to hold text
  #if(PBL_COLOR)
  if(!grayscale)
    snprintf(text, sizeof(text), "%cr=%d %cg=%d %cb=%d", cursor==0 ?'>':' ', color[red], cursor==1?'>':' ', color[green], cursor==2?'>':' ', color[blue]);
  else 
  #endif
    snprintf(text, sizeof(text), "%d", color[gray]);
  
  GRect bounds = layer_get_bounds(layer);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GSize text_size = graphics_text_layout_get_content_size(text, font, bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft); // Get how big the text is
  GRect text_rect = GRect((bounds.size.w - (text_size.w + 10))/2, 140, text_size.w + 10, text_size.h+5); // Nice 5px margin

  // Draw the background and text
  graphics_context_set_text_color(ctx, GColorWhite);  // Text Color
  graphics_context_set_fill_color(ctx, GColorBlack);  // Text Background Color
  graphics_fill_rect(ctx, text_rect, 0, GCornerNone); // Draw Background Rectangle
  graphics_draw_text(ctx, text, font, text_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}


// ------------------------------------------------------------------------------------------------------------------------------------------------ //
//  Button Functions
// ------------------------------------------------------------------------ //
static void     up_single_click_handler(ClickRecognizerRef recognizer, void *context) {color[cursor & ~grayscale]++;      layer_mark_dirty(dither_layer);}
static void select_single_click_handler(ClickRecognizerRef recognizer, void *context) {cursor = (cursor + 1) % cursormax; layer_mark_dirty(dither_layer);} // Advance cursor (if color)
static void   select_long_click_handler(ClickRecognizerRef recognizer, void *context) {grayscale = ~grayscale;            layer_mark_dirty(dither_layer);} // Hold to toggle grayscale (on color Pebbles)
static void   down_single_click_handler(ClickRecognizerRef recognizer, void *context) {color[cursor & ~grayscale]--;      layer_mark_dirty(dither_layer);}


static void click_config_provider(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP,    30,  up_single_click_handler);
            window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);
              window_long_click_subscribe(BUTTON_ID_SELECT, 0, select_long_click_handler, NULL);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN , 30, down_single_click_handler);
}


// ------------------------------------------------------------------------ //
//  Main Functions
// ------------------------------------------------------------------------ //
static void main_window_load(Window *window) {
  Layer *root_layer = window_get_root_layer(window);
  window_set_background_color(main_window, GColorBlack);
  window_set_click_config_provider(main_window, click_config_provider);

  dither_layer = layer_create(layer_get_frame(root_layer));
//   dither_layer = layer_create(GRect((layer_get_bounds(root_layer).size.w - 100) / 2,20,100,100)); // Non-fullscreen
//   dither_layer = layer_create(GRect( 100,20,100,100));  // testing out-of-bounds
  layer_add_child(root_layer, dither_layer);
  layer_set_update_proc(dither_layer, dither_layer_update);

  text_layer = layer_create(layer_get_frame(root_layer));
  layer_add_child(root_layer, text_layer);
  layer_set_update_proc(text_layer, text_layer_update);
}


static void main_window_unload(Window *window) {
  layer_destroy(text_layer);
  layer_destroy(dither_layer);
}


static void init() {
  main_window = window_create();
  window_set_window_handlers(main_window, (WindowHandlers) {.load = main_window_load, .unload = main_window_unload});
  window_stack_push(main_window, true);
}


static void deinit() {
  window_destroy(main_window);
}


int main(void) {
  init();
  app_event_loop();
  deinit();
}