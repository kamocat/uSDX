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
 * @file    hal_qei.c
 * @brief   QEI Driver code.
 *
 * @addtogroup QEI
 * @{
 */

#include "hal.h"

#if (HAL_USE_QEI == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

/*===========================================================================*/
/* Driver local variables.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   QEI Driver initialization.
 * @note    This function is implicitly invoked by @p halInit(), there is
 *          no need to explicitly initialize the driver.
 *
 * @init
 */
void qeiInit(void) {

  qei_lld_init();
}

/**
 * @brief   Initializes the standard part of a @p QEIDriver structure.
 *
 * @param[out] qeip     pointer to the @p QEIDriver object
 *
 * @init
 */
void qeiObjectInit(QEIDriver *qeip) {

  qeip->state  = QEI_STOP;
  qeip->config = NULL;
}

/**
 * @brief   Configures and activates the QEI peripheral.
 *
 * @param[in] qeip      pointer to the @p QEIDriver object
 * @param[in] config    pointer to the @p QEIConfig object
 *
 * @api
 */
void qeiStart(QEIDriver *qeip, const QEIConfig *config) {

  osalDbgCheck((qeip != NULL) && (config != NULL));

  osalSysLock();
  osalDbgAssert((qeip->state == QEI_STOP) || (qeip->state == QEI_READY), "invalid state");
  qeip->config = config;
  qei_lld_start(qeip);
  qeip->state = QEI_READY;
  osalSysUnlock();
}

/**
 * @brief   Deactivates the QEI peripheral.
 *
 * @param[in] qeip      pointer to the @p QEIDriver object
 *
 * @api
 */
void qeiStop(QEIDriver *qeip) {

  osalDbgCheck(qeip != NULL);

  osalSysLock();
  osalDbgAssert((qeip->state == QEI_STOP) || (qeip->state == QEI_READY), "invalid state");
  qei_lld_stop(qeip);
  qeip->state = QEI_STOP;
  osalSysUnlock();
}

/**
 * @brief   Enables the quadrature encoder.
 *
 * @param[in] qeip      pointer to the @p QEIDriver object
 *
 * @api
 */
void qeiEnable(QEIDriver *qeip) {

  osalDbgCheck(qeip != NULL);

  osalSysLock();
  osalDbgAssert(qeip->state == QEI_READY, "invalid state");
  qei_lld_enable(qeip);
  qeip->state = QEI_ACTIVE;
  osalSysUnlock();
}

/**
 * @brief   Disables the quadrature encoder.
 *
 * @param[in] qeip      pointer to the @p QEIDriver object
 *
 * @api
 */
void qeiDisable(QEIDriver *qeip) {

  osalDbgCheck(qeip != NULL);

  osalSysLock();
  osalDbgAssert((qeip->state == QEI_READY) || (qeip->state == QEI_ACTIVE), "invalid state");
  qei_lld_disable(qeip);
  qeip->state = QEI_READY;
  osalSysUnlock();
}

#endif /* HAL_USE_QEI */

/** @} */
