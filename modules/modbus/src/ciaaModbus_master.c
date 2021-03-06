/* Copyright 2014, Gustavo Muro
 *
 * This file is part of CIAA Firmware.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/** \brief This file implements the Modbus Slave main functionality
 **
 ** This file implements the main functionality of the Modbus
 **
 **/

/** \addtogroup CIAA_Firmware CIAA Firmware
 ** @{ */
/** \addtogroup Modbus CIAA Modbus
 ** @{ */

/*
 * Initials     Name
 * GMuro        Gustavo Muro
 *
 */

/*
 * modification history (new versions first)
 * -----------------------------------------------------------
 * 20141108 v0.0.1 GMuro   initial version
 */

/*==================[inclusions]=============================================*/

#include "ciaaPOSIX_stdio.h"
#include "ciaaModbus_master.h"
#include "ciaaModbus_transport.h"
#include "ciaaPOSIX_stdbool.h"
#include "ciaaModbus_config.h"
#include "os.h"

#if CIAA_MODBUS_TOTAL_MASTERS > 0

/*==================[macros and definitions]=================================*/

/** \brief Default timeout response (millisecond) */
#define CIAA_MODBUS_MASTER_DEFAULT_TIMEOUT      300

/** \brief Default retry communication */
#define CIAA_MODBUS_MASTER_DEFAULT_RETRY_COMM   3

/** \brief Object modbus master */
typedef struct
{
   modbusMaster_cbEndOfCommType cbEndComm; /** <- pointer to call back function */
   TaskType taskID;                    /** <- Task ID if blocking mode */
   int16_t *pData;                     /** <- pointer to read/write data */
   uint16_t respTimeout;               /** <- timeout configured */
   uint16_t retryComm;                 /** <- total retry if no correct respond */
   uint16_t retryCount;                /** <- retry count */
   uint16_t startAddressR;             /** <- start address to read */
   uint16_t startAddressW;             /** <- start address to write */
   uint16_t quantityR;                 /** <- quantity of read */
   uint16_t quantityW;                 /** <- quantity of write */
   uint8_t cmd;                        /** <- function to excecute on slave */
   uint8_t slaveId;                    /** <- slave ID */
   uint8_t exceptioncode;              /** <- exception code */
   bool inUse;                         /** <- Object in use */
}ciaaModbus_masterObjType;


/*==================[internal data declaration]==============================*/
/** \brief Array of Masters Object */
static ciaaModbus_masterObjType ciaaModbus_masterObj[CIAA_MODBUS_TOTAL_MASTERS];

/*==================[internal functions declaration]=========================*/

/*==================[internal data definition]===============================*/

/*==================[external data definition]===============================*/

/*==================[internal functions definition]==========================*/

/** \brief Process data to function 0x03
 **
 ** This function process PDU response as request function 0x03
 **
 ** \param[in] handler handler modbus master
 ** \param[in] pdu pdu received
 ** \param[out] size size of pdu.
 ** \return CIAA_MODBUS_E_NO_ERROR correct response
 **         CIAA_MODBUS_E_PDU_RECEIVED_WRONG incorrect response
 **/
static uint8_t ciaaModbus_masterProcess0x03(uint32_t hModbusMaster, uint8_t *pdu, uint16_t size)
{
   uint16_t loopi;
   uint8_t ret;

   /* check if valid byte count and PDU size */
   if ( (pdu[1] == (ciaaModbus_masterObj[hModbusMaster].quantityR * 2)) &&
        (size == (2 + ciaaModbus_masterObj[hModbusMaster].quantityR * 2) ) )
   {
      /* if valid, copy data received */
      for (loopi = 0 ; loopi < ciaaModbus_masterObj[hModbusMaster].quantityR ; loopi++)
      {
         /* copy data to holding registers address */
         ciaaModbus_masterObj[hModbusMaster].pData[loopi] = ciaaModbus_readInt(&pdu[2+loopi*2]);
      }

      /* return no error */
      ret = CIAA_MODBUS_E_NO_ERROR;
   }
   else
   {
      /* return wrong pdu */
      ret = CIAA_MODBUS_E_PDU_RECEIVED_WRONG;
   }

   return ret;
}

