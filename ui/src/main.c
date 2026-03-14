#include <gtk/gtk.h>
#include "app.h"

int main(int argc, char *argv[]) {
    PlanarApp *app = planar_app_new();
    int ret = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return ret;
}
