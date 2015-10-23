/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <string.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdktypes.h>
#include "gdkprivate-wayland.h"
#include "gdkwayland.h"
#include "gdkkeysyms.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicemanagerprivate.h"
#include "pointer-gestures-client-protocol.h"

#include <xkbcommon/xkbcommon.h>

#include <sys/time.h>
#include <sys/mman.h>
#include <linux/input.h>

typedef struct _GdkWaylandTouchData GdkWaylandTouchData;
typedef struct _GdkWaylandPointerData GdkWaylandPointerData;
typedef struct _GdkWaylandDeviceTabletPair GdkWaylandDeviceTabletPair;
typedef struct _GdkWaylandTabletData GdkWaylandTabletData;
typedef struct _GdkWaylandTabletToolData GdkWaylandTabletToolData;

struct _GdkWaylandTouchData
{
  uint32_t id;
  gdouble x;
  gdouble y;
  GdkWindow *window;
  uint32_t touch_down_serial;
  guint initial_touch : 1;
};

struct _GdkWaylandPointerData {
  GdkDevice *device;
  GdkWindow *focus;

  double surface_x, surface_y;

  GdkModifierType button_modifiers;

  uint32_t time;
  uint32_t enter_serial;
  uint32_t press_serial;

  GdkWindow *grab_window;
  uint32_t grab_time;

  struct wl_surface *pointer_surface;
  GdkCursor *cursor;
  guint cursor_timeout_id;
  guint cursor_image_index;
  guint cursor_image_delay;

  guint current_output_scale;
  GSList *pointer_surface_outputs;
};

struct _GdkWaylandDeviceTabletPair
{
  struct wl_tablet *wl_tablet;
  GdkDevice *master;
  GdkDevice *stylus_device;
  GdkDevice *eraser_device;
  GdkDevice *current_device;

  GdkWaylandDeviceData *wd;
  GdkWaylandPointerData pointer_info;

  gint pressure_axis_index;
  gint xtilt_axis_index;
  gint ytilt_axis_index;
  gint distance_axis_index;
  gdouble *axes;
};

struct _GdkWaylandTabletData
{
  char *name;
  uint32_t type;
  uint32_t vid;
  uint32_t pid;
  char *path;
};

struct _GdkWaylandTabletToolData
{
  uint64_t serial;
  uint64_t hw_id;
  GdkDeviceToolType type;
  GdkAxisFlags axis_flags;
};

struct _GdkWaylandDeviceData
{
  guint32 id;
  struct wl_seat *wl_seat;
  struct wl_pointer *wl_pointer;
  struct wl_keyboard *wl_keyboard;
  struct wl_touch *wl_touch;
  struct _wl_pointer_gesture_swipe *wl_pointer_gesture_swipe;
  struct _wl_pointer_gesture_pinch *wl_pointer_gesture_pinch;
  struct wl_tablet_manager *wl_tablet_manager;

  GdkDisplay *display;
  GdkDeviceManager *device_manager;

  GdkDevice *master_pointer;
  GdkDevice *master_keyboard;
  GdkDevice *pointer;
  GdkDevice *keyboard;
  GdkDevice *touch;
  GdkCursor *cursor;
  GdkKeymap *keymap;

  GHashTable *touches;

  GdkWaylandPointerData pointer_info;

  GdkModifierType key_modifiers;
  GdkWindow *keyboard_focus;

  gboolean have_server_repeat;
  uint32_t server_repeat_rate;
  uint32_t server_repeat_delay;
  guint32 repeat_timer;
  guint32 repeat_key;
  guint32 repeat_count;
  GSettings *keyboard_settings;

  GdkAtom pending_selection;
  struct wl_data_device *data_device;
  GdkDragContext *drop_context;

  /* Source/dest for non-local dnd */
  GdkWindow *foreign_dnd_window;

  /* Some tracking on gesture events */
  guint gesture_n_fingers;
  gdouble gesture_scale;
};

struct _GdkWaylandDevice
{
  GdkDevice parent_instance;
  GdkWaylandDeviceData *device;
  GdkWaylandPointerData *pointer;
};

struct _GdkWaylandDeviceClass
{
  GdkDeviceClass parent_class;
};

G_DEFINE_TYPE (GdkWaylandDevice, gdk_wayland_device, GDK_TYPE_DEVICE)

#define GDK_TYPE_WAYLAND_DEVICE_MANAGER        (gdk_wayland_device_manager_get_type ())
#define GDK_WAYLAND_DEVICE_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_WAYLAND_DEVICE_MANAGER, GdkWaylandDeviceManager))
#define GDK_WAYLAND_DEVICE_MANAGER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_WAYLAND_DEVICE_MANAGER, GdkWaylandDeviceManagerClass))
#define GDK_IS_WAYLAND_DEVICE_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_WAYLAND_DEVICE_MANAGER))
#define GDK_IS_WAYLAND_DEVICE_MANAGER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_WAYLAND_DEVICE_MANAGER))
#define GDK_WAYLAND_DEVICE_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_WAYLAND_DEVICE_MANAGER, GdkWaylandDeviceManagerClass))

#define GDK_SLOT_TO_EVENT_SEQUENCE(s) ((GdkEventSequence *) GUINT_TO_POINTER((s) + 1))
#define GDK_EVENT_SEQUENCE_TO_SLOT(s) (GPOINTER_TO_UINT(s) - 1)

typedef struct _GdkWaylandDeviceManager GdkWaylandDeviceManager;
typedef struct _GdkWaylandDeviceManagerClass GdkWaylandDeviceManagerClass;

struct _GdkWaylandDeviceManager
{
  GdkDeviceManager parent_object;
  GList *devices;
  GList *tablet_pairs;
};

struct _GdkWaylandDeviceManagerClass
{
  GdkDeviceManagerClass parent_class;
};

GType gdk_wayland_device_manager_get_type (void);

G_DEFINE_TYPE (GdkWaylandDeviceManager,
	       gdk_wayland_device_manager, GDK_TYPE_DEVICE_MANAGER)

static gboolean
gdk_wayland_device_get_history (GdkDevice      *device,
                                GdkWindow      *window,
                                guint32         start,
                                guint32         stop,
                                GdkTimeCoord ***events,
                                gint           *n_events)
{
  return FALSE;
}

static void
gdk_wayland_device_get_state (GdkDevice       *device,
                              GdkWindow       *window,
                              gdouble         *axes,
                              GdkModifierType *mask)
{
  gdouble x, y;

  gdk_window_get_device_position_double (window, device, &x, &y, mask);

  if (axes)
    {
      axes[0] = x;
      axes[1] = y;
    }
}

static void
gdk_wayland_pointer_stop_cursor_animation (GdkWaylandPointerData *pointer)
{
  if (pointer->cursor_timeout_id > 0)
    {
      g_source_remove (pointer->cursor_timeout_id);
      pointer->cursor_timeout_id = 0;
    }

  pointer->cursor_image_index = 0;
  pointer->cursor_image_delay = 0;
}

static GdkWaylandDeviceTabletPair *
gdk_wayland_device_manager_find_tablet_pair (GdkDeviceManager *device_manager,
                                             GdkDevice        *device)
{
  GdkWaylandDeviceManager *wdm = GDK_WAYLAND_DEVICE_MANAGER (device_manager);
  GList *l;

  for (l = wdm->tablet_pairs; l; l = l->next)
    {
      GdkWaylandDeviceTabletPair *pair = l->data;

      if (pair->master == device ||
          pair->stylus_device == device ||
          pair->eraser_device == device)
        return pair;
    }

  return NULL;
}

static gboolean
gdk_wayland_device_update_window_cursor (GdkDevice *device)
{
  GdkWaylandDeviceData *wd = GDK_WAYLAND_DEVICE (device)->device;
  GdkWaylandPointerData *pointer = GDK_WAYLAND_DEVICE (device)->pointer;
  struct wl_buffer *buffer;
  int x, y, w, h, scale;
  guint next_image_index, next_image_delay;
  GdkWaylandDeviceTabletPair *pair;
  gboolean retval = G_SOURCE_REMOVE;

  pair = gdk_wayland_device_manager_find_tablet_pair (wd->device_manager, device);

  if (pointer->cursor)
    {
      buffer = _gdk_wayland_cursor_get_buffer (pointer->cursor,
                                               pointer->cursor_image_index,
                                               &x, &y, &w, &h, &scale);
    }
  else
    {
      pointer->cursor_timeout_id = 0;
      return retval;
    }

  if (pair)
    {
      /* FIXME: the tool may exists, but we don't know the device pair until
       * it comes into proximity
       */
#if 0
      wl_tablet_tool_set_cursor (pair->wl_tablet,
                                 pointer->enter_serial,
                                 pointer->pointer_surface,
                                 x, y);
#endif
    }
  else if (wd->wl_pointer)
    {
      wl_pointer_set_cursor (wd->wl_pointer,
                             pointer->enter_serial,
                             pointer->pointer_surface,
                             x, y);
    }
  else
    return retval;

  if (buffer)
    {
      wl_surface_attach (pointer->pointer_surface, buffer, 0, 0);
      wl_surface_set_buffer_scale (pointer->pointer_surface, scale);
      wl_surface_damage (pointer->pointer_surface,  0, 0, w, h);
      wl_surface_commit (pointer->pointer_surface);
    }

  next_image_index =
    _gdk_wayland_cursor_get_next_image_index (pointer->cursor,
                                              pointer->cursor_image_index,
                                              &next_image_delay);

  if (next_image_index != pointer->cursor_image_index)
    {
      if (next_image_delay != pointer->cursor_image_delay)
        {
          guint id;

          gdk_wayland_pointer_stop_cursor_animation (pointer);

          /* Queue timeout for next frame */
          id = g_timeout_add (next_image_delay,
                              (GSourceFunc)gdk_wayland_device_update_window_cursor,
                              device);
          g_source_set_name_by_id (id, "[gtk+] gdk_wayland_device_update_window_cursor");
          pointer->cursor_timeout_id = id;
        }
      else
        retval = G_SOURCE_CONTINUE;

      pointer->cursor_image_index = next_image_index;
      pointer->cursor_image_delay = next_image_delay;
    }
  else
    gdk_wayland_pointer_stop_cursor_animation (pointer);

  return retval;
}

static void
gdk_wayland_device_set_window_cursor (GdkDevice *device,
                                      GdkWindow *window,
                                      GdkCursor *cursor)
{
  GdkWaylandDeviceData *wd = GDK_WAYLAND_DEVICE (device)->device;
  GdkWaylandPointerData *pointer = GDK_WAYLAND_DEVICE (device)->pointer;

  /* Setting the cursor to NULL means that we should use
   * the default cursor
   */
  if (!cursor)
    {
      guint scale = pointer->current_output_scale;
      cursor =
        _gdk_wayland_display_get_cursor_for_type_with_scale (wd->display,
                                                             GDK_LEFT_PTR,
                                                             scale);
    }
  else
    _gdk_wayland_cursor_set_scale (cursor, pointer->current_output_scale);

  if (cursor == pointer->cursor)
    return;

  gdk_wayland_pointer_stop_cursor_animation (pointer);

  if (pointer->cursor)
    g_object_unref (pointer->cursor);

  pointer->cursor = g_object_ref (cursor);

  gdk_wayland_device_update_window_cursor (device);
}

static void
gdk_wayland_device_warp (GdkDevice *device,
                         GdkScreen *screen,
                         gdouble    x,
                         gdouble    y)
{
}

static void
get_coordinates (GdkDevice *device,
                 double    *x,
                 double    *y,
                 double    *x_root,
                 double    *y_root)
{
  GdkWaylandPointerData *pointer = GDK_WAYLAND_DEVICE (device)->pointer;
  int root_x, root_y;

  if (x)
    *x = pointer->surface_x;
  if (y)
    *y = pointer->surface_y;

  if (pointer->focus)
    {
      gdk_window_get_root_coords (pointer->focus,
                                  pointer->surface_x,
                                  pointer->surface_y,
                                  &root_x, &root_y);
    }
  else
    {
      root_x = pointer->surface_x;
      root_y = pointer->surface_y;
    }

  if (x_root)
    *x_root = root_x;
  if (y_root)
    *y_root = root_y;
}

static GdkModifierType
device_get_modifiers (GdkDevice *device)
{
  GdkWaylandDeviceData *wd = GDK_WAYLAND_DEVICE (device)->device;
  GdkWaylandPointerData *pointer = GDK_WAYLAND_DEVICE (device)->pointer;
  GdkModifierType mask;

  mask = wd->key_modifiers;

  if (pointer)
    mask |= pointer->button_modifiers;

  return mask;
}

static void
gdk_wayland_device_query_state (GdkDevice        *device,
                                GdkWindow        *window,
                                GdkWindow       **root_window,
                                GdkWindow       **child_window,
                                gdouble          *root_x,
                                gdouble          *root_y,
                                gdouble          *win_x,
                                gdouble          *win_y,
                                GdkModifierType  *mask)
{
  GdkWaylandDeviceData *wd;
  GdkWaylandPointerData *pointer;
  GdkScreen *default_screen;

  wd = GDK_WAYLAND_DEVICE (device)->device;
  pointer = GDK_WAYLAND_DEVICE (device)->pointer;
  default_screen = gdk_display_get_default_screen (wd->display);

  if (root_window)
    *root_window = gdk_screen_get_root_window (default_screen);
  if (child_window)
    *child_window = pointer->focus;
  if (mask)
    *mask = device_get_modifiers (device);

  get_coordinates (device, win_x, win_y, root_x, root_y);
}

