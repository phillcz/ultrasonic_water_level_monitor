#define BITS_COUNT 71
#define BITS_SYNC 7
#define BIT1_LENGTH  1465
#define BIT0_LENGTH  975
#define BIT_TOLERANCE 100

#define DATA_PIN  D1

static uint16_t timings[BITS_COUNT];
static bool dataReady = false;
static uint8_t syncStart = (uint8_t) (-1);

static inline uint16_t getTiming(int timingIdx) {
    return timings[(BITS_COUNT + timingIdx) % BITS_COUNT];
}

static inline bool isOne(int timingIdx) {
    return (getTiming(timingIdx) > (BIT1_LENGTH - BIT_TOLERANCE)) && (getTiming(timingIdx) < (BIT1_LENGTH + BIT_TOLERANCE));
}

static inline bool isZero(int timingIdx) {
    return (getTiming(timingIdx) > (BIT0_LENGTH - BIT_TOLERANCE)) && (getTiming(timingIdx) < (BIT0_LENGTH + BIT_TOLERANCE));
}

static uint8_t getByte(int idx) {
    uint8_t ret = 0;
    uint8_t i = 0;

    /* first byte has only 7 bits */
    uint8_t bits = (idx == 0) ? 7 : 8;
    uint8_t offset = (idx == 0) ? syncStart : syncStart + 8 * idx - 1;

    for (i = 0; i < bits; i++) {
        ret = ret << 1;
        ret |= isOne(offset + i);
    }

    return ret;
}

static bool checkCrc() {
    return (getByte(0) ^
            getByte(1) ^
            getByte(2) ^
            getByte(3) ^
            getByte(4) ^
            getByte(5) ^
            getByte(6) ^
            getByte(7) ^
            getByte(8)) == 0x0;
}

/**
 * @return unit id, changes with transmitter power off&on
 */
static uint8_t getId() {
    return getByte(1);
}

/**
 * @return temp in Cx10
 */
static uint16_t getTemp() {
    return ((isOne(syncStart + 52) << 9) |
            (isOne(syncStart + 51) << 8) |
            (isOne(syncStart + 50) << 7) |
            (isOne(syncStart + 49) << 6) |
            (isOne(syncStart + 48) << 5) |
            (isOne(syncStart + 47) << 4) |
            (isOne(syncStart + 42) << 3) |
            (isOne(syncStart + 41) << 2) |
            (isOne(syncStart + 40) << 1) |
            (isOne(syncStart + 39) << 0)) - 400;
}

/**
 * @return distance in cm
 */
static uint16_t getDist() {
    return (isOne(syncStart + 28) << 9) |
            (isOne(syncStart + 27) << 8) |
            (isOne(syncStart + 26) << 7) |
            (isOne(syncStart + 25) << 6) |
            (isOne(syncStart + 24) << 5) |
            (isOne(syncStart + 23) << 4) |
            (isOne(syncStart + 34) << 3) |
            (isOne(syncStart + 33) << 2) |
            (isOne(syncStart + 32) << 1) |
            (isOne(syncStart + 31) << 0);
}

ICACHE_RAM_ATTR void handler() {
    static unsigned long duration = 0;
    static unsigned long lastTime = 0;
    static uint8_t idx = 0;

    if (dataReady == true) {
        return;
    }

    unsigned long time = micros();
    duration = time - lastTime;
    lastTime = time;

    idx = (idx + 1) % BITS_COUNT;
    timings[idx] = duration;

    /* Check that we received a valid bit */
    if (!isZero(idx) && !isOne(idx)) {
        syncStart = (uint8_t) (-1);
        return;
    }

    if (syncStart == (uint8_t) (-1)) {
        /* Try to find sync */
        syncStart = (BITS_COUNT + idx - BITS_SYNC + 1) % BITS_COUNT;
        if (getByte(0) != 0x5f /* 1011111 */) {
            syncStart = (uint8_t) (-1);
        }

        return;
    }

    if (idx == syncStart - 1) {
        /* All bits received */
        dataReady = true;
    }
    /* Need to capture more bits */
}

static void dumpTimings() {
    uint8_t i = 0;
    for (i = 0; i < BITS_COUNT; i++) {
        Serial.print(isOne(syncStart + i));
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Started.");

    pinMode(DATA_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(DATA_PIN), handler, RISING);
}

void loop() {
    if (dataReady) {
        detachInterrupt(digitalPinToInterrupt(DATA_PIN));

        dumpTimings();
        if (checkCrc()) {
            Serial.print(" id=");
            Serial.print(getId());
            Serial.print(" temp=");
            Serial.print(getTemp());
            Serial.print(" dist=");
            Serial.print(getDist());
        } else {
            Serial.print(" crc=error");
        }
        Serial.println("");

        dataReady = false;
        syncStart = (uint8_t) (-1);

        attachInterrupt(digitalPinToInterrupt(DATA_PIN), handler, RISING);
    }

    static unsigned long lastTime = 0;
    long time = micros();
    if (time - lastTime > 60 * 1000 * 1000) {
        lastTime = time;
        Serial.println(time);
    }
}
