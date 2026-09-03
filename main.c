#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define TICKS_GREEN    5U
#define TICKS_YELLOW   2U
#define TICKS_RED      4U
#define QUEUE_BUSY     6U    
#define LOG_LEN        20U

typedef enum { LIGHT_GREEN = 0, LIGHT_YELLOW, LIGHT_RED } LightState_t;

/* status bits */
#define BIT_NIGHT      0U
#define BIT_BUSY       1U
#define BIT_BLINK_ON   2U

#define SET_BIT(reg, n)    ((reg) |=  (uint8_t)(1U << (n)))
#define CLR_BIT(reg, n)    ((reg) &= (uint8_t)~(1U << (n)))
#define TOGGLE_BIT(reg, n) ((reg) ^=  (uint8_t)(1U << (n)))
#define READ_BIT(reg, n)   ((uint8_t)(((reg) >> (n)) & 1U))

static LightState_t light;
static uint8_t      status;        
static uint8_t      ticksLeft;      
static uint8_t      carsWaiting;
static uint32_t     carsPassed;
static uint32_t     totalTicks;
static char         logLine[LOG_LEN]; 

static void clear_input_buffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

static LightState_t nextState(LightState_t s) {
    if (s == LIGHT_GREEN) {
        return LIGHT_YELLOW;
    }
    if (s == LIGHT_YELLOW) {
        return LIGHT_RED;
    }
    return LIGHT_GREEN;
}

static uint8_t ticksFor(LightState_t s) {
    if (s == LIGHT_GREEN) {
        return READ_BIT(status, BIT_BUSY) ? (TICKS_GREEN + 2U) : TICKS_GREEN;
    }
    if (s == LIGHT_YELLOW) {
        return TICKS_YELLOW;
    }
    return TICKS_RED;
}

static void pushLog(char c) {
    memmove(logLine, logLine + 1, LOG_LEN - 1);
    logLine[LOG_LEN - 1] = c;
}

static void resetCrossing(void) {
    while (light != LIGHT_RED) {
        light = nextState(light);
    }
    status = 0U;
    carsWaiting = 0U;
    carsPassed = 0U;
    totalTicks = 0U;
    ticksLeft = ticksFor(LIGHT_RED);
    memset(logLine, ' ', LOG_LEN);
    printf("Crossing reset to default state.\n");
}

static void showLog(void) {
    printf("Log (oldest -> newest): [");
    for (size_t i = 0; i < LOG_LEN; ++i) {
        putchar(logLine[i]);
    }
    printf("]\n");
}

static void drawLight(void) {
    uint8_t is_night = READ_BIT(status, BIT_NIGHT);
    uint8_t blink = READ_BIT(status, BIT_BLINK_ON);

    printf("\n  +---+\n");
    printf("  | %s | Red\n", (!is_night && light == LIGHT_RED) ? "O" : " ");
    printf("  | %s | Yellow\n", (is_night ? (blink ? "O" : " ") : (light == LIGHT_YELLOW ? "O" : " ")));
    printf("  | %s | Green\n", (!is_night && light == LIGHT_GREEN) ? "O" : " ");
    printf("  +---+\n");

    if (is_night) {
        printf("Mode: NIGHT (Blinking) | Cars Waiting: %u\n", (unsigned int)carsWaiting);
    } else {
        const char *names[] = {"GREEN", "YELLOW", "RED"};
        printf("Colour: %s | Ticks Left: %u | Cars Waiting: %u\n",
               names[light], (unsigned int)ticksLeft, (unsigned int)carsWaiting);
    }
}

static void tick(void) {
    totalTicks++;
    if (READ_BIT(status, BIT_NIGHT)) {
        TOGGLE_BIT(status, BIT_BLINK_ON);
        pushLog(READ_BIT(status, BIT_BLINK_ON) ? 'y' : '.');
        return;
    }

    if (light == LIGHT_GREEN && carsWaiting > 0) {
        uint8_t passing = (carsWaiting >= 2U) ? 2U : carsWaiting;
        carsWaiting -= passing;
        carsPassed += passing;
        if (carsWaiting <= QUEUE_BUSY) {
            CLR_BIT(status, BIT_BUSY);
        }
    }

    pushLog(light == LIGHT_GREEN ? 'G' : (light == LIGHT_YELLOW ? 'Y' : 'R'));

    if (ticksLeft > 0) {
        ticksLeft--;
    }
    if (ticksLeft == 0) {
        light = nextState(light);
        ticksLeft = ticksFor(light);
    }
}

