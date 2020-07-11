/*
AMiRo-OS is an operating system designed for the Autonomous Mini Robot (AMiRo) platform.
Copyright (C) 2016..2018  Thomas Schöpping et al.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file    STM32/hal_qei_lld.h
 * @brief   STM32 QEI subsystem low level driver header.
 *
 * @addtogroup QEI
 * @{
 */

#ifndef HAL_QEI_LLD_H
#define HAL_QEI_LLD_H

#if (HAL_USE_QEI == TRUE) || defined(__DOXYGEN__)

#include "stm32_tim.h"

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @brief   Number of input channels per QEI driver.
 */
#define QEI_CHANNELS                            2

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */
/**
 * @brief   QEID1 driver enable switch.
 * @details If set to @p TRUE the support for QEID1 is included.
 * @note    The default is @p TRUE.
 */
#if !defined(STM32_QEI_USE_TIM1) || defined(__DOXYGEN__)
#define STM32_QEI_USE_TIM1                  FALSE
#endif

/**
 * @brief   QEID2 driver enable switch.
 * @details If set to @p TRUE the support for QEID2 is included.
 * @note    The default is @p TRUE.
 */
#if !defined(STM32_QEI_USE_TIM2) || defined(__DOXYGEN__)
#define STM32_QEI_USE_TIM2                  FALSE
#endif

/**
 * @brief   QEID3 driver enable switch.
 * @details If set to @p TRUE the support for QEID3 is included.
 * @note    The default is @p TRUE.
 */
#if !defined(STM32_QEI_USE_TIM3) || defined(__DOXYGEN__)
#define STM32_QEI_USE_TIM3                  FALSE
#endif

/**
 * @brief   QEID4 driver enable switch.
 * @details If set to @p TRUE the support for QEID4 is included.
 * @note    The default is @p TRUE.
 */
#if !defined(STM32_QEI_USE_TIM4) || defined(__DOXYGEN__)
#define STM32_QEI_USE_TIM4                  FALSE
#endif

/**
 * @brief   QEID5 driver enable switch.
 * @details If set to @p TRUE the support for QEID5 is included.
 * @note    The default is @p TRUE.
 */
#if !defined(STM32_QEI_USE_TIM5) || defined(__DOXYGEN__)
#define STM32_QEI_USE_TIM5                  FALSE
#endif

/**
 * @brief   QEID8 driver enable switch.
 * @details If set to @p TRUE the support for QEID8 is included.
 * @note    The default is @p TRUE.
 */
#if !defined(STM32_QEI_USE_TIM8) || defined(__DOXYGEN__)
#define STM32_QEI_USE_TIM8                  FALSE
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if STM32_QEI_USE_TIM1 && !STM32_HAS_TIM1
#error "TIM1 not present in the selected device"
#endif

#if STM32_QEI_USE_TIM2 && !STM32_HAS_TIM2
#error "TIM2 not present in the selected device"
#endif

#if STM32_QEI_USE_TIM3 && !STM32_HAS_TIM3
#error "TIM3 not present in the selected device"
#endif

#if STM32_QEI_USE_TIM4 && !STM32_HAS_TIM4
#error "TIM4 not present in the selected device"
#endif

#if STM32_QEI_USE_TIM5 && !STM32_HAS_TIM5
#error "TIM5 not present in the selected device"
#endif

#if STM32_QEI_USE_TIM8 && !STM32_HAS_TIM8
#error "TIM8 not present in the selected device"
#endif

#if !STM32_QEI_USE_TIM1 && !STM32_QEI_USE_TIM2 &&                           \
    !STM32_QEI_USE_TIM3 && !STM32_QEI_USE_TIM4 &&                           \
    !STM32_QEI_USE_TIM5 && !STM32_QEI_USE_TIM8
#error "QEI driver activated but no TIM peripheral assigned"
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief QEI driver mode.
 */
