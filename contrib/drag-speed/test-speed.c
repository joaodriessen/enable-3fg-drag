#include <assert.h>
#include <unistd.h>
#include "local-drag-speed.h"
static void config(const char *path, const char *text, double expected) {
    FILE *f = fopen(path, "w"); assert(f);
    fputs(text, f); fclose(f);
    assert(local_drag_speed_read() == expected);
}
int main(void) {
    unsetenv("THREE_FINGER_DRAG_SPEED_FILE");
    assert(local_drag_speed_read() == 1.0);
    char path[] = "/tmp/drag-speed-test-XXXXXX";
    int fd = mkstemp(path); assert(fd >= 0); close(fd);
    setenv("THREE_FINGER_DRAG_SPEED_FILE", path, 1);
    config(path, "0.5\n", 0.5);
    config(path, "0.1\n", 0.1);
    config(path, "1.0\n", 1.0);
    config(path, "nan\n", 1.0);
    config(path, "inf\n", 1.0);
    config(path, "0\n", 1.0);
    config(path, "-0.5\n", 1.0);
    config(path, "0.5 garbage\n", 1.0);
    config(path, "", 1.0);
    double x=12, y=-6, rx=8, ry=-4;
    local_drag_speed_scale(false, 0.5, &x, &y, &rx, &ry);
    assert(x==12 && y==-6 && rx==8 && ry==-4);
    local_drag_speed_scale(true, 0.5, &x, &y, &rx, &ry);
    assert(x==6 && y==-3 && rx==4 && ry==-2);
    local_drag_speed_scale(true, 1.0, &x, &y, &rx, &ry);
    assert(x==6 && y==-3 && rx==4 && ry==-2);
    unlink(path);
    assert(local_drag_speed_read() == 1.0);
    puts("PASS: drag-only scaling, normal movement, config bounds, invalid/missing config");
}
