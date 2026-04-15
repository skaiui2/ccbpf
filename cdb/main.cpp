#include "cdb.h"
#include <stdio.h>


int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: cdb <project_root>\n");
        return 1;
    }

    return CdbTool::run(argv[1]);
}