static void
emulate_crossing (GdkWindow       *window,
                  GdkWindow       *subwindow,
                  GdkDevice       *device,
                  GdkEventType     type,
                  GdkCrossingMode  mode,
                  guint32          time_)
{
  GdkEvent *event;

  event = gdk_event_new (type);
  event->crossing.window = window ? g_object_ref (window) : NULL;
  event->crossing.subwindow = subwindow ? g_object_ref (subwindow) : NULL;
  event->crossing.time = time_;
  event->crossing.mode = mode;
  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
  gdk_event_set_device (event, device);
  gdk_event_set_source_device (event, device);

  gdk_window_get_device_position_double (window, device,
                                         &event->crossing.x, &event->crossing.y,
                                         &event->crossing.state);
  event->crossing.x_root = event->crossing.x;
  event->crossing.y_root = event->crossing.y;

  _gdk_wayland_display_deliver_event (gdk_window_get_display (window), event);
}

static void
emulate_focus (GdkWindow *window,
               GdkDevice *device,
               gboolean   focus_in,
               guint32    time_)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = g_object_ref (window);
  event->focus_change.in = focus_in;
  gdk_event_set_device (event, device);
  gdk_event_set_source_device (event, device);

  _gdk_wayland_display_deliver_event (gdk_window_get_display (window), event);
}

static void
device_emit_grab_crossing (GdkDevice       *device,
                           GdkWindow       *from,
                           GdkWindow       *to,
                           GdkCrossingMode  mode,
                           guint32          time_)
{
  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      if (from)
        emulate_focus (from, device, FALSE, time_);
      if (to)
        emulate_focus (to, device, TRUE, time_);
    }
  else
    {
      if (from)
        emulate_crossing (from, to, device, GDK_LEAVE_NOTIFY, mode, time_);
      if (to)
        emulate_crossing (to, from, device, GDK_ENTER_NOTIFY, mode, time_);
    }
}

static GdkWindow *
gdk_wayland_device_get_focus (GdkDevice *device)
{
  GdkWaylandDeviceData *wayland_device = GDK_WAYLAND_DEVICE (device)->device;

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    return wayland_device->keyboard_focus;
  else if (gdk_device_get_source (device) == GDK_SOURCE_MOUSE)
    return wayland_device->pointer_info.focus;
  else
    {
      GdkWaylandDeviceTabletPair *pair;

      pair = gdk_wayland_device_manager_find_tablet_pair (wayland_device->device_manager,
                                                          device);
      if (pair)
        return pair->pointer_info.focus;
    }

  return NULL;
}

static GdkGrabStatus
gdk_wayland_device_grab (GdkDevice    *device,
                         GdkWindow    *window,
                         gboolean      owner_events,
                         GdkEventMask  event_mask,
                         GdkWindow    *confine_to,
                         GdkCursor    *cursor,
                         guint32       time_)
{
  GdkWaylandDeviceData *wayland_device = GDK_WAYLAND_DEVICE (device)->device;
  GdkWaylandPointerData *pointer = GDK_WAYLAND_DEVICE (device)->pointer;
  GdkWindow *prev_focus = gdk_wayland_device_get_focus (device);

  if (prev_focus != window)
    device_emit_grab_crossing (device, prev_focus, window, GDK_CROSSING_GRAB, time_);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      /* Device is a keyboard */
      return GDK_GRAB_SUCCESS;
    }
  else
    {
      /* Device is a pointer */
      if (pointer->grab_window != NULL &&
          time_ != 0 && pointer->grab_time > time_)
        {
          return GDK_GRAB_ALREADY_GRABBED;
        }

      if (time_ == 0)
        time_ = pointer->time;

      pointer->grab_window = window;
      pointer->grab_time = time_;

      /* FIXME: This probably breaks if you end up with multiple grabs
       * on the same window - but we need to know the input device for
       * when we are asked to map a popup window so that the grab can
       * be managed by the compositor.
       */
      _gdk_wayland_window_set_device_grabbed (window,
                                              device,
                                              wayland_device->wl_seat,
                                              time_);

      g_clear_object (&wayland_device->cursor);

      if (cursor)
        wayland_device->cursor = g_object_ref (cursor);

      gdk_wayland_device_update_window_cursor (device);
    }

  return GDK_GRAB_SUCCESS;
}

static void
gdk_wayland_device_ungrab (GdkDevice *device,
                           guint32    time_)
{
  GdkWaylandPointerData *pointer = GDK_WAYLAND_DEVICE (device)->pointer;
  GdkDisplay *display;
  GdkDeviceGrabInfo *grab;
  GdkWindow *focus, *prev_focus = NULL;

  display = gdk_device_get_display (device);

  grab = _gdk_display_get_last_device_grab (display, device);

  if (grab)
    {
      grab->serial_end = grab->serial_start;
      prev_focus = grab->window;
    }

  focus = gdk_wayland_device_get_focus (device);

  if (focus != prev_focus)
    device_emit_grab_crossing (device, prev_focus, focus, GDK_CROSSING_UNGRAB, time_);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      /* Device is a keyboard */
    }
  else
    {
      /* Device is a pointer */
      gdk_wayland_device_update_window_cursor (device);

      if (pointer->grab_window)
        _gdk_wayland_window_set_device_grabbed (pointer->grab_window,
                                                NULL,
                                                NULL,
                                                0);
    }
}

static GdkWindow *
gdk_wayland_device_window_at_position (GdkDevice       *device,
                                       gdouble         *win_x,
                                       gdouble         *win_y,
                                       GdkModifierType *mask,
                                       gboolean         get_toplevel)
{
  GdkWaylandPointerData *pointer;

  pointer = GDK_WAYLAND_DEVICE(device)->pointer;

  if (win_x)
    *win_x = pointer->surface_x;
  if (win_y)
    *win_y = pointer->surface_y;
  if (mask)
    *mask = device_get_modifiers (device);

  return pointer->focus;
}

static void
gdk_wayland_device_select_window_events (GdkDevice    *device,
                                         GdkWindow    *window,
                                         GdkEventMask  event_mask)
{
}

static void
gdk_wayland_device_class_init (GdkWaylandDeviceClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->get_history = gdk_wayland_device_get_history;
  device_class->get_state = gdk_wayland_device_get_state;
  device_class->set_window_cursor = gdk_wayland_device_set_window_cursor;
  device_class->warp = gdk_wayland_device_warp;
  device_class->query_state = gdk_wayland_device_query_state;
  device_class->grab = gdk_wayland_device_grab;
  device_class->ungrab = gdk_wayland_device_ungrab;
  device_class->window_at_position = gdk_wayland_device_window_at_position;
  device_class->select_window_events = gdk_wayland_device_select_window_events;
}

static void
gdk_wayland_device_init (GdkWaylandDevice *device_core)
{
  GdkDevice *device;

  device = GDK_DEVICE (device_core);

  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_X, 0, 0, 1);
  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_Y, 0, 0, 1);
}

/**
 * gdk_wayland_device_get_wl_seat:
 * @device: (type GdkWaylandDevice): a #GdkDevice
 *
 * Returns the Wayland wl_seat of a #GdkDevice.
 *
 * Returns: (transfer none): a Wayland wl_seat
 *
 * Since: 3.10
 */
struct wl_seat *
gdk_wayland_device_get_wl_seat (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (device), NULL);

  return GDK_WAYLAND_DEVICE (device)->device->wl_seat;
}

/**
 * gdk_wayland_device_get_wl_pointer:
 * @device: (type GdkWaylandDevice): a #GdkDevice
 *
 * Returns the Wayland wl_pointer of a #GdkDevice.
 *
 * Returns: (transfer none): a Wayland wl_pointer
 *
 * Since: 3.10
 */
struct wl_pointer *
gdk_wayland_device_get_wl_pointer (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (device), NULL);

  return GDK_WAYLAND_DEVICE (device)->device->wl_pointer;
}

/**
 * gdk_wayland_device_get_wl_keyboard:
 * @device: (type GdkWaylandDevice): a #GdkDevice
 *
 * Returns the Wayland wl_keyboard of a #GdkDevice.
 *
 * Returns: (transfer none): a Wayland wl_keyboard
 *
 * Since: 3.10
 */
struct wl_keyboard *
gdk_wayland_device_get_wl_keyboard (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (device), NULL);

  return GDK_WAYLAND_DEVICE (device)->device->wl_keyboard;
}

GdkKeymap *
_gdk_wayland_device_get_keymap (GdkDevice *device)
{
  return GDK_WAYLAND_DEVICE (device)->device->keymap;
}

static void
emit_selection_owner_change (GdkWindow *window,
                             GdkAtom    atom)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_OWNER_CHANGE);
  event->owner_change.window = g_object_ref (window);
  event->owner_change.owner = NULL;
  event->owner_change.reason = GDK_OWNER_CHANGE_NEW_OWNER;
  event->owner_change.selection = atom;
  event->owner_change.time = GDK_CURRENT_TIME;
  event->owner_change.selection_time = GDK_CURRENT_TIME;

  gdk_event_put (event);
  gdk_event_free (event);
}

static void
data_device_data_offer (void                  *data,
                        struct wl_data_device *data_device,
                        struct wl_data_offer  *offer)
{
  GdkWaylandDeviceData *device = (GdkWaylandDeviceData *)data;

  GDK_NOTE (EVENTS,
            g_message ("data device data offer, data device %p, offer %p",
                       data_device, offer));

  gdk_wayland_selection_ensure_offer (device->display, offer);
}

static void
data_device_enter (void                  *data,
                   struct wl_data_device *data_device,
                   uint32_t               serial,
                   struct wl_surface     *surface,
                   wl_fixed_t             x,
                   wl_fixed_t             y,
                   struct wl_data_offer  *offer)
{
  GdkWaylandDeviceData *device = (GdkWaylandDeviceData *)data;
  GdkWindow *dest_window, *dnd_owner;
  GdkAtom selection;

  dest_window = wl_surface_get_user_data (surface);

  if (!GDK_IS_WINDOW (dest_window))
    return;

  GDK_NOTE (EVENTS,
            g_message ("data device enter, data device %p serial %u, surface %p, x %f y %f, offer %p",
                       data_device, serial, surface, wl_fixed_to_double (x), wl_fixed_to_double (y), offer));

  /* Update pointer state, so device state queries work during DnD */
  device->pointer_info.focus = g_object_ref (dest_window);
  device->pointer_info.surface_x = wl_fixed_to_double (x);
  device->pointer_info.surface_y = wl_fixed_to_double (y);

  gdk_wayland_drop_context_update_targets (device->drop_context);

  selection = gdk_drag_get_selection (device->drop_context);
  dnd_owner = gdk_selection_owner_get_for_display (device->display, selection);

  if (!dnd_owner)
    dnd_owner = device->foreign_dnd_window;

  _gdk_wayland_drag_context_set_source_window (device->drop_context, dnd_owner);

  _gdk_wayland_drag_context_set_dest_window (device->drop_context,
                                             dest_window, serial);
  _gdk_wayland_drag_context_set_coords (device->drop_context,
                                        wl_fixed_to_double (x),
                                        wl_fixed_to_double (y));
  _gdk_wayland_drag_context_emit_event (device->drop_context, GDK_DRAG_ENTER,
                                        GDK_CURRENT_TIME);

  gdk_wayland_selection_set_offer (device->display, selection, offer);

  emit_selection_owner_change (dest_window, selection);
}

static void
data_device_leave (void                  *data,
                   struct wl_data_device *data_device)
{
  GdkWaylandDeviceData *device = (GdkWaylandDeviceData *) data;

  GDK_NOTE (EVENTS,
            g_message ("data device leave, data device %p", data_device));

  if (!gdk_drag_context_get_dest_window (device->drop_context))
    return;

  g_object_unref (device->pointer_info.focus);
  device->pointer_info.focus = NULL;

  _gdk_wayland_drag_context_set_coords (device->drop_context, -1, -1);
  _gdk_wayland_drag_context_emit_event (device->drop_context, GDK_DRAG_LEAVE,
                                        GDK_CURRENT_TIME);
  _gdk_wayland_drag_context_set_dest_window (device->drop_context, NULL, 0);
}

static void
data_device_motion (void                  *data,
                    struct wl_data_device *data_device,
                    uint32_t               time,
                    wl_fixed_t             x,
                    wl_fixed_t             y)
{
  GdkWaylandDeviceData *device = (GdkWaylandDeviceData *) data;

  g_debug (G_STRLOC ": %s data_device = %p, time = %d, x = %f, y = %f",
           G_STRFUNC, data_device, time, wl_fixed_to_double (x), wl_fixed_to_double (y));

  if (!gdk_drag_context_get_dest_window (device->drop_context))
    return;

  /* Update pointer state, so device state queries work during DnD */
  device->pointer_info.surface_x = wl_fixed_to_double (x);
  device->pointer_info.surface_y = wl_fixed_to_double (y);

  gdk_wayland_drop_context_update_targets (device->drop_context);
  _gdk_wayland_drag_context_set_coords (device->drop_context,
                                        wl_fixed_to_double (x),
                                        wl_fixed_to_double (y));
  _gdk_wayland_drag_context_emit_event (device->drop_context,
                                        GDK_DRAG_MOTION, time);
}

static void
data_device_drop (void                  *data,
                  struct wl_data_device *data_device)
{
  GdkWaylandDeviceData *device = (GdkWaylandDeviceData *) data;
  GdkWindow *local_dnd_owner;

  GDK_NOTE (EVENTS,
            g_message ("data device drop, data device %p", data_device));

