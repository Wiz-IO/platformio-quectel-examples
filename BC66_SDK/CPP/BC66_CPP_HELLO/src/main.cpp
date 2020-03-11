#include <os_wizio.h>
#include <string>
UART u;

void proc_main_task(s32 taskId)
{
    ST_MSG m;
    os_init();          // init CPP
    u = uart_create(0); // for port 0
    uart_begin(u, 0);   // open uart
    uart_retarget(u);   // enable printf
    printf("Hello World, Quectel BC66 2020 Georgi Angelov\n");
    std::string s = "We think in generalities, ";
    s += "but we live in details.";
    printf("%s\n", s.c_str());
    while (1)
    {
        Ql_OS_GetMessage(&m);
        printf("[MSG] %d, %d, %d\n", m.message, m.param1, m.param2);
        switch (m.message)
        {
        case MSG_ID_RIL_READY:
            Ql_RIL_Initialize();
            printf("[APP] Ril Ready\n");
            break;
        }
    }
}

/*
    Hello World, Quectel BC66 2020 Georgi Angelov
    We think in generalities, but we live in details.
    [MSG] 4097, 0, 0
    [APP] Ril Ready
    [MSG] 4098, 2, 1
    [MSG] 4098, 3, 2
    [MSG] 4098, 3, 1
*/
