#ifndef __GDK_ATTACHMENT_PARAMETERS_H__
#define __GDK_ATTACHMENT_PARAMETERS_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdktypes.h>
#include <gdk/gdkversionmacros.h>

G_BEGIN_DECLS

/**
 * GdkAttachmentOption:
 * @GDK_ATTACHMENT_END_OPTIONS: no more options
 * @GDK_ATTACHMENT_UNKNOWN_OPTION: indicates that the backend doesn't know
 *  what option was selected for GdkAttachmentPositionCallback(). Only used
 *  for GdkAttachmentPositionCallback().
 * @GDK_ATTACHMENT_FORCE_FIRST_OPTION: retry first option, pushing on-screen if needed
 * @GDK_ATTACHMENT_FORCE_FIRST_OPTION_IF_PRIMARY_FORCED: retry first secondary
 *  option if retrying a primary option, pushing on-screen if needed. Only use
 *  this as a secondary option.
 * @GDK_ATTACHMENT_FORCE_LAST_OPTION: retry recent option, pushing on-screen if needed
 * @GDK_ATTACHMENT_FORCE_LAST_OPTION_IF_PRIMARY_FORCED: retry recent secondary
 *  option if retrying a primary option, pushing on-screen if needed. Only use
 *  this as a secondary option.
 * @GDK_ATTACHMENT_ATTACH_TOP_EDGE: attach to top edge of attachment rectangle
 * @GDK_ATTACHMENT_ATTACH_LEFT_EDGE: attach to left edge of attachment rectangle
 * @GDK_ATTACHMENT_ATTACH_RIGHT_EDGE: attach to right edge of attachment rectangle
 * @GDK_ATTACHMENT_ATTACH_BOTTOM_EDGE: attach to bottom edge of attachment rectangle
 * @GDK_ATTACHMENT_ATTACH_FORWARD_EDGE: attach to forward edge of attachment rectangle
 * @GDK_ATTACHMENT_ATTACH_BACKWARD_EDGE: attach to backward edge of attachment rectangle
 * @GDK_ATTACHMENT_ALIGN_TOP_EDGES: align top edges of attachment rectangle and window
 * @GDK_ATTACHMENT_ALIGN_LEFT_EDGES: align left edges of attachment rectangle and window
 * @GDK_ATTACHMENT_ALIGN_RIGHT_EDGES: align right edges of attachment rectangle and window
 * @GDK_ATTACHMENT_ALIGN_BOTTOM_EDGES: align bottom edges of attachment rectangle and window
 * @GDK_ATTACHMENT_ALIGN_FORWARD_EDGES: align forward edges of attachment rectangle and window
 * @GDK_ATTACHMENT_ALIGN_BACKWARD_EDGES: align backward edges of attachment rectangle and window
 * @GDK_ATTACHMENT_CENTER_HORIZONTALLY: center window horizontally on attachment rectangle
 * @GDK_ATTACHMENT_CENTER_VERTICALLY: center window vertically on attachment rectangle
 * @GDK_ATTACHMENT_CENTER_ON_TOP_EDGE: center window on top edge of attachment rectangle
 * @GDK_ATTACHMENT_CENTER_ON_LEFT_EDGE: center window on left edge of attachment rectangle
 * @GDK_ATTACHMENT_CENTER_ON_RIGHT_EDGE: center window on right edge of attachment rectangle
 * @GDK_ATTACHMENT_CENTER_ON_BOTTOM_EDGE: center window on bottom edge of attachment rectangle
 * @GDK_ATTACHMENT_CENTER_ON_FORWARD_EDGE: center window on forward edge of attachment rectangle
 * @GDK_ATTACHMENT_CENTER_ON_BACKWARD_EDGE: center window on backward edge of attachment rectangle
 * @GDK_ATTACHMENT_ATTACH_ABOVE_CENTER: align bottom edge of window with center of attachment rectangle
 * @GDK_ATTACHMENT_ATTACH_BELOW_CENTER: align top edge of window with center of attachment rectangle
 * @GDK_ATTACHMENT_ATTACH_LEFT_OF_CENTER: align right edge of window with center of attachment rectangle
 * @GDK_ATTACHMENT_ATTACH_RIGHT_OF_CENTER: align left edge of window with center of attachment rectangle
 * @GDK_ATTACHMENT_ATTACH_FORWARD_OF_CENTER: align backward edge of window with center of attachment rectangle
 * @GDK_ATTACHMENT_ATTACH_BACKWARD_OF_CENTER: align forward edge of window with center of attachment rectangle
 *
 * Constraints on the position of the window relative to its attachment
 * rectangle.
 */
