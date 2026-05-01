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
  *   5) PC10 confirm button places the current ship
  *   6) Switch 8 selects double-ship orientation
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

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define ADC_CHANNEL_X              1   /* PA1 */
#define ADC_CHANNEL_Y              2   /* PA2 */
#define ADC_CHANNEL_ORIENTATION    3   /* PA3 */

#define BOARD_COLS                 8
#define BOARD_ROWS                 7
#define CURSOR_LEVELS              5

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
#define CONFIRM_BUTTON_PIN        10U

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
#define SIDE_RIGHT                 1U

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

#define ORIENT_HORIZONTAL          0
#define ORIENT_VERTICAL            1

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

/* Startup marquee message */
char Message[] =
{
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
uint8_t pc10IdleLevel = 0;
uint8_t gameState = GAME_STATE_TITLE;
uint8_t activePlayer = 0;
uint8_t activeShipIndex = 0;
uint8_t lastShotResult = SHOT_NONE;
uint8_t winningPlayer = 0;
uint8_t currentScrollIndex = 0;
uint8_t currentScrollLength = 0;
uint32_t stateStartTick = 0;
uint32_t lastScrollTick = 0;

uint8_t playerShips[PLAYER_COUNT][BOARD_ROWS][BOARD_COLS];
uint8_t playerShots[PLAYER_COUNT][BOARD_ROWS][BOARD_COLS];
const uint8_t shipLengths[SHIPS_PER_PLAYER] = {1, 1, 1, 2, 2};

char P1ShipsMessage[] =
{
    CHAR_P, CHAR_1, DASH, CHAR_S, CHAR_H, CHAR_I, CHAR_P, CHAR_S
};

char P2ShipsMessage[] =
{
    CHAR_P, CHAR_2, DASH, CHAR_S, CHAR_H, CHAR_I, CHAR_P, CHAR_S
};

char ReadyMessage[] =
{
    CHAR_R, CHAR_E, CHAR_A, CHAR_D, CHAR_Y, SPACE, SPACE, SPACE
};

char P1MoveMessage[] =
{
    CHAR_P, CHAR_1, DASH, CHAR_M, CHAR_O, CHAR_V, CHAR_E, SPACE
};

char P2MoveMessage[] =
{
    CHAR_P, CHAR_2, DASH, CHAR_M, CHAR_O, CHAR_V, CHAR_E, SPACE
};

char HitMessage[] =
{
    SPACE, SPACE, CHAR_H, CHAR_I, CHAR_T, SPACE, SPACE, SPACE
};

char MissMessage[] =
{
    SPACE, SPACE, CHAR_M, CHAR_I, CHAR_S, CHAR_S, SPACE, SPACE
};

char Player1WinsMessage[] =
{
    SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE,
    CHAR_P, CHAR_L, CHAR_A, CHAR_Y, CHAR_E, CHAR_R, SPACE, CHAR_1,
    SPACE, CHAR_W, CHAR_I, CHAR_N, CHAR_S,
    SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE
};

char Player2WinsMessage[] =
{
    SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE,
    CHAR_P, CHAR_L, CHAR_A, CHAR_Y, CHAR_E, CHAR_R, SPACE, CHAR_2,
    SPACE, CHAR_W, CHAR_I, CHAR_N, CHAR_S,
    SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE
};

char Player1GameOverMessage[] =
{
    SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE,
    CHAR_P, CHAR_L, CHAR_A, CHAR_Y, CHAR_E, CHAR_R, SPACE, CHAR_1,
    SPACE, CHAR_G, CHAR_A, CHAR_M, CHAR_E, DASH, CHAR_O, CHAR_V, CHAR_E, CHAR_R,
    SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE
};

char Player2GameOverMessage[] =
{
    SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE,
    CHAR_P, CHAR_L, CHAR_A, CHAR_Y, CHAR_E, CHAR_R, SPACE, CHAR_2,
    SPACE, CHAR_G, CHAR_A, CHAR_M, CHAR_E, DASH, CHAR_O, CHAR_V, CHAR_E, CHAR_R,
    SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE, SPACE
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
static void Advance_Setup_State(uint8_t nextState);
static void Run_Setup_State(void);
static uint8_t Segment_Row_To_Mask(uint8_t row);
static void Seven_Segment_Raw(uint8_t digit, uint8_t segmentMask);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static uint16_t ADC1_Read_Channel(uint8_t channel)
{
    uint16_t discard;

    ADC1->SQR3 = (channel & 0x1FU);

    if ((ADC1->SR & (1U << 1)) != 0)
    {
        discard = (uint16_t)ADC1->DR;
        (void)discard;
    }

    ADC1->CR2 |= (1U << 30);  /* start conversion */

    while ((ADC1->SR & (1U << 1)) == 0)
    {
    }

    /*
     * Discard the first sample after switching ADC channels. With high
     * impedance pots, the ADC sample capacitor can still be partly charged
     * from the previous channel, which makes X and Y appear linked together.
     */
    discard = (uint16_t)ADC1->DR;
    (void)discard;

    ADC1->CR2 |= (1U << 30);

    while ((ADC1->SR & (1U << 1)) == 0)
    {
    }

    return (uint16_t)(ADC1->DR & 0x0FFF);
}

static void Clear_Display(void)
{
    int i;
    for (i = 0; i < 8; i++)
    {
        Seven_Segment_Digit(i, SPACE, 0);
    }
}

static void Start_Title_Scroll(void)
{
    Message_Pointer = &Message[0];
    Save_Pointer = &Message[0];
    Message_Length = sizeof(Message) / sizeof(Message[0]);
    Delay_msec = TITLE_STEP_MS;
    Delay_counter = 0;
    Animate_On = 1;
}

static void Update_Title_Marquee(void)
{
    static uint32_t lastTitleTick = 0;
    static uint8_t titleIndex = 0;
    uint8_t digit;

    if ((HAL_GetTick() - lastTitleTick) >= TITLE_STEP_MS)
    {
        lastTitleTick = HAL_GetTick();
        titleIndex++;

        if (titleIndex > (Message_Length - 8U))
        {
            titleIndex = 0;
        }
    }

    for (digit = 0; digit < 8; digit++)
    {
        Seven_Segment_Digit((uint8_t)(7U - digit), Message[titleIndex + digit], 0);
    }
}

static void Start_Scroll_Message(uint8_t state)
{
    gameState = state;
    stateStartTick = HAL_GetTick();
    lastScrollTick = 0;
    currentScrollIndex = 0;

    if (state == GAME_STATE_WIN_SCROLL)
    {
        Message_Pointer = (winningPlayer == 0U) ? Player1WinsMessage : Player2WinsMessage;
        currentScrollLength = (winningPlayer == 0U) ?
            (uint8_t)(sizeof(Player1WinsMessage) / sizeof(Player1WinsMessage[0])) :
            (uint8_t)(sizeof(Player2WinsMessage) / sizeof(Player2WinsMessage[0]));
    }
    else
    {
        uint8_t losingPlayer = (winningPlayer == 0U) ? 1U : 0U;

        Message_Pointer = (losingPlayer == 0U) ? Player1GameOverMessage : Player2GameOverMessage;
        currentScrollLength = (losingPlayer == 0U) ?
            (uint8_t)(sizeof(Player1GameOverMessage) / sizeof(Player1GameOverMessage[0])) :
            (uint8_t)(sizeof(Player2GameOverMessage) / sizeof(Player2GameOverMessage[0]));
    }

    Clear_Display();
}

static void Render_Current_Scroll(void)
{
    uint8_t digit;

    if ((HAL_GetTick() - lastScrollTick) >= TITLE_STEP_MS)
    {
        lastScrollTick = HAL_GetTick();
        currentScrollIndex++;

        if (currentScrollIndex > (currentScrollLength - 8U))
        {
            currentScrollIndex = 0;
        }
    }

    for (digit = 0; digit < 8; digit++)
    {
        Seven_Segment_Digit((uint8_t)(7U - digit), Message_Pointer[currentScrollIndex + digit], 0);
    }
}

static void Show_Message_8(const char *message)
{
    uint8_t digit;

    for (digit = 0; digit < 8; digit++)
    {
        Seven_Segment_Digit((uint8_t)(7U - digit), (unsigned char)message[digit], 0);
    }
}

static void Update_Cursor_From_Pots(void)
{
    uint16_t xSlot;
    uint8_t yLevel;
    static const uint8_t leftPath[CURSOR_LEVELS] =
    {
        ROW_D, ROW_C, ROW_G, ROW_B, ROW_A
    };
    static const uint8_t rightPath[CURSOR_LEVELS] =
    {
        ROW_D, ROW_E, ROW_G, ROW_F, ROW_A
    };

    adcX = ADC1_Read_Channel(ADC_CHANNEL_X);
    adcY = ADC1_Read_Channel(ADC_CHANNEL_Y);

    xSlot = (uint16_t)((adcX * (BOARD_COLS * 2U)) / 4096U);
    yLevel = (uint8_t)((adcY * CURSOR_LEVELS) / 4096U);

    if (xSlot >= (BOARD_COLS * 2U)) xSlot = (BOARD_COLS * 2U) - 1U;
    if (yLevel >= CURSOR_LEVELS) yLevel = CURSOR_LEVELS - 1U;

    cursorCol = (uint8_t)(xSlot / 2U);
    cursorSide = (uint8_t)(xSlot & 1U);
    cursorRow = (cursorSide == SIDE_LEFT) ? leftPath[yLevel] : rightPath[yLevel];

    if (cursorCol >= BOARD_COLS) cursorCol = BOARD_COLS - 1;
    if (cursorRow >= BOARD_ROWS) cursorRow = BOARD_ROWS - 1;
}

static uint8_t Segment_Row_To_Mask(uint8_t row)
{
    static const uint8_t segmentByRow[BOARD_ROWS] =
    {
        SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G
    };

    return segmentByRow[row];
}

static void Seven_Segment_Raw(uint8_t digit, uint8_t segmentMask)
{
    uint16_t activeLowPattern = (uint16_t)(~segmentMask & 0x00FFU);

    GPIOE->ODR = (uint16_t)((0xFF00U | activeLowPattern) & ~(1U << (digit + 8U)));
    GPIOE->ODR |= 0xFF00U;
}

static void Init_Setup(void)
{
    int p, r, c;

    for (p = 0; p < PLAYER_COUNT; p++)
    {
        for (r = 0; r < BOARD_ROWS; r++)
        {
            for (c = 0; c < BOARD_COLS; c++)
            {
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

static uint8_t Confirm_Button_Pressed(void)
{
    uint8_t pc10Level = ((GPIOC->IDR & (1U << CONFIRM_BUTTON_PIN)) != 0) ? 1U : 0U;
    uint8_t pc10Pressed = (pc10Level != pc10IdleLevel) ? 1U : 0U;
    uint8_t confirmPressed = pc10Pressed;
    static uint32_t lastConfirmTick = 0;
    uint8_t pressedEvent = 0;

    if ((confirmPressed > 0) && (lastConfirmPressed == 0) &&
        ((HAL_GetTick() - lastConfirmTick) >= FIRE_DEBOUNCE_MS))
    {
        lastConfirmTick = HAL_GetTick();
        pressedEvent = 1;
    }

    lastConfirmPressed = confirmPressed;
    return pressedEvent;
}

static uint8_t Read_Orientation(void)
{
    adcOrientation = ADC1_Read_Channel(ADC_CHANNEL_ORIENTATION);

    if (adcOrientation >= 2048U)
    {
        return ORIENT_VERTICAL;
    }

    return ORIENT_HORIZONTAL;
}

static uint8_t Vertical_Next_Row(uint8_t row, uint8_t side)
{
    if (row == ROW_D) return ROW_G;
    if (row == ROW_A) return ROW_G;
    if (row == ROW_G) return ROW_A;

    if (side == SIDE_LEFT)
    {
        if (row == ROW_F) return ROW_E;
        if (row == ROW_E) return ROW_F;
    }
    else
    {
        if (row == ROW_B) return ROW_C;
        if (row == ROW_C) return ROW_B;
    }

    return 0xFFU;
}

static uint8_t Ship_Cell(uint8_t startRow, uint8_t startCol, uint8_t length,
                         uint8_t orientation, uint8_t side, uint8_t index,
                         uint8_t *cellRow, uint8_t *cellCol)
{
    if (index >= length)
    {
        return 0;
    }

    *cellRow = startRow;
    *cellCol = startCol;

    if ((orientation == ORIENT_VERTICAL) && (length > 1U))
    {
        if (index > 0U)
        {
            *cellRow = Vertical_Next_Row(startRow, side);
            if (*cellRow == 0xFFU)
            {
                return 0;
            }
        }
    }
    else
    {
        *cellCol = (uint8_t)(startCol + index);
    }

    return 1;
}

static uint8_t Ship_Cell_At(uint8_t startRow, uint8_t startCol, uint8_t length,
                            uint8_t orientation, uint8_t side,
                            uint8_t cellRow, uint8_t cellCol)
{
    uint8_t i;

    for (i = 0; i < length; i++)
    {
        uint8_t row, col;

        if (Ship_Cell(startRow, startCol, length, orientation, side, i, &row, &col) == 0)
        {
            return 0;
        }

        if ((row == cellRow) && (col == cellCol))
        {
            return 1;
        }
    }

    return 0;
}

static uint8_t Can_Place_Ship(uint8_t player, uint8_t startRow, uint8_t startCol,
                              uint8_t length, uint8_t orientation, uint8_t side)
{
    uint8_t i;

    for (i = 0; i < length; i++)
    {
        uint8_t row, col;

        if (Ship_Cell(startRow, startCol, length, orientation, side, i, &row, &col) == 0)
        {
            return 0;
        }

        if ((row >= BOARD_ROWS) || (col >= BOARD_COLS))
        {
            return 0;
        }

        if (playerShips[player][row][col] > 0)
        {
            return 0;
        }
    }

    return 1;
}

static void Advance_Setup_State(uint8_t nextState)
{
    gameState = nextState;
    stateStartTick = HAL_GetTick();
    lastConfirmPressed = 0;
    if ((((GPIOC->IDR & (1U << CONFIRM_BUTTON_PIN)) != 0) ? 1U : 0U) != pc10IdleLevel)
    {
        lastConfirmPressed = 1;
    }
    Clear_Display();
}

static void Place_Current_Ship(void)
{
    uint8_t i;
    uint8_t length = shipLengths[activeShipIndex];
    uint8_t orientation = Read_Orientation();

    if (Can_Place_Ship(activePlayer, cursorRow, cursorCol, length, orientation, cursorSide) == 0)
    {
        GPIOD->ODR |= (1U << 13);      /* orange = invalid placement */
        GPIOD->ODR &= ~(1U << 12);
        return;
    }

    for (i = 0; i < length; i++)
    {
        uint8_t row, col;

        (void)Ship_Cell(cursorRow, cursorCol, length, orientation, cursorSide, i, &row, &col);
        playerShips[activePlayer][row][col] = 1;
    }

    activeShipIndex++;
    GPIOD->ODR |= (1U << 12);          /* green = placed */
    GPIOD->ODR &= ~(1U << 13);

    if (activeShipIndex >= SHIPS_PER_PLAYER)
    {
        if (activePlayer == 0)
        {
            activePlayer = 1;
            activeShipIndex = 0;
            Advance_Setup_State(GAME_STATE_P2_MESSAGE);
        }
        else
        {
            Advance_Setup_State(GAME_STATE_READY);
        }
    }
}

static void Render_Placement_Board(uint8_t player)
{
    uint8_t col, row;
    uint8_t length = shipLengths[activeShipIndex];
    uint8_t orientation = Read_Orientation();
    uint8_t placementValid = Can_Place_Ship(player, cursorRow, cursorCol, length, orientation, cursorSide);

    for (col = 0; col < BOARD_COLS; col++)
    {
        uint8_t digitMask = 0;

        for (row = 0; row < BOARD_ROWS; row++)
        {
            if (playerShips[player][row][col] > 0)
            {
                digitMask |= Segment_Row_To_Mask(row);
            }

            if ((cursorBlinkOn > 0) &&
                (Ship_Cell_At(cursorRow, cursorCol, length, orientation, cursorSide, row, col) > 0))
            {
                if ((placementValid > 0) || (pwmPhase < 5))
                {
                    digitMask |= Segment_Row_To_Mask(row);
                }
            }
        }

        Seven_Segment_Raw((uint8_t)(7U - col), digitMask);
    }

    if (placementValid > 0)
    {
        GPIOD->ODR &= ~(1U << 13);
    }
    else
    {
        GPIOD->ODR |= (1U << 13);
    }
}

static void Fire_Current_Shot(void)
{
    uint8_t opponent = (activePlayer == 0U) ? 1U : 0U;

    if (playerShots[activePlayer][cursorRow][cursorCol] != SHOT_NONE)
    {
        GPIOD->ODR |= (1U << 13);      /* orange = already tried */
        return;
    }

    if (playerShips[opponent][cursorRow][cursorCol] > 0)
    {
        playerShots[activePlayer][cursorRow][cursorCol] = SHOT_HIT;
        lastShotResult = SHOT_HIT;
        GPIOD->ODR |= (1U << 12);      /* green = hit */
        GPIOD->ODR &= ~(1U << 13);

        if (Player_Has_Won(activePlayer) > 0)
        {
            winningPlayer = activePlayer;
        }
    }
    else
    {
        playerShots[activePlayer][cursorRow][cursorCol] = SHOT_MISS;
        lastShotResult = SHOT_MISS;
        GPIOD->ODR |= (1U << 13);      /* orange = miss */
        GPIOD->ODR &= ~(1U << 12);
    }

    if (activePlayer == 0U)
    {
        Advance_Setup_State(GAME_STATE_P1_RESULT);
    }
    else
    {
        Advance_Setup_State(GAME_STATE_P2_RESULT);
    }
}

static void Render_Shot_Board(uint8_t player)
{
    uint8_t col, row;

    for (col = 0; col < BOARD_COLS; col++)
    {
        uint8_t digitMask = 0;

        for (row = 0; row < BOARD_ROWS; row++)
        {
            uint8_t shot = playerShots[player][row][col];

            if (shot == SHOT_HIT)
            {
                digitMask |= Segment_Row_To_Mask(row);
            }
            else if ((shot == SHOT_MISS) && (dimPwmPhase < MISS_DIM_ON_STEPS))
            {
                digitMask |= Segment_Row_To_Mask(row);
            }
        }

        if ((cursorBlinkOn > 0) &&
            (playerShots[player][cursorRow][cursorCol] == SHOT_NONE) &&
            (col == cursorCol))
        {
            digitMask |= Segment_Row_To_Mask(cursorRow);
        }

        Seven_Segment_Raw((uint8_t)(7U - col), digitMask);
    }
}

static uint8_t Player_Has_Won(uint8_t player)
{
    uint8_t opponent = (player == 0U) ? 1U : 0U;
    uint8_t row, col;

    for (row = 0; row < BOARD_ROWS; row++)
    {
        for (col = 0; col < BOARD_COLS; col++)
        {
            if ((playerShips[opponent][row][col] > 0) &&
                (playerShots[player][row][col] != SHOT_HIT))
            {
                return 0;
            }
        }
    }

    return 1;
}

static void Run_Setup_State(void)
{
    switch (gameState)
    {
        case GAME_STATE_P1_MESSAGE:
            Show_Message_8(P1ShipsMessage);
            if (Confirm_Button_Pressed() > 0)
            {
                activePlayer = 0;
                activeShipIndex = 0;
                Advance_Setup_State(GAME_STATE_P1_PLACE);
            }
            break;

        case GAME_STATE_P1_PLACE:
        case GAME_STATE_P2_PLACE:
            Update_Cursor_From_Pots();
            if (Confirm_Button_Pressed() > 0)
            {
                Place_Current_Ship();
            }
            Render_Placement_Board(activePlayer);
            break;

        case GAME_STATE_P2_MESSAGE:
            Show_Message_8(P2ShipsMessage);
            if (Confirm_Button_Pressed() > 0)
            {
                activePlayer = 1;
                activeShipIndex = 0;
                Advance_Setup_State(GAME_STATE_P2_PLACE);
            }
            break;

        case GAME_STATE_READY:
            Show_Message_8(ReadyMessage);
            GPIOD->ODR |= (1U << 15);
            if (Confirm_Button_Pressed() > 0)
            {
                activePlayer = 0;
                Advance_Setup_State(GAME_STATE_P1_MOVE_MSG);
            }
            break;

        case GAME_STATE_P1_MOVE_MSG:
            Show_Message_8(P1MoveMessage);
            if ((HAL_GetTick() - stateStartTick) >= SETUP_MESSAGE_MS)
            {
                activePlayer = 0;
                Advance_Setup_State(GAME_STATE_P1_SHOOT);
            }
            break;

        case GAME_STATE_P1_SHOOT:
            Update_Cursor_From_Pots();
            if (Confirm_Button_Pressed() > 0)
            {
                Fire_Current_Shot();
            }
            else
            {
                Render_Shot_Board(activePlayer);
            }
            break;

        case GAME_STATE_P1_RESULT:
            Show_Message_8((lastShotResult == SHOT_HIT) ? HitMessage : MissMessage);
            if ((HAL_GetTick() - stateStartTick) >= SHOT_RESULT_MS)
            {
                if (Player_Has_Won(0) > 0)
                {
                    winningPlayer = 0;
                    Start_Scroll_Message(GAME_STATE_WIN_SCROLL);
                }
                else
                {
                    activePlayer = 1;
                    Advance_Setup_State(GAME_STATE_P2_MOVE_MSG);
                }
            }
            break;

        case GAME_STATE_P2_MOVE_MSG:
            Show_Message_8(P2MoveMessage);
            if ((HAL_GetTick() - stateStartTick) >= SETUP_MESSAGE_MS)
            {
                activePlayer = 1;
                Advance_Setup_State(GAME_STATE_P2_SHOOT);
            }
            break;

        case GAME_STATE_P2_SHOOT:
            Update_Cursor_From_Pots();
            if (Confirm_Button_Pressed() > 0)
            {
                Fire_Current_Shot();
            }
            else
            {
                Render_Shot_Board(activePlayer);
            }
            break;

        case GAME_STATE_P2_RESULT:
            Show_Message_8((lastShotResult == SHOT_HIT) ? HitMessage : MissMessage);
            if ((HAL_GetTick() - stateStartTick) >= SHOT_RESULT_MS)
            {
                if (Player_Has_Won(1) > 0)
                {
                    winningPlayer = 1;
                    Start_Scroll_Message(GAME_STATE_WIN_SCROLL);
                }
                else
                {
                    activePlayer = 0;
                    Advance_Setup_State(GAME_STATE_P1_MOVE_MSG);
                }
            }
            break;

        case GAME_STATE_WIN_SCROLL:
            Render_Current_Scroll();
            if ((HAL_GetTick() - stateStartTick) >= WIN_SCROLL_MS)
            {
                Start_Scroll_Message(GAME_STATE_GAME_OVER_SCROLL);
            }
            break;

        case GAME_STATE_GAME_OVER_SCROLL:
            Render_Current_Scroll();
            if ((HAL_GetTick() - stateStartTick) >= WIN_SCROLL_MS)
            {
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

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
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
    GPIOD->MODER = 0x55555555;        /* Port D outputs */
    GPIOA->MODER &= ~((3U << 2) | (3U << 4) | (3U << 6));
    GPIOA->MODER |=  ((3U << 2) | (3U << 4) | (3U << 6)); /* PA1, PA2, PA3 analog */
    GPIOA->MODER &= ~(3U << 0);       /* PA0 blue button input */
    GPIOE->MODER |= 0x55555555;       /* Port E outputs */
    GPIOC->MODER &= ~(3U << (CONFIRM_BUTTON_PIN * 2U)); /* PC10 confirm input */
    GPIOC->PUPDR &= ~(3U << (CONFIRM_BUTTON_PIN * 2U));
    pc10IdleLevel = ((GPIOC->IDR & (1U << CONFIRM_BUTTON_PIN)) != 0) ? 1U : 0U;
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
    titleStartTick = HAL_GetTick();
    stateStartTick = titleStartTick;

    /* USER CODE END 2 */

    while (1)
    {
        /* Let the startup marquee run first before the board renderer takes over */
        if (!titleFinished)
        {
            Update_Title_Marquee();

            if ((HAL_GetTick() - titleStartTick) >= TITLE_SCROLL_MS)
            {
                Animate_On = 0;
                Clear_Display();
                titleFinished = 1;
                Advance_Setup_State(GAME_STATE_P1_MESSAGE);
            }

            HAL_Delay(5);
            continue;
        }

        /* Cursor blinking */
        if ((HAL_GetTick() - lastBlinkTick) >= CURSOR_BLINK_MS)
        {
            lastBlinkTick = HAL_GetTick();
            cursorBlinkOn ^= 1U;
        }

        /* PWM phase for dim miss markers */
        if ((HAL_GetTick() - lastPwmTick) >= 10)
        {
            lastPwmTick = HAL_GetTick();
            pwmPhase++;
            if (pwmPhase >= 10)
            {
                pwmPhase = 0;
            }
        }

        /*
         * Display-rate low-duty PWM for miss markers. This advances every
         * pass through the loop rather than on HAL's 1 ms tick, so MISS runs
         * much faster than the visible cursor blink.
         */
        dimPwmPhase++;
        if (dimPwmPhase >= DIM_PWM_STEPS)
        {
            dimPwmPhase = 0;
        }

        Run_Setup_State();

        GPIOD->ODR |= (1U << 14); /* red = program alive */

        HAL_Delay(0);
    }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
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
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
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

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief TIM7 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM7_Init(void)
{
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htim7.Instance = TIM7;
    htim7.Init.Prescaler = 0;
    htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim7.Init.Period = 65535;
    htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim7) != HAL_OK)
    {
        Error_Handler();
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

    if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    HAL_GPIO_WritePin(CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OTG_FS_PowerSwitchOn_GPIO_Port, OTG_FS_PowerSwitchOn_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOD, LD4_Pin | LD3_Pin | LD5_Pin | LD6_Pin | Audio_RST_Pin, GPIO_PIN_RESET);

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

    GPIO_InitStruct.Pin = LD4_Pin | LD3_Pin | LD5_Pin | LD6_Pin | Audio_RST_Pin;
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

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif


