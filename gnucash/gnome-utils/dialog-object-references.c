/********************************************************************\
 * dialog-object-references.c -- dialog for displaying a list of    *
 *                               objects which refer to another     *
 *                               specific object                    *
 *                                                                  *
 * Copyright (C) 2010 Phil Longstaff (plongstaff@rogers.com)        *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, see                             *
 * <https://www.gnu.org/licenses/>                                  *
\********************************************************************/

#include <config.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gnc-ui.h"
#include "gnc-path.h"
#include "dialog-utils.h"
#include "dialog-object-references.h"

[[maybe_unused]] static QofLogModule log_module = GNC_MOD_GUI;

struct _GncObjectReferencesDialog
{
    GtkDialog parent_instance;
    GtkLabel *lbl_explanation;
    GtkWidget *hbox_list;
    GtkTreeView *listview;
    gboolean template_loaded;
};

enum
{
    PROP_0,
    PROP_EXPLANATION,
    PROP_OBJECT_LIST
};

G_DEFINE_TYPE (GncObjectReferencesDialog, gnc_object_references_dialog, GTK_TYPE_DIALOG)

/* Report a failure to the user.  A message box is used instead of
 * gnc_error_dialog() because the latter runs a nested main loop which
 * would block the caller until the user closes the message box. */
static void
gnc_object_references_dialog_report_error (const gchar *message)
{
    GtkWidget *message_box = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
                                                     GTK_MESSAGE_ERROR,
                                                     GTK_BUTTONS_CLOSE, "%s", message);
    g_signal_connect (message_box, "response", G_CALLBACK (gtk_widget_destroy), NULL);
    gtk_widget_show_all (message_box);
}

/* Load the dialog template for this instance.  The template is loaded per
 * instance instead of once per class so that a missing or broken template
 * is reported to the user instead of aborting the program. */
static gboolean
gnc_object_references_dialog_load_template (GncObjectReferencesDialog *self)
{
    gchar *directory = gnc_path_get_gtkbuilderdir ();
    gchar *filename = g_build_filename (directory, "dialog-object-references.glade", NULL);
    gchar *contents = NULL;
    gsize length = 0;
    GError *error = NULL;

    if (!g_file_get_contents (filename, &contents, &length, &error))
    {
        PWARN ("Could not load object references dialog template %s: %s",
               filename, error->message);
        g_clear_error (&error);
        g_free (filename);
        g_free (directory);
        return FALSE;
    }
    g_free (filename);
    g_free (directory);

    GtkBuilder *builder = gtk_builder_new ();
    if (!gtk_builder_extend_with_template (builder, GTK_WIDGET (self),
                                           GNC_TYPE_OBJECT_REFERENCES_DIALOG,
                                           contents, length, &error))
    {
        PWARN ("Could not parse object references dialog template: %s", error->message);
        g_clear_error (&error);
        g_object_unref (builder);
        g_free (contents);
        return FALSE;
    }
    g_free (contents);

    self->lbl_explanation = GTK_LABEL (gtk_builder_get_object (builder, "lbl_explanation"));
    self->hbox_list = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_list"));
    g_object_unref (builder);

    return GTK_IS_LABEL (self->lbl_explanation) && GTK_IS_BOX (self->hbox_list);
}

static void
gnc_object_references_dialog_init (GncObjectReferencesDialog *self)
{
    gtk_widget_set_name (GTK_WIDGET (self), "gnc-id-object-reference");
    if (!gnc_object_references_dialog_load_template (self))
    {
        PWARN ("Could not initialize object references dialog template");
        gnc_object_references_dialog_report_error
            (_("Could not display the objects referring to this one."));
        return;
    }
    self->template_loaded = TRUE;
    GtkListStore *store = gtk_list_store_new (1, G_TYPE_STRING);
    self->listview = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL (store)));
    g_object_unref (store);
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes ("Object", renderer, "text", 0, NULL);
    gtk_tree_view_append_column (self->listview, column);
    gtk_container_add (GTK_CONTAINER (self->hbox_list), GTK_WIDGET (self->listview));
}

static void
gnc_object_references_dialog_set_property (GObject *object, guint prop_id,
                                           const GValue *value, GParamSpec *pspec)
{
    GncObjectReferencesDialog *self = GNC_OBJECT_REFERENCES_DIALOG (object);
    switch (prop_id)
    {
    case PROP_EXPLANATION:
        if (self->lbl_explanation != NULL)
            gtk_label_set_text (self->lbl_explanation, g_value_get_string (value));
        break;
    case PROP_OBJECT_LIST:
    {
        if (self->listview == NULL)
            break;
        GtkListStore *store = GTK_LIST_STORE (gtk_tree_view_get_model (self->listview));
        for (const GList *node = g_value_get_pointer (value); node; node = node->next)
        {
            GtkTreeIter iter;
            gchar *name = qof_instance_get_display_name (node->data);
            gtk_list_store_append (store, &iter);
            gtk_list_store_set (store, &iter, 0, name, -1);
            g_free (name);
        }
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gnc_object_references_dialog_get_property (GObject *object, guint prop_id,
                                           GValue *value, GParamSpec *pspec)
{
    GncObjectReferencesDialog *self = GNC_OBJECT_REFERENCES_DIALOG (object);
    switch (prop_id)
    {
    case PROP_EXPLANATION:
        g_value_set_string (value, self->lbl_explanation != NULL
                            ? gtk_label_get_text (self->lbl_explanation) : NULL);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gnc_object_references_dialog_class_init (GncObjectReferencesDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->set_property = gnc_object_references_dialog_set_property;
    object_class->get_property = gnc_object_references_dialog_get_property;
    g_object_class_install_property (object_class, PROP_EXPLANATION,
        g_param_spec_string ("explanation", "Explanation", "Explanation of the object references", "",
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (object_class, PROP_OBJECT_LIST,
        g_param_spec_pointer ("object-list", "Object list", "Borrowed list of QofInstance objects",
                              G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

}

static void
gnc_object_references_dialog_response_cb (GtkDialog *dialog,
                                           [[maybe_unused]] int response,
                                           [[maybe_unused]] gpointer user_data)
{
    gtk_widget_destroy (GTK_WIDGET (dialog));
}

void
gnc_ui_object_references_show (const gchar *explanation_text, GList *objlist)
{
    GncObjectReferencesDialog *dialog =
        GNC_OBJECT_REFERENCES_DIALOG (g_object_new (GNC_TYPE_OBJECT_REFERENCES_DIALOG,
                                                    "explanation", explanation_text,
                                                    "object-list", objlist, "modal", TRUE, NULL));
    if (!dialog->template_loaded)
    {
        /* The failure has already been reported to the user. */
        gtk_widget_destroy (GTK_WIDGET (dialog));
        return;
    }
    g_signal_connect (dialog, "response", G_CALLBACK (gnc_object_references_dialog_response_cb), NULL);
    gtk_widget_show_all (GTK_WIDGET (dialog));
}