typedef enum _GdkAttachmentOption
{
  GDK_ATTACHMENT_END_OPTIONS = 0,
  GDK_ATTACHMENT_UNKNOWN_OPTION = 0,
  GDK_ATTACHMENT_FORCE_FIRST_OPTION,
  GDK_ATTACHMENT_FORCE_FIRST_OPTION_IF_PRIMARY_FORCED,
  GDK_ATTACHMENT_FORCE_LAST_OPTION,
  GDK_ATTACHMENT_FORCE_LAST_OPTION_IF_PRIMARY_FORCED,
  GDK_ATTACHMENT_ATTACH_TOP_EDGE,
  GDK_ATTACHMENT_ATTACH_LEFT_EDGE,
  GDK_ATTACHMENT_ATTACH_RIGHT_EDGE,
  GDK_ATTACHMENT_ATTACH_BOTTOM_EDGE,
  GDK_ATTACHMENT_ATTACH_FORWARD_EDGE,
  GDK_ATTACHMENT_ATTACH_BACKWARD_EDGE,
  GDK_ATTACHMENT_ALIGN_TOP_EDGES,
  GDK_ATTACHMENT_ALIGN_LEFT_EDGES,
  GDK_ATTACHMENT_ALIGN_RIGHT_EDGES,
  GDK_ATTACHMENT_ALIGN_BOTTOM_EDGES,
  GDK_ATTACHMENT_ALIGN_FORWARD_EDGES,
  GDK_ATTACHMENT_ALIGN_BACKWARD_EDGES,
  GDK_ATTACHMENT_CENTER_HORIZONTALLY,
  GDK_ATTACHMENT_CENTER_VERTICALLY,
  GDK_ATTACHMENT_CENTER_ON_TOP_EDGE,
  GDK_ATTACHMENT_CENTER_ON_LEFT_EDGE,
  GDK_ATTACHMENT_CENTER_ON_RIGHT_EDGE,
  GDK_ATTACHMENT_CENTER_ON_BOTTOM_EDGE,
  GDK_ATTACHMENT_CENTER_ON_FORWARD_EDGE,
  GDK_ATTACHMENT_CENTER_ON_BACKWARD_EDGE,
  GDK_ATTACHMENT_ATTACH_ABOVE_CENTER,
  GDK_ATTACHMENT_ATTACH_BELOW_CENTER,
  GDK_ATTACHMENT_ATTACH_LEFT_OF_CENTER,
  GDK_ATTACHMENT_ATTACH_RIGHT_OF_CENTER,
  GDK_ATTACHMENT_ATTACH_FORWARD_OF_CENTER,
  GDK_ATTACHMENT_ATTACH_BACKWARD_OF_CENTER
} GdkAttachmentOption;

/**
 * GdkAttachmentBorder:
 * @top: space above
 * @left: space to the left
 * @right: space to the right
 * @bottom: space below
 *
 * The space around the perimeter of an attachment rectangle.
 */
struct _GdkAttachmentBorder
{
  gint top;
  gint left;
  gint right;
  gint bottom;
};

typedef struct _GdkAttachmentBorder GdkAttachmentBorder;

/**
 * GdkAttachmentParameters:
 *
 * Opaque type containing the information needed to position a window relative
 * to an attachment rectangle.
 */
typedef struct _GdkAttachmentParameters GdkAttachmentParameters;

