

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Battleship game for the STM32F407G-DISCO 7-seg board
  ******************************************************************************
  * This version implements Battleship ship setup:
  *   1) Startup BATTLESHIP marquee
  *   2) P1-SHIPS and P2-SHIPS setup screens
  *   3) The 8 digits x 7 LED segments are the 56 target spots
  *   4) Two-potentiometer cursor control
  *   5) PC11 confirm button places the current ship
  *   6) PA3 selects double-ship orientation
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_host.h"
#include "seg7.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
    uint16_t frequency;
    uint16_t durationMs;
} ToneStep;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define ADC_CHANNEL_X              1   /* PA1 */
#define ADC_CHANNEL_Y              2   /* PA2 */
#define ADC_CHANNEL_ORIENTATION    3   /* PA3 */

#define BOARD_COLS                 8
#define BOARD_ROWS                 7
#define CURSOR_SIDE_COUNT          3
#define SIDE_LEVELS                2
#define MIDDLE_LEVELS              3

#define SHOT_NONE                  0
#define SHOT_MISS                  1
#define SHOT_HIT                   2

#define CURSOR_BLINK_MS          250
#define TITLE_SCROLL_MS         5000
#define TITLE_STEP_MS            175
#define FIRE_DEBOUNCE_MS         220
#define SETUP_MESSAGE_MS        2000
#define SHOT_RESULT_MS          1500
#define WIN_SCROLL_MS           4500
#define DIM_PWM_STEPS             16
#define MISS_DIM_ON_STEPS          3
#define PLAYER_COUNT               2
#define SHIPS_PER_PLAYER           5
#define CONFIRM_BUTTON_PORT        GPIOC
#define CONFIRM_BUTTON_PIN        11U
#define CONFIRM_BUTTON_PRESSED     0U
#define CONFIRM_MIN_PRESS_MS      20U
#define CONFIRM_MAX_PRESS_MS     800U
#define SPEAKER_PIN                0U
#define TIM7_TICK_HZ          210000U

#define SEG_A                   0x01U
#define SEG_B                   0x02U
#define SEG_C                   0x04U
#define SEG_D                   0x08U
#define SEG_E                   0x10U
#define SEG_F                   0x20U
#define SEG_G                   0x40U
#define SEG_DP                  0x80U

#define ROW_A                      0U
#define ROW_B                      1U
#define ROW_C                      2U
#define ROW_D                      3U
#define ROW_E                      4U
#define ROW_F                      5U
#define ROW_G                      6U

#define SIDE_LEFT                  0U
#define SIDE_MIDDLE                1U
#define SIDE_RIGHT                 2U

#define GAME_STATE_TITLE           0
#define GAME_STATE_P1_MESSAGE      1
#define GAME_STATE_P1_PLACE        2
#define GAME_STATE_P2_MESSAGE      3
#define GAME_STATE_P2_PLACE        4
#define GAME_STATE_READY           5
#define GAME_STATE_P1_MOVE_MSG     6
#define GAME_STATE_P1_SHOOT        7
#define GAME_STATE_P1_RESULT       8
#define GAME_STATE_P2_MOVE_MSG     9
#define GAME_STATE_P2_SHOOT       10
#define GAME_STATE_P2_RESULT      11
#define GAME_STATE_WIN_SCROLL     12
#define GAME_STATE_GAME_OVER_SCROLL 13
#define GAME_STATE_OFF           14
#define GAME_STATE_BATTLE_MSG    15

#define ORIENT_HORIZONTAL          0
#define ORIENT_VERTICAL            1

#define SOUND_STARTUP              0
#define SOUND_SHIP_SCREEN          1
#define SOUND_SHIP_PLACED          2
#define SOUND_HIT                  3
#define SOUND_MISS                 4
#define SOUND_WIN                  5
#define SOUND_MOVE_SCREEN          6

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
I2S_HandleTypeDef hi2s3;
SPI_HandleTypeDef hspi1;
TIM_HandleTypeDef htim7;

/* USER CODE BEGIN PV */

/* These globals are expected by stm32f4xx_it.c */
int DelayValue = 50;

char ramp = 0;
char RED_BRT = 0;
char GREEN_BRT = 0;
char BLUE_BRT = 0;
char RED_STEP = 1;
char GREEN_STEP = 2;
char BLUE_STEP = 3;
char DIM_Enable = 0;
char Music_ON = 0;
int TONE = 0;
int COUNT = 0;
int INDEX = 0;
int Note = 0;
int Save_Note = 0;
int Vibrato_Depth = 1;
int Vibrato_Rate = 40;
int Vibrato_Count = 0;
char Animate_On = 0;
char Message_Length = 0;
char *Message_Pointer;
char *Save_Pointer;
int Delay_msec = 0;
int Delay_counter = 0;

/* Left in place for compatibility with the older starter structure */
int CRC_Tx = 0;
int CRC_Rx = 0;

/* Startup Game-Title message */
char Message[] = {
    SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE,
    CHAR_B, CHAR_A, CHAR_T, CHAR_T, CHAR_L, CHAR_E, CHAR_S, CHAR_H,
    CHAR_I, CHAR_P,
    SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE
};

/* Declare song array so interrupt code still links correctly */
//Music Song[100];


/* Battleship UI variables */
uint16_t adcX = 0;
uint16_t adcY = 0;
uint16_t adcOrientation = 0;

uint8_t cursorCol = 0;          /* 0..7, one 7-seg digit per column */
uint8_t cursorRow = 0;          /* 0..6, one LED segment per row */
uint8_t cursorSide = SIDE_LEFT; /* chooses the left/right vertical segment path */

