#ifndef __GDK_ATTACHMENT_PARAMETERS_PRIVATE_H__
#define __GDK_ATTACHMENT_PARAMETERS_PRIVATE_H__

#include "gdkattachmentparameters.h"

G_BEGIN_DECLS

/*
 * GdkAttachmentParameters:
 * @attachment_origin: root origin of @attachment_rectangle coordinate system
 * @has_attachment_rectangle: %TRUE if @attachment_rectangle is valid
 * @attachment_rectangle: the attachment rectangle to attach the window to
 * @attachment_margin: the space to leave around @attachment_rectangle
 * @window_margin: the space to leave around the window
 * @window_padding: the space between the window and its contents
 * @window_offset: the offset to displace the window by
 * @window_type_hint: the window type hint
 * @is_right_to_left: %TRUE if the text direction is right to left
 * @primary_options: a #GList of primary #GdkAttachmentOptions
 * @secondary_options: a #GList of secondary #GdkAttachmentOptions
 * @position_callback: a function to call when the final position is known
 * @position_callback_user_data: additional data to pass to @position_callback
 * @position_callback_destroy_notify: a function to free
 *                                    @position_callback_user_data
 *
 * Opaque type containing the information needed to position a window relative
 * to an attachment rectangle.
 */
struct _GdkAttachmentParameters
{
  /*< private >*/
  GdkPoint attachment_origin;

  gboolean has_attachment_rectangle;
  GdkRectangle attachment_rectangle;

  GdkAttachmentBorder attachment_margin;
  GdkAttachmentBorder window_margin;
  GdkAttachmentBorder window_padding;

  GdkPoint window_offset;

  GdkWindowTypeHint window_type_hint;

  gboolean is_right_to_left;

  GList *primary_options;
  GList *secondary_options;

  GdkAttachmentPositionCallback position_callback;
  gpointer position_callback_user_data;
  GDestroyNotify position_callback_destroy_notify;
};

G_GNUC_INTERNAL
gboolean gdk_attachment_parameters_choose_position            (const GdkAttachmentParameters *parameters,
                                                               gint                           width,
                                                               gint                           height,
                                                               const GdkRectangle            *bounds,
                                                               GdkPoint                      *position,
                                                               GdkPoint                      *offset,
                                                               GdkAttachmentOption           *primary_option,
                                                               GdkAttachmentOption           *secondary_option);

G_GNUC_INTERNAL
gboolean gdk_attachment_parameters_choose_position_for_window (const GdkAttachmentParameters *parameters,
                                                               GdkWindow                     *window,
                                                               GdkPoint                      *position,
                                                               GdkPoint                      *offset,
                                                               GdkAttachmentOption           *primary_option,
                                                               GdkAttachmentOption           *secondary_option);

G_GNUC_INTERNAL
void     gdk_window_move_using_attachment_parameters          (GdkWindow                     *window,
                                                               const GdkAttachmentParameters *parameters);

G_END_DECLS

#endif /* __GDK_ATTACHMENT_PARAMETERS_PRIVATE_H__ */
