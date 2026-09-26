#include <neorv32.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "xmodem.h"

const char XM_SOH = 0x01;
const char XM_EOT = 0x04;
const char XM_ACK = 0x06;
const char XM_NAK = 0x15;
const char XM_ETB = 0x17;
const char XM_CAN = 0x18;

#define log_info(...) neorv32_uart_printf(NEORV32_UART0, "[INFO] " __VA_ARGS__)
#define log_error(...) neorv32_uart_printf(NEORV32_UART0, "[ERROR] " __VA_ARGS__)

int uart_getc() {
    if (neorv32_uart0_char_received()) {
        return (int)neorv32_uart0_char_received_get();
    }
    return -1;
}

void uart_putc(char c) {
    if (c == '\n') {
        neorv32_uart0_puts("\r\n");
    } else {
        neorv32_uart0_putc(c);
    }
}

volatile int timer_tick = 0;
typedef enum {
    ST_WAIT,
    ST_BLOCK,
    ST_BLOCK_INV,
    ST_PAYLOAD,
    ST_CHECKSUM,
    ST_FIN,
} xmodem_state_t;

struct xmodem_block {
    uint8_t number;
    uint8_t inverted;
    uint32_t data[128/4];
    uint8_t checksum;
};

void mti_irq_handler(void);

static void start_timer(void) {
    neorv32_clint_mtimecmp_set(-1);
    neorv32_rte_handler_install(TRAP_CODE_MTI, mti_irq_handler);
    neorv32_clint_mtimecmp_set(neorv32_clint_time_get() +
                               neorv32_sysinfo_get_clk());
    neorv32_cpu_csr_set(CSR_MIE, (1 << CSR_MIE_MTIE));
    neorv32_cpu_csr_set(CSR_MSTATUS, 1 << CSR_MSTATUS_MIE);
}

static void stop_timer(void) {
    neorv32_clint_mtimecmp_set(-1);
    neorv32_cpu_csr_clr(CSR_MIE, (1 << CSR_MIE_MTIE));
    neorv32_cpu_csr_clr(CSR_MSTATUS, 1 << CSR_MSTATUS_MIE);
    neorv32_rte_handler_uninstall(TRAP_CODE_MTI);
}