/*==================[external functions definition]==========================*/
extern void ciaaModbus_masterInit(void)
{
   int32_t loopi;

   /* initialize all Transport Objects */
   for (loopi = 0 ; loopi < CIAA_MODBUS_TOTAL_MASTERS ; loopi++)
   {
      /* no function to execute */
      ciaaModbus_masterObj[loopi].cmd = 0;

      /* invalid slave id */
      ciaaModbus_masterObj[loopi].slaveId = 0;

      /* not in use */
      ciaaModbus_masterObj[loopi].inUse = false;
   }
}

extern int32_t ciaaModbus_masterOpen(void)
{
   int32_t hModbusMaster = 0;

   /* enter critical section */
   GetResource(MODBUSR);

   /* search a Master Object not in use */
   while ( (hModbusMaster < CIAA_MODBUS_TOTAL_MASTERS) &&
           (ciaaModbus_masterObj[hModbusMaster].inUse == true) )
   {
      hModbusMaster++;
   }

   /* if object available, use it */
   if (hModbusMaster < CIAA_MODBUS_TOTAL_MASTERS)
   {
      /* set object in use */
      ciaaModbus_masterObj[hModbusMaster].inUse = true;

      /* no function to execute */
      ciaaModbus_masterObj[hModbusMaster].cmd = 0x00;

      /* set default timeout */
      ciaaModbus_masterObj[hModbusMaster].respTimeout = CIAA_MODBUS_MASTER_DEFAULT_TIMEOUT;

      /* set default retry comm */
      ciaaModbus_masterObj[hModbusMaster].retryComm = CIAA_MODBUS_MASTER_DEFAULT_RETRY_COMM;
   }
   else
   {
      /* if no object available, return invalid handler */
      hModbusMaster = -1;
   }

   /* exit critical section */
   ReleaseResource(MODBUSR);

   return hModbusMaster;
}

extern int8_t ciaaModbus_masterCmd0x03ReadHoldingReg(
      int32_t hModbusMaster,
      uint16_t startAddress,
      uint16_t quantity,
      int16_t *hrValue,
      uint8_t slaveId,
      modbusMaster_cbEndOfCommType cbEndComm)
{
   int8_t ret;

   /* check if no command pending and valid slave id */
   if ( (ciaaModbus_masterObj[hModbusMaster].cmd == 0) &&
      (0 != slaveId) )
   {
      /* no exception code */
      ciaaModbus_masterObj[hModbusMaster].exceptioncode = 0;

      /* set start address */
      ciaaModbus_masterObj[hModbusMaster].startAddressR = startAddress;

      /* set quantity of registers */
      ciaaModbus_masterObj[hModbusMaster].quantityR = quantity;

      /* set pointer to store data read */
      ciaaModbus_masterObj[hModbusMaster].pData = hrValue;

      /* set slave id */
      ciaaModbus_masterObj[hModbusMaster].slaveId = slaveId;

      /* set retry count */
      ciaaModbus_masterObj[hModbusMaster].retryCount = ciaaModbus_masterObj[hModbusMaster].retryComm;

      /* set task id to set event if blocking operation */
      GetTaskID(&ciaaModbus_masterObj[hModbusMaster].taskID);

      /* set call back if non-blocking operation */
      ciaaModbus_masterObj[hModbusMaster].cbEndComm = cbEndComm;

      /* set command to execute */
      ciaaModbus_masterObj[hModbusMaster].cmd = CIAA_MODBUS_FCN_READ_HOLDING_REGISTERS;

      /* if no callback wait event... */
      if (NULL == cbEndComm)
      {
         /* wait for event */
         WaitEvent(MODBUSE);

         ClearEvent(MODBUSE);

         /* return exception code */
         ret = ciaaModbus_masterObj[hModbusMaster].exceptioncode;
      }
      else
      {
         /* if non-blocking, return 0 */
         ret = 0;
      }
   }
   else
   {
      /* return -1 if it was not possible execute the function */
      ret = -1;
   }

   return ret;
}