uint8_t cursorBlinkOn = 1;
uint8_t pwmPhase = 0;
uint8_t dimPwmPhase = 0;
uint8_t lastConfirmPressed = 0;
uint8_t ignoreConfirmRelease = 0;
uint32_t confirmPressStartTick = 0;
uint8_t gameState = GAME_STATE_TITLE;
uint8_t activePlayer = 0;
uint8_t activeShipIndex = 0;
uint8_t lastShotResult = SHOT_NONE;
uint8_t winningPlayer = 0;
uint8_t currentScrollIndex = 0;
uint8_t currentScrollLength = 0;
uint32_t stateStartTick = 0;
uint32_t lastScrollTick = 0;

volatile uint16_t audioHalfPeriodTicks = 0;
volatile uint16_t audioTickCounter = 0;
volatile uint8_t audioPinHigh = 0;
const ToneStep *activeSound = 0;
uint8_t activeSoundLength = 0;
uint8_t activeSoundIndex = 0;
uint32_t activeSoundNoteTick = 0;

uint8_t playerShips[PLAYER_COUNT][BOARD_ROWS][BOARD_COLS];
uint8_t playerShots[PLAYER_COUNT][BOARD_ROWS][BOARD_COLS];
const uint8_t shipLengths[SHIPS_PER_PLAYER] = {1, 1, 1, 2, 2};


					// DISPLAY MESSAGES

char P1ShipsMessage[] = {
    CHAR_P, CHAR_1, DASH, CHAR_S, CHAR_H, CHAR_I, CHAR_P, CHAR_S
};


char P2ShipsMessage[] = {
    CHAR_P, CHAR_2, DASH, CHAR_S, CHAR_H, CHAR_I, CHAR_P, CHAR_S
};


char ReadyMessage[] = {
    CHAR_R, CHAR_E, CHAR_A, CHAR_D, CHAR_Y, CHAR_QUESTION, SPACE, SPACE
};


char BattleMessage[] = {
    CHAR_B, CHAR_A, CHAR_T, CHAR_T, CHAR_L, CHAR_E, CHAR_EXCLAMATION, SPACE
};


char P1MoveMessage[] = {
    CHAR_P, CHAR_1, DASH, CHAR_M, CHAR_O, CHAR_V, CHAR_E, SPACE
};


char P2MoveMessage[] = {
    CHAR_P, CHAR_2, DASH, CHAR_M, CHAR_O, CHAR_V, CHAR_E, SPACE
};


char HitMessage[] = {
    SPACE, SPACE, CHAR_H, CHAR_I, CHAR_T, CHAR_EXCLAMATION, SPACE, SPACE
};


char MissMessage[] = {
    SPACE, CHAR_M, CHAR_I, CHAR_S, CHAR_S, CHAR_EXCLAMATION, SPACE, SPACE
};


char Player1WinsMessage[] = {
    SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE,
    CHAR_P, CHAR_L, CHAR_A, CHAR_Y, CHAR_E, CHAR_R, SPACE, CHAR_1,
    SPACE, CHAR_W, CHAR_I, CHAR_N, CHAR_S, CHAR_EXCLAMATION,
    SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE
};


char Player2WinsMessage[] = {
    SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE,
    CHAR_P, CHAR_L, CHAR_A, CHAR_Y, CHAR_E, CHAR_R, SPACE, CHAR_2,
    SPACE, CHAR_W, CHAR_I, CHAR_N, CHAR_S, CHAR_EXCLAMATION,
    SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE
};


char Player1GameOverMessage[] = {
    SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE,
    CHAR_P, CHAR_L, CHAR_A, CHAR_Y, CHAR_E, CHAR_R, SPACE, CHAR_1,
    SPACE, CHAR_G, CHAR_A, CHAR_M, CHAR_E, DASH, CHAR_O, CHAR_V, CHAR_E, CHAR_R, CHAR_EXCLAMATION,
    SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE
};


char Player2GameOverMessage[] = {
    SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE,
    CHAR_P, CHAR_L, CHAR_A, CHAR_Y, CHAR_E, CHAR_R, SPACE, CHAR_2,
    SPACE, CHAR_G, CHAR_A, CHAR_M, CHAR_E, DASH, CHAR_O, CHAR_V, CHAR_E, CHAR_R, CHAR_EXCLAMATION,
    SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE
};



			//MUSIC TONES

const ToneStep StartupSound[] = {
    {196, 120}, {262, 120}, {330, 120}, {392, 160},
    {330, 100}, {392, 100}, {523, 220}
};


const ToneStep ShipScreenSound[] = {
    {196, 140}, {0, 40}, {165, 140}, {0, 40}, {131, 220}
};


const ToneStep ShipPlacedSound[] = {
    {131, 140}
};


const ToneStep MoveScreenSound[] = {
    {196, 150}, {0, 35}, {262, 360}
};


const ToneStep HitSound[] = {
    {523, 90}, {784, 140}
};


const ToneStep MissSound[] = {
    {165, 130}, {131, 180}
};


const ToneStep WinSound[] = {
    {262, 130}, {330, 110}, {392, 110}, {523, 170},
    {392, 110}, {523, 110}, {659, 180}, {523, 260}
};

/* USER CODE END PV */



/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM7_Init(void);
void MX_USB_HOST_Process(void);

/* USER CODE BEGIN PFP */

static uint16_t ADC1_Read_Channel(uint8_t channel);
static void Clear_Display(void);
static void Start_Title_Scroll(void);
static void Update_Title_Marquee(void);
static void Start_Scroll_Message(uint8_t state);
static void Render_Current_Scroll(void);
static void Show_Message_8(const char *message);
static void Update_Cursor_From_Pots(void);
static void Init_Setup(void);
static uint8_t Confirm_Button_Pressed(void);
static uint8_t Read_Orientation(void);
static uint8_t Vertical_Next_Row(uint8_t row, uint8_t side);
static uint8_t Horizontal_Next_Cell(uint8_t row, uint8_t col, uint8_t side,
                                    uint8_t *nextRow, uint8_t *nextCol);
static uint8_t Ship_Cell(uint8_t startRow, uint8_t startCol, uint8_t length,
                         uint8_t orientation, uint8_t side, uint8_t index,
                         uint8_t *cellRow, uint8_t *cellCol);