int xmodem_receive(uint32_t *base_addr) {
    uint32_t *dest_ptr = base_addr;
    struct xmodem_block curr_block;
    int ret = 0;
    uint8_t timeout_cnt = 0;
    uint8_t c = 0;
    uint8_t done = 0;
    xmodem_state_t curr_state = ST_WAIT;
    uint8_t checksum_receiver = 0;

    timer_tick = 0;
    // log_info("starting timer\r\n");
    start_timer();
    // log_info("timer started\r\n");

    while (!done) {
        switch (curr_state) {
        case ST_WAIT:
            if (timer_tick) {
                // log_info("Timer ticked\r\n");
                uart_putc(XM_NAK);
                timer_tick = 0;
            }
            if ((ret = uart_getc()) >= 0) {
                c = (uint8_t)ret;
                if (c == XM_SOH) {
                    // log_info("Start of Header received. Getting new block\r\n");
                    memset((void *)&curr_block, 0, sizeof(curr_block));
                    stop_timer();
                    curr_state = ST_BLOCK;
                } else if (c == XM_EOT) {
                    // log_info("End of transmission received\r\n");
                    curr_state = ST_WAIT;
                    uart_putc(XM_ACK);
                    done = 1;
                }
            }
            break;
        case ST_BLOCK:
            if ((ret = uart_getc()) >= 0) {
                // log_info("ST_BLOCK\r\n");
                curr_block.number = (uint8_t)ret;
                // log_info("block 0x%02x received\r\n", curr_block.number);
                curr_state = ST_BLOCK_INV;
            }
            break;
        case ST_BLOCK_INV:
            if ((ret = uart_getc()) >= 0) {
                // log_info("ST_BLOCK_INV\r\n");
                curr_block.inverted = (uint8_t)ret;
                // log_info("block inv 0x%02x received\r\n", curr_block.inverted);
                curr_state = ST_PAYLOAD;
            }
            break;
        case ST_PAYLOAD:
            // log_info("getting block %d\r\n", curr_block);
            checksum_receiver = xmodem_get_payload(curr_block.data);
            start_timer();
            timeout_cnt = 0;
            curr_state = ST_CHECKSUM;
            break;
        case ST_CHECKSUM:
            // // log_info("ST_CHECKSUM");
            if (timer_tick) {
                // log_info("Timer ticked\r\n");
                timer_tick = 0;
                timeout_cnt++;
                if (timeout_cnt >= 10) {
                    // log_error("Timeout!!\r\n");
                    // for (int i = 0; i < 10; ++i) {
                    //     uart_putc(uart_fd, XM_NAK);
                    // }
                    uart_putc(XM_CAN);
                    curr_state = ST_WAIT;
                    timeout_cnt = 0;
                    stop_timer();
                    return -1;
                }
            }
            if ((ret = uart_getc()) >= 0) {
                c = (unsigned char)ret;
                c &= 0xff;
                curr_block.checksum = c;
                // log_info("checksum: 0x%02x\r\n", c);
                curr_state = ST_FIN;
            }
            break;
        case ST_FIN:
            stop_timer();
            if (checksum_receiver != curr_block.checksum) {
                // log_error("CHECKSUMS NOT EQUAL! Have: 0x%02x, Want: 0x%02x\r\n",
                //          checksum_receiver, curr_block.checksum);
                uart_putc(XM_NAK);
                curr_state = ST_WAIT;
            } else {
                // log_info("Checksum fine (0x%02x == 0x%02x). Sending ACK\r\n",
                //         checksum_receiver, curr_block.checksum);
                // write data to the file/memory before sending ACK. Should be
                // well within time limits
                // memcpy((uint32_t *)(base_addr + ((curr_block.number - 1) * 128/4)),
                //        (uint32_t *)curr_block.data, 128);
                memcpy(dest_ptr, (uint32_t *)curr_block.data, 128);
                dest_ptr += (128 / 4);
                uart_putc(XM_ACK);
                curr_state = ST_WAIT;
            }
            break;
        }
    }
    // log_info("xmodem loading done\r\n");
    // neorv32_uart0_printf("curr_block[0]: 0x%x\r\n", curr_block.data[0]);
    // neorv32_uart0_printf("curr_block[1]: 0x%x\r\n", curr_block.data[1]);
    // neorv32_uart0_printf("curr_block[2]: 0x%x\r\n", curr_block.data[3]);
    // neorv32_uart0_printf("curr_block[3]: 0x%x\r\n", curr_block.data[3]);

    return 0;
}

uint8_t xmodem_get_payload(uint32_t *buf) {
    static uint32_t tmp_word = 0;
    int ret = 0;
    unsigned char c = 0;
    unsigned char checksum_receiver = 0;
    uint8_t byte_cnt = 0;

    while (byte_cnt < 128) {
        // log_info("ST_PAYLOAD, byte_cnt: %d\r\n", byte_cnt);
        if ((ret = uart_getc()) >= 0) {
            c = (unsigned char)ret;
            checksum_receiver += c;
            switch (byte_cnt%4) {
                case 0:
                    tmp_word = 0;
                    tmp_word |= ((uint32_t) c << 0);
                    break;
                case 1:
                    tmp_word |= ((uint32_t) c << 8);
                    break;
                case 2:
                    tmp_word |= ((uint32_t) c << 16);
                    break;
                case 3:
                    tmp_word |= ((uint32_t) c << 24);
                    buf[byte_cnt/4] = tmp_word;
                    break;
            }
            byte_cnt++;
            // log_info("payload 0x%02x, byte_cnt: %d", c, byte_cnt);
        }
    }
    // log_info("full block received. Checksum is 0x%02x\r\n", checksum_receiver);

    return checksum_receiver;
}

void mti_irq_handler() {
    // log_info("timer triggered");
    timer_tick = 1;
    neorv32_clint_mtimecmp_set(neorv32_clint_mtimecmp_get() +
                               neorv32_sysinfo_get_clk());
}