typedef enum {
  QEI_COUNT_BOTH = 0,
  QEI_COUNT_CH1 = 1,
  QEI_COUNT_CH2 = 2,
} qeimode_t;

/**
 * @brief QEI input mode.
 */
typedef enum {
  QEI_INPUT_NONINVERTED = 0, /**< Input channel noninverted.*/
  QEI_INPUT_INVERTED = 1, /**< Input channel inverted.*/
} qeiinputmode_t;

/**
 * @brief   QEI count type.
 */
typedef uint32_t qeicnt_t;

/**
 * @brief   Driver channel configuration structure.
 */
typedef struct {
  /**
   * @brief Channel input logic.
   */
  qeiinputmode_t                 mode;
  /* End of the mandatory fields.*/
} QEIChannelConfig;

/**
 * @brief   Driver configuration structure.
 */
typedef struct {
  /**
   * @brief   Driver mode.
   */
  qeimode_t                 mode;
  /**
   * @brief   Channels configurations.
   */
  QEIChannelConfig          channels[QEI_CHANNELS];
  /**
   * @brief   Range in pulses.
   */
  qeicnt_t                  range;
  /* End of the mandatory fields.*/
} QEIConfig;

/**
 * @brief   Structure representing an QEI driver.
 */
struct QEIDriver {
  /**
   * @brief Driver state.
   */
  qeistate_t                state;
  /**
   * @brief Current configuration data.
   */
  const QEIConfig           *config;
#if defined(QEI_DRIVER_EXT_FIELDS)
  QEI_DRIVER_EXT_FIELDS
#endif
  /* End of the mandatory fields.*/
  /**
   * @brief Pointer to the TIMx registers block.
   */
  stm32_tim_t               *tim;
};

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/**
 * @brief   Returns the direction of the last transition.
 * @details The direction is defined as boolean and is
 *          calculated at each transition on any input.
 *
 * @param[in] qeip      pointer to the @p QEIDriver object
 * @return              The request direction.
 * @retval FALSE        Position counted up.
 * @retval TRUE         Position counted down.
 *
 * @iclass
 */
#define qei_lld_get_direction(qeip) !!((qeip)->tim->CR1 & TIM_CR1_DIR)

/**
 * @brief   Returns the position of the encoder.
 * @details The position is defined as number of pulses since last reset.
 *
 * @param[in] qeip      pointer to the @p QEIDriver object
 * @return              The number of pulses.
 *
 * @iclass
 */
#define qei_lld_get_position(qeip) ((qeip)->tim->CNT)

/**
 * @brief   Returns the range of the encoder.
 * @details The range is defined as number of maximum pulse count.
 *
 * @param[in] qeip      pointer to the @p QEIDriver object
 * @return              The number of pulses.
 *
 * @iclass
 */
#define qei_lld_get_range(qeip) ((qeip)->tim->ARR + 1)

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if STM32_QEI_USE_TIM1 && !defined(__DOXYGEN__)
extern QEIDriver QEID1;
#endif

#if STM32_QEI_USE_TIM2 && !defined(__DOXYGEN__)
extern QEIDriver QEID2;
#endif

#if STM32_QEI_USE_TIM3 && !defined(__DOXYGEN__)
extern QEIDriver QEID3;
#endif

#if STM32_QEI_USE_TIM4 && !defined(__DOXYGEN__)
extern QEIDriver QEID4;
#endif

#if STM32_QEI_USE_TIM5 && !defined(__DOXYGEN__)
extern QEIDriver QEID5;
#endif

#if STM32_QEI_USE_TIM8 && !defined(__DOXYGEN__)
extern QEIDriver QEID8;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void qei_lld_init(void);
  void qei_lld_start(QEIDriver *qeip);
  void qei_lld_stop(QEIDriver *qeip);
  void qei_lld_enable(QEIDriver *qeip);
  void qei_lld_disable(QEIDriver *qeip);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_QEI */

#endif /* HAL_QEI_LLD_H */

/** @} */
