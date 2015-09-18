#include "config.h"

#include "gdkattachmentparameters.h"
#include "gdkattachmentparametersprivate.h"
#include "gdkscreen.h"
#include "gdkwindow.h"

/**
 * SECTION: gdkattachmentparameters
 * @section_id: gdkattachmentparameters
 * @title: Attachment Parameters
 * @short_description: Describing relative window position
 * @stability: Unstable
 * @include: gdk/gdkattachmentparameters.h
 *
 * A full description of how a window should be positioned relative to an
 * attachment rectangle.
 *
 * Since: 3.20
 */

/**
 * gdk_attachment_parameters_new:
 *
 * Creates a new #GdkAttachmentParameters for describing the position of a
 * #GdkWindow relative to an attachment #GdkRectangle.
 *
 * Returns: (transfer full): a new #GdkAttachmentParameters, to be freed with
 *          gdk_attachment_parameters_free()
 *
 * Since: 3.20
 */
GdkAttachmentParameters *
gdk_attachment_parameters_new (void)
{
  return g_new0 (GdkAttachmentParameters, 1);
}

/**
 * gdk_attachment_parameters_copy:
 * @src: (transfer none) (nullable): the parameters to copy
 * @data: (transfer none) (nullable): unused
 *
 * Creates a deep copy of @src.
 *
 * Returns: (transfer full) (nullable): a deep copy of @src, to be freed with
 *          gdk_attachment_parameters_free()
 *
 * Since: 3.20
 */
gpointer
gdk_attachment_parameters_copy (gconstpointer src,
                                gpointer      data)
{
  GdkAttachmentParameters *copy;
  const GdkAttachmentParameters *parameters;

  if (!src)
    return NULL;

  parameters = src;

  copy = g_memdup (parameters, sizeof (*parameters));

  copy->primary_options = g_list_copy (parameters->primary_options);
  copy->secondary_options = g_list_copy (parameters->secondary_options);

  return copy;
}

/**
 * gdk_attachment_parameters_free:
 * @data: (transfer full) (nullable): the parameters to free
 *
 * Releases @data.
 *
 * Since: 3.20
 */
void
gdk_attachment_parameters_free (gpointer data)
{
  GdkAttachmentParameters *parameters;

  if (!data)
    return;

  parameters = data;

  if (parameters->position_callback_user_data && parameters->position_callback_destroy_notify)
    parameters->position_callback_destroy_notify (parameters->position_callback_user_data);

  g_list_free (parameters->secondary_options);
  g_list_free (parameters->primary_options);

  g_free (parameters);
}

/**
 * gdk_attachment_parameters_set_attachment_origin:
 * @parameters: (transfer none) (not nullable): a #GdkAttachmentParameters
 * @origin: (transfer none) (nullable): the attachment rectangle's origin
 *
 * Sets the origin of the attachment rectangle's coordinate system in root
 * coordinates.
 *
 * Since: 3.20
 */
void
gdk_attachment_parameters_set_attachment_origin (GdkAttachmentParameters *parameters,
                                                 const GdkPoint          *origin)
{
  GdkPoint zero = { 0 };

  g_return_if_fail (parameters);

  parameters->attachment_origin = origin ? *origin : zero;
}

/**
 * gdk_attachment_parameters_set_attachment_rectangle:
 * @parameters: (transfer none) (not nullable): a #GdkAttachmentParameters
 * @rectangle: (transfer none) (nullable): the attachment rectangle
 *
 * Sets the attachment rectangle the window needs to be aligned relative to.
 *
 * Since: 3.20
 */
void
gdk_attachment_parameters_set_attachment_rectangle (GdkAttachmentParameters *parameters,
                                                    const GdkRectangle      *rectangle)
{
  g_return_if_fail (parameters);

  if (rectangle)
    {
      parameters->has_attachment_rectangle = TRUE;
      parameters->attachment_rectangle = *rectangle;
    }
  else
    parameters->has_attachment_rectangle = FALSE;
}

/**
 * gdk_attachment_parameters_set_attachment_margin:
 * @parameters: (transfer none) (not nullable): a #GdkAttachmentParameters
 * @margin: (transfer none) (nullable): the space around the attachment rectangle
 *
 * Sets the amount of space to leave around the attachment rectangle.
 *
 * Since: 3.20
 */
void
gdk_attachment_parameters_set_attachment_margin (GdkAttachmentParameters   *parameters,
                                                 const GdkAttachmentBorder *margin)
{
  GdkAttachmentBorder zero = { 0 };

  g_return_if_fail (parameters);

  parameters->attachment_margin = margin ? *margin : zero;
}

/**
 * gdk_attachment_parameters_set_window_margin:
 * @parameters: (transfer none) (not nullable): a #GdkAttachmentParameters
 * @margin: (transfer none) (nullable): the space around the window
 *
 * Sets the amount of space to leave around the window.
 *
 * Since: 3.20
 */
void
gdk_attachment_parameters_set_window_margin (GdkAttachmentParameters   *parameters,
                                             const GdkAttachmentBorder *margin)
{
  GdkAttachmentBorder zero = { 0 };

  g_return_if_fail (parameters);

  parameters->window_margin = margin ? *margin : zero;
}

