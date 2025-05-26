#define DEBUG 1

#include <init.h>
#include <asm/io.h>
#include <linux/delay.h>

DECLARE_GLOBAL_DATA_PTR;

typedef struct {
    uint32_t pad[18];
    uint32_t gates[10];
} s5l87xx_clkcon;

#define S5L87XX_PWRCON(i)    (*((uint32_t volatile*)(0x3C500000 + ((i) == 1 ? 0x40 : 0x28))))

bool s5l87xx_clockgate_get_state(int gate)
{
    return !(S5L87XX_PWRCON(gate >> 5) & (1 << (gate & 0x1f)));
}

void s5l87xx_clockgate_enable(int gate, bool enable)
{
    if (enable) S5L87XX_PWRCON(gate >> 5) &= ~(1 << (gate & 0x1f));
    else S5L87XX_PWRCON(gate >> 5) |= 1 << (gate & 0x1f);
}

// TODO: enum with all? https://freemyipod.org/wiki/Nano2G_clock_gates
#define CLOCKGATE_TIMER 4
#define CLOCKGATE_UART 8
#define CLOCKGATE_USB_PHY 14
#define CLOCKGATE_USB_OTG (32 + 11)

struct s5l87xx_uart {
    uint32_t ulcon;    // 0x00
    uint32_t ucon;     // 0x04
    uint32_t ufcon;    // 0x08
    uint32_t pad1;     // 0x0c
    uint32_t utrstat;  // 0x10
    uint32_t pad2[3];  // 0x14
    uint32_t utxh;     // 0x20
    uint32_t pad3;     // 0x24
    uint32_t ubrdiv;   // 0x28
    uint32_t pad4[2];  // 0x2c
    uint32_t ubrconrx; // 0x34
    uint32_t ubrcontx; // 0x38
};

// These are all undocumented. The following is gathered from
// reverse-engineering work of the original iPod firmware.
//
// Reference: https://en.wikipedia.org/wiki/Korean_profanity

struct s5l87xx_timer {
    uint32_t con;     // 0x000
    uint32_t cmd;     // 0x004
    uint32_t data0;   // 0x008
    uint32_t data1;   // 0x00c
    uint32_t pre;     // 0x010
    uint32_t cnt;     // 0x014
};

struct s5l87xx_otgphy {
    uint32_t pwr;
    uint32_t con;
    uint32_t rstcon;
    uint32_t unk[4];
    uint32_t unkcon;
};

struct s5l87xx_buscon {
    uint32_t unk[3];
    uint32_t remap;
};

struct s5l87xx_lcdcon {
    uint32_t con;    // 0x00
    uint32_t cmd;    // 0x04
    uint32_t unk1;   // 0x08
    uint32_t unk2;   // 0x0C
    uint32_t ack;    // 0x10
    uint32_t read;   // 0x14
    uint32_t unk3;   // 0x18
    uint32_t status; // 0x1C
    uint32_t unk[8]; // 0x20
    uint32_t write;  // 0x40
};

#define S5L87XX_LCDCON ((volatile struct s5l87xx_lcdcon *)0x38600000)

static void s5l87xx_lcdcon_read_byte(uint8_t *out) {
    udelay(100);
    writel(0, &S5L87XX_LCDCON->ack);

    uint32_t status;
    do {
        status = readl(&S5L87XX_LCDCON->status);
    } while((status & 1) == 0);

    udelay(100);

    uint32_t data = readl(&S5L87XX_LCDCON->read);
    if (out != NULL) {
        *out = (data >> 1);
    }
}

static void s5l87xx_lcdcon_wait_ready() {
    debug("%s: start...\n", __func__);
    uint32_t status;
    do {
        status = readl(&S5L87XX_LCDCON->status);
    } while((status & (1<<4)) != 0);
    debug("%s: done.\n", __func__);
}

static void s5l87xx_lcdcon_transact_read(uint32_t cmd, uint32_t len, uint8_t *out) {
    writel(0x1000c20, &S5L87XX_LCDCON->con);
    s5l87xx_lcdcon_wait_ready();
    writel(cmd, &S5L87XX_LCDCON->cmd);

    // Discard first byte???
    s5l87xx_lcdcon_read_byte(out);

    for (uint32_t i = 0; i < len; i++) {
        s5l87xx_lcdcon_read_byte(out);
        debug("%s: out: %02x\n", __func__, *out);
        out++;
    }
}
#define PHYBASE 0x3C400000

static void s5l87xx_otgphy_off(void) {
    *((volatile uint32_t*)(PHYBASE + 0x00)) = 0xf;  /* PHY: Power down */
    udelay(10);
    *((volatile uint32_t*)(PHYBASE + 0x08)) = 7;  /* PHY: Assert Software Reset */
    udelay(10);
#if 0
    debug("s5l87xx_otgphy: turning off\n");
    volatile struct s5l87xx_otgphy *otgphy = (struct s5l87xx_otgphy *)0x3c400000;
    otgphy->pwr = 0xff;
    mdelay(10);
    otgphy->rstcon = 0xff;
    mdelay(10);
    otgphy->unkcon = 4;
#endif
}

