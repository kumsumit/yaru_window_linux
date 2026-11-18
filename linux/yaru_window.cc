#include "yaru_window.h"

static GdkPoint get_cursor_position(GtkWindow* window) {
  GdkWindow* handle = gtk_widget_get_window(GTK_WIDGET(window));
  GdkDisplay* display = gdk_window_get_display(handle);
  GdkSeat* seat = gdk_display_get_default_seat(display);
  GdkDevice* pointer = gdk_seat_get_pointer(seat);
  GdkPoint pos;
  gdk_device_get_position(pointer, nullptr, &pos.x, &pos.y);
  return pos;
}

static GdkPoint get_window_origin(GtkWindow* window) {
  GdkWindow* handle = gtk_widget_get_window(GTK_WIDGET(window));
  GdkPoint pos;
  gdk_window_get_origin(handle, &pos.x, &pos.y);
  return pos;
}

// Returns true if the widget is GtkHeaderBar or HdyHeaderBar from libhandy.
static gboolean is_header_bar(GtkWidget* widget) {
  return widget != nullptr &&
         (GTK_IS_HEADER_BAR(widget) ||
          g_str_has_suffix(G_OBJECT_TYPE_NAME(widget), "HeaderBar"));
}

// Returns true if the widget is a GtkEventBox.
static gboolean is_event_box(GtkWidget* widget) {
  return widget != nullptr && GTK_IS_EVENT_BOX(widget);
}

// Recursively searches for a child in the widget tree.
static GtkWidget* find_child(GtkWidget* widget,
                             gboolean (*predicate)(GtkWidget*)) {
  if (predicate(widget)) {
    return widget;
  }

  if (GTK_IS_CONTAINER(widget)) {
    g_autoptr(GList) children = nullptr;
    gtk_container_forall(
        GTK_CONTAINER(widget),
        [](GtkWidget* widget, gpointer client_data) {
          GList** children = reinterpret_cast<GList**>(client_data);
          *children = g_list_prepend(*children, widget);
        },
        &children);
    for (GList* l = children; l != nullptr; l = l->next) {
      GtkWidget* event_box = find_child(GTK_WIDGET(l->data), predicate);
      if (event_box != nullptr) {
        return event_box;
      }
    }
  }

  return nullptr;
}

static GtkWidget* find_header_bar(GtkWindow* window) {
  GtkWidget* titlebar = gtk_window_get_titlebar(window);
  if (!is_header_bar(titlebar)) {
    titlebar = find_child(GTK_WIDGET(window), is_header_bar);
  }
  return titlebar;
}

void yaru_window_init(GtkWindow* window) {
  // nothing to do
}

FlValue* yaru_window_get_state(GtkWindow* window) {
  GdkWindow* handle = gtk_widget_get_window(GTK_WIDGET(window));
  GdkWindowState state = gdk_window_get_state(handle);
  GdkWindowTypeHint type = gdk_window_get_type_hint(handle);

  gboolean active = state & GDK_WINDOW_STATE_FOCUSED;
  gboolean closable = gtk_window_get_deletable(window);
  gboolean fullscreen = state & GDK_WINDOW_STATE_FULLSCREEN;
  gboolean maximized = gtk_window_is_maximized(window);
  gboolean minimized = state & GDK_WINDOW_STATE_ICONIFIED;
  gboolean normal = type == GDK_WINDOW_TYPE_HINT_NORMAL;
  gboolean restorable = normal && (fullscreen || maximized || minimized);
  const gchar* title = yaru_window_get_title(window);
  gboolean visible = gtk_widget_is_visible(GTK_WIDGET(window));

  FlValue* result = fl_value_new_map();
  fl_value_set_string_take(result, "id", fl_value_new_int(0));  // TODO
  fl_value_set_string_take(result, "type", fl_value_new_string("state"));
  fl_value_set_string_take(result, "active", fl_value_new_bool(active));
  fl_value_set_string_take(result, "closable", fl_value_new_bool(closable));
  fl_value_set_string_take(result, "fullscreen", fl_value_new_bool(fullscreen));
  fl_value_set_string_take(result, "maximizable",
                           fl_value_new_bool(normal && !maximized));
  fl_value_set_string_take(result, "maximized", fl_value_new_bool(maximized));
  fl_value_set_string_take(result, "minimizable",
                           fl_value_new_bool(normal && !minimized));
  fl_value_set_string_take(result, "minimized", fl_value_new_bool(minimized));
  fl_value_set_string_take(result, "movable", fl_value_new_bool(true));
  fl_value_set_string_take(result, "restorable", fl_value_new_bool(restorable));
  if (title != nullptr) {
    fl_value_set_string_take(result, "title", fl_value_new_string(title));
  }
  fl_value_set_string_take(result, "visible", fl_value_new_bool(visible));
  return result;
}