/**
 * gdk_attachment_parameters_set_window_padding:
 * @parameters: (transfer none) (not nullable): a #GdkAttachmentParameters
 * @padding: (transfer none) (nullable): the space between the window and its
 *           contents.
 *
 * Sets the amount of space between the window and its contents.
 *
 * Since: 3.20
 */
void
gdk_attachment_parameters_set_window_padding (GdkAttachmentParameters   *parameters,
                                              const GdkAttachmentBorder *padding)
{
  GdkAttachmentBorder zero = { 0 };

  g_return_if_fail (parameters);

  parameters->window_padding = padding ? *padding : zero;
}

/**
 * gdk_attachment_parameters_set_window_offset:
 * @parameters: (transfer none) (not nullable): a #GdkAttachmentParameters
 * @offset: (transfer none) (nullable): the window displacement
 *
 * Sets the offset to displace the window by.
 *
 * Since: 3.20
 */
void
gdk_attachment_parameters_set_window_offset (GdkAttachmentParameters *parameters,
                                             const GdkPoint          *offset)
{
  GdkPoint zero = { 0 };

  g_return_if_fail (parameters);

  parameters->window_offset = offset ? *offset : zero;
}

/**
 * gdk_attachment_parameters_get_window_type_hint:
 * @parameters: (transfer none) (not nullable): a #GdkAttachmentParameters
 *
 * Gets the window type hint set on @parameters.
 *
 * Returns: the window type hint set on @parameters
 *
 * Since: 3.20
 */
GdkWindowTypeHint
gdk_attachment_parameters_get_window_type_hint (GdkAttachmentParameters *parameters)
{
  g_return_val_if_fail (parameters, GDK_WINDOW_TYPE_HINT_NORMAL);

  return parameters->window_type_hint;
}

/**
 * gdk_attachment_parameters_set_window_type_hint:
 * @parameters: (transfer none) (not nullable): a #GdkAttachmentParameters
 * @window_type_hint: the window type hint
 *
 * Sets the window type hint.
 *
 * Since: 3.20
 */
void
gdk_attachment_parameters_set_window_type_hint (GdkAttachmentParameters *parameters,
                                                GdkWindowTypeHint        window_type_hint)
{
  g_return_if_fail (parameters);

  parameters->window_type_hint = window_type_hint;
}

/**
 * gdk_attachment_parameters_set_right_to_left:
 * @parameters: (transfer none) (not nullable): a #GdkAttachmentParameters
 * @is_right_to_left: %TRUE if the text direction is right to left
 *
 * Sets the window text direction.
 *
 * Since: 3.20
 */
void
gdk_attachment_parameters_set_right_to_left (GdkAttachmentParameters *parameters,
                                             gboolean                 is_right_to_left)
{
  g_return_if_fail (parameters);

  parameters->is_right_to_left = is_right_to_left;
}

static void
add_options (GList               **list,
             GdkAttachmentOption   first_option,
             va_list               args)
{
  GdkAttachmentOption option;

  for (option = first_option; option; option = va_arg (args, GdkAttachmentOption))
    *list = g_list_append (*list, GINT_TO_POINTER (option));
}

/**
 * gdk_attachment_parameters_add_primary_options:
 * @parameters: (transfer none) (not nullable): a #GdkAttachmentParameters
 * @first_option: first primary option
 * @...: a %NULL-terminated list of options
 *
 * Appends to the list of primary positioning options to try.
 *
 * A typical backend will try each primary option in the order they're added.
 * If an option can be satisfied, it will then try each secondary option until
 * it finds a satisfiable secondary option that doesn't conflict with the
 * primary option. If it finds a pair of satisfiable non-conflicting options,
 * then it will place the window there. If it cannot find a pair, it proceeds
 * to the next primary option and tries again.
 *
 * If %GDK_ATTACHMENT_FORCE_FIRST_OPTION or %GDK_ATTACHMENT_FORCE_LAST_OPTION
 * is encountered, the backend will probably retry the first or most recent
 * option respectively, ignoring its satisfiability and forcing the window
 * on-screen.
 *
 * Since: 3.20
 */
void
gdk_attachment_parameters_add_primary_options (GdkAttachmentParameters *parameters,
                                               GdkAttachmentOption      first_option,
                                               ...)
{
  va_list args;

  g_return_if_fail (parameters);

  va_start (args, first_option);

  add_options (&parameters->primary_options, first_option, args);

  va_end (args);
}