  local_dnd_owner = gdk_selection_owner_get_for_display (device->display, gdk_drag_get_selection (device->drop_context));

  if (local_dnd_owner)
    gdk_wayland_device_unset_grab (device->master_pointer);

  _gdk_wayland_drag_context_emit_event (device->drop_context,
                                        GDK_DROP_START, GDK_CURRENT_TIME);
}

static void
data_device_selection (void                  *data,
                       struct wl_data_device *wl_data_device,
                       struct wl_data_offer  *offer)
{
  GdkWaylandDeviceData *device = (GdkWaylandDeviceData *) data;
  GdkAtom selection;

  GDK_NOTE (EVENTS,
            g_message ("data device selection, data device %p, data offer %p",
                       wl_data_device, offer));

  selection = gdk_atom_intern_static_string ("CLIPBOARD");
  gdk_wayland_selection_set_offer (device->display, selection, offer);

  /* If we already have keyboard focus, the selection was targeted at the
   * focused surface. If we don't we will receive keyboard focus directly after
   * this, so lets wait and find out what window will get the focus before
   * emitting the owner-changed event.
   */
  if (device->keyboard_focus)
    emit_selection_owner_change (device->keyboard_focus, selection);
  else
    device->pending_selection = selection;
}

static const struct wl_data_device_listener data_device_listener = {
  data_device_data_offer,
  data_device_enter,
  data_device_leave,
  data_device_motion,
  data_device_drop,
  data_device_selection
};

static void
pointer_handle_enter (void              *data,
                      struct wl_pointer *pointer,
                      uint32_t           serial,
                      struct wl_surface *surface,
                      wl_fixed_t         sx,
                      wl_fixed_t         sy)
{
  GdkWaylandDeviceData *device = data;
  GdkEvent *event;
  GdkWaylandDisplay *wayland_display =
    GDK_WAYLAND_DISPLAY (device->display);

  if (!surface)
    return;
  if (!GDK_IS_WINDOW (wl_surface_get_user_data (surface)))
    return;

  _gdk_wayland_display_update_serial (wayland_display, serial);

  device->pointer_info.focus = wl_surface_get_user_data(surface);
  g_object_ref(device->pointer_info.focus);

  device->pointer_info.surface_x = wl_fixed_to_double (sx);
  device->pointer_info.surface_y = wl_fixed_to_double (sy);
  device->pointer_info.enter_serial = serial;

  event = gdk_event_new (GDK_ENTER_NOTIFY);
  event->crossing.window = g_object_ref (device->pointer_info.focus);
  gdk_event_set_device (event, device->master_pointer);
  gdk_event_set_source_device (event, device->pointer);
  event->crossing.subwindow = NULL;
  event->crossing.time = (guint32)(g_get_monotonic_time () / 1000);
  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
  event->crossing.focus = TRUE;
  event->crossing.state = 0;

  gdk_wayland_device_update_window_cursor (device->master_pointer);

  get_coordinates (device->master_pointer,
                   &event->crossing.x,
                   &event->crossing.y,
                   &event->crossing.x_root,
                   &event->crossing.y_root);

  _gdk_wayland_display_deliver_event (device->display, event);

  GDK_NOTE (EVENTS,
            g_message ("enter, device %p surface %p",
                       device, device->pointer_info.focus));
}

static void
pointer_handle_leave (void              *data,
                      struct wl_pointer *pointer,
                      uint32_t           serial,
                      struct wl_surface *surface)
{
  GdkWaylandDeviceData *device = data;
  GdkEvent *event;
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (device->display);

  if (!surface)
    return;

  if (!GDK_IS_WINDOW (wl_surface_get_user_data (surface)))
    return;

  if (!device->pointer_info.focus)
    return;

  _gdk_wayland_display_update_serial (wayland_display, serial);

  event = gdk_event_new (GDK_LEAVE_NOTIFY);
  event->crossing.window = g_object_ref (device->pointer_info.focus);
  gdk_event_set_device (event, device->master_pointer);
  gdk_event_set_source_device (event, device->pointer);
  event->crossing.subwindow = NULL;
  event->crossing.time = (guint32)(g_get_monotonic_time () / 1000);
  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
  event->crossing.focus = TRUE;
  event->crossing.state = 0;

  gdk_wayland_device_update_window_cursor (device->master_pointer);

  get_coordinates (device->master_pointer,
                   &event->crossing.x,
                   &event->crossing.y,
                   &event->crossing.x_root,
                   &event->crossing.y_root);

  _gdk_wayland_display_deliver_event (device->display, event);

  GDK_NOTE (EVENTS,
            g_message ("leave, device %p surface %p",
                       device, device->pointer_info.focus));

  g_object_unref (device->pointer_info.focus);
  device->pointer_info.focus = NULL;
  if (device->cursor)
    gdk_wayland_pointer_stop_cursor_animation (&device->pointer_info);
}

static void
pointer_handle_motion (void              *data,
                       struct wl_pointer *pointer,
                       uint32_t           time,
                       wl_fixed_t         sx,
                       wl_fixed_t         sy)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);
  GdkEvent *event;

  if (!device->pointer_info.focus)
    return;

  event = gdk_event_new (GDK_NOTHING);

  device->pointer_info.time = time;
  device->pointer_info.surface_x = wl_fixed_to_double (sx);
  device->pointer_info.surface_y = wl_fixed_to_double (sy);

  event->motion.type = GDK_MOTION_NOTIFY;
  event->motion.window = g_object_ref (device->pointer_info.focus);
  gdk_event_set_device (event, device->master_pointer);
  gdk_event_set_source_device (event, device->pointer);
  event->motion.time = time;
  event->motion.axes = NULL;
  event->motion.state = device_get_modifiers (device->master_pointer);
  event->motion.is_hint = 0;
  gdk_event_set_screen (event, display->screen);

  get_coordinates (device->master_pointer,
                   &event->motion.x,
                   &event->motion.y,
                   &event->motion.x_root,
                   &event->motion.y_root);

  GDK_NOTE (EVENTS,
            g_message ("motion %f %f, device %p state %d",
                       wl_fixed_to_double (sx), wl_fixed_to_double (sy),
		       device, event->button.state));

  _gdk_wayland_display_deliver_event (device->display, event);
}

static void
pointer_handle_button (void              *data,
                       struct wl_pointer *pointer,
                       uint32_t           serial,
                       uint32_t           time,
                       uint32_t           button,
                       uint32_t           state)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);
  GdkEvent *event;
  uint32_t modifier;
  int gdk_button;

  if (!device->pointer_info.focus)
    return;

  _gdk_wayland_display_update_serial (display, serial);

  switch (button)
    {
    case 273:
      gdk_button = 3;
      break;
    case 274:
      gdk_button = 2;
      break;
    default:
      gdk_button = button - 271;
      break;
    }

  device->pointer_info.time = time;
  if (state)
    device->pointer_info.press_serial = serial;

  event = gdk_event_new (state ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE);
  event->button.window = g_object_ref (device->pointer_info.focus);
  gdk_event_set_device (event, device->master_pointer);
  gdk_event_set_source_device (event, device->pointer);
  event->button.time = time;
  event->button.axes = NULL;
  event->button.state = device_get_modifiers (device->master_pointer);
  event->button.button = gdk_button;
  gdk_event_set_screen (event, display->screen);

  get_coordinates (device->master_pointer,
                   &event->button.x,
                   &event->button.y,
                   &event->button.x_root,
                   &event->button.y_root);

  modifier = 1 << (8 + gdk_button - 1);
  if (state)
    device->pointer_info.button_modifiers |= modifier;
  else
    device->pointer_info.button_modifiers &= ~modifier;

  GDK_NOTE (EVENTS,
	    g_message ("button %d %s, device %p state %d",
		       event->button.button,
		       state ? "press" : "release",
                       device,
                       event->button.state));

  _gdk_wayland_display_deliver_event (device->display, event);
}

static void
pointer_handle_axis (void              *data,
                     struct wl_pointer *pointer,
                     uint32_t           time,
                     uint32_t           axis,
                     wl_fixed_t         value)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);
  GdkEvent *event;
  gdouble delta_x, delta_y;

  if (!device->pointer_info.focus)
    return;

  /* get the delta and convert it into the expected range */
  switch (axis)
    {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
      delta_x = 0;
      delta_y = wl_fixed_to_double (value) / 10.0;
      break;
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
      delta_x = wl_fixed_to_double (value) / 10.0;
      delta_y = 0;
      break;
    default:
      g_return_if_reached ();
    }

  device->pointer_info.time = time;
  event = gdk_event_new (GDK_SCROLL);
  event->scroll.window = g_object_ref (device->pointer_info.focus);
  gdk_event_set_device (event, device->master_pointer);
  gdk_event_set_source_device (event, device->pointer);
  event->scroll.time = time;
  event->scroll.direction = GDK_SCROLL_SMOOTH;
  event->scroll.delta_x = delta_x;
  event->scroll.delta_y = delta_y;
  event->scroll.state = device_get_modifiers (device->master_pointer);
  gdk_event_set_screen (event, display->screen);

  get_coordinates (device->master_pointer,
                   &event->scroll.x,
                   &event->scroll.y,
                   &event->scroll.x_root,
                   &event->scroll.y_root);

  GDK_NOTE (EVENTS,
            g_message ("scroll %f %f, device %p",
                       event->scroll.delta_x, event->scroll.delta_y, device));

  _gdk_wayland_display_deliver_event (device->display, event);
}

static void
keyboard_handle_keymap (void               *data,
                        struct wl_keyboard *keyboard,
                        uint32_t            format,
                        int                 fd,
                        uint32_t            size)
{
  GdkWaylandDeviceData *device = data;

  _gdk_wayland_keymap_update_from_fd (device->keymap, format, fd, size);

  g_signal_emit_by_name (device->keymap, "keys-changed");
  g_signal_emit_by_name (device->keymap, "state-changed");
  g_signal_emit_by_name (device->keymap, "direction-changed");
}

static void
keyboard_handle_enter (void               *data,
                       struct wl_keyboard *keyboard,
                       uint32_t            serial,
                       struct wl_surface  *surface,
                       struct wl_array    *keys)
{
  GdkWaylandDeviceData *device = data;
  GdkEvent *event;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);

  if (!surface)
    return;

  if (!GDK_IS_WINDOW (wl_surface_get_user_data (surface)))
    return;

  _gdk_wayland_display_update_serial (display, serial);

  device->keyboard_focus = wl_surface_get_user_data (surface);
  g_object_ref (device->keyboard_focus);

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = g_object_ref (device->keyboard_focus);
  event->focus_change.send_event = FALSE;
  event->focus_change.in = TRUE;
  gdk_event_set_device (event, device->master_keyboard);
  gdk_event_set_source_device (event, device->keyboard);

  GDK_NOTE (EVENTS,
            g_message ("focus in, device %p surface %p",
                       device, device->keyboard_focus));

  _gdk_wayland_display_deliver_event (device->display, event);

  if (device->pending_selection != GDK_NONE)
    {
      emit_selection_owner_change (device->keyboard_focus,
                                   device->pending_selection);
      device->pending_selection = GDK_NONE;
    }
}

static void stop_key_repeat (GdkWaylandDeviceData *device);

static void
keyboard_handle_leave (void               *data,
                       struct wl_keyboard *keyboard,
                       uint32_t            serial,
                       struct wl_surface  *surface)
{
  GdkWaylandDeviceData *device = data;
  GdkEvent *event;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);

  if (!device->keyboard_focus)
    return;

  /* gdk_window_is_destroyed() might already return TRUE for
   * device->keyboard_focus here, which would happen if we destroyed the
   * window before loosing keyboard focus.
   */

  stop_key_repeat (device);

  _gdk_wayland_display_update_serial (display, serial);

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = g_object_ref (device->keyboard_focus);
  event->focus_change.send_event = FALSE;
  event->focus_change.in = FALSE;
  gdk_event_set_device (event, device->master_keyboard);
  gdk_event_set_source_device (event, device->keyboard);

  g_object_unref (device->keyboard_focus);
  device->keyboard_focus = NULL;

  GDK_NOTE (EVENTS,
            g_message ("focus out, device %p surface %p",
                       device, device->keyboard_focus));

  _gdk_wayland_display_deliver_event (device->display, event);
}

static gboolean keyboard_repeat (gpointer data);

static void
translate_keyboard_string (GdkEventKey *event)
{
  gunichar c = 0;
  gchar buf[7];

  /* Fill in event->string crudely, since various programs
   * depend on it.
   */
  event->string = NULL;

  if (event->keyval != GDK_KEY_VoidSymbol)
    c = gdk_keyval_to_unicode (event->keyval);

  if (c)
    {
      gsize bytes_written;
      gint len;

      /* Apply the control key - Taken from Xlib */
      if (event->state & GDK_CONTROL_MASK)
        {
          if ((c >= '@' && c < '\177') || c == ' ')
            c &= 0x1F;
          else if (c == '2')
            {
              event->string = g_memdup ("\0\0", 2);
              event->length = 1;
              buf[0] = '\0';
              return;
            }
          else if (c >= '3' && c <= '7')
            c -= ('3' - '\033');
          else if (c == '8')
            c = '\177';
          else if (c == '/')
            c = '_' & 0x1F;
        }

      len = g_unichar_to_utf8 (c, buf);
      buf[len] = '\0';

      event->string = g_locale_from_utf8 (buf, len,
                                          NULL, &bytes_written,
                                          NULL);
      if (event->string)
        event->length = bytes_written;
    }
  else if (event->keyval == GDK_KEY_Escape)
    {
      event->length = 1;
      event->string = g_strdup ("\033");
    }
  else if (event->keyval == GDK_KEY_Return ||
           event->keyval == GDK_KEY_KP_Enter)
    {
      event->length = 1;
      event->string = g_strdup ("\r");
    }

  if (!event->string)
    {
      event->length = 0;
      event->string = g_strdup ("");
    }
}