void yaru_window_begin_drag(GtkWindow* window) {
  GdkPoint cursor = get_cursor_position(window);
  g_object_set_data(G_OBJECT(window), "dragging", GINT_TO_POINTER(1));
  gtk_window_begin_move_drag(window, GDK_BUTTON_PRIMARY, cursor.x, cursor.y,
                             GDK_CURRENT_TIME);
}

void yaru_window_end_drag(GtkWindow* window) {
  gpointer dragging = g_object_get_data(G_OBJECT(window), "dragging");
  if (dragging == nullptr) {
    return;
  }

  GtkWidget* event_box = find_child(GTK_WIDGET(window), is_event_box);
  if (event_box == nullptr) {
    return;
  }

  GdkPoint cursor = get_cursor_position(window);
  GdkPoint origin = get_window_origin(window);

  g_autoptr(GdkEvent) event = gdk_event_new(GDK_BUTTON_RELEASE);
  event->button.button = GDK_BUTTON_PRIMARY;
  event->button.x = cursor.x - origin.x;
  event->button.y = cursor.y - origin.y;
  event->button.time = GDK_CURRENT_TIME;

  gboolean result = false;
  g_signal_emit_by_name(event_box, "button-release-event", event, &result);
  g_object_set_data(G_OBJECT(window), "dragging", nullptr);
}

const gchar* yaru_window_get_title(GtkWindow* window) {
  GtkWidget* titlebar = find_header_bar(window);
  if (titlebar != nullptr) {
    const gchar* title;
    g_object_get(titlebar, "title", &title, nullptr);
    return title;
  }
  return gtk_window_get_title(window);
}

void yaru_window_set_title(GtkWindow* window, const gchar* title) {
  GtkWidget* titlebar = find_header_bar(window);
  if (titlebar != nullptr) {
    g_object_set(titlebar, "title", title, nullptr);
  }
  gtk_window_set_title(window, title);
}

void yaru_window_hide_title(GtkWindow* window) {
  GtkWidget* titlebar = find_header_bar(window);
  if (titlebar != nullptr) {
    gtk_widget_hide(titlebar);
  }
}

void yaru_window_show_title(GtkWindow* window) {
  GtkWidget* titlebar = find_header_bar(window);
  if (titlebar != nullptr) {
    gtk_widget_show(titlebar);
  }
}

void yaru_window_restore(GtkWindow* window) {
  GdkWindow* handle = gtk_widget_get_window(GTK_WIDGET(window));
  GdkWindowState state = gdk_window_get_state(handle);
  if (state & GDK_WINDOW_STATE_FULLSCREEN) {
    gtk_window_unfullscreen(window);
  } else if (state & GDK_WINDOW_STATE_MAXIMIZED) {
    gtk_window_unmaximize(window);
  } else if (state & GDK_WINDOW_STATE_ICONIFIED) {
    gtk_window_deiconify(window);
  }
}

void yaru_window_show_menu(GtkWindow* window) {
  GdkWindow* handle = gtk_widget_get_window(GTK_WIDGET(window));
  GdkDisplay* display = gdk_window_get_display(handle);
  GdkSeat* seat = gdk_display_get_default_seat(display);
  GdkDevice* pointer = gdk_seat_get_pointer(seat);

  GdkPoint cursor = get_cursor_position(window);
  GdkPoint origin = get_window_origin(window);

  g_autoptr(GdkEvent) event = gdk_event_new(GDK_BUTTON_PRESS);
  event->button.button = GDK_BUTTON_SECONDARY;
  event->button.device = pointer;
  event->button.window = handle;
  event->button.x_root = cursor.x;
  event->button.y_root = cursor.y;
  event->button.x = cursor.x - origin.x;
  event->button.y = cursor.y - origin.y;
  gdk_window_show_window_menu(handle, event);
}

