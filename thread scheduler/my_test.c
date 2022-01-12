#include "sut.h"
#include <stdio.h>
#include <string.h>

int main(){
    printf("Calling sut_init();\n");
    sut_init();
    sut_shutdown();
}