static uint8_t Ship_Cell_At(uint8_t startRow, uint8_t startCol, uint8_t length,
                            uint8_t orientation, uint8_t side,
                            uint8_t cellRow, uint8_t cellCol);
static uint8_t Can_Place_Ship(uint8_t player, uint8_t startRow, uint8_t startCol,
                              uint8_t length, uint8_t orientation, uint8_t side);
static void Place_Current_Ship(void);
static void Render_Placement_Board(uint8_t player);
static void Fire_Current_Shot(void);
static void Render_Shot_Board(uint8_t player);
static uint8_t Player_Has_Won(uint8_t player);
static void Play_Sound(uint8_t soundId);
static void Audio_Update(void);
static void Audio_Set_Frequency(uint16_t frequency);
static void Advance_Setup_State(uint8_t nextState);
static void Run_Setup_State(void);
static uint8_t Segment_Row_To_Mask(uint8_t row);
static void Seven_Segment_Raw(uint8_t digit, uint8_t segmentMask);
void Audio_Timer_Tick(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */



		// READS ONE ADC CHANNEL AND RETURNS THE SETTLED VALUE

static uint16_t ADC1_Read_Channel(uint8_t channel) {
    uint16_t discard;

    ADC1->SQR3 = (channel & 0x1FU);

    if ((ADC1->SR & (1U << 1)) != 0) {
        discard = (uint16_t)ADC1->DR;
        (void)discard;

    }

    ADC1->CR2 |= (1U << 30);  /* start conversion */

    while ((ADC1->SR & (1U << 1)) == 0) {
    }

    /*
     * Discard the first sample after switching ADC channels. With high
     * impedance pots, the ADC sample capacitor can still be partly charged
     * from the previous channel, which makes X and Y appear linked together.
     */
    discard = (uint16_t)ADC1->DR;
    (void)discard;

    ADC1->CR2 |= (1U << 30);

    while ((ADC1->SR & (1U << 1)) == 0) {
    }

    return (uint16_t)(ADC1->DR & 0x0FFF);
}



		// CLEARS THE DISPLAY

static void Clear_Display(void) {
    int i;
    for (i = 0; i < 8; i++) {
        Seven_Segment_Digit(i, SPACE, 0);

    }

}



		// STARTS THE BEGINNING BATTLESHIP TITLE

static void Start_Title_Scroll(void) {
    Message_Pointer = &Message[0];
    Save_Pointer = &Message[0];
    Message_Length = sizeof(Message) / sizeof(Message[0]);
    Delay_msec = TITLE_STEP_MS;
    Delay_counter = 0;
    Animate_On = 1;
}



		// MONITORS TIME OF OPENING SCROLL

static void Update_Title_Marquee(void) {
    static uint32_t lastTitleTick = 0;
    static uint8_t titleIndex = 0;
    uint8_t digit;

    if ((HAL_GetTick() - lastTitleTick) >= TITLE_STEP_MS) {
        lastTitleTick = HAL_GetTick();
        titleIndex++;

        if (titleIndex > (Message_Length - 8U)) {
            titleIndex = 0;

        }

    }

    for (digit = 0; digit < 8; digit++) {
        Seven_Segment_Digit((uint8_t)(7U - digit), Message[titleIndex + digit], 0);

    }

}



		// SCROLL MESSAGE TO PLAY WHEN ONE PLAYER WINS

static void Start_Scroll_Message(uint8_t state) {
    gameState = state;
    stateStartTick = HAL_GetTick();
    lastScrollTick = 0;
    currentScrollIndex = 0;

    if (state == GAME_STATE_WIN_SCROLL) {
        Play_Sound(SOUND_WIN);
        Message_Pointer = (winningPlayer == 0U) ? Player1WinsMessage : Player2WinsMessage;
        currentScrollLength = (winningPlayer == 0U) ?
            (uint8_t)(sizeof(Player1WinsMessage) / sizeof(Player1WinsMessage[0])) :
            (uint8_t)(sizeof(Player2WinsMessage) / sizeof(Player2WinsMessage[0]));

    }
    else {
        uint8_t losingPlayer = (winningPlayer == 0U) ? 1U : 0U;

        Message_Pointer = (losingPlayer == 0U) ? Player1GameOverMessage : Player2GameOverMessage;
        currentScrollLength = (losingPlayer == 0U) ?
            (uint8_t)(sizeof(Player1GameOverMessage) / sizeof(Player1GameOverMessage[0])) :
            (uint8_t)(sizeof(Player2GameOverMessage) / sizeof(Player2GameOverMessage[0]));

    }

    Clear_Display();
}



		// RENDERS THE CURRENT SCROLLING WIN OR GAME OVER MESSAGE

static void Render_Current_Scroll(void) {
    uint8_t digit;
    unsigned char displayChar;

    if ((HAL_GetTick() - lastScrollTick) >= TITLE_STEP_MS) {
        lastScrollTick = HAL_GetTick();
        currentScrollIndex++;

        if (currentScrollIndex > (currentScrollLength - 8U)) {
            currentScrollIndex = 0;

        }

    }

    for (digit = 0; digit < 8; digit++) {
        displayChar = (unsigned char)Message_Pointer[currentScrollIndex + digit];
        Seven_Segment_Digit((uint8_t)(7U - digit), displayChar, 0);

    }

}



		// DISPLAYS A FIXED 8 DIGIT MESSAGE

static void Show_Message_8(const char *message) {
    uint8_t digit;
    unsigned char displayChar;

    for (digit = 0; digit < 8; digit++) {
        displayChar = (unsigned char)message[digit];
        Seven_Segment_Digit((uint8_t)(7U - digit), displayChar, 0);

    }

}



		// UPDATES THE CURSOR POSITION FROM THE X AND Y POTENTIOMETERS

static void Update_Cursor_From_Pots(void) {
    uint16_t xSlot;
    uint8_t yLevel;
    static const uint8_t leftPath[SIDE_LEVELS] = {
        ROW_E, ROW_F
    };
    static const uint8_t middlePath[MIDDLE_LEVELS] = {
        ROW_D, ROW_G, ROW_A
    };
    static const uint8_t rightPath[SIDE_LEVELS] = {
        ROW_C, ROW_B
    };

    adcX = ADC1_Read_Channel(ADC_CHANNEL_X);
    adcY = ADC1_Read_Channel(ADC_CHANNEL_Y);

    xSlot = (uint16_t)((adcX * (BOARD_COLS * CURSOR_SIDE_COUNT)) / 4096U);

    if (xSlot >= (BOARD_COLS * CURSOR_SIDE_COUNT)) {
        xSlot = (BOARD_COLS * CURSOR_SIDE_COUNT) - 1U;

    }

    cursorCol = (uint8_t)(xSlot / CURSOR_SIDE_COUNT);
    cursorSide = (uint8_t)(xSlot % CURSOR_SIDE_COUNT);

    if (cursorSide == SIDE_MIDDLE) {
        yLevel = (uint8_t)((adcY * MIDDLE_LEVELS) / 4096U);
        if (yLevel >= MIDDLE_LEVELS) yLevel = MIDDLE_LEVELS - 1U;
        cursorRow = middlePath[yLevel];

    }
    else if (cursorSide == SIDE_LEFT) {
        yLevel = (uint8_t)((adcY * SIDE_LEVELS) / 4096U);
        if (yLevel >= SIDE_LEVELS) yLevel = SIDE_LEVELS - 1U;
        cursorRow = leftPath[yLevel];

    }
    else {
        yLevel = (uint8_t)((adcY * SIDE_LEVELS) / 4096U);
        if (yLevel >= SIDE_LEVELS) yLevel = SIDE_LEVELS - 1U;
        cursorRow = rightPath[yLevel];

    }

    if (cursorCol >= BOARD_COLS) cursorCol = BOARD_COLS - 1;
    if (cursorRow >= BOARD_ROWS) cursorRow = BOARD_ROWS - 1;
}



		// MAPS A LOGICAL BOARD ROW TO A PHYSICAL 7 SEGMENT LED

static uint8_t Segment_Row_To_Mask(uint8_t row) {
    static const uint8_t segmentByRow[BOARD_ROWS] = {
        SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G
    };

    return segmentByRow[row];

}



		// WRITES A RAW SEGMENT MASK DIRECTLY TO ONE 7 SEG DISPLAY

static void Seven_Segment_Raw(uint8_t digit, uint8_t segmentMask) {
    uint16_t activeLowPattern = (uint16_t)(~segmentMask & 0x00FFU);

    GPIOE->ODR = (uint16_t)((0xFF00U | activeLowPattern) & ~(1U << (digit + 8U)));
    GPIOE->ODR |= 0xFF00U;
}



		// RESETS ALL PLAYER SHIPS, SHOTS, AND GAME SETUP VARIABLES

static void Init_Setup(void) {
    int p, r, c;

    for (p = 0; p < PLAYER_COUNT; p++) {
        for (r = 0; r < BOARD_ROWS; r++) {
            for (c = 0; c < BOARD_COLS; c++) {
                playerShips[p][r][c] = 0;
                playerShots[p][r][c] = SHOT_NONE;

            }

        }

    }

    activePlayer = 0;
    activeShipIndex = 0;
    lastShotResult = SHOT_NONE;
    winningPlayer = 0;
    gameState = GAME_STATE_TITLE;
}



		// CHECKS FOR A DEBOUNCED PRESS OF THE CONFIRM BUTTON

static uint8_t Confirm_Button_Pressed(void) {
    uint8_t buttonLevel = ((CONFIRM_BUTTON_PORT->IDR & (1U << CONFIRM_BUTTON_PIN)) != 0) ? 1U : 0U;
    uint8_t confirmPressed = (buttonLevel == CONFIRM_BUTTON_PRESSED) ? 1U : 0U;
    static uint32_t lastConfirmTick = 0;
    uint32_t now = HAL_GetTick();
    uint32_t pressLength;
    uint8_t pressedEvent = 0;

    if ((confirmPressed > 0) && (lastConfirmPressed == 0)) {
        confirmPressStartTick = now;

    }
    else if ((confirmPressed == 0) && (lastConfirmPressed > 0)) {
        pressLength = now - confirmPressStartTick;

        if (ignoreConfirmRelease > 0) {
            ignoreConfirmRelease = 0;

        }
        else if ((pressLength >= CONFIRM_MIN_PRESS_MS) &&
                 (pressLength <= CONFIRM_MAX_PRESS_MS) &&
                 ((now - lastConfirmTick) >= FIRE_DEBOUNCE_MS)) {
            lastConfirmTick = now;
            pressedEvent = 1;

        }

    }

    lastConfirmPressed = confirmPressed;
    return pressedEvent;

}



		// READS THE THIRD POTENTIOMETER TO SELECT SHIP ORIENTATION

static uint8_t Read_Orientation(void) {
    adcOrientation = ADC1_Read_Channel(ADC_CHANNEL_ORIENTATION);

    if (adcOrientation >= 2048U) {
        return ORIENT_VERTICAL;

    }

    return ORIENT_HORIZONTAL;

}



		// FINDS THE NEXT VERTICAL SEGMENT FOR A TWO SEGMENT SHIP

static uint8_t Vertical_Next_Row(uint8_t row, uint8_t side) {
    if (side == SIDE_MIDDLE) {
        if (row == ROW_D) return ROW_G;
        if (row == ROW_A) return ROW_G;
        if (row == ROW_G) return ROW_A;

    }
    else if (side == SIDE_LEFT) {
        if (row == ROW_F) return ROW_E;
        if (row == ROW_E) return ROW_F;

    }
    else {
        if (row == ROW_B) return ROW_C;
        if (row == ROW_C) return ROW_B;

    }

    return 0xFFU;

}



		// FINDS THE NEXT HORIZONTAL LED FOR A TWO SEGMENT SHIP

static uint8_t Horizontal_Next_Cell(uint8_t row, uint8_t col, uint8_t side,
                                    uint8_t *nextRow, uint8_t *nextCol) {
    *nextRow = row;
    *nextCol = col;

    if (side == SIDE_LEFT) {
        if (row == ROW_E) {
            *nextRow = ROW_C;
            return 1;

        }
        if (row == ROW_F) {
            *nextRow = ROW_B;
            return 1;

        }
        return 0;

    }

    if (side == SIDE_RIGHT) {
        if (col >= (BOARD_COLS - 1U)) {
            return 0;

        }

        if (row == ROW_C) {
            *nextRow = ROW_E;
            *nextCol = (uint8_t)(col + 1U);
            return 1;

        }
        if (row == ROW_B) {
            *nextRow = ROW_F;
            *nextCol = (uint8_t)(col + 1U);
            return 1;

        }
        return 0;

    }

    if (col >= (BOARD_COLS - 1U)) {
        return 0;

    }

    *nextCol = (uint8_t)(col + 1U);
    return 1;

}



		// RETURNS THE ROW AND COLUMN FOR ONE CELL OF A SHIP

static uint8_t Ship_Cell(uint8_t startRow, uint8_t startCol, uint8_t length,
                         uint8_t orientation, uint8_t side, uint8_t index,
                         uint8_t *cellRow, uint8_t *cellCol) {
    if (index >= length) {
        return 0;

    }

    *cellRow = startRow;
    *cellCol = startCol;

    if (length <= 1U) {
        return 1;

    }

    if (index > 1U) {
        return 0;

    }

    if (orientation == ORIENT_VERTICAL) {
        if (index > 0U) {
            *cellRow = Vertical_Next_Row(startRow, side);
            if (*cellRow == 0xFFU) {
                return 0;

            }

        }

    }
    else if (index > 0U) {
        if (Horizontal_Next_Cell(startRow, startCol, side, cellRow, cellCol) == 0) {
            return 0;

        }

    }

    return 1;

}



		// CHECKS WHETHER A BOARD CELL IS PART OF THE CURRENT SHIP PREVIEW

static uint8_t Ship_Cell_At(uint8_t startRow, uint8_t startCol, uint8_t length,
                            uint8_t orientation, uint8_t side,
                            uint8_t cellRow, uint8_t cellCol) {
    uint8_t i;

    for (i = 0; i < length; i++) {
        uint8_t row, col;

        if (Ship_Cell(startRow, startCol, length, orientation, side, i, &row, &col) == 0) {
            return 0;

        }

        if ((row == cellRow) && (col == cellCol)) {
            return 1;

        }

    }

    return 0;

}



		// CHECKS IF THE CURRENT SHIP CAN BE PLACED WITHOUT OVERLAP OR EDGE ERRORS

static uint8_t Can_Place_Ship(uint8_t player, uint8_t startRow, uint8_t startCol,
                              uint8_t length, uint8_t orientation, uint8_t side) {
    uint8_t i;

    for (i = 0; i < length; i++) {
        uint8_t row, col;

        if (Ship_Cell(startRow, startCol, length, orientation, side, i, &row, &col) == 0) {
            return 0;

        }

        if ((row >= BOARD_ROWS) || (col >= BOARD_COLS)) {
            return 0;

        }

        if (playerShips[player][row][col] > 0) {
            return 0;

        }

    }

    return 1;

}



		// CHANGES THE GAME STATE AND STARTS ANY STATE ENTRY EFFECTS

static void Advance_Setup_State(uint8_t nextState) {
    gameState = nextState;
    stateStartTick = HAL_GetTick();
    lastConfirmPressed = 0;
    if ((((CONFIRM_BUTTON_PORT->IDR & (1U << CONFIRM_BUTTON_PIN)) != 0) ? 1U : 0U) == CONFIRM_BUTTON_PRESSED) {
        lastConfirmPressed = 1;
        ignoreConfirmRelease = 1;
        confirmPressStartTick = HAL_GetTick();

    }
    Clear_Display();

    if ((nextState == GAME_STATE_P1_MESSAGE) || (nextState == GAME_STATE_P2_MESSAGE)) {
        Play_Sound(SOUND_SHIP_SCREEN);

    }
    else if ((nextState == GAME_STATE_P1_MOVE_MSG) || (nextState == GAME_STATE_P2_MOVE_MSG)) {
        Play_Sound(SOUND_MOVE_SCREEN);

    }

}



		// PLACES THE CURRENT PLAYER'S NEXT SHIP IF THE POSITION IS VALID

static void Place_Current_Ship(void) {
    uint8_t i;
    uint8_t length = shipLengths[activeShipIndex];
    uint8_t orientation = Read_Orientation();

    if (Can_Place_Ship(activePlayer, cursorRow, cursorCol, length, orientation, cursorSide) == 0) {
        return;

    }

    for (i = 0; i < length; i++) {
        uint8_t row, col;

        (void)Ship_Cell(cursorRow, cursorCol, length, orientation, cursorSide, i, &row, &col);
        playerShips[activePlayer][row][col] = 1;

    }

    activeShipIndex++;
    Play_Sound(SOUND_SHIP_PLACED);
    if (activeShipIndex >= SHIPS_PER_PLAYER) {
        if (activePlayer == 0) {
            activePlayer = 1;
            activeShipIndex = 0;
            Advance_Setup_State(GAME_STATE_P2_MESSAGE);

        }
        else {
            Advance_Setup_State(GAME_STATE_READY);

        }

    }

}



		// RENDERS THE SHIP PLACEMENT BOARD AND CURRENT SHIP PREVIEW

static void Render_Placement_Board(uint8_t player) {
    uint8_t col, row;
    uint8_t length = shipLengths[activeShipIndex];
    uint8_t orientation = Read_Orientation();
    uint8_t placementValid = Can_Place_Ship(player, cursorRow, cursorCol, length, orientation, cursorSide);

    for (col = 0; col < BOARD_COLS; col++) {
        uint8_t digitMask = 0;

        for (row = 0; row < BOARD_ROWS; row++) {
            if (playerShips[player][row][col] > 0) {
                digitMask |= Segment_Row_To_Mask(row);

            }

            if ((cursorBlinkOn > 0) &&
                (Ship_Cell_At(cursorRow, cursorCol, length, orientation, cursorSide, row, col) > 0)) {
                if ((placementValid > 0) || (pwmPhase < 5)) {
                    digitMask |= Segment_Row_To_Mask(row);

                }

            }

        }

        Seven_Segment_Raw((uint8_t)(7U - col), digitMask);

    }

}



		// FIRES A SHOT AT THE CURRENT CURSOR LOCATION

static void Fire_Current_Shot(void) {
    uint8_t opponent = (activePlayer == 0U) ? 1U : 0U;

    if (playerShots[activePlayer][cursorRow][cursorCol] != SHOT_NONE) {
        return;

    }

    if (playerShips[opponent][cursorRow][cursorCol] > 0) {
        playerShots[activePlayer][cursorRow][cursorCol] = SHOT_HIT;
        lastShotResult = SHOT_HIT;
        Play_Sound(SOUND_HIT);
        if (Player_Has_Won(activePlayer) > 0) {
            winningPlayer = activePlayer;

        }

    }
    else {
        playerShots[activePlayer][cursorRow][cursorCol] = SHOT_MISS;
        lastShotResult = SHOT_MISS;
        Play_Sound(SOUND_MISS);
    }

    if (activePlayer == 0U) {
        Advance_Setup_State(GAME_STATE_P1_RESULT);

    }
    else {
        Advance_Setup_State(GAME_STATE_P2_RESULT);

    }

}



		// RENDERS A PLAYER'S SHOT HISTORY WITH HITS AND DIM MISSES

static void Render_Shot_Board(uint8_t player) {
    uint8_t col, row;

    for (col = 0; col < BOARD_COLS; col++) {
        uint8_t digitMask = 0;

        for (row = 0; row < BOARD_ROWS; row++) {
            uint8_t shot = playerShots[player][row][col];

            if (shot == SHOT_HIT) {
                digitMask |= Segment_Row_To_Mask(row);

            }
            else if ((shot == SHOT_MISS) && (dimPwmPhase < MISS_DIM_ON_STEPS)) {
                digitMask |= Segment_Row_To_Mask(row);

            }

        }

        if ((cursorBlinkOn > 0) &&
            (playerShots[player][cursorRow][cursorCol] == SHOT_NONE) &&
            (col == cursorCol)) {
            digitMask |= Segment_Row_To_Mask(cursorRow);

        }

        Seven_Segment_Raw((uint8_t)(7U - col), digitMask);

    }

}



		// CHECKS IF THE PLAYER HAS HIT ALL OF THE OPPONENT'S SHIP SEGMENTS

static uint8_t Player_Has_Won(uint8_t player) {
    uint8_t opponent = (player == 0U) ? 1U : 0U;
    uint8_t row, col;

    for (row = 0; row < BOARD_ROWS; row++) {
        for (col = 0; col < BOARD_COLS; col++) {
            if ((playerShips[opponent][row][col] > 0) &&
                (playerShots[player][row][col] != SHOT_HIT)) {
                return 0;

            }

        }

    }

    return 1;

}



		// SETS THE CURRENT SPEAKER TONE FREQUENCY

static void Audio_Set_Frequency(uint16_t frequency) {
    if (frequency == 0U) {
        audioHalfPeriodTicks = 0;
        audioTickCounter = 0;
        audioPinHigh = 0;
        GPIOD->BSRR = (uint32_t)(1U << (SPEAKER_PIN + 16U));
        return;

    }

    audioHalfPeriodTicks = (uint16_t)(TIM7_TICK_HZ / (2U * frequency));
    if (audioHalfPeriodTicks == 0U) {
        audioHalfPeriodTicks = 1U;

    }
    audioTickCounter = 0;

}



		// STARTS ONE OF THE PREDEFINED SOUND EFFECTS

static void Play_Sound(uint8_t soundId) {
    switch (soundId) {
        case SOUND_STARTUP:
            activeSound = StartupSound;
            activeSoundLength = (uint8_t)(sizeof(StartupSound) / sizeof(StartupSound[0]));
            break;

        case SOUND_SHIP_SCREEN:
            activeSound = ShipScreenSound;
            activeSoundLength = (uint8_t)(sizeof(ShipScreenSound) / sizeof(ShipScreenSound[0]));
            break;

        case SOUND_SHIP_PLACED:
            activeSound = ShipPlacedSound;
            activeSoundLength = (uint8_t)(sizeof(ShipPlacedSound) / sizeof(ShipPlacedSound[0]));
            break;

        case SOUND_MOVE_SCREEN:
            activeSound = MoveScreenSound;
            activeSoundLength = (uint8_t)(sizeof(MoveScreenSound) / sizeof(MoveScreenSound[0]));
            break;

        case SOUND_HIT:
            activeSound = HitSound;
            activeSoundLength = (uint8_t)(sizeof(HitSound) / sizeof(HitSound[0]));
            break;

        case SOUND_MISS:
            activeSound = MissSound;
            activeSoundLength = (uint8_t)(sizeof(MissSound) / sizeof(MissSound[0]));
            break;

        case SOUND_WIN:
            activeSound = WinSound;
            activeSoundLength = (uint8_t)(sizeof(WinSound) / sizeof(WinSound[0]));
            break;

        default:
            return;

    }

    activeSoundIndex = 0;
    activeSoundNoteTick = HAL_GetTick();
    Audio_Set_Frequency(activeSound[0].frequency);

}



		// ADVANCES THE CURRENT SOUND EFFECT TO THE NEXT NOTE WHEN NEEDED

static void Audio_Update(void) {
    if ((activeSound == 0) || (activeSoundIndex >= activeSoundLength)) {
        return;

    }

    if ((HAL_GetTick() - activeSoundNoteTick) >= activeSound[activeSoundIndex].durationMs) {
        activeSoundIndex++;
        activeSoundNoteTick = HAL_GetTick();

        if (activeSoundIndex >= activeSoundLength) {
            activeSound = 0;
            activeSoundLength = 0;
            Audio_Set_Frequency(0);

        }
        else {
            Audio_Set_Frequency(activeSound[activeSoundIndex].frequency);

        }

    }

}



		// TIMER CALLBACK THAT TOGGLES THE SPEAKER PIN FOR THE CURRENT TONE

void Audio_Timer_Tick(void) {
    if (audioHalfPeriodTicks == 0U) {
        return;

    }

    audioTickCounter++;
    if (audioTickCounter >= audioHalfPeriodTicks) {
        audioTickCounter = 0;
        audioPinHigh ^= 1U;

        if (audioPinHigh > 0) {
            GPIOD->BSRR = (uint32_t)(1U << SPEAKER_PIN);

        }
        else {
            GPIOD->BSRR = (uint32_t)(1U << (SPEAKER_PIN + 16U));

        }

    }

}



		// RUNS THE MAIN GAME STATE MACHINE

static void Run_Setup_State(void) {
    switch (gameState) {
        case GAME_STATE_P1_MESSAGE:
            Show_Message_8(P1ShipsMessage);
            if (Confirm_Button_Pressed() > 0) {
                activePlayer = 0;
                activeShipIndex = 0;
                Advance_Setup_State(GAME_STATE_P1_PLACE);

            }
            break;

        case GAME_STATE_P1_PLACE:
        case GAME_STATE_P2_PLACE:
            Update_Cursor_From_Pots();
            if (Confirm_Button_Pressed() > 0) {
                Place_Current_Ship();

            }
            Render_Placement_Board(activePlayer);
            break;

        case GAME_STATE_P2_MESSAGE:
            Show_Message_8(P2ShipsMessage);
            if (Confirm_Button_Pressed() > 0) {
                activePlayer = 1;
                activeShipIndex = 0;
                Advance_Setup_State(GAME_STATE_P2_PLACE);

            }
            break;

        case GAME_STATE_READY:
            Show_Message_8(ReadyMessage);
            if (Confirm_Button_Pressed() > 0) {
                activePlayer = 0;
                Advance_Setup_State(GAME_STATE_BATTLE_MSG);

            }
            break;

        case GAME_STATE_BATTLE_MSG:
            Show_Message_8(BattleMessage);
            if ((HAL_GetTick() - stateStartTick) >= SETUP_MESSAGE_MS) {
                activePlayer = 0;
                Advance_Setup_State(GAME_STATE_P1_MOVE_MSG);

            }
            break;

        case GAME_STATE_P1_MOVE_MSG:
            Show_Message_8(P1MoveMessage);
            if ((HAL_GetTick() - stateStartTick) >= SETUP_MESSAGE_MS) {
                activePlayer = 0;
                Advance_Setup_State(GAME_STATE_P1_SHOOT);

            }
            break;

        case GAME_STATE_P1_SHOOT:
            Update_Cursor_From_Pots();
            if (Confirm_Button_Pressed() > 0) {
                Fire_Current_Shot();

            }
            else {
                Render_Shot_Board(activePlayer);

            }
            break;

        case GAME_STATE_P1_RESULT:
            Show_Message_8((lastShotResult == SHOT_HIT) ? HitMessage : MissMessage);
            if ((HAL_GetTick() - stateStartTick) >= SHOT_RESULT_MS) {
                if (Player_Has_Won(0) > 0) {
                    winningPlayer = 0;
                    Start_Scroll_Message(GAME_STATE_WIN_SCROLL);

                }
                else {
                    activePlayer = 1;
                    Advance_Setup_State(GAME_STATE_P2_MOVE_MSG);

                }

            }
            break;

        case GAME_STATE_P2_MOVE_MSG:
            Show_Message_8(P2MoveMessage);
            if ((HAL_GetTick() - stateStartTick) >= SETUP_MESSAGE_MS) {
                activePlayer = 1;
                Advance_Setup_State(GAME_STATE_P2_SHOOT);

            }
            break;

        case GAME_STATE_P2_SHOOT:
            Update_Cursor_From_Pots();
            if (Confirm_Button_Pressed() > 0) {
                Fire_Current_Shot();

            }
            else {
                Render_Shot_Board(activePlayer);

            }
            break;

        case GAME_STATE_P2_RESULT:
            Show_Message_8((lastShotResult == SHOT_HIT) ? HitMessage : MissMessage);
            if ((HAL_GetTick() - stateStartTick) >= SHOT_RESULT_MS) {
                if (Player_Has_Won(1) > 0) {
                    winningPlayer = 1;
                    Start_Scroll_Message(GAME_STATE_WIN_SCROLL);

                }
                else {
                    activePlayer = 0;
                    Advance_Setup_State(GAME_STATE_P1_MOVE_MSG);

                }

            }
            break;

        case GAME_STATE_WIN_SCROLL:
            Render_Current_Scroll();
            if ((HAL_GetTick() - stateStartTick) >= WIN_SCROLL_MS) {
                Start_Scroll_Message(GAME_STATE_GAME_OVER_SCROLL);

            }
            break;

        case GAME_STATE_GAME_OVER_SCROLL:
            Render_Current_Scroll();
            if ((HAL_GetTick() - stateStartTick) >= WIN_SCROLL_MS) {
                Advance_Setup_State(GAME_STATE_OFF);

            }
            break;

        case GAME_STATE_OFF:
            Clear_Display();
            break;

        default:
            Advance_Setup_State(GAME_STATE_READY);
            break;
    }

}



/* USER CODE END 0 */


		// MAIN PROGRAM ENTRY POINT

int main(void) {
    uint32_t titleStartTick;
    uint32_t lastBlinkTick = 0;
    uint32_t lastPwmTick = 0;
    uint8_t titleFinished = 0;

    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_TIM7_Init();

    /* USER CODE BEGIN 2 */

    /*** Configure GPIOs ***/
    GPIOD->MODER &= ~(3U << (SPEAKER_PIN * 2U));
    GPIOD->MODER |=  (1U << (SPEAKER_PIN * 2U)); /* speaker output only */
    GPIOD->MODER &= ~((3U << 24U) | (3U << 26U) | (3U << 28U) | (3U << 30U));
    GPIOD->PUPDR &= ~((3U << 24U) | (3U << 26U) | (3U << 28U) | (3U << 30U));
    GPIOA->MODER &= ~((3U << 2) | (3U << 4) | (3U << 6));
    GPIOA->MODER |=  ((3U << 2) | (3U << 4) | (3U << 6)); /* PA1, PA2, PA3 analog */
    GPIOC->MODER &= ~(3U << (CONFIRM_BUTTON_PIN * 2U)); /* PC11 confirm input */
    GPIOC->PUPDR &= ~(3U << (CONFIRM_BUTTON_PIN * 2U));
    GPIOC->PUPDR |= (1U << (CONFIRM_BUTTON_PIN * 2U));  /* pull-up, button pulls low */
    GPIOE->MODER |= 0x55555555;       /* Port E outputs */
    GPIOE->ODR = 0xFFFF;              /* default high */

    /*** Configure ADC1 ***/
    RCC->APB2ENR |= (1U << 8);        /* ADC1 clock enable */
    ADC1->CR1 = 0;                    /* independent, single conversion */
    ADC1->CR2 = 0;
    ADC1->SQR1 = 0;                   /* one conversion in sequence */
    ADC1->SMPR2 &= ~((7U << 3) | (7U << 6) | (7U << 9));
    ADC1->SMPR2 |=  ((7U << 3) | (7U << 6) | (7U << 9)); /* max sample time for pots */
    ADC1->CR2 |= 1U;                  /* ADC1 on */

    /*** Timer 7 setup retained for later sound work ***/
    TIM7->PSC = 199;
    TIM7->ARR = 1;
    TIM7->DIER |= 1;
    TIM7->CR1 |= 1;

    Init_Setup();

    Start_Title_Scroll();
    Play_Sound(SOUND_STARTUP);
    titleStartTick = HAL_GetTick();
    stateStartTick = titleStartTick;

    /* USER CODE END 2 */

    while (1) {
        Audio_Update();

        /* Let the startup marquee run first before the board renderer takes over */
        if (!titleFinished) {
            Update_Title_Marquee();

            if ((HAL_GetTick() - titleStartTick) >= TITLE_SCROLL_MS) {
                Animate_On = 0;
                Clear_Display();
                titleFinished = 1;
                Advance_Setup_State(GAME_STATE_P1_MESSAGE);

            }

            HAL_Delay(5);
            continue;

        }

        /* Cursor blinking */
        if ((HAL_GetTick() - lastBlinkTick) >= CURSOR_BLINK_MS) {
            lastBlinkTick = HAL_GetTick();
            cursorBlinkOn ^= 1U;

        }

        /* PWM phase for dim miss markers */
        if ((HAL_GetTick() - lastPwmTick) >= 10) {
            lastPwmTick = HAL_GetTick();
            pwmPhase++;
            if (pwmPhase >= 10) {
                pwmPhase = 0;

            }

        }

        /*
         * Display-rate low-duty PWM for miss markers. This advances every
         * pass through the loop rather than on HAL's 1 ms tick, so MISS runs
         * much faster than the visible cursor blink.
         */
        dimPwmPhase++;
        if (dimPwmPhase >= DIM_PWM_STEPS) {
            dimPwmPhase = 0;

        }

        Run_Setup_State();

        HAL_Delay(0);
    }

}



		// SETS UP THE MAIN SYSTEM CLOCK

void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();

    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                  RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 |
                                  RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();

    }

}



		// INITIALIZES TIMER 7 FOR AUDIO TIMING