void yaru_window_set_background(GtkWindow* window, guint color) {
  GdkRGBA rgba = {
      .red = ((color >> 16) & 0xFF) / 255.0,
      .green = ((color >> 8) & 0xFF) / 255.0,
      .blue = (color & 0xFF) / 255.0,
      .alpha = (color >> 24) / 255.0,
  };

  gpointer css_provider = g_object_get_data(G_OBJECT(window), "css_provider");
  if (css_provider == nullptr) {
    css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(GTK_WIDGET(window)),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_set_data_full(G_OBJECT(window), "css_provider", css_provider,
                           g_object_unref);
  }

  g_autofree gchar* str = gdk_rgba_to_string(&rgba);
  g_autofree gchar* css = g_strdup_printf("window { background: %s; }", str);

  g_autoptr(GError) error = nullptr;
  gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(css_provider), css, -1,
                                  &error);
  if (error != nullptr) {
    g_warning("set_window_background: %s", error->message);
  }
}

static gboolean theme_exists(const gchar* name) {
  if (name == nullptr || *name == '\0') return FALSE;
  g_autofree gchar* user_themes = g_build_filename(g_get_home_dir(), ".themes", name, nullptr);
  if (g_file_test(user_themes, G_FILE_TEST_IS_DIR)) return TRUE;
  g_autofree gchar* data_themes =
      g_build_filename(g_get_user_data_dir(), "themes", name, nullptr);
  if (g_file_test(data_themes, G_FILE_TEST_IS_DIR)) return TRUE;
  const gchar* const* dirs = g_get_system_data_dirs();
  for (; dirs != nullptr && *dirs != nullptr; dirs++) {
    g_autofree gchar* path = g_build_filename(*dirs, "themes", name, nullptr);
    if (g_file_test(path, G_FILE_TEST_IS_DIR)) return TRUE;
  }
  return FALSE;
}

// Returns the name without a trailing "-dark" / "-Dark" (any case), or null if
// the theme name does not end with one.
static gchar* strip_dark_suffix(const gchar* name) {
  gsize len = strlen(name);
  if (len > 5 && g_ascii_strcasecmp(name + len - 5, "-dark") == 0) {
    return g_strndup(name, len - 5);
  }
  return nullptr;
}

void yaru_window_set_brightness(GtkWindow* window, const gchar* brightness) {
  gboolean dark = g_strcmp0(brightness, "dark") == 0;

  GtkSettings* settings = gtk_settings_get_default();
  g_object_set(settings, "gtk-application-prefer-dark-theme", dark, nullptr);

  // `gtk-application-prefer-dark-theme` only affects themes that ship a dark
  // variant of their own (such as Adwaita or Yaru). For other themes, e.g.
  // "Breeze" and "Breeze-Dark", switch between the separate themes.
  g_autofree gchar* theme_name = nullptr;
  g_object_get(settings, "gtk-theme-name", &theme_name, nullptr);
  if (theme_name == nullptr) {
    return;
  }

  // Remember the theme the application started with, so that light mode
  // restores it exactly.
  if (g_object_get_data(G_OBJECT(settings), "yaru_original_theme") == nullptr) {
    gchar* original = strip_dark_suffix(theme_name);
    g_object_set_data_full(G_OBJECT(settings), "yaru_original_theme",
                           original != nullptr ? original
                                               : g_strdup(theme_name),
                           g_free);
  }
  const gchar* base = static_cast<const gchar*>(
      g_object_get_data(G_OBJECT(settings), "yaru_original_theme"));

  g_autofree gchar* target = nullptr;
  if (dark) {
    const gchar* suffixes[] = {"-dark", "-Dark", nullptr};
    for (int i = 0; suffixes[i] != nullptr && target == nullptr; i++) {
      g_autofree gchar* candidate = g_strconcat(base, suffixes[i], nullptr);
      if (theme_exists(candidate)) {
        target = g_steal_pointer(&candidate);
      }
    }
  } else {
    target = g_strdup(base);
  }
  if (target != nullptr && g_strcmp0(target, theme_name) != 0) {
    g_object_set(settings, "gtk-theme-name", target, nullptr);
  }
}