static GSettings *
get_keyboard_settings (GdkWaylandDeviceData *device)
{
  if (!device->keyboard_settings)
    {
      GSettingsSchemaSource *source;
      GSettingsSchema *schema;

      source = g_settings_schema_source_get_default ();
      schema = g_settings_schema_source_lookup (source, "org.gnome.settings-daemon.peripherals.keyboard", FALSE);
      if (schema != NULL)
        {
          device->keyboard_settings = g_settings_new_full (schema, NULL, NULL);
          g_settings_schema_unref (schema);
        }
    }

  return device->keyboard_settings;
}

static gboolean
get_key_repeat (GdkWaylandDeviceData *device,
                guint                *delay,
                guint                *interval)
{
  gboolean repeat;

  if (device->have_server_repeat)
    {
      if (device->server_repeat_rate > 0)
        {
          repeat = TRUE;
          *delay = device->server_repeat_delay;
          *interval = (1000 / device->server_repeat_rate);
        }
      else
        {
          repeat = FALSE;
        }
    }
  else
    {
      GSettings *keyboard_settings = get_keyboard_settings (device);

      if (keyboard_settings)
        {
          repeat = g_settings_get_boolean (keyboard_settings, "repeat");
          *delay = g_settings_get_uint (keyboard_settings, "delay");
          *interval = g_settings_get_uint (keyboard_settings, "repeat-interval");
        }
      else
        {
          repeat = TRUE;
          *delay = 400;
          *interval = 80;
        }
    }

  return repeat;
}

static void
stop_key_repeat (GdkWaylandDeviceData *device)
{
  if (device->repeat_timer)
    {
      g_source_remove (device->repeat_timer);
      device->repeat_timer = 0;
    }
}

static gboolean
deliver_key_event (GdkWaylandDeviceData *device,
                   uint32_t              time_,
                   uint32_t              key,
                   uint32_t              state)
{
  GdkEvent *event;
  struct xkb_state *xkb_state;
  struct xkb_keymap *xkb_keymap;
  GdkKeymap *keymap;
  xkb_keysym_t sym;
  guint delay, interval;

  keymap = device->keymap;
  xkb_state = _gdk_wayland_keymap_get_xkb_state (keymap);
  xkb_keymap = _gdk_wayland_keymap_get_xkb_keymap (keymap);

  sym = xkb_state_key_get_one_sym (xkb_state, key);

  device->pointer_info.time = time_;
  device->key_modifiers = gdk_keymap_get_modifier_state (keymap);

  event = gdk_event_new (state ? GDK_KEY_PRESS : GDK_KEY_RELEASE);
  event->key.window = device->keyboard_focus ? g_object_ref (device->keyboard_focus) : NULL;
  gdk_event_set_device (event, device->master_keyboard);
  gdk_event_set_source_device (event, device->keyboard);
  event->key.time = time_;
  event->key.state = device_get_modifiers (device->master_pointer);
  event->key.group = 0;
  event->key.hardware_keycode = key;
  event->key.keyval = sym;
  event->key.is_modifier = _gdk_wayland_keymap_key_is_modifier (keymap, key);

  translate_keyboard_string (&event->key);

  _gdk_wayland_display_deliver_event (device->display, event);

  GDK_NOTE (EVENTS,
            g_message ("keyboard event, code %d, sym %d, "
                       "string %s, mods 0x%x",
                       event->key.hardware_keycode, event->key.keyval,
                       event->key.string, event->key.state));

  if (!xkb_keymap_key_repeats (xkb_keymap, key))
    return FALSE;

  if (!get_key_repeat (device, &delay, &interval))
    return FALSE;

  device->repeat_count++;
  device->repeat_key = key;

  if (state == 0)
    {
      stop_key_repeat (device);
      return FALSE;
    }
  else
    {
      switch (device->repeat_count)
        {
        case 1:
          stop_key_repeat (device);
          device->repeat_timer =
            gdk_threads_add_timeout (delay, keyboard_repeat, device);
          g_source_set_name_by_id (device->repeat_timer, "[gtk+] keyboard_repeat");
          return TRUE;
        case 2:
          device->repeat_timer =
            gdk_threads_add_timeout (interval, keyboard_repeat, device);
          g_source_set_name_by_id (device->repeat_timer, "[gtk+] keyboard_repeat");
          return FALSE;
        default:
          return TRUE;
        }
    }
}

static gboolean
keyboard_repeat (gpointer data)
{
  GdkWaylandDeviceData *device = data;

  return deliver_key_event (device, device->pointer_info.time, device->repeat_key, 1);
}

static void
keyboard_handle_key (void               *data,
                     struct wl_keyboard *keyboard,
                     uint32_t            serial,
                     uint32_t            time,
                     uint32_t            key,
                     uint32_t            state_w)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);

  if (!device->keyboard_focus)
    return;

  device->repeat_count = 0;
  _gdk_wayland_display_update_serial (display, serial);
  deliver_key_event (data, time, key + 8, state_w);
}

static void
keyboard_handle_modifiers (void               *data,
                           struct wl_keyboard *keyboard,
                           uint32_t            serial,
                           uint32_t            mods_depressed,
                           uint32_t            mods_latched,
                           uint32_t            mods_locked,
                           uint32_t            group)
{
  GdkWaylandDeviceData *device = data;
  GdkKeymap *keymap;
  struct xkb_state *xkb_state;
  PangoDirection direction;

  keymap = device->keymap;
  direction = gdk_keymap_get_direction (keymap);
  xkb_state = _gdk_wayland_keymap_get_xkb_state (keymap);
  device->key_modifiers = mods_depressed | mods_latched | mods_locked;

  xkb_state_update_mask (xkb_state, mods_depressed, mods_latched, mods_locked, group, 0, 0);

  g_signal_emit_by_name (keymap, "state-changed");
  if (direction != gdk_keymap_get_direction (keymap))
    g_signal_emit_by_name (keymap, "direction-changed");
}

static void
keyboard_handle_repeat_info (void               *data,
                             struct wl_keyboard *keyboard,
                             int32_t             rate,
                             int32_t             delay)
{
  GdkWaylandDeviceData *device = data;

  device->have_server_repeat = TRUE;
  device->server_repeat_rate = rate;
  device->server_repeat_delay = delay;
}

static GdkWaylandTouchData *
gdk_wayland_device_add_touch (GdkWaylandDeviceData *device,
                              uint32_t              id,
                              struct wl_surface    *surface)
{
  GdkWaylandTouchData *touch;

  touch = g_new0 (GdkWaylandTouchData, 1);
  touch->id = id;
  touch->window = wl_surface_get_user_data (surface);
  touch->initial_touch = (g_hash_table_size (device->touches) == 0);

  g_hash_table_insert (device->touches, GUINT_TO_POINTER (id), touch);

  return touch;
}

static GdkWaylandTouchData *
gdk_wayland_device_get_touch (GdkWaylandDeviceData *device,
                              uint32_t              id)
{
  return g_hash_table_lookup (device->touches, GUINT_TO_POINTER (id));
}

static void
gdk_wayland_device_remove_touch (GdkWaylandDeviceData *device,
                                 uint32_t              id)
{
  g_hash_table_remove (device->touches, GUINT_TO_POINTER (id));
}

static GdkEvent *
_create_touch_event (GdkWaylandDeviceData *device,
                     GdkWaylandTouchData  *touch,
                     GdkEventType          evtype,
                     uint32_t              time)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);
  gint x_root, y_root;
  GdkEvent *event;

  event = gdk_event_new (evtype);
  event->touch.window = g_object_ref (touch->window);
  gdk_event_set_device (event, device->master_pointer);
  gdk_event_set_source_device (event, device->touch);
  event->touch.time = time;
  gdk_event_set_screen (event, display->screen);
  event->touch.sequence = GDK_SLOT_TO_EVENT_SEQUENCE (touch->id);
  event->touch.state = device->key_modifiers;

  if (evtype != GDK_TOUCH_BEGIN)
    event->touch.state |= GDK_BUTTON1_MASK;

  if (touch->initial_touch)
    {
      _gdk_event_set_pointer_emulated (event, TRUE);
      event->touch.emulating_pointer = TRUE;
    }

  gdk_window_get_root_coords (touch->window,
                              touch->x, touch->y,
                              &x_root, &y_root);

  event->touch.x = touch->x;
  event->touch.y = touch->y;
  event->touch.x_root = x_root;
  event->touch.y_root = y_root;

  return event;
}

static void
touch_handle_down (void              *data,
                   struct wl_touch   *wl_touch,
                   uint32_t           serial,
                   uint32_t           time,
                   struct wl_surface *wl_surface,
                   int32_t            id,
                   wl_fixed_t         x,
                   wl_fixed_t         y)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  _gdk_wayland_display_update_serial (display, serial);

  touch = gdk_wayland_device_add_touch (device, id, wl_surface);
  touch->x = wl_fixed_to_double (x);
  touch->y = wl_fixed_to_double (y);
  touch->touch_down_serial = serial;

  event = _create_touch_event (device, touch, GDK_TOUCH_BEGIN, time);

  GDK_NOTE (EVENTS,
            g_message ("touch begin %f %f", event->touch.x, event->touch.y));

  _gdk_wayland_display_deliver_event (device->display, event);
}

static void
touch_handle_up (void            *data,
                 struct wl_touch *wl_touch,
                 uint32_t         serial,
                 uint32_t         time,
                 int32_t          id)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  _gdk_wayland_display_update_serial (display, serial);

  touch = gdk_wayland_device_get_touch (device, id);
  event = _create_touch_event (device, touch, GDK_TOUCH_END, time);

  GDK_NOTE (EVENTS,
            g_message ("touch end %f %f", event->touch.x, event->touch.y));

  _gdk_wayland_display_deliver_event (device->display, event);
  gdk_wayland_device_remove_touch (device, id);
}

static void
touch_handle_motion (void            *data,
                     struct wl_touch *wl_touch,
                     uint32_t         time,
                     int32_t          id,
                     wl_fixed_t       x,
                     wl_fixed_t       y)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  touch = gdk_wayland_device_get_touch (device, id);
  touch->x = wl_fixed_to_double (x);
  touch->y = wl_fixed_to_double (y);

  event = _create_touch_event (device, touch, GDK_TOUCH_UPDATE, time);

  GDK_NOTE (EVENTS,
            g_message ("touch update %f %f", event->touch.x, event->touch.y));

  _gdk_wayland_display_deliver_event (device->display, event);
}

static void
touch_handle_frame (void            *data,
                    struct wl_touch *wl_touch)
{
}

static void
touch_handle_cancel (void            *data,
                     struct wl_touch *wl_touch)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandTouchData *touch;
  GHashTableIter iter;
  GdkEvent *event;

  g_hash_table_iter_init (&iter, device->touches);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &touch))
    {
      event = _create_touch_event (device, touch, GDK_TOUCH_CANCEL,
                                   GDK_CURRENT_TIME);
      _gdk_wayland_display_deliver_event (device->display, event);

      g_hash_table_iter_remove (&iter);
    }

  GDK_NOTE (EVENTS, g_message ("touch cancel"));
}

static void
emit_gesture_swipe_event (GdkWaylandDeviceData    *device,
                          GdkTouchpadGesturePhase  phase,
                          guint32                  _time,
                          guint32                  n_fingers,
                          gdouble                  dx,
                          gdouble                  dy)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);
  GdkEvent *event;

  if (!device->pointer_info.focus)
    return;

  device->pointer_info.time = _time;

  event = gdk_event_new (GDK_TOUCHPAD_SWIPE);
  event->touchpad_swipe.phase = phase;
  event->touchpad_swipe.window = g_object_ref (device->pointer_info.focus);
  gdk_event_set_device (event, device->master_pointer);
  gdk_event_set_source_device (event, device->pointer);
  event->touchpad_swipe.time = _time;
  event->touchpad_swipe.state = device_get_modifiers (device->master_pointer);
  gdk_event_set_screen (event, display->screen);
  event->touchpad_swipe.dx = dx;
  event->touchpad_swipe.dy = dy;
  event->touchpad_swipe.n_fingers = n_fingers;

  get_coordinates (device->master_pointer,
                   &event->touchpad_swipe.x,
                   &event->touchpad_swipe.y,
                   &event->touchpad_swipe.x_root,
                   &event->touchpad_swipe.y_root);

  GDK_NOTE (EVENTS,
            g_message ("swipe event %d, coords: %f %f, device %p state %d",
                       event->type, event->touchpad_swipe.x,
                       event->touchpad_swipe.y, device,
                       event->touchpad_swipe.state));

  _gdk_wayland_display_deliver_event (device->display, event);
}

static void
gesture_swipe_begin (void                             *data,
                     struct _wl_pointer_gesture_swipe *swipe,
                     uint32_t                          serial,
                     uint32_t                          time,
                     struct wl_surface                *surface,
                     uint32_t                          fingers)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);

  _gdk_wayland_display_update_serial (display, serial);

  emit_gesture_swipe_event (device,
                            GDK_TOUCHPAD_GESTURE_PHASE_BEGIN,
                            time, fingers, 0, 0);
  device->gesture_n_fingers = fingers;
}

