//
// Created by An on 2025/12/14.
//

#ifndef Y_TRACK_DRV_UART_H
#define Y_TRACK_DRV_UART_H

#ifdef __cplusplus
extern "C" {
#endif

int shell_uart_init(int baudrate);

void shell_uart_putc(int ch);

char shell_uart_getc(void);

void shell_uart_puts(const char *str);


#ifdef __cplusplus
}
#endif



#endif //Y_TRACK_DRV_UART_H