extern void ciaaModbus_masterTask(int32_t handler)
{
   /* is there a function to perform? */
   if (0x00 != ciaaModbus_masterObj[handler].cmd)
   {
      /* if retry count available, decrement it */
      if (ciaaModbus_masterObj[handler].retryCount > 0)
      {
         ciaaModbus_masterObj[handler].retryCount--;
      }
      /* if no retry count available, cancel */
      else
      {
         /* if no exception code, set CIAA_MODBUS_E_SLAVE_NOT_RESPOND */
         if (ciaaModbus_masterObj[handler].exceptioncode == CIAA_MODBUS_E_NO_ERROR)
         {
            ciaaModbus_masterObj[handler].exceptioncode = CIAA_MODBUS_E_SLAVE_NOT_RESPOND;
         }
         /* if no callback, set event */
         if (NULL == ciaaModbus_masterObj[handler].cbEndComm)
         {
            SetEvent(ciaaModbus_masterObj[handler].taskID, MODBUSE);
         }
         else
         {
            /* if callback set, perform it */
            ciaaModbus_masterObj[handler].cbEndComm(
                  ciaaModbus_masterObj[handler].slaveId,
                  ciaaModbus_masterObj[handler].cmd,
                  ciaaModbus_masterObj[handler].exceptioncode);
         }

         /* cancel function */
         ciaaModbus_masterObj[handler].cmd = 0;
      }
   }
}

extern void ciaaModbus_masterRecvMsg(
      int32_t handler,
      uint8_t *id,
      uint8_t *pdu,
      uint32_t *size)
{
   /* is there function to perform? */
   if (0x00 != ciaaModbus_masterObj[handler].cmd)
   {
      /* set slave id */
      *id = ciaaModbus_masterObj[handler].slaveId;

      /* set function code */
      pdu[0] = ciaaModbus_masterObj[handler].cmd;

      /* according to function, make pdu */
      switch (pdu[0])
      {
         /* read holding registers */
         case CIAA_MODBUS_FCN_READ_HOLDING_REGISTERS:

            /* write in buffer: start address */
            ciaaModbus_writeInt(&pdu[1], ciaaModbus_masterObj[handler].startAddressR);

            /* write in buffer: quantity of registers */
            ciaaModbus_writeInt(&pdu[3], ciaaModbus_masterObj[handler].quantityR);

            /* lenght of pdu */
            *size = 5;

            break;

         /* function not defined */
         default:

            /* lenght of pdu */
            *size = 0;
            break;
      }
   }
}

extern void ciaaModbus_masterSendMsg(
      int32_t handler,
      uint8_t id,
      uint8_t *pdu,
      uint32_t size)
{
   uint32_t loopi;

   /* check slave id */
   if (id == ciaaModbus_masterObj[handler].slaveId)
   {
      /* if correct function code, process data */
      if (pdu[0] == ciaaModbus_masterObj[handler].cmd)
      {
         switch (pdu[0])
         {
            /* read holding register */
            case CIAA_MODBUS_FCN_READ_HOLDING_REGISTERS:
               ciaaModbus_masterObj[handler].exceptioncode = \
                  ciaaModbus_masterProcess0x03(handler, pdu, size);
               break;
         }
      }
      /* if received error function */
      else if (pdu[0] == (ciaaModbus_masterObj[handler].cmd | 0x80) )
      {
         /* set exception code send from slave */
         ciaaModbus_masterObj[handler].exceptioncode = pdu[1];
      }
      /* if invalid function code */
      else
      {
         /* set exception code CIAA_MODBUS_E_PDU_RECEIVED_WRONG */
         ciaaModbus_masterObj[handler].exceptioncode = CIAA_MODBUS_E_PDU_RECEIVED_WRONG;
      }

      /* if no error */
      if (CIAA_MODBUS_E_NO_ERROR == ciaaModbus_masterObj[handler].exceptioncode)
      {
         /* if no callback, set event */
         if (NULL == ciaaModbus_masterObj[handler].cbEndComm)
         {
            SetEvent(ciaaModbus_masterObj[handler].taskID, MODBUSE);
         }
         else
         {
            /* if callback set, perform it */
            ciaaModbus_masterObj[handler].cbEndComm(
                  ciaaModbus_masterObj[handler].slaveId,
                  ciaaModbus_masterObj[handler].cmd,
                  ciaaModbus_masterObj[handler].exceptioncode);
         }
         /* finish function */
         ciaaModbus_masterObj[handler].cmd = 0x00;
      }
   }
}

extern uint32_t ciaaModbus_masterGetRespTimeout(int32_t handler)
{
   return ciaaModbus_masterObj[handler].respTimeout;
}

#endif /* #if CIAA_MODBUS_TOTAL_MASTERS > 0 */

/** @} doxygen end group definition */
/** @} doxygen end group definition */
/*==================[end of file]============================================*/