static void
gesture_swipe_update (void                             *data,
                      struct _wl_pointer_gesture_swipe *swipe,
                      uint32_t                          time,
                      wl_fixed_t                        dx,
                      wl_fixed_t                        dy)
{
  GdkWaylandDeviceData *device = data;

  emit_gesture_swipe_event (device,
                            GDK_TOUCHPAD_GESTURE_PHASE_UPDATE,
                            time,
                            device->gesture_n_fingers,
                            wl_fixed_to_double (dx),
                            wl_fixed_to_double (dy));
}

static void
gesture_swipe_end (void                             *data,
                   struct _wl_pointer_gesture_swipe *swipe,
                   uint32_t                          serial,
                   uint32_t                          time,
                   int32_t                           cancelled)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);
  GdkTouchpadGesturePhase phase;

  _gdk_wayland_display_update_serial (display, serial);

  phase = (cancelled) ?
    GDK_TOUCHPAD_GESTURE_PHASE_CANCEL :
    GDK_TOUCHPAD_GESTURE_PHASE_END;

  emit_gesture_swipe_event (device, phase, time,
                            device->gesture_n_fingers, 0, 0);
}

static void
emit_gesture_pinch_event (GdkWaylandDeviceData    *device,
                          GdkTouchpadGesturePhase  phase,
                          guint32                  _time,
                          guint                    n_fingers,
                          gdouble                  dx,
                          gdouble                  dy,
                          gdouble                  scale,
                          gdouble                  angle_delta)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);
  GdkEvent *event;

  if (!device->pointer_info.focus)
    return;

  device->pointer_info.time = _time;

  event = gdk_event_new (GDK_TOUCHPAD_PINCH);
  event->touchpad_pinch.phase = phase;
  event->touchpad_pinch.window = g_object_ref (device->pointer_info.focus);
  gdk_event_set_device (event, device->master_pointer);
  gdk_event_set_source_device (event, device->pointer);
  event->touchpad_pinch.time = _time;
  event->touchpad_pinch.state = device_get_modifiers (device->master_pointer);
  gdk_event_set_screen (event, display->screen);
  event->touchpad_pinch.dx = dx;
  event->touchpad_pinch.dy = dy;
  event->touchpad_pinch.scale = scale;
  event->touchpad_pinch.angle_delta = angle_delta * G_PI / 180;
  event->touchpad_pinch.n_fingers = n_fingers;

  get_coordinates (device->master_pointer,
                   &event->touchpad_pinch.x,
                   &event->touchpad_pinch.y,
                   &event->touchpad_pinch.x_root,
                   &event->touchpad_pinch.y_root);

  GDK_NOTE (EVENTS,
            g_message ("pinch event %d, coords: %f %f, device %p state %d",
                       event->type, event->touchpad_pinch.x,
                       event->touchpad_pinch.y, device,
                       event->touchpad_pinch.state));

  _gdk_wayland_display_deliver_event (device->display, event);
}

static void
gesture_pinch_begin (void                             *data,
                     struct _wl_pointer_gesture_pinch *pinch,
                     uint32_t                          serial,
                     uint32_t                          time,
                     struct wl_surface                *surface,
                     uint32_t                          fingers)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);

  _gdk_wayland_display_update_serial (display, serial);
  emit_gesture_pinch_event (device,
                            GDK_TOUCHPAD_GESTURE_PHASE_BEGIN,
                            time, fingers, 0, 0, 1, 0);
  device->gesture_n_fingers = fingers;
}

static void
gesture_pinch_update (void                             *data,
                      struct _wl_pointer_gesture_pinch *pinch,
                      uint32_t                          time,
                      wl_fixed_t                        dx,
                      wl_fixed_t                        dy,
                      wl_fixed_t                        scale,
                      wl_fixed_t                        rotation)
{
  GdkWaylandDeviceData *device = data;

  emit_gesture_pinch_event (device,
                            GDK_TOUCHPAD_GESTURE_PHASE_UPDATE, time,
                            device->gesture_n_fingers,
                            wl_fixed_to_double (dx),
                            wl_fixed_to_double (dy),
                            wl_fixed_to_double (scale),
                            wl_fixed_to_double (rotation));
}

static void
gesture_pinch_end (void                             *data,
                   struct _wl_pointer_gesture_pinch *pinch,
                   uint32_t                          serial,
                   uint32_t                          time,
                   int32_t                           cancelled)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);
  GdkTouchpadGesturePhase phase;

  _gdk_wayland_display_update_serial (display, serial);

  phase = (cancelled) ?
    GDK_TOUCHPAD_GESTURE_PHASE_CANCEL :
    GDK_TOUCHPAD_GESTURE_PHASE_END;

  emit_gesture_pinch_event (device, phase,
                            time, device->gesture_n_fingers,
                            0, 0, 1, 0);
}

static GdkDevice *
tablet_select_device_for_tool (GdkWaylandDeviceTabletPair *device_pair,
                               GdkDeviceTool              *tool)
{
  GdkDevice *device;

  if (gdk_device_tool_get_tool_type (tool) == GDK_DEVICE_TOOL_TYPE_ERASER)
    device = device_pair->eraser_device;
  else
    device = device_pair->stylus_device;

  return device;
}

static void
_gdk_wayland_device_manager_remove_tablet (GdkWaylandDeviceTabletPair *device_pair)
{
  GdkWaylandDeviceManager *device_manager =
    GDK_WAYLAND_DEVICE_MANAGER (device_pair->wd->device_manager);

  wl_tablet_destroy (device_pair->wl_tablet);

  device_manager->devices =
    g_list_remove (device_manager->devices, device_pair->master);
  device_manager->devices =
    g_list_remove (device_manager->devices, device_pair->stylus_device);
  device_manager->devices =
    g_list_remove (device_manager->devices, device_pair->eraser_device);

  g_signal_emit_by_name (device_manager, "device-removed",
                         device_pair->stylus_device);
  g_signal_emit_by_name (device_manager, "device-removed",
                         device_pair->eraser_device);
  g_signal_emit_by_name (device_manager, "device-removed",
                         device_pair->master);

  _gdk_device_set_associated_device (device_pair->master, NULL);
  _gdk_device_set_associated_device (device_pair->stylus_device, NULL);
  _gdk_device_set_associated_device (device_pair->eraser_device, NULL);

  if (device_pair->pointer_info.focus)
    g_object_unref (device_pair->pointer_info.focus);

  wl_surface_destroy (device_pair->pointer_info.pointer_surface);
  g_object_unref (device_pair->master);
  g_object_unref (device_pair->stylus_device);
  g_object_unref (device_pair->eraser_device);
  g_free (device_pair);

  if (device_pair->axes)
    g_free (device_pair->axes);

  device_manager->tablet_pairs =
    g_list_remove (device_manager->tablet_pairs, device_pair);
}

static void
gdk_wayland_device_tablet_clone_tool_axes (GdkWaylandDeviceTabletPair *tablet,
                                           GdkDeviceTool              *tool)
{
  gint axis_pos;

  g_object_freeze_notify (G_OBJECT (tablet->current_device));
  _gdk_device_reset_axes (tablet->current_device);

  _gdk_device_add_axis (tablet->current_device, GDK_NONE, GDK_AXIS_X, 0, 0, 0);
  _gdk_device_add_axis (tablet->current_device, GDK_NONE, GDK_AXIS_Y, 0, 0, 0);

  if (tool->tool_axes & (GDK_AXIS_FLAG_XTILT | GDK_AXIS_FLAG_YTILT))
    {
      axis_pos = _gdk_device_add_axis (tablet->current_device, GDK_NONE,
                                       GDK_AXIS_XTILT, -65535, 65535, 0);
      tablet->xtilt_axis_index = axis_pos;

      axis_pos = _gdk_device_add_axis (tablet->current_device, GDK_NONE,
                                       GDK_AXIS_YTILT, -65535, 65535, 0);
      tablet->ytilt_axis_index = axis_pos;
    }
  if (tool->tool_axes & GDK_AXIS_FLAG_DISTANCE)
    {
      axis_pos = _gdk_device_add_axis (tablet->current_device, GDK_NONE,
                                       GDK_AXIS_DISTANCE, 0, 65535, 0);
      tablet->distance_axis_index = axis_pos;
    }
  if (tool->tool_axes & GDK_AXIS_FLAG_PRESSURE)
    {
      axis_pos = _gdk_device_add_axis (tablet->current_device, GDK_NONE,
                                       GDK_AXIS_PRESSURE, 0, 65535, 0);
      tablet->pressure_axis_index = axis_pos;
    }

  if (tablet->axes)
    g_free(tablet->axes);

  tablet->axes =
    g_new0 (gdouble, gdk_device_get_n_axes (tablet->current_device));

  g_object_thaw_notify (G_OBJECT (tablet->current_device));
}

static void
gdk_wayland_mimic_device_axes (GdkDevice *master,
                               GdkDevice *slave)
{
  gdouble axis_min, axis_max, axis_resolution;
  GdkAtom axis_label;
  GdkAxisUse axis_use;
  gint axis_count;
  gint i;

  g_object_freeze_notify (G_OBJECT (master));
  _gdk_device_reset_axes (master);
  axis_count = gdk_device_get_n_axes (slave);

  for (i = 0; i < axis_count; i++)
    {
      _gdk_device_get_axis_info (slave, i, &axis_label, &axis_use, &axis_min,
                                 &axis_max, &axis_resolution);
      _gdk_device_add_axis (master, axis_label, axis_use, axis_min,
                            axis_max, axis_resolution);
    }

  g_object_thaw_notify (G_OBJECT (master));
}

static void
tablet_handle_name (void             *data,
                    struct wl_tablet *wl_tablet,
                    const char *name)
{
  GdkWaylandTabletData *tablet_data = data;

  tablet_data->name = g_strdup(name);
}

static void
tablet_handle_id (void             *data,
                  struct wl_tablet *wl_tablet,
                  uint32_t          vid,
                  uint32_t          pid)
{
  GdkWaylandTabletData *tablet_data = data;

  tablet_data->vid = vid;
  tablet_data->pid = pid;
}

static void
tablet_handle_type (void             *data,
                    struct wl_tablet *wl_tablet,
                    uint32_t         type)
{
  GdkWaylandTabletData *tablet_data = data;

  tablet_data->type = type;
}

static void
tablet_handle_path (void             *data,
                    struct wl_tablet *wl_tablet,
                    const char       *path)
{
  GdkWaylandTabletData *tablet_data = data;

  tablet_data->path = g_strdup(path);
}

static void
tablet_handle_done (void             *data,
                    struct wl_tablet *wl_tablet)
{
  GdkWaylandTabletData *tablet_data = data;
  GdkWaylandDeviceData *device = wl_tablet_get_user_data (wl_tablet);
  GdkWaylandDeviceManager *device_manager =
    GDK_WAYLAND_DEVICE_MANAGER (device->device_manager);
  GdkWaylandDisplay *wayland_display =
    GDK_WAYLAND_DISPLAY (device->device_manager->display);
  GdkWaylandDeviceTabletPair *device_pair;
  GdkDevice *master, *stylus_device, *eraser_device;
  gchar *master_name, *eraser_name;
  const gchar *name = tablet_data->name;

  device_pair = g_new0 (GdkWaylandDeviceTabletPair, 1);
  device_pair->wd = device;
  device_pair->pointer_info.current_output_scale = 1;
  device_pair->pointer_info.pointer_surface =
    wl_compositor_create_surface (wayland_display->compositor);
  device_pair->wl_tablet = wl_tablet;

  master_name = g_strdup_printf ("Master pointer for %s", name);
  master = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                         "name", master_name,
                         "type", GDK_DEVICE_TYPE_MASTER,
                         "input-source", GDK_SOURCE_MOUSE,
                         "input-mode", GDK_MODE_SCREEN,
                         "has-cursor", TRUE,
                         "display", device->display,
                         "device-manager", device->device_manager,
                         NULL);
  GDK_WAYLAND_DEVICE (master)->device = device;
  GDK_WAYLAND_DEVICE (master)->pointer = &device_pair->pointer_info;

  eraser_name = g_strconcat (name, " (Eraser)", NULL);

  stylus_device = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                "name", name,
                                "type", GDK_DEVICE_TYPE_SLAVE,
                                "input-source", GDK_SOURCE_PEN,
                                "input-mode", GDK_MODE_SCREEN,
                                "has-cursor", FALSE,
                                "display", device->display,
                                "device-manager", device->device_manager,
                                NULL);
  GDK_WAYLAND_DEVICE (stylus_device)->device = device;

  eraser_device = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                "name", eraser_name,
                                "type", GDK_DEVICE_TYPE_SLAVE,
                                "input-source", GDK_SOURCE_ERASER,
                                "input-mode", GDK_MODE_SCREEN,
                                "has-cursor", FALSE,
                                "display", device->display,
                                "device-manager", device->device_manager,
                                NULL);
  GDK_WAYLAND_DEVICE (eraser_device)->device = device;

  device_pair->master = master;
  device_manager->devices =
    g_list_prepend (device_manager->devices, device_pair->master);
  g_signal_emit_by_name (device_manager, "device-added", master);

  device_pair->stylus_device = stylus_device;
  device_manager->devices =
    g_list_prepend (device_manager->devices, device_pair->stylus_device);
  g_signal_emit_by_name (device_manager, "device-added", stylus_device);

  device_pair->eraser_device = eraser_device;
  device_manager->devices =
    g_list_prepend (device_manager->devices, device_pair->eraser_device);
  g_signal_emit_by_name (device_manager, "device-added", eraser_device);

  _gdk_device_set_associated_device (master, device->master_keyboard);
  _gdk_device_set_associated_device (stylus_device, master);
  _gdk_device_set_associated_device (eraser_device, master);

  device_manager->tablet_pairs =
    g_list_prepend (device_manager->tablet_pairs, device_pair);

  g_free (eraser_name);
  g_free (master_name);

  g_free (tablet_data->path);
  g_free (tablet_data->name);
  g_free (tablet_data);
}

