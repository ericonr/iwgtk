/*
 *  Copyright 2020 Jesse Lentz
 *
 *  This file is part of iwgtk.
 *
 *  iwgtk is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  iwgtk is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with iwgtk.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "iwgtk.h"

void validation_callback(GDBusProxy *proxy, GAsyncResult *res, CallbackMessages *messages) {
    GVariant *ret;
    GError *err;

    err = NULL;
    ret = g_dbus_proxy_call_finish(proxy, res, &err);

    if (ret) {
	g_variant_unref(ret);
	if (messages && messages->success) {
	    send_notification(messages->success, G_NOTIFICATION_PRIORITY_NORMAL);
	}
    }
    else {
	if (messages && messages->failure) {
	    const gchar *detailed;

	    detailed = NULL;
	    if (messages->error_table) {
		int i = 0;
		while (TRUE) {
		    int code = messages->error_table[i].code;
		    if ( (err->domain == global.iwd_error_domain && code == err->code) || code == 0) {
			detailed = messages->error_table[i].message;
			break;
		    }
		    i ++;
		}
	    }

	    if (detailed) {
		char *message_text;
		message_text = g_strconcat(messages->failure, ": ", detailed, NULL);
		send_notification(message_text, G_NOTIFICATION_PRIORITY_NORMAL);
		g_free(message_text);
	    }
	    else {
		send_notification(messages->failure, G_NOTIFICATION_PRIORITY_NORMAL);
	    }
	}

	fprintf(stderr, "%s\n", err->message);
	g_error_free(err);
    }
}

void validation_callback_log(GDBusProxy *proxy, GAsyncResult *res, const gchar *message) {
    GVariant *ret;
    GError *err;

    err = NULL;
    ret = g_dbus_proxy_call_finish(proxy, res, &err);

    if (ret) {
	g_variant_unref(ret);
    }
    else {
	fprintf(stderr, message, err->message);
	g_error_free(err);
    }
}

void set_remote_property_callback(GDBusProxy *proxy, GAsyncResult *res, FailureClosure *failure) {
    GVariant *ret;
    GError *err;

    err = NULL;
    ret = g_dbus_proxy_call_finish(proxy, res, &err);

    if (ret) {
	g_variant_unref(ret);
    }
    else {
	fprintf(stderr, "Error setting remote property '%s': %s\n", failure->property, err->message);
	g_error_free(err);

	failure->callback(failure->data);
    }

    free(failure);
}

/*
 * When a property is updated remotely, iwgtk responds by updating a widget. This widget
 * state change triggers a signal which causes set_remote_property() to be called. The
 * equality check in this function prevents the property change from being volleyed back
 * to iwd. This is kind of a hack; it would be more elegant if set_remote_property() were
 * only called for user-initiated state changes.
 */
void set_remote_property(GDBusProxy *proxy, const gchar *property, GVariant *value, SetFunction failure_callback, gpointer failure_data) {
    GVariant *value_cached;

    value_cached = g_dbus_proxy_get_cached_property(proxy, property);
    if (!g_variant_equal(value, value_cached)) {
	FailureClosure *failure_closure;

	failure_closure = malloc(sizeof(FailureClosure));
	failure_closure->callback = failure_callback;
	failure_closure->data = failure_data;
	failure_closure->property = property;

	g_dbus_proxy_call(
	    proxy,
	    "org.freedesktop.DBus.Properties.Set",
	    g_variant_new("(ssv)", g_dbus_proxy_get_interface_name(proxy), property, value),
	    G_DBUS_CALL_FLAGS_NONE,
	    -1,
	    NULL,
	    (GAsyncReadyCallback) set_remote_property_callback,
	    (gpointer) failure_closure);
    }
    else {
	g_variant_unref(value);
    }
    g_variant_unref(value_cached);
}

GVariant* lookup_property(GVariant *dictionary, const gchar *property) {
    GVariantIter iter;
    gchar *key;
    GVariant *value;

    g_variant_iter_init(&iter, dictionary);
    while (g_variant_iter_next(&iter, "{sv}", &key, &value)) {
	if (strcmp(property, key) == 0) {
	    g_free(key);
	    return value;
	}

	g_free(key);
	g_variant_unref(value);
    }

    return NULL;
}

void send_notification(const gchar *text, GNotificationPriority priority) {
    if (!global.notifications_disable) {
	GNotification *notification;

	notification = g_notification_new("iwgtk");
	g_notification_set_body(notification, text);
	g_notification_set_priority(notification, priority);
	g_application_send_notification(G_APPLICATION(global.application), NULL, notification);
    }
}

void grid_column_set_alignment(GtkWidget *grid, int col, GtkAlign align) {
    GtkWidget *cell;
    int i;

    i = 0;
    while (cell = gtk_grid_get_child_at(GTK_GRID(grid), col, i)) {
	gtk_widget_set_halign(cell, align);
	i ++;
    }
}

GtkWidget* label_with_spinner(const gchar *text) {
    GtkWidget *box, *spinner;

    spinner = gtk_spinner_new();
    gtk_spinner_start(GTK_SPINNER(spinner));

    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(box), spinner, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), gtk_label_new(text), FALSE, FALSE, 0);
    gtk_widget_show_all(box);

    return box;
}

GtkWidget* new_label_bold(const gchar *text) {
    PangoAttrList *attr_list;
    GtkWidget *label;

    attr_list = pango_attr_list_new();
    pango_attr_list_insert(attr_list, pango_attr_weight_new(PANGO_WEIGHT_BOLD));

    label = gtk_label_new(text);
    gtk_label_set_attributes(GTK_LABEL(label), attr_list);
    pango_attr_list_unref(attr_list);
    return label;
}

GtkWidget* new_label_gray(const gchar *text) {
    PangoAttrList *attr_list;
    GtkWidget *label;

    attr_list = pango_attr_list_new();
    pango_attr_list_insert(attr_list, pango_attr_weight_new(PANGO_WEIGHT_SEMILIGHT));
    pango_attr_list_insert(attr_list, pango_attr_foreground_new(RGB_MAX/2, RGB_MAX/2, RGB_MAX/2));

    label = gtk_label_new(text);
    gtk_label_set_attributes(GTK_LABEL(label), attr_list);
    pango_attr_list_unref(attr_list);
    return label;
}

void bin_empty(GtkBin *parent) {
    GtkWidget *child;
    child = gtk_bin_get_child(parent);
    if (child) {
	gtk_container_remove(GTK_CONTAINER(parent), child);
    }
}
