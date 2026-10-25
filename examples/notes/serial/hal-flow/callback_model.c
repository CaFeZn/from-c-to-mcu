/* C99, single-threaded event model; not HAL or an MCU simulator. */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef enum { MODEL_OK, MODEL_ERROR, MODEL_BUSY } Status;
typedef enum { RESET, READY, BUSY_TX } GlobalState;
typedef enum { RX_READY, RX_BUSY } RxState;
typedef enum { RX_COMPLETE, MSP_INIT, INVALID_ID } CallbackId;
typedef struct Uart Uart;
typedef void (*Callback)(Uart *);

struct Uart {
    GlobalState gstate;
    RxState rxstate;
    Callback rx_callback;
    Callback msp_callback;
    unsigned remaining;
    unsigned callbacks;
    unsigned defaults;
    unsigned msp_calls;
    unsigned tx_callbacks;
    bool invalid_callback;
    bool irq_connected;
    bool rx_interrupt;
    bool rxne;
    bool tc_pending;
};

static void legacy_rx(Uart *u) { ++u->defaults; }
static void app_rx(Uart *u)
{
    assert(u->rxstate == RX_READY);
    assert(!u->rx_interrupt);
    ++u->callbacks;
}
static void app_msp(Uart *u) { ++u->msp_calls; }

static Status register_callback(Uart *u, CallbackId id, Callback callback)
{
    if (callback == NULL || id == INVALID_ID ||
        !(u->gstate == READY || (u->gstate == RESET && id == MSP_INIT))) {
        u->invalid_callback = true;
        return MODEL_ERROR;
    }
    if (id == RX_COMPLETE) u->rx_callback = callback;
    else u->msp_callback = callback;
    return MODEL_OK;
}

static void initialize(Uart *u)
{
    if (u->gstate == RESET) {
        u->rx_callback = legacy_rx;
        if (u->msp_callback != NULL) u->msp_callback(u);
    }
    u->gstate = READY;
    u->rxstate = RX_READY;
    u->invalid_callback = false;
}

static Status receive_it(Uart *u, unsigned count)
{
    if (u->rxstate != RX_READY) return MODEL_BUSY;
    if (count == 0u) return MODEL_ERROR;
    u->remaining = count;
    u->rxstate = RX_BUSY;
    u->rx_interrupt = true;
    return MODEL_OK;
}

static void uart_irq(Uart *u)
{
    if (!u->irq_connected) return;
    if (u->rxne && u->rx_interrupt && u->rxstate == RX_BUSY) {
        u->rxne = false;
        if (--u->remaining == 0u) {
            u->rx_interrupt = false;
            u->rxstate = RX_READY;
            u->rx_callback(u);
        }
    }
    if (u->tc_pending) {
        u->tc_pending = false;
        u->gstate = READY;
        ++u->tx_callbacks;
    }
}

static void byte_arrives(Uart *u)
{
    /* This model tests no-overrun input only. */
    assert(!u->rxne);
    u->rxne = true;
    uart_irq(u);
}

int main(void)
{
    Uart a = {0};
    Uart b = {0};
    assert(register_callback(&a, RX_COMPLETE, app_rx) == MODEL_ERROR);
    assert(a.invalid_callback);
    assert(register_callback(&a, MSP_INIT, app_msp) == MODEL_OK);
    initialize(&a);
    initialize(&b);
    assert(a.msp_calls == 1u && a.rx_callback == legacy_rx);
    assert(register_callback(&a, RX_COMPLETE, NULL) == MODEL_ERROR);
    assert(register_callback(&a, INVALID_ID, app_rx) == MODEL_ERROR);
    a.gstate = BUSY_TX;
    assert(register_callback(&a, RX_COMPLETE, app_rx) == MODEL_ERROR);
    a.gstate = READY;
    assert(register_callback(&a, RX_COMPLETE, app_rx) == MODEL_OK);
    puts("callback: RESET rejects RX; RESET accepts MSP; READY accepts RX");

    assert(receive_it(&a, 0u) == MODEL_ERROR);
    assert(receive_it(&a, 4u) == MODEL_OK);
    assert(receive_it(&a, 4u) == MODEL_BUSY);
    /* gState alone permits registration, even with active reception. */
    assert(register_callback(&a, RX_COMPLETE, app_rx) == MODEL_OK);
    byte_arrives(&a);
    assert(a.callbacks == 0u && a.remaining == 4u);
    a.irq_connected = true;
    uart_irq(&a);
    byte_arrives(&a);
    byte_arrives(&a);
    byte_arrives(&a);
    assert(a.callbacks == 1u && a.rxstate == RX_READY);
    uart_irq(&a);
    assert(a.callbacks == 1u);
    puts("callback: disconnected IRQ gives 0; four serviced bytes give 1");

    assert(receive_it(&a, 1u) == MODEL_OK);
    byte_arrives(&a);
    b.irq_connected = true;
    assert(receive_it(&b, 1u) == MODEL_OK);
    byte_arrives(&b);
    assert(a.callbacks == 2u && b.callbacks == 0u && b.defaults == 1u);
    assert(register_callback(&a, RX_COMPLETE, legacy_rx) == MODEL_OK);
    assert(receive_it(&a, 1u) == MODEL_OK);
    byte_arrives(&a);
    assert(a.defaults == 1u && a.callbacks == 2u);
    puts("callback: one-shot rearm and per-handle dispatch verified");

    a.gstate = BUSY_TX;
    a.irq_connected = false;
    /* Normal DMA completion enables the later UART TC interrupt. */
    a.tc_pending = true;
    uart_irq(&a);
    assert(a.tx_callbacks == 0u && a.gstate == BUSY_TX);
    a.irq_connected = true;
    uart_irq(&a);
    assert(a.tx_callbacks == 1u && a.gstate == READY);
    puts("callback: normal DMA completion still waits for UART TC IRQ");
    puts("callback model: PASS");
    return 0;
}