/**
 * gdk_attachment_parameters_add_secondary_options:
 * @parameters: (transfer none) (not nullable): a #GdkAttachmentParameters
 * @first_option: first secondary option
 * @...: a %NULL-terminated list of options
 *
 * Appends to the list of secondary positioning options to try.
 *
 * A typical backend will try each primary option in the order they're added.
 * If an option can be satisfied, it will then try each secondary option until
 * it finds a satisfiable secondary option that doesn't conflict with the
 * primary option. If it finds a pair of satisfiable non-conflicting options,
 * then it will place the window there. If it cannot find a pair, it proceeds
 * to the next primary option and tries again.
 *
 * If %GDK_ATTACHMENT_FORCE_FIRST_OPTION or %GDK_ATTACHMENT_FORCE_LAST_OPTION
 * is encountered, the backend will probably retry the first or most recent
 * option respectively, ignoring its satisfiability and forcing the window
 * on-screen. If %GDK_ATTACHMENT_FORCE_FIRST_OPTION_IF_PRIMARY_FORCED or
 * %GDK_ATTACHMENT_FORCE_LAST_OPTION_IF_PRIMARY_FORCED is encountered, the
 * backend will probably retry it as above, but only if the current primary
 * option is being forced. These last two flags exist for defining fallback
 * positioning behaviour.
 *
 * Since: 3.20
 */
void
gdk_attachment_parameters_add_secondary_options (GdkAttachmentParameters *parameters,
                                                 GdkAttachmentOption      first_option,
                                                 ...)
{
  va_list args;

  g_return_if_fail (parameters);

  va_start (args, first_option);

  add_options (&parameters->secondary_options, first_option, args);

  va_end (args);
}

/**
 * gdk_attachment_parameters_set_position_callback:
 * @parameters: (transfer none) (not nullable): a #GdkAttachmentParameters
 * @callback: (nullable): a function to be called when the final position of
 *            the window is known
 * @user_data: (transfer full) (nullable): additional data to pass to @callback
 * @destroy_notify: (nullable): a function to release @user_data
 *
 * Sets the function to be called when the final position of the window is
 * known. Since the position might be determined asynchronously, don't assume
 * it will be called directly from gdk_window_set_attachment_parameters().
 *
 * Since: 3.20
 */
void
gdk_attachment_parameters_set_position_callback (GdkAttachmentParameters       *parameters,
                                                 GdkAttachmentPositionCallback  callback,
                                                 gpointer                       user_data,
                                                 GDestroyNotify                 destroy_notify)
{
  g_return_if_fail (parameters);

  parameters->position_callback = callback;

  if (user_data != parameters->position_callback_user_data)
    {
      if (parameters->position_callback_user_data && parameters->position_callback_destroy_notify)
        parameters->position_callback_destroy_notify (parameters->position_callback_user_data);

      parameters->position_callback_user_data = user_data;
      parameters->position_callback_destroy_notify = destroy_notify;
    }
  else if (user_data)
    g_warning ("%s (): parameters already owns user data", G_STRFUNC);
}

static gint
get_axis (GdkAttachmentOption option)
{
  switch (option)
    {
    case GDK_ATTACHMENT_ATTACH_LEFT_EDGE:
    case GDK_ATTACHMENT_ATTACH_RIGHT_EDGE:
    case GDK_ATTACHMENT_ATTACH_FORWARD_EDGE:
    case GDK_ATTACHMENT_ATTACH_BACKWARD_EDGE:
    case GDK_ATTACHMENT_ALIGN_LEFT_EDGES:
    case GDK_ATTACHMENT_ALIGN_RIGHT_EDGES:
    case GDK_ATTACHMENT_ALIGN_FORWARD_EDGES:
    case GDK_ATTACHMENT_ALIGN_BACKWARD_EDGES:
    case GDK_ATTACHMENT_CENTER_HORIZONTALLY:
    case GDK_ATTACHMENT_CENTER_ON_LEFT_EDGE:
    case GDK_ATTACHMENT_CENTER_ON_RIGHT_EDGE:
    case GDK_ATTACHMENT_CENTER_ON_FORWARD_EDGE:
    case GDK_ATTACHMENT_CENTER_ON_BACKWARD_EDGE:
    case GDK_ATTACHMENT_ATTACH_LEFT_OF_CENTER:
    case GDK_ATTACHMENT_ATTACH_RIGHT_OF_CENTER:
    case GDK_ATTACHMENT_ATTACH_FORWARD_OF_CENTER:
    case GDK_ATTACHMENT_ATTACH_BACKWARD_OF_CENTER:
      return 0;

    case GDK_ATTACHMENT_ATTACH_TOP_EDGE:
    case GDK_ATTACHMENT_ATTACH_BOTTOM_EDGE:
    case GDK_ATTACHMENT_ALIGN_TOP_EDGES:
    case GDK_ATTACHMENT_ALIGN_BOTTOM_EDGES:
    case GDK_ATTACHMENT_CENTER_VERTICALLY:
    case GDK_ATTACHMENT_CENTER_ON_TOP_EDGE:
    case GDK_ATTACHMENT_CENTER_ON_BOTTOM_EDGE:
    case GDK_ATTACHMENT_ATTACH_ABOVE_CENTER:
    case GDK_ATTACHMENT_ATTACH_BELOW_CENTER:
      return 1;

    default:
      break;
    }

  return -1;
}