static void addCars(void) {
    int count = 0;
    printf("Enter number of cars arriving (1-50): ");
    if (scanf("%d", &count) != 1) {
        clear_input_buffer();
        printf("Invalid input.\n");
        return;
    }
    clear_input_buffer();
    if (count <= 0 || count > 50) {
        printf("Silly or invalid number of cars rejected.\n");
        return;
    }
    carsWaiting += (uint8_t)count;
    if (carsWaiting > QUEUE_BUSY) {
        SET_BIT(status, BIT_BUSY);
    }
    printf("Added %d cars. Total waiting: %u\n", count, (unsigned int)carsWaiting);
}

static void toggleNight(void) {
    TOGGLE_BIT(status, BIT_NIGHT);
    if (READ_BIT(status, BIT_NIGHT)) {
        SET_BIT(status, BIT_BLINK_ON);
        printf("Switched to NIGHT mode (Blinking Yellow).\n");
    } else {
        CLR_BIT(status, BIT_BLINK_ON);
        while (light != LIGHT_RED) {
            light = nextState(light);
        }
        ticksLeft = ticksFor(LIGHT_RED);
        printf("Switched to DAY mode (Reset to RED light).\n");
    }
}

static void crossingReport(void) {
    printf("\n=== CROSSING REPORT ===\n");
    printf("Total Ticks: %u\n", (unsigned int)totalTicks);
    printf("Cars Passed: %u\n", (unsigned int)carsPassed);
    printf("Cars Waiting: %u\n", (unsigned int)carsWaiting);
    printf("Night Mode: %s\n", READ_BIT(status, BIT_NIGHT) ? "YES" : "NO");
    printf("Busy Status: %s\n", READ_BIT(status, BIT_BUSY) ? "YES" : "NO");
    printf("Status Byte (Hex): 0x%02X\n", (unsigned int)status);
    printf("Status Byte (Bin): ");
    for (int i = 7; i >= 0; i--) {
        putchar(READ_BIT(status, (uint8_t)i) ? '1' : '0');
    }
    putchar('\n');
}

static void runMultipleTicks(void) {
    int count = 0;
    printf("Enter number of ticks to simulate (1-100): ");
    if (scanf("%d", &count) != 1) {
        clear_input_buffer();
        printf("Invalid input.\n");
        return;
    }
    clear_input_buffer();
    if (count <= 0 || count > 100) {
        printf("Invalid tick count.\n");
        return;
    }
    for (int i = 0; i < count; ++i) {
        tick();
    }
    printf("Simulated %d ticks.\n", count);
}

static void print_menu(void) {
    printf("\n--- TRAFFIC LIGHT ROBOT ---\n");
    printf("1. Tick (1 Second)\n");
    printf("2. Tick N Seconds\n");
    printf("3. Add Cars\n");
    printf("4. Toggle Night Mode\n");
    printf("5. Draw Light\n");
    printf("6. Show Log\n");
    printf("7. Crossing Report\n");
    printf("8. Reset Crossing\n");
    printf("9. Exit\n");
    printf("Select choice: ");
}

static int handle_menu(void) {
    print_menu();
    int choice = 0;
    if (scanf("%d", &choice) != 1) {
        clear_input_buffer();
        printf("Invalid choice.\n");
        return 1;
    }
    clear_input_buffer();

    switch (choice) {
        case 1: tick(); drawLight(); break;
        case 2: runMultipleTicks(); drawLight(); break;
        case 3: addCars(); break;
        case 4: toggleNight(); drawLight(); break;
        case 5: drawLight(); break;
        case 6: showLog(); break;
        case 7: crossingReport(); break;
        case 8: resetCrossing(); drawLight(); break;
        case 9: return 0;
        default: printf("Invalid option.\n"); break;
    }
    return 1;
}

static void run_app(void) {
    resetCrossing();
    int running = 1;
    do {
        running = handle_menu();
    } while (running);
}

int main(void) {
    run_app();
    return 0;
}