/**
 * GdkAttachmentPositionCallback:
 * @window: (transfer none) (nullable): the #GdkWindow that was moved
 * @parameters: (transfer none) (nullable): the #GdkAttachmentParameters that was used
 * @position: (transfer none) (nullable): the final position of @window
 * @offset: (transfer none) (nullable): the displacement applied to keep
 *          @window on-screen
 * @primary_option: the primary option that was used for positioning. If
 *                  unknown, this will be %GDK_ATTACHMENT_UNKNOWN_OPTION
 * @secondary_option: the secondary option that was used for positioning. If
 *                    unknown, this will be %GDK_ATTACHMENT_UNKNOWN_OPTION
 * @user_data: (transfer none) (nullable): the user data that was set on
 *             @parameters
 *
 * A function that can be used to receive information about the final position
 * of a window after gdk_window_set_attachment_parameters() is called. Since
 * the position might be determined asynchronously, don't assume it will be
 * called directly from gdk_window_set_attachment_parameters().
 */
typedef void (*GdkAttachmentPositionCallback) (GdkWindow                     *window,
                                               const GdkAttachmentParameters *parameters,
                                               const GdkPoint                *position,
                                               const GdkPoint                *offset,
                                               GdkAttachmentOption            primary_option,
                                               GdkAttachmentOption            secondary_option,
                                               gpointer                       user_data);

GDK_AVAILABLE_IN_3_20
GdkAttachmentParameters * gdk_attachment_parameters_new                      (void);

GDK_AVAILABLE_IN_3_20
gpointer                  gdk_attachment_parameters_copy                     (gconstpointer                  src,
                                                                              gpointer                       data);

GDK_AVAILABLE_IN_3_20
void                      gdk_attachment_parameters_free                     (gpointer                       data);

GDK_AVAILABLE_IN_3_20
void                      gdk_attachment_parameters_set_attachment_origin    (GdkAttachmentParameters       *parameters,
                                                                              const GdkPoint                *origin);

GDK_AVAILABLE_IN_3_20
void                      gdk_attachment_parameters_set_attachment_rectangle (GdkAttachmentParameters       *parameters,
                                                                              const GdkRectangle            *rectangle);

GDK_AVAILABLE_IN_3_20
void                      gdk_attachment_parameters_set_attachment_margin    (GdkAttachmentParameters       *parameters,
                                                                              const GdkAttachmentBorder     *margin);

GDK_AVAILABLE_IN_3_20
void                      gdk_attachment_parameters_set_window_margin        (GdkAttachmentParameters       *parameters,
                                                                              const GdkAttachmentBorder     *margin);

GDK_AVAILABLE_IN_3_20
void                      gdk_attachment_parameters_set_window_padding       (GdkAttachmentParameters       *parameters,
                                                                              const GdkAttachmentBorder     *padding);

GDK_AVAILABLE_IN_3_20
void                      gdk_attachment_parameters_set_window_offset        (GdkAttachmentParameters       *parameters,
                                                                              const GdkPoint                *offset);

GDK_AVAILABLE_IN_3_20
GdkWindowTypeHint         gdk_attachment_parameters_get_window_type_hint     (GdkAttachmentParameters       *parameters);

GDK_AVAILABLE_IN_3_20
void                      gdk_attachment_parameters_set_window_type_hint     (GdkAttachmentParameters       *parameters,
                                                                              GdkWindowTypeHint              window_type_hint);

GDK_AVAILABLE_IN_3_20
void                      gdk_attachment_parameters_set_right_to_left        (GdkAttachmentParameters       *parameters,
                                                                              gboolean                       is_right_to_left);

GDK_AVAILABLE_IN_3_20
void                      gdk_attachment_parameters_add_primary_options      (GdkAttachmentParameters       *parameters,
                                                                              GdkAttachmentOption            first_option,
                                                                              ...) G_GNUC_NULL_TERMINATED;

GDK_AVAILABLE_IN_3_20
void                      gdk_attachment_parameters_add_secondary_options    (GdkAttachmentParameters       *parameters,
                                                                              GdkAttachmentOption            first_option,
                                                                              ...) G_GNUC_NULL_TERMINATED;

GDK_AVAILABLE_IN_3_20
void                      gdk_attachment_parameters_set_position_callback    (GdkAttachmentParameters       *parameters,
                                                                              GdkAttachmentPositionCallback  callback,
                                                                              gpointer                       user_data,
                                                                              GDestroyNotify                 destroy_notify);

G_END_DECLS

#endif /* __GDK_ATTACHMENT_PARAMETERS_H__ */
