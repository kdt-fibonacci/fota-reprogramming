#ifndef STD_TYPES_H_
#define STD_TYPES_H_

#include "Platform_Types.h"

typedef uint8 Std_ReturnType;

#ifndef E_OK
#define E_OK     ((Std_ReturnType)0U)
#endif

#ifndef E_NOT_OK
#define E_NOT_OK ((Std_ReturnType)1U)
#endif

#ifndef CAN_BUSY
#define CAN_BUSY ((Std_ReturnType)2U)
#endif

#ifndef TRUE
#define TRUE     ((boolean)1U)
#endif

#ifndef FALSE
#define FALSE    ((boolean)0U)
#endif

#ifndef NULL_PTR
#define NULL_PTR ((void*)0)
#endif

#endif /* STD_TYPES_H_ */