static void
tablet_handle_removed (void             *data,
                       struct wl_tablet *wl_tablet)
{
  GdkWaylandDeviceTabletPair *device_pair = data;

  _gdk_wayland_device_manager_remove_tablet (device_pair);
}

static const struct wl_pointer_listener pointer_listener = {
  pointer_handle_enter,
  pointer_handle_leave,
  pointer_handle_motion,
  pointer_handle_button,
  pointer_handle_axis,
};

static const struct wl_keyboard_listener keyboard_listener = {
  keyboard_handle_keymap,
  keyboard_handle_enter,
  keyboard_handle_leave,
  keyboard_handle_key,
  keyboard_handle_modifiers,
  keyboard_handle_repeat_info,
};

static const struct wl_touch_listener touch_listener = {
  touch_handle_down,
  touch_handle_up,
  touch_handle_motion,
  touch_handle_frame,
  touch_handle_cancel
};

static const struct _wl_pointer_gesture_swipe_listener gesture_swipe_listener = {
  gesture_swipe_begin,
  gesture_swipe_update,
  gesture_swipe_end
};

static const struct _wl_pointer_gesture_pinch_listener gesture_pinch_listener = {
  gesture_pinch_begin,
  gesture_pinch_update,
  gesture_pinch_end
};

static const struct wl_tablet_listener tablet_listener = {
  tablet_handle_name,
  tablet_handle_id,
  tablet_handle_type,
  tablet_handle_path,
  tablet_handle_done,
  tablet_handle_removed,
};

static void
seat_handle_capabilities (void                    *data,
                          struct wl_seat          *seat,
                          enum wl_seat_capability  caps)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandDeviceManager *device_manager = GDK_WAYLAND_DEVICE_MANAGER (device->device_manager);
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (device->display);

  GDK_NOTE (MISC,
            g_message ("seat %p with %s%s%s", seat,
                       (caps & WL_SEAT_CAPABILITY_POINTER) ? " pointer, " : "",
                       (caps & WL_SEAT_CAPABILITY_KEYBOARD) ? " keyboard, " : "",
                       (caps & WL_SEAT_CAPABILITY_TOUCH) ? " touch" : ""));

  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !device->wl_pointer)
    {
      device->wl_pointer = wl_seat_get_pointer (seat);
      wl_pointer_set_user_data (device->wl_pointer, device);
      wl_pointer_add_listener (device->wl_pointer, &pointer_listener, device);

      device->pointer = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                      "name", "Wayland Pointer",
                                      "type", GDK_DEVICE_TYPE_SLAVE,
                                      "input-source", GDK_SOURCE_MOUSE,
                                      "input-mode", GDK_MODE_SCREEN,
                                      "has-cursor", TRUE,
                                      "display", device->display,
                                      "device-manager", device->device_manager,
                                      NULL);
      _gdk_device_set_associated_device (device->pointer, device->master_pointer);
      GDK_WAYLAND_DEVICE (device->pointer)->device = device;

      device_manager->devices =
        g_list_prepend (device_manager->devices, device->pointer);

      device->drop_context = _gdk_wayland_drop_context_new (device->master_pointer,
                                                            device->data_device);
      if (wayland_display->pointer_gestures)
        {
          device->wl_pointer_gesture_swipe =
            _wl_pointer_gestures_get_swipe_gesture (wayland_display->pointer_gestures,
                                                    device->wl_pointer);
          _wl_pointer_gesture_swipe_set_user_data (device->wl_pointer_gesture_swipe,
                                                   device);
          _wl_pointer_gesture_swipe_add_listener (device->wl_pointer_gesture_swipe,
                                                  &gesture_swipe_listener, device);

          device->wl_pointer_gesture_pinch =
            _wl_pointer_gestures_get_pinch_gesture (wayland_display->pointer_gestures,
                                                    device->wl_pointer);
          _wl_pointer_gesture_pinch_set_user_data (device->wl_pointer_gesture_pinch,
                                                   device);
          _wl_pointer_gesture_pinch_add_listener (device->wl_pointer_gesture_pinch,
                                                  &gesture_pinch_listener, device);
        }

      g_signal_emit_by_name (device_manager, "device-added", device->pointer);
    }
  else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && device->wl_pointer)
    {
      wl_pointer_release (device->wl_pointer);
      device->wl_pointer = NULL;
      _gdk_device_set_associated_device (device->pointer, NULL);

      device_manager->devices =
        g_list_remove (device_manager->devices, device->pointer);

      g_clear_object (&device->drop_context);

      g_signal_emit_by_name (device_manager, "device-removed", device->pointer);
      g_object_unref (device->pointer);
      device->pointer = NULL;
    }

  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !device->wl_keyboard)
    {
      device->wl_keyboard = wl_seat_get_keyboard (seat);
      wl_keyboard_set_user_data (device->wl_keyboard, device);
      wl_keyboard_add_listener (device->wl_keyboard, &keyboard_listener, device);

      device->keyboard = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                       "name", "Wayland Keyboard",
                                       "type", GDK_DEVICE_TYPE_SLAVE,
                                       "input-source", GDK_SOURCE_KEYBOARD,
                                       "input-mode", GDK_MODE_SCREEN,
                                       "has-cursor", FALSE,
                                       "display", device->display,
                                       "device-manager", device->device_manager,
                                       NULL);
      _gdk_device_set_associated_device (device->keyboard, device->master_keyboard);
      GDK_WAYLAND_DEVICE (device->keyboard)->device = device;

      device_manager->devices =
        g_list_prepend (device_manager->devices, device->keyboard);

      g_signal_emit_by_name (device_manager, "device-added", device->keyboard);
    }
  else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && device->wl_keyboard)
    {
      wl_keyboard_release (device->wl_keyboard);
      device->wl_keyboard = NULL;
      _gdk_device_set_associated_device (device->keyboard, NULL);

      device_manager->devices =
        g_list_remove (device_manager->devices, device->keyboard);

      g_signal_emit_by_name (device_manager, "device-removed", device->keyboard);
      g_object_unref (device->keyboard);
      device->keyboard = NULL;
    }

  if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !device->wl_touch)
    {
      device->wl_touch = wl_seat_get_touch (seat);
      wl_touch_set_user_data (device->wl_touch, device);
      wl_touch_add_listener (device->wl_touch, &touch_listener, device);

      device->touch = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                    "name", "Wayland Touch",
                                    "type", GDK_DEVICE_TYPE_SLAVE,
                                    "input-source", GDK_SOURCE_TOUCHSCREEN,
                                    "input-mode", GDK_MODE_SCREEN,
                                    "has-cursor", FALSE,
                                    "display", device->display,
                                    "device-manager", device->device_manager,
                                    NULL);
      _gdk_device_set_associated_device (device->touch, device->master_pointer);
      GDK_WAYLAND_DEVICE (device->touch)->device = device;

      device_manager->devices =
        g_list_prepend (device_manager->devices, device->touch);

      g_signal_emit_by_name (device_manager, "device-added", device->touch);
    }
  else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && device->wl_touch)
    {
      wl_touch_release (device->wl_touch);
      device->wl_touch = NULL;
      _gdk_device_set_associated_device (device->touch, NULL);

      device_manager->devices =
        g_list_remove (device_manager->devices, device->touch);

      g_signal_emit_by_name (device_manager, "device-removed", device->touch);
      g_object_unref (device->touch);
      device->touch = NULL;
    }
}

static void
seat_handle_name (void           *data,
                  struct wl_seat *seat,
                  const char     *name)
{
  /* We don't care about the name. */
  GDK_NOTE (MISC,
            g_message ("seat %p name %s", seat, name));
}

static const struct wl_seat_listener seat_listener = {
  seat_handle_capabilities,
  seat_handle_name,
};

static inline GdkDeviceToolType
wl_tool_type_to_gdk_tool_type (enum wl_tablet_tool_type wl_tool_type)
{
  GdkDeviceToolType tool_type;

  switch (wl_tool_type)
    {
    case WL_TABLET_TOOL_TYPE_PEN:
      tool_type = GDK_DEVICE_TOOL_TYPE_PEN;
      break;
    case WL_TABLET_TOOL_TYPE_BRUSH:
      tool_type = GDK_DEVICE_TOOL_TYPE_BRUSH;
      break;
    case WL_TABLET_TOOL_TYPE_AIRBRUSH:
      tool_type = GDK_DEVICE_TOOL_TYPE_AIRBRUSH;
      break;
    case WL_TABLET_TOOL_TYPE_PENCIL:
      tool_type = GDK_DEVICE_TOOL_TYPE_PENCIL;
      break;
    case WL_TABLET_TOOL_TYPE_ERASER:
      tool_type = GDK_DEVICE_TOOL_TYPE_ERASER;
      break;
    default:
      tool_type = GDK_DEVICE_TOOL_TYPE_UNKNOWN;
      break;
    };

  return tool_type;
}

static void
tablet_tool_handle_type (void                  *data,
                         struct wl_tablet_tool *wl_tool,
                         uint32_t               tool_type)
{
  GdkWaylandTabletToolData *tool = data;

  tool->type = wl_tool_type_to_gdk_tool_type (tool_type);
}

static void
tablet_tool_handle_serial_id (void                  *data,
                              struct wl_tablet_tool *wl_tool,
                              uint32_t               serial_id_msb,
                              uint32_t               serial_id_lsb)
{
  GdkWaylandTabletToolData *tool = data;

  tool->serial = ((uint64_t)serial_id_msb << 32) | serial_id_lsb;
}

static void
tablet_tool_handle_hardware_id (void                  *data,
                                struct wl_tablet_tool *wl_tool,
                                uint32_t               format,
                                uint32_t               hardware_id_msb,
                                uint32_t               hardware_id_lsb)
{
  GdkWaylandTabletToolData *tool = data;

  tool->hw_id = ((uint64_t)hardware_id_msb << 32) | hardware_id_lsb;
}

GdkAxisFlags
wl_tablet_tool_axis_cap_to_gdk_axes (uint32_t tool_axis_cap)
{
  switch (tool_axis_cap)
    {
    case WL_TABLET_TOOL_CAPABILITY_TILT:
      return GDK_AXIS_FLAG_XTILT | GDK_AXIS_FLAG_YTILT;
    case WL_TABLET_TOOL_CAPABILITY_DISTANCE:
      return GDK_AXIS_FLAG_DISTANCE;
    case WL_TABLET_TOOL_CAPABILITY_PRESSURE:
      return GDK_AXIS_FLAG_PRESSURE;
    }

  return 0;
}

static void
tablet_tool_handle_capability (void                  *data,
                               struct wl_tablet_tool *wl_tool,
                               uint32_t               capability)
{
  GdkWaylandTabletToolData *tool = data;

  tool->axis_flags |= wl_tablet_tool_axis_cap_to_gdk_axes(capability);
}

static void
tablet_tool_handle_done (void                  *data,
                         struct wl_tablet_tool *wl_tool)
{
  GdkWaylandTabletToolData *tool_data = data;
  GdkDeviceTool *tool;

  tool = gdk_device_tool_new (tool_data->serial, tool_data->type,
                              tool_data->axis_flags);
  g_free (tool_data);

  /* FIXME:

     We can't guarantee that a tablet exists when a tool is added, and if
     more than one exists we don't know which one this one belongs to until
     the proximity_in event. Previous code, missing here is:

     GdkWaylandDeviceTabletPair *device_pair = wl_tablet_get_user_data (wl_tablet);
     ...
     gdk_device_add_tool (tablet_select_device_for_tool (device_pair, tool),
                       tool);
     */

}

static void
tablet_tool_handle_removed (void                  *data,
                            struct wl_tablet_tool *wl_tool)
{
  GdkDeviceTool *tool = data;

  gdk_device_tool_unref (tool);
}

static void
tablet_tool_handle_proximity_in (void                  *data,
                                 struct wl_tablet_tool *wl_tablet_tool,
                                 uint32_t               serial,
                                 uint32_t               time,
                                 struct wl_tablet      *wl_tablet,
                                 struct wl_surface     *surface)
{
  GdkWaylandDeviceTabletPair *device_pair = data;
  GdkWaylandDeviceData *device = device_pair->wd;
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (device->display);
  GdkDeviceTool *tool =
    wl_tablet_tool_get_user_data (wl_tablet_tool);
  GdkWindow *window = wl_surface_get_user_data (surface);
  GdkEvent *event;

  if (!surface)
      return;
  if (!GDK_IS_WINDOW (window))
      return;

  _gdk_wayland_display_update_serial (wayland_display, serial);
  device_pair->pointer_info.enter_serial = serial;

  device_pair->pointer_info.focus = g_object_ref (window);
  device_pair->current_device =
    tablet_select_device_for_tool (device_pair, tool);

  /* Add the tool to the device if we haven't already done so */
  if (serial != 0 &&
      !gdk_device_lookup_tool (device_pair->current_device, serial))
    gdk_device_add_tool (device_pair->current_device, tool);

  gdk_device_update_tool (device_pair->current_device, tool);
  gdk_wayland_device_tablet_clone_tool_axes (device_pair, tool);
  gdk_wayland_mimic_device_axes (device_pair->master, device_pair->current_device);

