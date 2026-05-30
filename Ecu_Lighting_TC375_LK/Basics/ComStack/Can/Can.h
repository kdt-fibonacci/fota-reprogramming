#ifndef CAN_H_
#define CAN_H_

#include "Std_Types.h"
#include "ComStack_Types.h"

#include "Platform_Types.h"
#include "IfxCan.h"
#include "IfxCan_reg.h"
#include "IfxCpu_Irq.h"
#include "IfxCan_Can.h"
#include "IfxPort.h"

#include <stdio.h>
#include <string.h>

typedef uint32 Can_IdType;
typedef uint8  Can_HwHandleType;

typedef enum
{
    CAN_CS_UNINIT  = 0x00,
    CAN_CS_STARTED = 0x01,
    CAN_CS_STOPPED = 0x02,
    CAN_CS_SLEEP   = 0x03
} Can_ControllerStateType;

typedef struct
{
    PduIdType  swPduHandle;
    Can_IdType id;
    uint8*     sdu;
    uint8      length;
} Can_PduType;

typedef struct
{
    Can_IdType       CanId;
    Can_HwHandleType Hoh;
    uint8            ControllerId;
} Can_HwType;

/*********************************************************************************************************************/
/*----------------------------------------------Function Declarations------------------------------------------------*/
/*********************************************************************************************************************/

void Can_Init(
    void
);

Std_ReturnType Can_SetControllerMode(
    uint8 Controller,
    Can_ControllerStateType Transition
);

Std_ReturnType Can_GetControllerMode(
    uint8 Controller,
    Can_ControllerStateType *ControllerModePtr
);

Std_ReturnType Can_Write(
    Can_HwHandleType Hth,
    const Can_PduType* PduInfo
);

void Can_MainFunction_Read(
    void
);

void Can_MainFunction_Write(
    void
);

#endif /* CAN_H_ */
