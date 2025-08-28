/* UI Component Library Demo
 * 
 * Showcases all UI components with different styles and configurations.
 */

#include "ui_demo.h"
#include "ui_button.h"
#include "ui_label.h"
#include "ui_panel.h"
#include "ui_theme.h"
#include "layout.h"
#include "view.h"
#include "graphics_context.h"
#include "display_driver.h"
#include "dispi_init.h"
#include "dispi.h"
#include "dispi_cursor.h"
#include "serial.h"
#include "timer.h"
#include "input.h"
#include "mouse.h"
#include "memory.h"

/* Global for mouse handler */
static Layout *g_ui_demo_layout = NULL;
static int g_ui_demo_needs_redraw = 0;

/* Button callbacks */
static void on_button_normal(Button *button, void *user_data) {
    serial_write_string("Normal button clicked!\n");
}

static void on_button_primary(Button *button, void *user_data) {
    serial_write_string("Primary button clicked!\n");
}

static void on_button_danger(Button *button, void *user_data) {
    serial_write_string("Danger button clicked!\n");
}

static void on_button_exit(Button *button, void *user_data) {
    int *running = (int*)user_data;
    *running = 0;
    serial_write_string("Exit button clicked - exiting demo\n");
}

/* Mouse event handler */
static void ui_demo_mouse_handler(InputEvent *event) {
    if (!event || !g_ui_demo_layout) return;
    
    /* Update cursor position on any mouse event */
    if (event->type == EVENT_MOUSE_MOVE || 
        event->type == EVENT_MOUSE_DOWN || 
        event->type == EVENT_MOUSE_UP) {
        dispi_cursor_move(event->data.mouse.x, event->data.mouse.y);
        g_ui_demo_needs_redraw = 1;
    }
    
    /* Pass event to layout for handling */
    if (layout_handle_event(g_ui_demo_layout, event)) {
        g_ui_demo_needs_redraw = 1;
    }
}