  event = gdk_event_new (GDK_PROXIMITY_IN);
  event->proximity.type = GDK_PROXIMITY_IN;
  event->proximity.window = g_object_ref (device_pair->pointer_info.focus);
  gdk_event_set_device (event, device_pair->master);
  gdk_event_set_source_device (event, device_pair->current_device);
  event->proximity.time = time;

  GDK_NOTE (EVENTS,
            g_message ("proximity in, tool %d",
                       gdk_device_tool_get_tool_type (tool)));

  _gdk_wayland_display_deliver_event (device->display, event);

  gdk_wayland_device_update_window_cursor (device_pair->master);

  event = gdk_event_new (GDK_ENTER_NOTIFY);
  event->crossing.window = g_object_ref (device_pair->pointer_info.focus);
  gdk_event_set_device (event, device_pair->master);
  gdk_event_set_source_device (event, device_pair->current_device);
  event->crossing.subwindow = NULL;
  event->crossing.time = time;
  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = GDK_NOTIFY_ANCESTOR;
  event->crossing.focus = TRUE;
  event->crossing.state = device_get_modifiers (device->master_pointer);

  get_coordinates (device_pair->master,
                   &event->crossing.x,
                   &event->crossing.y,
                   &event->crossing.x_root,
                   &event->crossing.y_root);

  GDK_NOTE (EVENTS,
            g_message ("enter, device %p surface %p",
                       device, device_pair->pointer_info.focus));

  _gdk_wayland_display_deliver_event (device->display, event);
}

static void
tablet_tool_handle_proximity_out (void                  *data,
                                  struct wl_tablet_tool *wl_tablet_tool,
                                  uint32_t               time)
{
  GdkWaylandDeviceTabletPair *device_pair = data;
  GdkWaylandDeviceData *device = device_pair->wd;
  GdkEvent *event;

  event = gdk_event_new (GDK_LEAVE_NOTIFY);
  event->crossing.window = g_object_ref (device_pair->pointer_info.focus);
  gdk_event_set_device (event, device_pair->master);
  gdk_event_set_source_device (event, device_pair->current_device);
  event->crossing.subwindow = NULL;
  event->crossing.time = time;
  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = GDK_NOTIFY_ANCESTOR;
  event->crossing.focus = TRUE;
  event->crossing.state = device_get_modifiers (device->master_pointer);

  get_coordinates (device_pair->master,
                   &event->crossing.x,
                   &event->crossing.y,
                   &event->crossing.x_root,
                   &event->crossing.y_root);

  GDK_NOTE (EVENTS,
            g_message ("leave, device %p surface %p",
                       device, device->pointer));

  _gdk_wayland_display_deliver_event (device->display, event);

  event = gdk_event_new (GDK_PROXIMITY_OUT);
  event->proximity.window = g_object_ref (device_pair->pointer_info.focus);
  gdk_event_set_device (event, device_pair->master);
  gdk_event_set_source_device (event, device_pair->current_device);
  event->proximity.time = time;

  GDK_NOTE (EVENTS,
            g_message ("proximity out"));

  _gdk_wayland_display_deliver_event (device->display, event);

  if (device->cursor)
    gdk_wayland_pointer_stop_cursor_animation (&device_pair->pointer_info);

  gdk_wayland_device_update_window_cursor (device_pair->master);
  g_object_unref (device_pair->pointer_info.focus);
  device_pair->pointer_info.focus = NULL;
}

static void
tablet_tool_handle_motion (void                  *data,
                           struct wl_tablet_tool *wl_tablet_tool,
                           uint32_t               time,
                           wl_fixed_t             sx,
                           wl_fixed_t             sy)
{
  GdkWaylandDeviceTabletPair *device_pair = data;

  device_pair->pointer_info.time = time;
  device_pair->pointer_info.surface_x = wl_fixed_to_double (sx);
  device_pair->pointer_info.surface_y = wl_fixed_to_double (sy);
}

static void
tablet_notify_button_event (GdkWaylandDeviceTabletPair *tablet,
                            GdkEventType                evtype,
                            guint                       button)
{
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (tablet->wd->display);
  GdkEvent *event;

  event = gdk_event_new (evtype);
  event->button.window = g_object_ref (tablet->pointer_info.focus);
  gdk_event_set_device (event, tablet->master);
  gdk_event_set_source_device (event, tablet->current_device);
  event->button.time = tablet->pointer_info.time;
  event->button.axes =
    g_memdup (tablet->axes,
              sizeof (gdouble) *
              gdk_device_get_n_axes (tablet->current_device));
  event->button.state = device_get_modifiers (tablet->master);
  event->button.button = button;
  gdk_event_set_screen (event, wayland_display->screen);

  get_coordinates (tablet->master,
                   &event->button.x,
                   &event->button.y,
                   &event->button.x_root,
                   &event->button.y_root);

  GDK_NOTE (EVENTS,
            g_message ("button %d %s, state %d",
                       event->button.button,
                       evtype == GDK_BUTTON_PRESS ? "press" : "release",
                       event->button.state));

  _gdk_wayland_display_deliver_event (tablet->wd->display, event);
}

static void
tablet_tool_handle_down (void                  *data,
                         struct wl_tablet_tool *wl_tablet_tool,
                         uint32_t               serial,
                         uint32_t               time)
{
  GdkWaylandDeviceTabletPair *device_pair = data;
  GdkWaylandDeviceData *device = device_pair->wd;
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (device->display);

  if (!device_pair->pointer_info.focus)
    return;

  _gdk_wayland_display_update_serial (wayland_display, serial);

  device_pair->pointer_info.time = time;
  device_pair->pointer_info.press_serial = serial;

  tablet_notify_button_event (device_pair, GDK_BUTTON_PRESS, GDK_BUTTON_PRIMARY);

  device_pair->pointer_info.button_modifiers |= GDK_BUTTON1_MASK;
}

static void
tablet_tool_handle_up (void                  *data,
                       struct wl_tablet_tool *wl_tablet_tool,
                       uint32_t               serial)
{
  GdkWaylandDeviceTabletPair *device_pair = data;
  GdkWaylandDeviceData *device = device_pair->wd;
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (device->display);
  guint32 time;

  if (!device_pair->pointer_info.focus)
    return;

  _gdk_wayland_display_update_serial (wayland_display, serial);

  time = (guint32) (g_get_monotonic_time () / 1000);

  device_pair->pointer_info.time = time;
  device_pair->pointer_info.press_serial = serial;

  tablet_notify_button_event (device_pair, GDK_BUTTON_RELEASE, GDK_BUTTON_PRIMARY);

  device_pair->pointer_info.button_modifiers &= ~GDK_BUTTON1_MASK;
}

static void
tablet_tool_handle_pressure (void                  *data,
                             struct wl_tablet_tool *wl_tablet_tool,
                             uint32_t               time,
                             uint32_t               pressure)
{
  GdkWaylandDeviceTabletPair *device_pair = data;
  gint axis_index = device_pair->pressure_axis_index;

  device_pair->pointer_info.time = time;
  _gdk_device_translate_axis (device_pair->current_device, axis_index,
                              wl_fixed_to_double (pressure),
                              &device_pair->axes[axis_index]);
}

static void
tablet_tool_handle_distance (void                  *data,
                             struct wl_tablet_tool *wl_tablet_tool,
                             uint32_t               time,
                             uint32_t               distance)
{
  GdkWaylandDeviceTabletPair *device_pair = data;
  gint axis_index = device_pair->distance_axis_index;

  device_pair->pointer_info.time = time;
  _gdk_device_translate_axis (device_pair->current_device, axis_index,
                              wl_fixed_to_double (distance),
                              &device_pair->axes[axis_index]);
}

static void
tablet_tool_handle_tilt (void                  *data,
                         struct wl_tablet_tool *wl_tablet_tool,
                         uint32_t               time,
                         int32_t                x,
                         int32_t                y)
{
  GdkWaylandDeviceTabletPair *device_pair = data;
  gint xtilt_axis_index = device_pair->xtilt_axis_index;
  gint ytilt_axis_index = device_pair->ytilt_axis_index;

  device_pair->pointer_info.time = time;
  _gdk_device_translate_axis (device_pair->current_device, xtilt_axis_index,
                              wl_fixed_to_double (x),
                              &device_pair->axes[xtilt_axis_index]);
  _gdk_device_translate_axis (device_pair->current_device, ytilt_axis_index,
                              wl_fixed_to_double (y),
                              &device_pair->axes[ytilt_axis_index]);
}

static void
tablet_tool_handle_button (void                  *data,
                           struct wl_tablet_tool *wl_tablet_tool,
                           uint32_t               serial,
                           uint32_t               time,
                           uint32_t               button,
                           uint32_t               state)
{
  GdkWaylandDeviceTabletPair *device_pair = data;
  GdkEventType evtype;
  guint n_button;

  if (!device_pair->pointer_info.focus)
    return;

  device_pair->pointer_info.time = time;
  device_pair->pointer_info.press_serial = serial;

  if (button == BTN_STYLUS)
    n_button = GDK_BUTTON_SECONDARY;
  else if (button == BTN_STYLUS2)
    n_button = GDK_BUTTON_MIDDLE;
  else
    return;

  if (state == WL_TABLET_TOOL_BUTTON_STATE_PRESSED)
    evtype = GDK_BUTTON_PRESS;
  else if (state == WL_TABLET_TOOL_BUTTON_STATE_RELEASED)
    evtype = GDK_BUTTON_RELEASE;
  else
    return;

  tablet_notify_button_event (device_pair, evtype, n_button);
}

static void
tablet_tool_handle_frame (void                  *data,
                          struct wl_tablet_tool *wl_tablet_tool)
{
  GdkWaylandDeviceTabletPair *device_pair = data;
  GdkWaylandDeviceData *device = device_pair->wd;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);
  GdkEvent *event;

  if (!device_pair->pointer_info.focus)
    return;

  event = gdk_event_new (GDK_MOTION_NOTIFY);
  event->motion.window = g_object_ref (device_pair->pointer_info.focus);
  gdk_event_set_device (event, device_pair->master);
  gdk_event_set_source_device (event, device_pair->current_device);
  event->motion.time = device_pair->pointer_info.time;
  event->motion.axes =
    g_memdup (device_pair->axes,
              sizeof (gdouble) *
              gdk_device_get_n_axes (device_pair->current_device));
  event->motion.state = device_get_modifiers (device_pair->master);
  event->motion.is_hint = FALSE;
  gdk_event_set_screen (event, display->screen);

  get_coordinates (device_pair->master,
                   &event->motion.x,
                   &event->motion.y,
                   &event->motion.x_root,
                   &event->motion.y_root);

  GDK_NOTE (EVENTS,
            g_message ("tablet motion %lf %lf, state %d",
                       device_pair->pointer_info.surface_x,
                       device_pair->pointer_info.surface_y,
                       event->button.state));

  _gdk_wayland_display_deliver_event (device->display, event);
}

static const struct wl_tablet_tool_listener tablet_tool_listener = {
  tablet_tool_handle_type,
  tablet_tool_handle_serial_id,
  tablet_tool_handle_hardware_id,
  tablet_tool_handle_capability,
  tablet_tool_handle_done,
  tablet_tool_handle_removed,
  tablet_tool_handle_proximity_in,
  tablet_tool_handle_proximity_out,
  tablet_tool_handle_down,
  tablet_tool_handle_up,
  tablet_tool_handle_motion,
  tablet_tool_handle_pressure,
  tablet_tool_handle_distance,
  tablet_tool_handle_tilt,
  tablet_tool_handle_button,
  tablet_tool_handle_frame,
};

static void
tablet_seat_handle_tablet_added (void                     *data,
                                 struct wl_tablet_seat    *wl_tablet_seat,
                                 struct wl_tablet         *wl_tablet)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandTabletData *tablet_data;

  tablet_data = g_new0(GdkWaylandTabletData, 1);

  wl_tablet_set_user_data (wl_tablet, device);
  wl_tablet_add_listener (wl_tablet, &tablet_listener, tablet_data);
}

static void
tablet_seat_handle_tool_added (void                  *data,
                               struct wl_tablet_seat *wl_tablet_seat,
                               struct wl_tablet_tool *wl_tablet_tool)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandTabletToolData *tool;

  tool = g_new0(GdkWaylandTabletToolData, 1);
  tool->axis_flags = GDK_AXIS_FLAG_X | GDK_AXIS_FLAG_Y;

  wl_tablet_tool_set_user_data (wl_tablet_tool, device);
  wl_tablet_tool_add_listener (wl_tablet_tool, &tablet_tool_listener, tool);
}

static const struct wl_tablet_seat_listener tablet_seat_listener = {
  tablet_seat_handle_tablet_added,
  tablet_seat_handle_tool_added,
};

static void
init_devices (GdkWaylandDeviceData *device)
{
  GdkWaylandDeviceManager *device_manager = GDK_WAYLAND_DEVICE_MANAGER (device->device_manager);

  /* pointer */
  device->master_pointer = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                         "name", "Core Pointer",
                                         "type", GDK_DEVICE_TYPE_MASTER,
                                         "input-source", GDK_SOURCE_MOUSE,
                                         "input-mode", GDK_MODE_SCREEN,
                                         "has-cursor", TRUE,
                                         "display", device->display,
                                         "device-manager", device_manager,
                                         NULL);
  GDK_WAYLAND_DEVICE (device->master_pointer)->device = device;
  GDK_WAYLAND_DEVICE (device->master_pointer)->pointer = &device->pointer_info;

  device_manager->devices =
    g_list_prepend (device_manager->devices, device->master_pointer);
  g_signal_emit_by_name (device_manager, "device-added", device->master_pointer);

  /* keyboard */
  device->master_keyboard = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                          "name", "Core Keyboard",
                                          "type", GDK_DEVICE_TYPE_MASTER,
                                          "input-source", GDK_SOURCE_KEYBOARD,
                                          "input-mode", GDK_MODE_SCREEN,
                                          "has-cursor", FALSE,
                                          "display", device->display,
                                          "device-manager", device_manager,
                                          NULL);
  GDK_WAYLAND_DEVICE (device->master_keyboard)->device = device;

  device_manager->devices =
    g_list_prepend (device_manager->devices, device->master_keyboard);
  g_signal_emit_by_name (device_manager, "device-added", device->master_keyboard);

  /* link both */
  _gdk_device_set_associated_device (device->master_pointer, device->master_keyboard);
  _gdk_device_set_associated_device (device->master_keyboard, device->master_pointer);
}

