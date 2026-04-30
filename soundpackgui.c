#include <gtk/gtk.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "placeholder.h"

static GtkWidget *combo = NULL;
static GtkWidget *path_label = NULL;
static GtkWidget *preview = NULL;

static char *install_path = NULL;
static char **packs = NULL;
static int pack_count = 0;

static char selected_pack[256] = "default";

// ----------------------------
// config path
// ----------------------------
static void get_config_path(char *out, size_t size) {
    const char *home = getenv("HOME");
    snprintf(out, size,
             "%s/.config/TeamSpeak/soundpack.conf",
             home);
}

// ----------------------------
// load config
// ----------------------------
static void load_config() {
    char cfg[512];
    get_config_path(cfg, sizeof(cfg));

    FILE *f = fopen(cfg, "r");
    if (!f) return;

    char line[512];

    while (fgets(line, sizeof(line), f)) {

        if (strncmp(line, "pack=", 5) == 0) {
            strncpy(selected_pack, line + 5, sizeof(selected_pack) - 1);

            size_t n = strlen(selected_pack);
            if (n && selected_pack[n - 1] == '\n')
                selected_pack[n - 1] = '\0';
        }

        if (strncmp(line, "ts_path=", 8) == 0) {
            free(install_path);
            install_path = strdup(line + 8);

            size_t n = strlen(install_path);
            if (n && install_path[n - 1] == '\n')
                install_path[n - 1] = '\0';
        }
    }

    fclose(f);
}

// ----------------------------
// save config
// ----------------------------
static void write_config(const char *pack) {
    char cfg[512];
    get_config_path(cfg, sizeof(cfg));

    FILE *f = fopen(cfg, "w");
    if (!f) return;

    fprintf(f, "pack=%s\n", pack);

    if (install_path)
        fprintf(f, "ts_path=%s\n", install_path);

    fclose(f);
}

// ----------------------------
// placeholder image (embedded)
// ----------------------------
static void set_placeholder_image() {
    GInputStream *stream =
        g_memory_input_stream_new_from_data(
            logo_256_png,
            logo_256_png_len,
            NULL
        );

    GError *error = NULL;

    GdkPixbuf *pixbuf =
        gdk_pixbuf_new_from_stream(stream, NULL, &error);

    g_object_unref(stream);

    if (!pixbuf) {
        if (error) g_error_free(error);
        return;
    }

    GdkPixbuf *scaled =
        gdk_pixbuf_scale_simple(pixbuf, 256, 256,
                                GDK_INTERP_BILINEAR);

    GdkTexture *tex =
        gdk_texture_new_for_pixbuf(scaled);

    gtk_picture_set_paintable(GTK_PICTURE(preview),
                             GDK_PAINTABLE(tex));

    g_object_unref(pixbuf);
    g_object_unref(scaled);
    g_object_unref(tex);
}

// ----------------------------
// load real pack image (FIXED GTK4)
// ----------------------------
static void set_pack_image(const char *path) {
    GError *error = NULL;

    GFile *file = g_file_new_for_path(path);

    GdkTexture *tex =
        gdk_texture_new_from_file(file, &error);

    g_object_unref(file);

    if (!tex) {
        set_placeholder_image();
        if (error) g_error_free(error);
        return;
    }

    gtk_picture_set_paintable(GTK_PICTURE(preview),
                             GDK_PAINTABLE(tex));

    g_object_unref(tex);
}

// ----------------------------
// load pack preview
// ----------------------------
static void load_pack_image(const char *pack) {
    if (!install_path) return;

    char path[1024];
    snprintf(path, sizeof(path),
             "%s/html/client_ui/sound/%s/pack.png",
             install_path, pack);

    if (access(path, F_OK) != 0) {
        set_placeholder_image();
        return;
    }

    set_pack_image(path);
}

// ----------------------------
// scan packs
// ----------------------------
static void scan_packs() {
    if (!install_path) return;

    char path[1024];
    snprintf(path, sizeof(path),
             "%s/html/client_ui/sound/",
             install_path);

    DIR *dir = opendir(path);
    if (!dir) return;

    for (int i = 0; i < pack_count; i++)
        free(packs[i]);
    free(packs);

    packs = NULL;
    pack_count = 0;

    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        packs = realloc(packs,
                        sizeof(char*) * (pack_count + 1));

        packs[pack_count++] = strdup(ent->d_name);
    }

    closedir(dir);
}