/* Main UI demo function */
void test_ui_demo(void) {
    Layout *layout;
    GraphicsContext *gc;
    DisplayDriver *driver;
    Panel *main_panel, *button_panel, *label_panel;
    Button *btn_normal, *btn_primary, *btn_danger, *btn_disabled;
    Button *btn_6x8, *btn_9x16, *btn_exit;
    Label *lbl_title, *lbl_left, *lbl_center, *lbl_right;
    Label *lbl_colors;
    int running = 1;
    int key;
    unsigned int last_update = 0;
    
    serial_write_string("Starting UI Component Library Demo\n");
    
    /* Initialize DISPI graphics mode using common init */
    driver = dispi_graphics_init();
    if (!driver) {
        serial_write_string("ERROR: Failed to initialize DISPI graphics\n");
        return;
    }
    
    /* Set mouse callback for UI demo */
    mouse_set_callback(ui_demo_mouse_handler);
    
    /* Create graphics context */
    gc = gc_create(driver);
    if (!gc) {
        serial_write_string("ERROR: Failed to create graphics context\n");
        return;
    }
    
    /* Create layout */
    layout = layout_create();
    if (!layout) {
        serial_write_string("ERROR: Failed to create layout\n");
        gc_destroy(gc);
        return;
    }
    
    /* Store layout for mouse handler */
    g_ui_demo_layout = layout;
    g_ui_demo_needs_redraw = 0;
    
    /* Create main panel */
    main_panel = panel_create(0, 0, 640, 480);
    panel_set_title(main_panel, "UI Component Library Demo", FONT_9X16);
    panel_set_background(main_panel, THEME_BG);
    
    /* Create title label */
    lbl_title = label_create(1, 1, 600, "Aquinas OS Component Library", FONT_9X16);
    label_set_align(lbl_title, ALIGN_CENTER);
    label_set_colors(lbl_title, COLOR_BLACK, COLOR_TRANSPARENT);
    
    /* Create button panel */
    button_panel = panel_create(1, 2, 300, 200);
    panel_set_title(button_panel, "Buttons", FONT_6X8);
    panel_set_border(button_panel, BORDER_RAISED, THEME_BORDER);
    
    /* Create buttons with different styles - coordinates relative to parent panel */
    btn_normal = button_create(0, 1, "Normal", FONT_6X8);
    button_set_callback(btn_normal, on_button_normal, NULL);
    
    btn_primary = button_create(1, 1, "Primary", FONT_6X8);
    button_set_style(btn_primary, BUTTON_STYLE_PRIMARY);
    button_set_callback(btn_primary, on_button_primary, NULL);
    
    btn_danger = button_create(2, 1, "Danger", FONT_6X8);
    button_set_style(btn_danger, BUTTON_STYLE_DANGER);
    button_set_callback(btn_danger, on_button_danger, NULL);
    
    btn_disabled = button_create(0, 2, "Disabled", FONT_6X8);
    button_set_enabled(btn_disabled, 0);
    
    /* Create buttons with different fonts */
    btn_6x8 = button_create(1, 2, "6x8 Font", FONT_6X8);
    btn_9x16 = button_create(2, 2, "9x16 Font", FONT_9X16);
    
    /* Create label panel */
    label_panel = panel_create(4, 2, 300, 200);
    panel_set_title(label_panel, "Labels", FONT_6X8);
    panel_set_border(label_panel, BORDER_SUNKEN, THEME_BORDER);
    
    /* Create labels with different alignments - coordinates relative to parent panel */
    lbl_left = label_create(0, 1, 200, "Left aligned", FONT_6X8);
    label_set_align(lbl_left, ALIGN_LEFT);
    
    lbl_center = label_create(0, 2, 200, "Center aligned", FONT_6X8);
    label_set_align(lbl_center, ALIGN_CENTER);
    
    lbl_right = label_create(0, 3, 200, "Right aligned", FONT_6X8);
    label_set_align(lbl_right, ALIGN_RIGHT);
    
    /* Create colored label */
    lbl_colors = label_create(1, 5, 400, "Cyan on dark gray background", FONT_9X16);
    label_set_colors(lbl_colors, COLOR_BRIGHT_CYAN, COLOR_DARK_GRAY);
    
    /* Create exit button */
    btn_exit = button_create(5, 5, "Exit Demo", FONT_9X16);
    button_set_style(btn_exit, BUTTON_STYLE_DANGER);
    button_set_callback(btn_exit, on_button_exit, &running);
    
    /* Add all components to layout */
    layout_set_region_content(layout, 0, 0, 7, 6, (View*)main_panel);
    view_add_child((View*)main_panel, (View*)lbl_title);
    view_add_child((View*)main_panel, (View*)button_panel);
    view_add_child((View*)main_panel, (View*)label_panel);
    view_add_child((View*)button_panel, (View*)btn_normal);
    view_add_child((View*)button_panel, (View*)btn_primary);
    view_add_child((View*)button_panel, (View*)btn_danger);
    view_add_child((View*)button_panel, (View*)btn_disabled);
    view_add_child((View*)button_panel, (View*)btn_6x8);
    view_add_child((View*)button_panel, (View*)btn_9x16);
    view_add_child((View*)label_panel, (View*)lbl_left);
    view_add_child((View*)label_panel, (View*)lbl_center);
    view_add_child((View*)label_panel, (View*)lbl_right);
    view_add_child((View*)main_panel, (View*)lbl_colors);
    view_add_child((View*)main_panel, (View*)btn_exit);
    
    /* Initial draw */
    layout_draw(layout, gc);
    if (dispi_is_double_buffered()) {
        dispi_cursor_show();
        dispi_flip_buffers();
    }
    
    serial_write_string("UI demo displayed. Click buttons, ESC to exit\n");
    
    /* Main loop */
    last_update = get_ticks();
    while (running) {
        unsigned int current_time = get_ticks();
        int delta_ms = current_time - last_update;
        
        /* Update views */
        if (delta_ms > 16) {  /* ~60 FPS */
            view_update_tree(layout->root_view, delta_ms);
            last_update = current_time;
            if (layout->root_view && layout->root_view->needs_redraw) {
                g_ui_demo_needs_redraw = 1;
            }
        }
        
        /* Poll mouse */
        mouse_poll();
        
        /* Check keyboard */
        key = keyboard_check();
        if (key == 27) {  /* ESC */
            running = 0;
            serial_write_string("ESC pressed, exiting UI demo\n");
        }
        
        /* Redraw if needed */
        if (g_ui_demo_needs_redraw || (layout && layout->needs_redraw) || 
            (layout->root_view && layout->root_view->needs_redraw)) {
            layout_draw(layout, gc);
            dispi_cursor_hide();
            dispi_cursor_show();
            dispi_flip_buffers();
            g_ui_demo_needs_redraw = 0;
        }
    }
    
    /* Cleanup */
    serial_write_string("Cleaning up UI demo\n");
    
    /* Destroy components */
    button_destroy(btn_exit);
    label_destroy(lbl_colors);
    label_destroy(lbl_right);
    label_destroy(lbl_center);
    label_destroy(lbl_left);
    button_destroy(btn_9x16);
    button_destroy(btn_6x8);
    button_destroy(btn_disabled);
    button_destroy(btn_danger);
    button_destroy(btn_primary);
    button_destroy(btn_normal);
    label_destroy(lbl_title);
    panel_destroy(label_panel);
    panel_destroy(button_panel);
    panel_destroy(main_panel);
    
    layout_destroy(layout);
    
    /* Cleanup DISPI graphics mode using common cleanup */
    dispi_graphics_cleanup(gc);
    
    serial_write_string("UI demo complete\n");
}