static void
pointer_surface_update_scale (GdkDevice *device)
{
  GdkWaylandDeviceData *wd = GDK_WAYLAND_DEVICE (device)->device;
  GdkWaylandPointerData *pointer = GDK_WAYLAND_DEVICE (device)->pointer;
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (wd->display);
  guint32 scale;
  GSList *l;

  if (wayland_display->compositor_version < WL_SURFACE_HAS_BUFFER_SCALE)
    {
      /* We can't set the scale on this surface */
      return;
    }

  scale = 1;
  for (l = pointer->pointer_surface_outputs; l != NULL; l = l->next)
    {
      guint32 output_scale =
        _gdk_wayland_screen_get_output_scale (wayland_display->screen,
                                              l->data);
      scale = MAX (scale, output_scale);
    }

  pointer->current_output_scale = scale;

  if (pointer->cursor)
    _gdk_wayland_cursor_set_scale (pointer->cursor, scale);

  gdk_wayland_device_update_window_cursor (device);
}

static void
pointer_surface_enter (void              *data,
                       struct wl_surface *wl_surface,
                       struct wl_output  *output)

{
  GdkWaylandDeviceData *device = data;

  GDK_NOTE (EVENTS,
            g_message ("pointer surface of device %p entered output %p",
                       device, output));

  device->pointer_info.pointer_surface_outputs =
    g_slist_append (device->pointer_info.pointer_surface_outputs, output);

  pointer_surface_update_scale (device->master_pointer);
}

static void
pointer_surface_leave (void              *data,
                       struct wl_surface *wl_surface,
                       struct wl_output  *output)
{
  GdkWaylandDeviceData *device = data;

  GDK_NOTE (EVENTS,
            g_message ("pointer surface of device %p left output %p",
                       device, output));

  device->pointer_info.pointer_surface_outputs =
    g_slist_remove (device->pointer_info.pointer_surface_outputs, output);

  pointer_surface_update_scale (device->master_pointer);
}

static const struct wl_surface_listener pointer_surface_listener = {
  pointer_surface_enter,
  pointer_surface_leave
};

static GdkWindow *
create_foreign_dnd_window (GdkDisplay *display)
{
  GdkWindowAttr attrs;
  GdkScreen *screen;
  guint mask;

  screen = gdk_display_get_default_screen (display);

  attrs.x = attrs.y = 0;
  attrs.width = attrs.height = 1;
  attrs.wclass = GDK_INPUT_OUTPUT;
  attrs.window_type = GDK_WINDOW_TEMP;
  attrs.visual = gdk_screen_get_system_visual (screen);

  mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  return gdk_window_new (gdk_screen_get_root_window (screen), &attrs, mask);
}

void
add_tablet_seat (GdkWaylandDeviceData *device, struct wl_seat *wl_seat)
{
  GdkDisplay *display;
  GdkWaylandDisplay *display_wayland;
  struct wl_tablet_seat *tablet_seat;

  display = gdk_device_manager_get_display (device->device_manager);
  display_wayland = GDK_WAYLAND_DISPLAY (display);

  tablet_seat = wl_tablet_manager_get_tablet_seat (display_wayland->tablet_manager, wl_seat);
  wl_tablet_seat_add_listener (tablet_seat, &tablet_seat_listener, device);
}

void
_gdk_wayland_device_manager_add_seat (GdkDeviceManager *device_manager,
                                      guint32           id,
				      struct wl_seat   *wl_seat)
{
  GdkDisplay *display;
  GdkWaylandDisplay *display_wayland;
  GdkWaylandDeviceData *device;

  display = gdk_device_manager_get_display (device_manager);
  display_wayland = GDK_WAYLAND_DISPLAY (display);

  device = g_new0 (GdkWaylandDeviceData, 1);
  device->id = id;
  device->keymap = _gdk_wayland_keymap_new ();
  device->display = display;
  device->device_manager = device_manager;
  device->touches = g_hash_table_new_full (NULL, NULL, NULL,
                                           (GDestroyNotify) g_free);
  device->foreign_dnd_window = create_foreign_dnd_window (display);
  device->wl_seat = wl_seat;

  device->pending_selection = GDK_NONE;

  wl_seat_add_listener (device->wl_seat, &seat_listener, device);
  wl_seat_set_user_data (device->wl_seat, device);

  device->data_device =
    wl_data_device_manager_get_data_device (display_wayland->data_device_manager,
                                            device->wl_seat);
  wl_data_device_add_listener (device->data_device,
                               &data_device_listener, device);

  device->pointer_info.current_output_scale = 1;
  device->pointer_info.pointer_surface =
    wl_compositor_create_surface (display_wayland->compositor);
  wl_surface_add_listener (device->pointer_info.pointer_surface,
                           &pointer_surface_listener,
                           device);

  init_devices (device);

  /* FIXME: is the tablet manager initialized yet? */
  add_tablet_seat (device, wl_seat);
}

void
_gdk_wayland_device_manager_remove_seat (GdkDeviceManager *manager,
                                         guint32           id)
{
  GdkWaylandDeviceManager *device_manager = GDK_WAYLAND_DEVICE_MANAGER (manager);
  GList *l;

  for (l = device_manager->tablet_pairs; l != NULL; l = l->next)
    _gdk_wayland_device_manager_remove_tablet (l->data);

  for (l = device_manager->devices; l != NULL; l = l->next)
    {
      GdkWaylandDevice *wayland_device = l->data;
      GdkWaylandDeviceData *device = wayland_device->device;

      if (device->id == id)
        {
          seat_handle_capabilities (device, device->wl_seat, 0);
          g_object_unref (device->keymap);
          wl_surface_destroy (device->pointer_info.pointer_surface);
          /* FIXME: destroy data_device */
          g_clear_object (&device->keyboard_settings);
          g_hash_table_destroy (device->touches);
          gdk_window_destroy (device->foreign_dnd_window);
          stop_key_repeat (device);
          wl_tablet_manager_destroy (device->wl_tablet_manager);
          g_free (device);

          break;
        }
    }
}

static void
free_device (gpointer data)
{
  g_object_unref (data);
}

static void
gdk_wayland_device_manager_finalize (GObject *object)
{
  GdkWaylandDeviceManager *device_manager;

  device_manager = GDK_WAYLAND_DEVICE_MANAGER (object);

  g_list_free_full (device_manager->devices, free_device);

  G_OBJECT_CLASS (gdk_wayland_device_manager_parent_class)->finalize (object);
}

static GList *
gdk_wayland_device_manager_list_devices (GdkDeviceManager *device_manager,
                                         GdkDeviceType     type)
{
  GdkWaylandDeviceManager *wayland_device_manager;
  GList *devices = NULL, *l;

  wayland_device_manager = GDK_WAYLAND_DEVICE_MANAGER (device_manager);

  for (l = wayland_device_manager->devices; l; l = l->next)
    {
      if (gdk_device_get_device_type (l->data) == type)
        devices = g_list_prepend (devices, l->data);
    }

  return devices;
}

static GdkDevice *
gdk_wayland_device_manager_get_client_pointer (GdkDeviceManager *device_manager)
{
  GdkWaylandDeviceManager *wayland_device_manager;
  GList *l;

  wayland_device_manager = GDK_WAYLAND_DEVICE_MANAGER (device_manager);

  /* Find the first master pointer device */
  for (l = wayland_device_manager->devices; l != NULL; l = l->next)
    {
      GdkDevice *device = l->data;

      if (gdk_device_get_source (device) == GDK_SOURCE_MOUSE &&
          gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_MASTER)
        return device;
    }

  return NULL;
}

static void
gdk_wayland_device_manager_class_init (GdkWaylandDeviceManagerClass *klass)
{
  GdkDeviceManagerClass *device_manager_class = GDK_DEVICE_MANAGER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_wayland_device_manager_finalize;
  device_manager_class->list_devices = gdk_wayland_device_manager_list_devices;
  device_manager_class->get_client_pointer = gdk_wayland_device_manager_get_client_pointer;
}

static void
gdk_wayland_device_manager_init (GdkWaylandDeviceManager *device_manager)
{
}

GdkDeviceManager *
_gdk_wayland_device_manager_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_WAYLAND_DEVICE_MANAGER,
                       "display", display,
                       NULL);
}

uint32_t
_gdk_wayland_device_get_implicit_grab_serial (GdkWaylandDevice *device,
                                              const GdkEvent   *event)
{
  GdkEventSequence *sequence = NULL;
  GdkWaylandTouchData *touch = NULL;

  if (event)
    sequence = gdk_event_get_event_sequence (event);

  if (sequence)
    touch = gdk_wayland_device_get_touch (device->device,
                                          GDK_EVENT_SEQUENCE_TO_SLOT (sequence));
  if (touch)
    return touch->touch_down_serial;
  else
    return device->pointer->press_serial;
}

uint32_t
_gdk_wayland_device_get_last_implicit_grab_serial (GdkWaylandDevice  *device,
                                                   GdkEventSequence **sequence)
{
  GdkWaylandTouchData *touch;
  GHashTableIter iter;
  uint32_t serial = 0;

  g_hash_table_iter_init (&iter, device->device->touches);

  if (sequence)
    *sequence = NULL;

  if (device->pointer->press_serial > serial)
    serial = device->pointer->press_serial;

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &touch))
    {
      if (touch->touch_down_serial > serial)
        {
          if (sequence)
            *sequence = GDK_SLOT_TO_EVENT_SEQUENCE (touch->id);
          serial = touch->touch_down_serial;
        }
    }

  return serial;
}

void
gdk_wayland_device_unset_touch_grab (GdkDevice        *gdk_device,
                                     GdkEventSequence *sequence)
{
  GdkWaylandDeviceData *device;

  g_return_if_fail (GDK_IS_WAYLAND_DEVICE (gdk_device));

  device = GDK_WAYLAND_DEVICE (gdk_device)->device;

  gdk_wayland_device_remove_touch (device, GDK_EVENT_SEQUENCE_TO_SLOT (sequence));
  _gdk_display_end_touch_grab (gdk_device_get_display (gdk_device),
                               gdk_device, sequence);
}

struct wl_data_device *
gdk_wayland_device_get_data_device (GdkDevice *gdk_device)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (gdk_device), NULL);

  return GDK_WAYLAND_DEVICE (gdk_device)->device->data_device;
}

void
gdk_wayland_device_set_selection (GdkDevice             *gdk_device,
                                  struct wl_data_source *source)
{
  GdkWaylandDeviceData *device;
  GdkWaylandDisplay *display_wayland;

  g_return_if_fail (GDK_IS_WAYLAND_DEVICE (gdk_device));

  device = GDK_WAYLAND_DEVICE (gdk_device)->device;
  display_wayland = GDK_WAYLAND_DISPLAY (gdk_device_get_display (gdk_device));

  wl_data_device_set_selection (device->data_device, source,
                                _gdk_wayland_display_get_serial (display_wayland));
}

void
gdk_wayland_device_unset_grab (GdkDevice *gdk_device)
{
  GdkWaylandPointerData *pointer;
  GdkEventSequence *sequence;
  GdkModifierType state;
  GdkEvent *event;
  guint button;
  gdouble x, y;

  pointer = GDK_WAYLAND_DEVICE (gdk_device)->pointer;
  _gdk_wayland_device_get_last_implicit_grab_serial (GDK_WAYLAND_DEVICE (gdk_device), &sequence);
  gdk_window_get_device_position_double (pointer->grab_window,
                                         gdk_device, &x, &y, &state);

  if (sequence)
    {
      event = gdk_event_new (GDK_TOUCH_END);
      event->touch.window = g_object_ref (pointer->grab_window);
      event->touch.send_event = TRUE;
      event->touch.sequence = sequence;
      event->touch.time = GDK_CURRENT_TIME;
      event->touch.x = event->touch.x_root = x;
      event->touch.y = event->touch.y_root = y;
    }
  else if (state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK))
    {
      if (state & GDK_BUTTON1_MASK)
        button = 1;
      else if (state & GDK_BUTTON2_MASK)
        button = 2;
      else if (state & GDK_BUTTON3_MASK)
        button = 3;
      else
        return;

      event = gdk_event_new (GDK_BUTTON_RELEASE);
      event->button.window = g_object_ref (pointer->grab_window);
      event->button.send_event = TRUE;
      event->button.button = button;
      event->button.time = GDK_CURRENT_TIME;
      event->button.x = event->button.x_root = x;
      event->button.y = event->button.y_root = y;
    }
  else
    return;

  pointer->button_modifiers = 0;
  gdk_event_set_device (event, gdk_device);
  gdk_event_set_source_device (event, gdk_device);

  _gdk_wayland_display_deliver_event (gdk_device_get_display (gdk_device), event);
}