// ----------------------------
// refresh dropdown
// ----------------------------
static void refresh_combo() {
    GtkStringList *model = gtk_string_list_new(NULL);

    int selected_index = 0;

    for (int i = 0; i < pack_count; i++) {
        gtk_string_list_append(model, packs[i]);

        if (strcmp(packs[i], selected_pack) == 0)
            selected_index = i;
    }

    gtk_drop_down_set_model(GTK_DROP_DOWN(combo),
                            G_LIST_MODEL(model));

    gtk_drop_down_set_selected(GTK_DROP_DOWN(combo),
                                selected_index);

    load_pack_image(selected_pack);
}

// ----------------------------
// apply selection
// ----------------------------
static void on_apply(GtkButton *btn, gpointer user_data) {
    int index = gtk_drop_down_get_selected(GTK_DROP_DOWN(combo));

    if (index < 0 || index >= pack_count)
        return;

    strncpy(selected_pack, packs[index],
            sizeof(selected_pack) - 1);

    write_config(selected_pack);
    load_pack_image(selected_pack);
}

// ----------------------------
// dropdown change
// ----------------------------
static void on_pack_changed(GObject *obj,
                            GParamSpec *pspec,
                            gpointer data) {
    int index = gtk_drop_down_get_selected(GTK_DROP_DOWN(combo));

    if (index < 0 || index >= pack_count)
        return;

    load_pack_image(packs[index]);
}

// ----------------------------
// folder picker
// ----------------------------
static void folder_selected_cb(GObject *source,
                               GAsyncResult *res,
                               gpointer user_data)
{
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);

    GError *error = NULL;
    GFile *file =
        gtk_file_dialog_select_folder_finish(dialog, res, &error);

    if (!file) return;

    char *path = g_file_get_path(file);

    free(install_path);
    install_path = strdup(path);

    gtk_label_set_text(GTK_LABEL(path_label), install_path);

    g_free(path);
    g_object_unref(file);

    scan_packs();
    refresh_combo();
}

// ----------------------------
// choose folder
// ----------------------------
static void on_choose_folder(GtkButton *btn,
                             gpointer user_data) {
    GtkFileDialog *dialog = gtk_file_dialog_new();

    gtk_file_dialog_select_folder(
        dialog,
        NULL,
        NULL,
        folder_selected_cb,
        NULL
    );
}

// ----------------------------
// UI
// ----------------------------
static void activate(GtkApplication *app,
                      gpointer user_data) {

    GtkWidget *window =
        gtk_application_window_new(app);

    gtk_window_set_title(GTK_WINDOW(window),
                         "Soundpack Manager");

    gtk_window_set_default_size(GTK_WINDOW(window),
                               256, 350);

    gtk_window_set_resizable(GTK_WINDOW(window),
                             FALSE);

    GtkWidget *box =
        gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    gtk_window_set_child(GTK_WINDOW(window), box);

    path_label =
        gtk_label_new("No TeamSpeak directory selected");

    gtk_box_append(GTK_BOX(box), path_label);

    GtkWidget *btn =
        gtk_button_new_with_label("Select TeamSpeak Folder");

    g_signal_connect(btn, "clicked",
                     G_CALLBACK(on_choose_folder), NULL);

    gtk_box_append(GTK_BOX(box), btn);

    combo = gtk_drop_down_new(NULL, NULL);

    g_signal_connect(combo, "notify::selected",
                     G_CALLBACK(on_pack_changed), NULL);

    gtk_box_append(GTK_BOX(box), combo);

    GtkWidget *apply =
        gtk_button_new_with_label("Apply");

    g_signal_connect(apply, "clicked",
                     G_CALLBACK(on_apply), NULL);

    gtk_box_append(GTK_BOX(box), apply);

    preview = gtk_picture_new();
    gtk_widget_set_size_request(preview, 256, 256);

    gtk_box_append(GTK_BOX(box), preview);

    load_config();

    if (install_path) {
        gtk_label_set_text(GTK_LABEL(path_label),
                           install_path);

        scan_packs();
        refresh_combo();
    }

    gtk_window_present(GTK_WINDOW(window));
}

// ----------------------------
// main
// ----------------------------
int main(int argc, char **argv) {
    GtkApplication *app =
        gtk_application_new("com.soundpack.ui",
                            G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(app, "activate",
                     G_CALLBACK(activate), NULL);

    int status =
        g_application_run(G_APPLICATION(app),
                          argc, argv);

    g_object_unref(app);
    return status;
}