/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : seg7.h
  * @brief          : Header for seg7.c library file.
  *                   This file contains the common defines of the library.
  ******************************************************************************
  *
  *
  * Copyright (c) 2023 Boise State University
  * All rights reserved.
  *
  * This file provides a library for outputting characters to the 7 segment
  * displays on the STM32F4 Discovery Peripheral Board for ECE330L lab.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SEG7_H
#define __SEG7_H


#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

void Seven_Segment_Digit(unsigned char digit, unsigned char hex_char, unsigned char dot); // show one digit
void Seven_Segment_ASCII(unsigned char digit, unsigned char ascii_char, unsigned char dot); // show ascii
void Seven_Segment(unsigned int HexValue); // show full value

#define SPACE 45
#define DOT 36
#define DASH 44
#define EQUAL 37
#define UNDER 41
#define OVER 38

#define CHAR_0 0
#define CHAR_1 1
#define CHAR_2 2
#define CHAR_3 3
#define CHAR_4 4
#define CHAR_5 5
#define CHAR_6 6
#define CHAR_7 7
#define CHAR_8 8
#define CHAR_9 9
#define CHAR_A 10
#define CHAR_B 11
#define CHAR_C 12
#define CHAR_D 13
#define CHAR_E 14
#define CHAR_F 15
#define CHAR_G 16
#define CHAR_H 17
#define CHAR_I 18
#define CHAR_J 19
#define CHAR_K 20
#define CHAR_L 21
#define CHAR_M 22
#define CHAR_N 23
#define CHAR_O 24
#define CHAR_P 25
#define CHAR_Q 26
#define CHAR_R 27
#define CHAR_S 28
#define CHAR_T 29
#define CHAR_U 30
#define CHAR_V 31
#define CHAR_W 32
#define CHAR_X 33
#define CHAR_Y 34
#define CHAR_Z 35

#endif /* __SEG7_H */