static GdkAttachmentOption
get_base_option (GdkAttachmentOption option,
                 gboolean            is_right_to_left)
{
  switch (option)
    {
    case GDK_ATTACHMENT_ATTACH_FORWARD_EDGE:
      return is_right_to_left ? GDK_ATTACHMENT_ATTACH_LEFT_EDGE : GDK_ATTACHMENT_ATTACH_RIGHT_EDGE;

    case GDK_ATTACHMENT_ATTACH_BACKWARD_EDGE:
      return is_right_to_left ? GDK_ATTACHMENT_ATTACH_RIGHT_EDGE : GDK_ATTACHMENT_ATTACH_LEFT_EDGE;

    case GDK_ATTACHMENT_ALIGN_FORWARD_EDGES:
      return is_right_to_left ? GDK_ATTACHMENT_ALIGN_LEFT_EDGES : GDK_ATTACHMENT_ALIGN_RIGHT_EDGES;

    case GDK_ATTACHMENT_ALIGN_BACKWARD_EDGES:
      return is_right_to_left ? GDK_ATTACHMENT_ALIGN_RIGHT_EDGES : GDK_ATTACHMENT_ALIGN_LEFT_EDGES;

    case GDK_ATTACHMENT_CENTER_ON_FORWARD_EDGE:
      return is_right_to_left ? GDK_ATTACHMENT_CENTER_ON_LEFT_EDGE : GDK_ATTACHMENT_CENTER_ON_RIGHT_EDGE;

    case GDK_ATTACHMENT_CENTER_ON_BACKWARD_EDGE:
      return is_right_to_left ? GDK_ATTACHMENT_CENTER_ON_RIGHT_EDGE : GDK_ATTACHMENT_CENTER_ON_LEFT_EDGE;

    case GDK_ATTACHMENT_ATTACH_FORWARD_OF_CENTER:
      return is_right_to_left ? GDK_ATTACHMENT_ATTACH_LEFT_OF_CENTER : GDK_ATTACHMENT_ATTACH_RIGHT_OF_CENTER;

    case GDK_ATTACHMENT_ATTACH_BACKWARD_OF_CENTER:
      return is_right_to_left ? GDK_ATTACHMENT_ATTACH_RIGHT_OF_CENTER : GDK_ATTACHMENT_ATTACH_LEFT_OF_CENTER;

    default:
      break;
    }

  return option;
}