void yaru_window_set_control_buttons_visible(GtkWindow* window,
                                             gboolean visible) {
  GtkWidget* titlebar = find_header_bar(window);
  if (titlebar != nullptr) {
    g_object_set(titlebar, "show-close-button", visible, nullptr);
  }
}

// Applies the stored minimum and maximum size as geometry hints. A size of
// zero (or less) in either dimension means "no limit".
static void apply_size_hints(GtkWindow* window) {
  gint min_w = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(window), "min_w"));
  gint min_h = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(window), "min_h"));
  gint max_w = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(window), "max_w"));
  gint max_h = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(window), "max_h"));

  GdkGeometry geometry = {};
  gint mask = 0;
  if (min_w > 0 || min_h > 0) {
    geometry.min_width = min_w > 0 ? min_w : 0;
    geometry.min_height = min_h > 0 ? min_h : 0;
    mask |= GDK_HINT_MIN_SIZE;
  }
  if (max_w > 0 || max_h > 0) {
    geometry.max_width = max_w > 0 ? max_w : G_MAXINT;
    geometry.max_height = max_h > 0 ? max_h : G_MAXINT;
    mask |= GDK_HINT_MAX_SIZE;
  }
  gtk_window_set_geometry_hints(window, nullptr, &geometry,
                                static_cast<GdkWindowHints>(mask));
}

void yaru_window_set_minimum_size(GtkWindow* window, gint width, gint height) {
  g_object_set_data(G_OBJECT(window), "min_w", GINT_TO_POINTER(width));
  g_object_set_data(G_OBJECT(window), "min_h", GINT_TO_POINTER(height));
  apply_size_hints(window);
}

void yaru_window_set_maximum_size(GtkWindow* window, gint width, gint height) {
  g_object_set_data(G_OBJECT(window), "max_w", GINT_TO_POINTER(width));
  g_object_set_data(G_OBJECT(window), "max_h", GINT_TO_POINTER(height));
  apply_size_hints(window);
}

void yaru_window_center(GtkWindow* window) {
  GdkWindow* handle = gtk_widget_get_window(GTK_WIDGET(window));
  if (handle == nullptr) {
    gtk_window_set_position(window, GTK_WIN_POS_CENTER);
    return;
  }
  GdkDisplay* display = gdk_window_get_display(handle);
  GdkMonitor* monitor = gdk_display_get_monitor_at_window(display, handle);
  GdkRectangle area;
  gdk_monitor_get_workarea(monitor, &area);

  gint width, height;
  gtk_window_get_size(window, &width, &height);
  gtk_window_move(window, area.x + (area.width - width) / 2,
                  area.y + (area.height - height) / 2);
}

// A window that was hidden comes back with an empty (black) Flutter surface
// on Wayland. Changing the size by a pixel and back makes the surface commit
// a fresh frame.
static gboolean restore_size_cb(gpointer user_data) {
  GtkWindow* window = GTK_WINDOW(user_data);
  gint width = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(window), "nudge_w"));
  gint height = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(window), "nudge_h"));
  gtk_window_resize(window, width, height);
  return G_SOURCE_REMOVE;
}

static gboolean nudge_size_cb(gpointer user_data) {
  GtkWindow* window = GTK_WINDOW(user_data);
  gint width, height;
  gtk_window_get_size(window, &width, &height);
  g_object_set_data(G_OBJECT(window), "nudge_w", GINT_TO_POINTER(width));
  g_object_set_data(G_OBJECT(window), "nudge_h", GINT_TO_POINTER(height));
  gtk_window_resize(window, width + 1, height);
  g_timeout_add(50, restore_size_cb, window);
  return G_SOURCE_REMOVE;
}

void yaru_window_refresh_surface(GtkWindow* window) {
  gtk_window_present(window);
  if (!gtk_window_is_maximized(window) &&
      !(gdk_window_get_state(gtk_widget_get_window(GTK_WIDGET(window))) &
        GDK_WINDOW_STATE_FULLSCREEN)) {
    g_timeout_add(100, nudge_size_cb, window);
  }
}
