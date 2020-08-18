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
 * @file    hal_qei.h
 * @brief   QEI Driver macros and structures.
 *
 * @addtogroup QEI
 * @{
 */

#ifndef HAL_QEI_H
#define HAL_QEI_H

#if (HAL_USE_QEI == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   Driver state machine possible states.
 */
typedef enum {
  QEI_UNINIT = 0,                   /**< Not initialized.                   */
  QEI_STOP = 1,                     /**< Stopped.                           */
  QEI_READY = 2,                    /**< Ready.                             */
  QEI_ACTIVE = 4,                   /**< Active.                            */
} qeistate_t;

/**
 * @brief   Type of a structure representing an QEI driver.
 */
typedef struct QEIDriver QEIDriver;

#include "hal_qei_lld.h"

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/**
 * @name    Macro Functions
 * @{
 */
/**
 * @brief   Enables the quadrature encoder.
 *
 * @param[in] qeip      pointer to the @p QEIDriver object
 *
 * @iclass
 */
#define qeiEnableI(qeip) qei_lld_enable(qeip)

/**
 * @brief   Disables the quadrature encoder.
 *
 * @param[in] qeip      pointer to the @p QEIDriver object
 *
 * @iclass
 */
#define qeiDisableI(qeip) qei_lld_disable(qeip)

/**
 * @brief   Returns the direction of the last transition.
 * @details The direction is defined as boolean and is
 *          calculated at each transition on any input.
 *
 * @param[in] qeip      pointer to the @p QEIDriver object
 * @return              The request direction.
 * @retval FALSE        Position counted up.
 * @retval TRUE         Position counted down.
 * @iclass
 */
#define qeiGetDirectionI(qeip) qei_lld_get_direction(qeip)

/**
 * @brief   Returns the position of the encoder.
 * @details The position is defined as number of pulses since last reset.
 *
 * @param[in] qeip      pointer to the @p QEIDriver object
 * @return              The number of pulses.
 *
 * @iclass
 */
#define qeiGetPositionI(qeip) qei_lld_get_position(qeip)

/**
 * @brief   Returns the range of the encoder.
 * @details The range is defined as number of maximum pulse count.
 *
 * @param[in] qeip      pointer to the @p QEIDriver object
 * @return              The number of pulses.
 *
 * @iclass
 */
#define qeiGetRangeI(qeip) qei_lld_get_range(qeip)
/** @} */

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif
  void qeiInit(void);
  void qeiObjectInit(QEIDriver *qeip);
  void qeiStart(QEIDriver *qeip, const QEIConfig *config);
  void qeiStop(QEIDriver *qeip);
  void qeiEnable(QEIDriver *qeip);
  void qeiDisable(QEIDriver *qeip);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_QEI */

#endif /* HAL_QEI_H */

/** @} */
