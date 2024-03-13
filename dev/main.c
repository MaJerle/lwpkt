/*
 * main.c : Defines the entry point for the console application.
 */

#include <stdio.h>
#include <string.h>
#include "lwpkt/lwpkt.h"

extern void example_lwpkt(void);
extern void example_lwpkt_evt(void);
extern void test_lwpkt(void);

int
main() {
    test_lwpkt();
    //  example_lwpkt_evt();
    return 0;
}