static void s5l87xx_otgphy_on(void) {
    debug("s5l87xx_otgphy: turning on\n");
    s5l87xx_clockgate_enable(CLOCKGATE_USB_OTG, true);
    s5l87xx_clockgate_enable(CLOCKGATE_USB_PHY, true);
    mdelay(10);

    *((volatile uint32_t*)(PHYBASE + 0x00)) = 0;  /* PHY: Power up */
    udelay(10);
    *((volatile uint32_t*)(PHYBASE + 0x1c)) = 1;
    *((volatile uint32_t*)(PHYBASE + 0x44)) = 0xe3f;
    *((volatile uint32_t*)(PHYBASE + 0x08)) = 1;  /* PHY: Assert Software Reset */
    udelay(10);
    *((volatile uint32_t*)(PHYBASE + 0x08)) = 0;  /* PHY: Deassert Software Reset */
    udelay(10);
    *((volatile uint32_t*)(PHYBASE + 0x18)) = 0x600;
    *((volatile uint32_t*)(PHYBASE + 0x04)) = 0;
    udelay(400);
#if 0
    volatile struct s5l87xx_otgphy *otgphy = (struct s5l87xx_otgphy *)0x3c400000;
    otgphy->pwr = 0;
    mdelay(10);
    otgphy->rstcon = 1;
    mdelay(10);
    otgphy->rstcon = 0;
    mdelay(10);
    otgphy->unkcon = 6;
    otgphy->con = 1;
    mdelay(400);
#endif
}

void otg_phy_init(void *unused) {
    s5l87xx_otgphy_on();
}

void otg_phy_off(void *unused) {
    s5l87xx_otgphy_off();
}

enum s5l87xx_timer_id {
    //  Timers A, B, C, D: 16-bit
    S5L87XX_TIMER_A = 0,
    S5L87XX_TIMER_B = 1,
    S5L87XX_TIMER_C = 2,
    S5L87XX_TIMER_D = 3,
};

enum s5l87xx_timer_cmd {
    S5L87XX_TIMER_CMD_STOP = 0,
    S5L87XX_TIMER_CMD_START = 1,
    S5L87XX_TIMER_CMD_CLR = 2,
};

static struct s5l87xx_timer *s5l87xx_timer_registers(enum s5l87xx_timer_id id) {
    switch (id) {
    case S5L87XX_TIMER_A:
        return (struct s5l87xx_timer *)0x3c700000;
    case S5L87XX_TIMER_B:
        return (struct s5l87xx_timer *)0x3c700020;
    case S5L87XX_TIMER_C:
        return (struct s5l87xx_timer *)0x3c700040;
    case S5L87XX_TIMER_D:
        return (struct s5l87xx_timer *)0x3c700060;
    default:
        panic("requested invalid timer id %d", id);
    }
}

static int s5l87xx_timer_clockgate(enum s5l87xx_timer_id id) {
    return CLOCKGATE_TIMER;
}

static void s5l87xx_timer_configure_interval(enum s5l87xx_timer_id id) {
    debug("s5l87xx_timer: configuring %d in interval mode\n", id);
    s5l87xx_clockgate_enable(s5l87xx_timer_clockgate(id), true);

    volatile struct s5l87xx_timer *timer = s5l87xx_timer_registers(id);
    /* configure timer for 1000 Hz??? */
    timer->cmd = S5L87XX_TIMER_CMD_STOP;
    timer->con = (3 << 8) | (1 << 4);
    timer->pre = 511;
    timer->data0 = 0xffff;
    timer->data1 = 0xffff;
    timer->cmd = S5L87XX_TIMER_CMD_CLR;
}

static void s5l87xx_timer_start(enum s5l87xx_timer_id id) {
    debug("s5l87xx_timer: starting %d\n", id);
    volatile struct s5l87xx_timer *timer = s5l87xx_timer_registers(id);
    timer->cmd = S5L87XX_TIMER_CMD_START;
}

static void s5l87xx_timer_stop(enum s5l87xx_timer_id id) {
    debug("s5l87xx_timer: stopping %d\n", id);
    volatile struct s5l87xx_timer *timer = s5l87xx_timer_registers(id);
    timer->cmd = S5L87XX_TIMER_CMD_STOP;
}

static uint32_t s5l87xx_timer_read(enum s5l87xx_timer_id id) {
    volatile struct s5l87xx_timer *timer = s5l87xx_timer_registers(id);
    return timer->cnt;
}

int timer_init(void)
{
    s5l87xx_timer_configure_interval(S5L87XX_TIMER_C);
    s5l87xx_timer_start(S5L87XX_TIMER_C);

    return 0;
}

unsigned long timer_read_counter(void)
{
    static uint16_t last = 0;
    static uint16_t high = 0;
    uint16_t now = s5l87xx_timer_read(S5L87XX_TIMER_C);
    if (last > now) {
        high++;
    }
    last = now;
    return ((uint32_t)high << 16) | (uint32_t)now;
}

// TODO(q3k): move board early init to board
int board_early_init_f(void)
{
    debug("board_early_init_f\n");

    s5l87xx_clockgate_enable(CLOCKGATE_USB_OTG, true);
    s5l87xx_clockgate_enable(CLOCKGATE_USB_PHY, true);

    // Disable USB suspend. TODO(q3k): move this to DWC2?
    volatile uint32_t *pcgcctl = (uint32_t *)0x38800e00;
    *pcgcctl = 0;

    return 0;
}

#ifdef CONFIG_DEBUG_UART_BOARD_INIT
void board_debug_uart_init(void)
{
    s5l87xx_clockgate_enable(CLOCKGATE_UART, true);
}

#endif