static gboolean
is_satisfiable (const GdkAttachmentParameters *parameters,
                GdkAttachmentOption            option,
                gint                           width,
                gint                           height,
                const GdkRectangle            *bounds,
                gint                          *axis,
                gint                          *value)
{
  gint a;
  gint v;

  g_return_val_if_fail (parameters, FALSE);
  g_return_val_if_fail (parameters->has_attachment_rectangle, FALSE);

  if (!parameters || !parameters->has_attachment_rectangle)
    return FALSE;

  if (!axis)
    axis = &a;

  if (!value)
    value = &v;

  switch (get_base_option (option, parameters->is_right_to_left))
    {
    case GDK_ATTACHMENT_END_OPTIONS:
      g_warning ("%s (): unexpected GDK_ATTACHMENT_END_OPTIONS", G_STRFUNC);
      return FALSE;

    case GDK_ATTACHMENT_FORCE_FIRST_OPTION:
      g_warning ("%s (): unexpected GDK_ATTACHMENT_FORCE_FIRST_OPTION", G_STRFUNC);
      return FALSE;

    case GDK_ATTACHMENT_FORCE_FIRST_OPTION_IF_PRIMARY_FORCED:
      g_warning ("%s (): unexpected GDK_ATTACHMENT_FORCE_FIRST_OPTION_IF_PRIMARY_FORCED", G_STRFUNC);
      return FALSE;

    case GDK_ATTACHMENT_FORCE_LAST_OPTION:
      g_warning ("%s (): unexpected GDK_ATTACHMENT_FORCE_LAST_OPTION", G_STRFUNC);
      return FALSE;

    case GDK_ATTACHMENT_FORCE_LAST_OPTION_IF_PRIMARY_FORCED:
      g_warning ("%s (): unexpected GDK_ATTACHMENT_FORCE_LAST_OPTION_IF_PRIMARY_FORCED", G_STRFUNC);
      return FALSE;

    case GDK_ATTACHMENT_ATTACH_TOP_EDGE:
      *axis = 1;
      *value = parameters->attachment_origin.y
             + parameters->attachment_rectangle.y
             - parameters->attachment_margin.top
             - height
             - parameters->window_margin.bottom
             + parameters->window_padding.bottom
             + parameters->window_offset.y;
      break;

    case GDK_ATTACHMENT_ATTACH_LEFT_EDGE:
      *axis = 0;
      *value = parameters->attachment_origin.x
             + parameters->attachment_rectangle.x
             - parameters->attachment_margin.left
             - width
             - parameters->window_margin.right
             + parameters->window_padding.right
             + parameters->window_offset.x;
      break;

    case GDK_ATTACHMENT_ATTACH_RIGHT_EDGE:
      *axis = 0;
      *value = parameters->attachment_origin.x
             + parameters->attachment_rectangle.x
             + parameters->attachment_rectangle.width
             + parameters->attachment_margin.right
             + parameters->window_margin.left
             - parameters->window_padding.left
             + parameters->window_offset.x;
      break;

    case GDK_ATTACHMENT_ATTACH_BOTTOM_EDGE:
      *axis = 1;
      *value = parameters->attachment_origin.y
             + parameters->attachment_rectangle.y
             + parameters->attachment_rectangle.height
             + parameters->attachment_margin.bottom
             + parameters->window_margin.top
             - parameters->window_padding.top
             + parameters->window_offset.y;
      break;

    case GDK_ATTACHMENT_ALIGN_TOP_EDGES:
      *axis = 1;
      *value = parameters->attachment_origin.y
             + parameters->attachment_rectangle.y
             - parameters->window_padding.top
             + parameters->window_offset.y;
      break;

    case GDK_ATTACHMENT_ALIGN_LEFT_EDGES:
      *axis = 0;
      *value = parameters->attachment_origin.x
             + parameters->attachment_rectangle.x
             - parameters->window_padding.left
             + parameters->window_offset.x;
      break;

    case GDK_ATTACHMENT_ALIGN_RIGHT_EDGES:
      *axis = 0;
      *value = parameters->attachment_origin.x
             + parameters->attachment_rectangle.x
             + parameters->attachment_rectangle.width
             - width
             + parameters->window_padding.right
             + parameters->window_offset.x;
      break;

    case GDK_ATTACHMENT_ALIGN_BOTTOM_EDGES:
      *axis = 1;
      *value = parameters->attachment_origin.y
             + parameters->attachment_rectangle.y
             + parameters->attachment_rectangle.height
             - height
             + parameters->window_padding.bottom
             + parameters->window_offset.y;
      break;

    case GDK_ATTACHMENT_CENTER_HORIZONTALLY:
      *axis = 0;
      *value = parameters->attachment_origin.x
             + parameters->attachment_rectangle.x
             + parameters->attachment_rectangle.width / 2
             - (width - parameters->window_padding.left - parameters->window_padding.right) / 2
             - parameters->window_padding.left
             + parameters->window_offset.x;
      break;

    case GDK_ATTACHMENT_CENTER_VERTICALLY:
      *axis = 1;
      *value = parameters->attachment_origin.y
             + parameters->attachment_rectangle.y
             + parameters->attachment_rectangle.height / 2
             - (height - parameters->window_padding.top - parameters->window_padding.bottom) / 2
             - parameters->window_padding.top
             + parameters->window_offset.y;
      break;

    case GDK_ATTACHMENT_CENTER_ON_TOP_EDGE:
      *axis = 1;
      *value = parameters->attachment_origin.y
             + parameters->attachment_rectangle.y
             - (height - parameters->window_padding.top - parameters->window_padding.bottom) / 2
             - parameters->window_padding.top
             + parameters->window_offset.y;
      break;

    case GDK_ATTACHMENT_CENTER_ON_LEFT_EDGE:
      *axis = 0;
      *value = parameters->attachment_origin.x
             + parameters->attachment_rectangle.x
             - (width - parameters->window_padding.left - parameters->window_padding.right) / 2
             - parameters->window_padding.left
             + parameters->window_offset.x;
      break;

    case GDK_ATTACHMENT_CENTER_ON_RIGHT_EDGE:
      *axis = 0;
      *value = parameters->attachment_origin.x
             + parameters->attachment_rectangle.x
             + parameters->attachment_rectangle.width
             - (width - parameters->window_padding.left - parameters->window_padding.right) / 2
             - parameters->window_padding.left
             + parameters->window_offset.x;
      break;

    case GDK_ATTACHMENT_CENTER_ON_BOTTOM_EDGE:
      *axis = 1;
      *value = parameters->attachment_origin.y
             + parameters->attachment_rectangle.y
             + parameters->attachment_rectangle.height
             - (height - parameters->window_padding.top - parameters->window_padding.bottom) / 2
             - parameters->window_padding.top
             + parameters->window_offset.y;
      break;

    case GDK_ATTACHMENT_ATTACH_ABOVE_CENTER:
      *axis = 1;
      *value = parameters->attachment_origin.y
             + parameters->attachment_rectangle.y
             + parameters->attachment_rectangle.height / 2
             - height
             + parameters->window_padding.bottom
             + parameters->window_offset.y;
      break;

    case GDK_ATTACHMENT_ATTACH_BELOW_CENTER:
      *axis = 1;
      *value = parameters->attachment_origin.y
             + parameters->attachment_rectangle.y
             + parameters->attachment_rectangle.height / 2
             - parameters->window_padding.top
             + parameters->window_offset.y;
      break;

    case GDK_ATTACHMENT_ATTACH_LEFT_OF_CENTER:
      *axis = 0;
      *value = parameters->attachment_origin.x
             + parameters->attachment_rectangle.x
             + parameters->attachment_rectangle.width / 2
             - width
             + parameters->window_padding.right
             + parameters->window_offset.x;
      break;

    case GDK_ATTACHMENT_ATTACH_RIGHT_OF_CENTER:
      *axis = 0;
      *value = parameters->attachment_origin.x
             + parameters->attachment_rectangle.x
             + parameters->attachment_rectangle.width / 2
             - parameters->window_padding.left
             + parameters->window_offset.x;
      break;

    default:
      g_warning ("%s (): unknown option", G_STRFUNC);
      return FALSE;
    }

  return !bounds || (*axis == 0 && bounds->x <= *value && *value + width <= bounds->x + bounds->width)
                 || (*axis == 1 && bounds->y <= *value && *value + height <= bounds->y + bounds->height);
}

