#include "proc/process_manager.h"
#include "shell/shell.h"

int main() {
    ProcessManager pm(16, 4096); // 16 frames, 4KB page size
    Shell shell(pm);
    shell.run();
    return 0;
}