static void MX_TIM7_Init(void) {
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htim7.Instance = TIM7;
    htim7.Init.Prescaler = 0;
    htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim7.Init.Period = 65535;
    htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim7) != HAL_OK) {
        Error_Handler();

    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

    if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK) {
        Error_Handler();

    }

}



		// INITIALIZES THE GPIO PINS

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    HAL_GPIO_WritePin(CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OTG_FS_PowerSwitchOn_GPIO_Port, OTG_FS_PowerSwitchOn_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOD, Audio_RST_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = CS_I2C_SPI_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(CS_I2C_SPI_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = OTG_FS_PowerSwitchOn_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(OTG_FS_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = PDM_OUT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(PDM_OUT_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = B1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = BOOT1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BOOT1_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = CLK_IN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(CLK_IN_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = Audio_RST_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = OTG_FS_OverCurrent_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(OTG_FS_OverCurrent_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = MEMS_INT2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(MEMS_INT2_GPIO_Port, &GPIO_InitStruct);
}



		// STOPS THE PROGRAM IF A HARDWARE SETUP ERROR OCCURS

void Error_Handler(void) {
    __disable_irq();
    while (1) {
    }

}

#ifdef USE_FULL_ASSERT



		// HANDLES FULL ASSERT FAILURES WHEN ASSERTS ARE ENABLED

void assert_failed(uint8_t *file, uint32_t line) {
}
#endif