/**
 * gdk_attachment_parameters_choose_position:
 * @parameters: (transfer none) (not nullable): a #GdkAttachmentParameters
 * @width: window width
 * @height: window height
 * @bounds: (transfer none) (nullable): monitor geometry
 * @position: (out) (optional): the best position for the window
 * @offset: (out) (optional): the displacement needed to push the window
 *          on-screen
 * @primary_option: (out) (optional): the best primary option
 * @secondary_option: (out) (optional): the best secondary option
 *
 * Finds the best position for a window of size @width and @height on a screen
 * with @bounds using the given @parameters.
 *
 * Returns: %TRUE if there is a pair of satisfiable primary and secondary
 *          options that do not conflict with each other
 *
 * Since: 3.20
 */
gboolean
gdk_attachment_parameters_choose_position (const GdkAttachmentParameters *parameters,
                                           gint                           width,
                                           gint                           height,
                                           const GdkRectangle            *bounds,
                                           GdkPoint                      *position,
                                           GdkPoint                      *offset,
                                           GdkAttachmentOption           *primary_option,
                                           GdkAttachmentOption           *secondary_option)
{
  GdkPoint p;
  GdkPoint o;
  GdkAttachmentOption po;
  GdkAttachmentOption so;
  GList *i;
  GList *j;
  GList *k;
  gint axis;
  gboolean satisfiable;
  GdkAttachmentOption first_satisfiable_primary_option[2];
  GdkAttachmentOption last_satisfiable_primary_option[2];
  GdkAttachmentOption first_satisfiable_secondary_option[2];
  GdkAttachmentOption last_satisfiable_secondary_option[2];
  gint primary_axis;
  gint secondary_axis;
  gboolean primary_force;
  gboolean secondary_force;
  gint primary_value;
  gint secondary_value;

  g_return_val_if_fail (parameters, FALSE);
  g_return_val_if_fail (parameters->has_attachment_rectangle, FALSE);

  if (!position)
    position = &p;

  if (!offset)
    offset = &o;

  if (!primary_option)
    primary_option = &po;

  if (!secondary_option)
    secondary_option = &so;

  first_satisfiable_primary_option[0] = GDK_ATTACHMENT_END_OPTIONS;
  first_satisfiable_primary_option[1] = GDK_ATTACHMENT_END_OPTIONS;
  last_satisfiable_primary_option[0] = GDK_ATTACHMENT_END_OPTIONS;
  last_satisfiable_primary_option[1] = GDK_ATTACHMENT_END_OPTIONS;

  for (i = parameters->primary_options; i; i = i->next)
    {
      *primary_option = GPOINTER_TO_INT (i->data);
      primary_force = FALSE;

      if (*primary_option == GDK_ATTACHMENT_END_OPTIONS)
        {
          g_warning ("%s (): unexpected GDK_ATTACHMENT_END_OPTIONS", G_STRFUNC);
          i = NULL;
          break;
        }
      else if (*primary_option == GDK_ATTACHMENT_FORCE_FIRST_OPTION ||
               *primary_option == GDK_ATTACHMENT_FORCE_FIRST_OPTION_IF_PRIMARY_FORCED)
        {
          g_warn_if_fail (*primary_option != GDK_ATTACHMENT_FORCE_FIRST_OPTION_IF_PRIMARY_FORCED);

          if (first_satisfiable_primary_option[0] == GDK_ATTACHMENT_END_OPTIONS)
            *primary_option = GPOINTER_TO_INT (parameters->primary_options->data);
          else
            *primary_option = first_satisfiable_primary_option[0];

          primary_force = TRUE;
        }
      else if (*primary_option == GDK_ATTACHMENT_FORCE_LAST_OPTION ||
               *primary_option == GDK_ATTACHMENT_FORCE_LAST_OPTION_IF_PRIMARY_FORCED)
        {
          g_warn_if_fail (*primary_option != GDK_ATTACHMENT_FORCE_LAST_OPTION_IF_PRIMARY_FORCED);

          if (last_satisfiable_primary_option[0] == GDK_ATTACHMENT_END_OPTIONS)
            {
              if (!i->prev)
                {
                  g_warning ("%s (): started with GDK_ATTACHMENT_FORCE_LAST_OPTION", G_STRFUNC);
                  continue;
                }

              *primary_option = GPOINTER_TO_INT (i->prev->data);
            }
          else
            *primary_option = last_satisfiable_primary_option[0];

          primary_force = TRUE;
        }

      satisfiable = is_satisfiable (parameters,
                                    *primary_option,
                                    width,
                                    height,
                                    bounds,
                                    &primary_axis,
                                    &primary_value);

      if (satisfiable && primary_axis >= 0)
        {
          if (first_satisfiable_primary_option[0] == GDK_ATTACHMENT_END_OPTIONS)
            first_satisfiable_primary_option[0] = *primary_option;
          else if (first_satisfiable_primary_option[1] == GDK_ATTACHMENT_END_OPTIONS &&
                   primary_axis != get_axis (first_satisfiable_primary_option[0]))
            first_satisfiable_primary_option[1] = *primary_option;

          if (last_satisfiable_primary_option[0] != GDK_ATTACHMENT_END_OPTIONS &&
              primary_axis != last_satisfiable_primary_option[0])
            last_satisfiable_primary_option[1] = last_satisfiable_primary_option[0];

          last_satisfiable_primary_option[0] = *primary_option;
        }

      if (satisfiable || primary_force)
        {
          first_satisfiable_secondary_option[0] = GDK_ATTACHMENT_END_OPTIONS;
          first_satisfiable_secondary_option[1] = GDK_ATTACHMENT_END_OPTIONS;
          last_satisfiable_secondary_option[0] = GDK_ATTACHMENT_END_OPTIONS;
          last_satisfiable_secondary_option[1] = GDK_ATTACHMENT_END_OPTIONS;

          for (j = parameters->secondary_options; j; j = j->next)
            {
              *secondary_option = GPOINTER_TO_INT (j->data);
              secondary_force = FALSE;
              secondary_axis = get_axis (*secondary_option);

              if (secondary_axis >= 0 && secondary_axis == primary_axis)
                continue;

              if (*secondary_option == GDK_ATTACHMENT_END_OPTIONS)
                {
                  g_warning ("%s (): unexpected GDK_ATTACHMENT_END_OPTIONS", G_STRFUNC);
                  j = NULL;
                  break;
                }
              else if (*secondary_option == GDK_ATTACHMENT_FORCE_FIRST_OPTION ||
                       *secondary_option == GDK_ATTACHMENT_FORCE_FIRST_OPTION_IF_PRIMARY_FORCED)
                {
                  if (*secondary_option == GDK_ATTACHMENT_FORCE_FIRST_OPTION_IF_PRIMARY_FORCED && !primary_force)
                    continue;

                  axis = primary_axis == 0 ? 1 : (primary_axis == 1 ? 0 : -1);

                  if (axis >= 0 && get_axis (first_satisfiable_secondary_option[0]) == axis)
                    *secondary_option = first_satisfiable_secondary_option[0];
                  else if (axis >= 0 && get_axis (first_satisfiable_secondary_option[1]) == axis)
                    *secondary_option = first_satisfiable_secondary_option[1];
                  else
                    {
                      for (k = parameters->secondary_options; k; k = k->next)
                        {
                          *secondary_option = GPOINTER_TO_INT (k->data);
                          secondary_axis = get_axis (*secondary_option);

                          if (secondary_axis >= 0 && secondary_axis != primary_axis)
                            break;
                        }

                      if (!k)
                        continue;
                    }

                  secondary_force = TRUE;
                }
              else if (*secondary_option == GDK_ATTACHMENT_FORCE_LAST_OPTION ||
                       *secondary_option == GDK_ATTACHMENT_FORCE_LAST_OPTION_IF_PRIMARY_FORCED)
                {
                  if (*secondary_option == GDK_ATTACHMENT_FORCE_LAST_OPTION_IF_PRIMARY_FORCED && !primary_force)
                    continue;

                  axis = primary_axis == 0 ? 1 : (primary_axis == 1 ? 0 : -1);

                  if (axis >= 0 && get_axis (last_satisfiable_secondary_option[0]) == axis)
                    *secondary_option = last_satisfiable_secondary_option[0];
                  else if (axis >= 0 && get_axis (last_satisfiable_secondary_option[1]) == axis)
                    *secondary_option = last_satisfiable_secondary_option[1];
                  else
                    {
                      if (!j->prev)
                        {
                          if (*secondary_option == GDK_ATTACHMENT_FORCE_LAST_OPTION)
                            g_warning ("%s (): started with GDK_ATTACHMENT_FORCE_LAST_OPTION", G_STRFUNC);
                          else if (*secondary_option == GDK_ATTACHMENT_FORCE_LAST_OPTION_IF_PRIMARY_FORCED)
                            g_warning ("%s (): started with GDK_ATTACHMENT_FORCE_LAST_OPTION_IF_PRIMARY_FORCED", G_STRFUNC);

                          continue;
                        }

                      for (k = j->prev; k; k = k->prev)
                        {
                          *secondary_option = GPOINTER_TO_INT (k->data);
                          secondary_axis = get_axis (*secondary_option);

                          if (secondary_axis >= 0 && secondary_axis != primary_axis)
                            break;
                        }

                      if (!k)
                        continue;
                    }

                  secondary_force = TRUE;
                }

              satisfiable = is_satisfiable (parameters,
                                            *secondary_option,
                                            width,
                                            height,
                                            bounds,
                                            &secondary_axis,
                                            &secondary_value);

              if (satisfiable && secondary_axis >= 0)
                {
                  if (first_satisfiable_secondary_option[0] == GDK_ATTACHMENT_END_OPTIONS)
                    first_satisfiable_secondary_option[0] = *secondary_option;
                  else if (first_satisfiable_secondary_option[1] == GDK_ATTACHMENT_END_OPTIONS &&
                           secondary_axis != get_axis (first_satisfiable_secondary_option[0]))
                    first_satisfiable_secondary_option[1] = *secondary_option;

                  if (last_satisfiable_secondary_option[0] != GDK_ATTACHMENT_END_OPTIONS &&
                      secondary_axis != last_satisfiable_secondary_option[0])
                    last_satisfiable_secondary_option[1] = last_satisfiable_secondary_option[0];

                  last_satisfiable_secondary_option[0] = *secondary_option;
                }

              if (satisfiable || secondary_force)
                {
                  if (primary_axis == 0)
                    {
                      position->x = primary_value;
                      position->y = secondary_value;
                    }
                  else
                    {
                      position->x = secondary_value;
                      position->y = primary_value;
                    }

                  break;
                }
            }

          if (j)
            break;
        }
    }

  if (i)
    {
      offset->x = 0;
      offset->y = 0;

      if (bounds)
        {
          if (position->x + width - parameters->window_padding.right > bounds->x + bounds->width)
            {
              offset->x += bounds->x + bounds->width - width + parameters->window_padding.right - position->x;
              position->x = bounds->x + bounds->width - width + parameters->window_padding.right;
            }

          if (position->x + parameters->window_padding.left < bounds->x)
            {
              offset->x += bounds->x - parameters->window_padding.left - position->x;
              position->x = bounds->x - parameters->window_padding.left;
            }

          if (position->y + height - parameters->window_padding.bottom > bounds->y + bounds->height)
            {
              offset->y += bounds->y + bounds->height - height + parameters->window_padding.bottom - position->y;
              position->y = bounds->y + bounds->height - height + parameters->window_padding.bottom;
            }

          if (position->y + parameters->window_padding.top < bounds->y)
            {
              offset->y += bounds->y - parameters->window_padding.top - position->y;
              position->y = bounds->y - parameters->window_padding.top;
            }
        }

      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_attachment_parameters_choose_position_for_window:
 * @parameters: (transfer none) (not nullable): a #GdkAttachmentParameters
 * @window: (transfer none) (not nullable): the #GdkWindow to find the best
 *          position for
 * @position: (out) (optional): the best position for the window
 * @offset: (out) (optional): the displacement needed to push the window
 *          on-screen
 * @primary_option: (out) (optional): the best primary option
 * @secondary_option: (out) (optional): the best secondary option
 *
 * Finds the best position for @window according to @parameters.
 *
 * Returns: %TRUE if there's a best position
 *
 * Since: 3.20
 */
gboolean
gdk_attachment_parameters_choose_position_for_window (const GdkAttachmentParameters *parameters,
                                                      GdkWindow                     *window,
                                                      GdkPoint                      *position,
                                                      GdkPoint                      *offset,
                                                      GdkAttachmentOption           *primary_option,
                                                      GdkAttachmentOption           *secondary_option)
{
  GdkScreen *screen;
  gint x;
  gint y;
  gint monitor;
  GdkRectangle bounds;
  gint width;
  gint height;

  g_return_val_if_fail (parameters, FALSE);
  g_return_val_if_fail (parameters->has_attachment_rectangle, FALSE);
  g_return_val_if_fail (window, FALSE);

  screen = gdk_window_get_screen (window);
  x = parameters->attachment_origin.x + parameters->attachment_rectangle.x + parameters->attachment_rectangle.width / 2;
  y = parameters->attachment_origin.y + parameters->attachment_rectangle.y + parameters->attachment_rectangle.height / 2;
  monitor = gdk_screen_get_monitor_at_point (screen, x, y);
  gdk_screen_get_monitor_workarea (screen, monitor, &bounds);
  width = gdk_window_get_width (window);
  height = gdk_window_get_height (window);

  return gdk_attachment_parameters_choose_position (parameters,
                                                    width,
                                                    height,
                                                    &bounds,
                                                    position,
                                                    offset,
                                                    primary_option,
                                                    secondary_option);
}

/**
 * gdk_window_move_using_attachment_parameters:
 * @window: (transfer none) (not nullable): the #GdkWindow to position
 * @parameters: (transfer none) (nullable): a description of how to position
 *              @window
 *
 * Moves @window to the best position according to @parameters.
 *
 * Since: 3.20
 */
void
gdk_window_move_using_attachment_parameters (GdkWindow                     *window,
                                             const GdkAttachmentParameters *parameters)
{
  GdkPoint position;
  GdkPoint offset;
  GdkAttachmentOption primary_option;
  GdkAttachmentOption secondary_option;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (!parameters || !parameters->has_attachment_rectangle)
    return;

  if (gdk_attachment_parameters_choose_position_for_window (parameters,
                                                            window,
                                                            &position,
                                                            &offset,
                                                            &primary_option,
                                                            &secondary_option))
    {
      gdk_window_move (window, position.x, position.y);

      if (parameters->position_callback)
        parameters->position_callback (window,
                                       parameters,
                                       &position,
                                       &offset,
                                       primary_option,
                                       secondary_option,
                                       parameters->position_callback_user_data);
    }
}
