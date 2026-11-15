/*	USB PD Library
 *	2025-06-21 Bogdan Ionescu
 *	Configuration:
 *		- USBPD_IMPLEMENTATION: Enable USB PD implementation
 *		- FUNCONF_USBPD_NO_STR: Disable string conversion functions
 *	Notes:
 *		- This library is based on the USB Power Delivery Specification.
 *			https://www.usb.org/document-library/usb-power-delivery
 *		- Packed bitfield structs are used for de/serialization of USB PD messages and
 *			are taken directly from the spec above.
 *		- Not all messages are implemented.
 *		- Formatting macros are provided next to the struct deffinitions.
 *	Basic usage:
 *		USBPD_VCC_e vcc = eUSBPD_VCC_5V0; // set the VCC voltage
 *		USBPD_Result_e result = USBPD_Init( vcc ); // initialize the peripheral
 *
 *		// wait for negotiation to complete.
 *		while ( eUSBPD_BUSY == ( result = USBPD_SinkNegotiate() ) );
 *
 *		USBPD_SPR_CapabilitiesMessage_t *capabilities;
 *		const size_t count = USBPD_GetCapabilities( &capabilities );
 *		USBPD_SelectPDO( count - 1, voltage ); // select the last supply (voltage is only used for PPS)
 *
 *	The above is not a complete example, check the funtion declarations below for more details.
 */

#pragma once

#include "ch32fun.h"
#include "funconfig.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "pd_caps_integrity.h"

#ifndef PACKED
#define PACKED __attribute__( ( packed ) )
#endif

// USB PD spec type definitions
typedef enum PACKED
{
	eUSBPD_PORTDATAROLE_DFP = 0, // Downstream Facing Port
	eUSBPD_PORTDATAROLE_UFP = 1, // Upstream Facing Port
} USBPD_PortDataRole_e;
typedef enum PACKED
{
	eUSBPD_PORTPOWEROLE_SINK = 0, // Sink Power Role
	eUSBPD_PORTPOWEROLE_SOURCE = 1, // Source Power Role
} USBPD_PortPowerRole_e;

typedef enum PACKED
{
	eUSBPD_REV_10 = 0x00u, // Revision 1.0
	eUSBPD_REV_20 = 0x01u, // Revision 2.0
	eUSBPD_REV_30 = 0x02u, // Revision 3.0
} USBPD_SpecificationRevision_e;

typedef enum PACKED
{
	eUSBPD_CTRL_MSG_GOODCRC = 0x01u,
	eUSBPD_CTRL_MSG_GOTOMIN = 0x02u, // Depracated
	eUSBPD_CTRL_MSG_ACCEPT = 0x03u,
	eUSBPD_CTRL_MSG_REJECT = 0x04u,
	eUSBPD_CTRL_MSG_PING = 0x05u, // Deprecated
	eUSBPD_CTRL_MSG_PS_RDY = 0x06u,
	eUSBPD_CTRL_MSG_GET_SOURCE_CAP = 0x07u,
	eUSBPD_CTRL_MSG_GET_SINK_CAP = 0x08u,
	eUSBPD_CTRL_MSG_DR_SWAP = 0x09u,
	eUSBPD_CTRL_MSG_PR_SWAP = 0x0Au,
	eUSBPD_CTRL_MSG_VCONN_SWAP = 0x0Bu,
	eUSBPD_CTRL_MSG_WAIT = 0x0Cu,
	eUSBPD_CTRL_MSG_SOFT_RESET = 0x0Du,
	eUSBPD_CTRL_MSG_DATA_RESET = 0x0Eu,
	eUSBPD_CTRL_MSG_DATA_RESET_COMPLETE = 0x0Fu,
	eUSBPD_CTRL_MSG_NOT_SUPPORTED = 0x10u,
	eUSBPD_CTRL_MSG_GET_SOURCE_CAPEXT = 0x11u,
	eUSBPD_CTRL_MSG_GET_STATUS = 0x12u,
	eUSBPD_CTRL_MSG_FR_SWAP = 0x13u,
	eUSBPD_CTRL_MSG_GET_PPS_STATUS = 0x14u,
	eUSBPD_CTRL_MSG_GET_COUNTRY_CODES = 0x15u,
	eUSBPD_CTRL_MSG_GET_SINK_CAPEXT = 0x16u,
	eUSBPD_CTRL_MSG_GET_SOURCE_INFO = 0x17u,
	eUSBPD_CTRL_MSG_GET_REVISION = 0x18u,
} USBPD_ControlMessage_e;

typedef enum PACKED
{
	eUSBPD_DATA_MSG_SOURCE_CAP = 0x01u,
	eUSBPD_DATA_MSG_REQUEST = 0x02u,
	eUSBPD_DATA_MSG_BIST = 0x03u,
	eUSBPD_DATA_MSG_SINK_CAP = 0x04u,
	eUSBPD_DATA_MSG_BATTERY_STATUS = 0x05u,
	eUSBPD_DATA_MSG_ALERT = 0x06u,
	eUSBPD_DATA_MSG_GET_COUNTRY_INFO = 0x07u,
	eUSBPD_DATA_MSG_ENTER_USB = 0x08u,
	eUSBPD_DATA_MSG_EPR_REUEST = 0x09u,
	eUSBPD_DATA_MSG_EPR_MODE = 0x0Au,
	eUSBPD_DATA_MSG_SOURCE_INFO = 0x0Bu,
	eUSBPD_DATA_MSG_REVISION = 0x0Cu,
	eUSBPD_DATA_MSG_VENDOR_DEFINED = 0x0Fu
} USBPD_DataMessage_e;

typedef union
{
	uint16_t data;
	struct
	{
		uint16_t MessageType : 5u; // USBPD_ControlMessage_t | USBPD_DataMessage_t
		USBPD_PortDataRole_e PortDataRole : 1u;
		USBPD_SpecificationRevision_e SpecificationRevision : 2u;
		USBPD_PortPowerRole_e PortPowerRole : 1u;
		uint16_t MessageID : 3u;
		uint16_t NumberOfDataObjects : 3u; // 0: Control Message, >0: Data Message
		uint16_t Extended : 1u;
	};
} USBPD_MessageHeader_t;

typedef USBPD_MessageHeader_t USBPD_ControlMessage_t;
static_assert( sizeof( USBPD_MessageHeader_t ) == sizeof( uint16_t ), "USBPD_MessageHeader_t size mismatch" );

typedef enum PACKED
{
	eUSBPD_PDO_FIXED = 0,
	eUSBPD_PDO_BATTERY = 1,
	eUSBPD_PDO_VARIABLE = 2,
	eUSBPD_PDO_AUGMENTED = 3,
} USBPD_PowerDataObject_e;

typedef enum PACKED
{
	eUSBPD_APDO_SPR_PPS = 0, // Standard Power Range Programmable Power Supply
	eUSBPD_APDO_EPR_AVS = 1, // Extended Power Range Adjustable Voltage Supply
	eUSBPD_APDO_SPR_AVS = 2, // Standard Power Range Adjustable Voltage Supply
	eUSBPD_APDO_RESERVED = 3,
} USBPD_AugmentedPDO_e;

typedef enum PACKED
{
	eUSBPD_PEAK_CURRENT_0 = 0, /* Peak Current equals IoC */

	eUSBPD_PEAK_CURRENT_1 = 1, /* 150% IoC for 1ms @ 5% duty cycle
	                              125% IoC for 2ms @ 10% duty cycle
	                              110% IoC for 10ms @ 50% duty cycle */

	eUSBPD_PEAK_CURRENT_2 = 2, /* 200% IoC for 1ms @ 5% duty cycle
	                              150% IoC for 2ms @ 10% duty cycle
	                              125% IoC for 10ms @ 50% duty cycle */

	eUSBPD_PEAK_CURRENT_3 = 3, /* 200% IoC for 1ms @ 5% duty cycle
	                              175% IoC for 2ms @ 10% duty cycle
	                              150% IoC for 10ms @ 50% duty cycle */
} USBPD_PeakCurrent_e;

typedef struct
{
	uint32_t data : 28u; // PDO specific data based on PDO type
	USBPD_AugmentedPDO_e AugmentedType : 2u; // shall be eUSBPD_APDO_SPR_PPS
	USBPD_PowerDataObject_e PDOType : 2u; // shall be eUSBPD_PDO_AUGMENTED
} USBPD_PDOHeader_t;

typedef struct
{
	uint32_t MaxCurrentIn10mA : 10u;
	uint32_t VoltageIn50mV : 10u;
	USBPD_PeakCurrent_e PeakCurrent : 2u;
	uint32_t Reserved_22bit : 1u;
	uint32_t EPRModeCapable : 1u;
	uint32_t UnchunkedExtendedMessage : 1u;
	uint32_t DualRoleData : 1u;
	uint32_t USBCommunicationsCapable : 1u;
	uint32_t UnconstrainedPower : 1u;
	uint32_t USBSuspendSupported : 1u;
	uint32_t DualRolePower : 1u;
	USBPD_PowerDataObject_e PDOType : 2u; // shall be eUSBPD_PDO_FIXED
} USBPD_SourceFixedSupplyPDO_t;

#define FIXED_SUPPLY_FMT                 \
	"\nFixed Supply:\n"                  \
	"\tMax Current: %d mA\n"             \
	"\tVoltage: %d mV\n"                 \
	"\tPeak Current: %d\n"               \
	"\tEPR Mode Capable: %s\n"           \
	"\tUnchunked Extended Message: %s\n" \
	"\tDual Role Data: %s\n"             \
	"\tUSB Communications Capable: %s\n" \
	"\tUnconstrained Power: %s\n"        \
	"\tUSB Suspend Supported: %s\n"      \
	"\tDual Role Power: %s\n"

#define FIXED_SUPPLY_FMT_ARGS( pdo )                                                                  \
	( ( pdo )->FixedSupply.MaxCurrentIn10mA * 10 ), ( ( pdo )->FixedSupply.VoltageIn50mV * 50 ),      \
		( ( pdo )->FixedSupply.PeakCurrent ), ( ( pdo )->FixedSupply.EPRModeCapable ? "Yes" : "No" ), \
		( ( pdo )->FixedSupply.UnchunkedExtendedMessage ? "Yes" : "No" ),                             \
		( ( pdo )->FixedSupply.DualRoleData ? "Yes" : "No" ),                                         \
		( ( pdo )->FixedSupply.USBCommunicationsCapable ? "Yes" : "No" ),                             \
		( ( pdo )->FixedSupply.UnconstrainedPower ? "Yes" : "No" ),                                   \
		( ( pdo )->FixedSupply.USBSuspendSupported ? "Yes" : "No" ),                                  \
		( ( pdo )->FixedSupply.DualRolePower ? "Yes" : "No" )

typedef struct
{
	uint32_t MaxCurrentIn10mA : 10u;
	uint32_t MinVoltageIn50mV : 10u;
	uint32_t MaxVoltageIn50mV : 10u;
	USBPD_PowerDataObject_e PDOType : 2u; // shall be eUSBPD_PDO_VARIABLE
} USBPD_VariablePDO_t;

#define VARIABLE_SUPPLY_FMT  \
	"\nVariable Supply:\n"   \
	"\tMax Current: %d mA\n" \
	"\tMin Voltage: %d mV\n" \
	"\tMax Voltage: %d mV\n"

#define VARIABLE_SUPPLY_FMT_ARGS( pdo )                                                                   \
	( ( pdo )->VariableSupply.MaxCurrentIn10mA * 10 ), ( ( pdo )->VariableSupply.MinVoltageIn50mV * 50 ), \
		( ( pdo )->VariableSupply.MaxVoltageIn50mV * 50 )

typedef struct
{
	uint32_t MaxPowerIn250mW : 10u;
	uint32_t MinVoltageIn50mV : 10u;
	uint32_t MaxVoltageIn50mV : 10u;
	USBPD_PowerDataObject_e PDOType : 2u; // shall be eUSBPD_PDO_BATTERY
} USBPD_BatteryPDO_t;

#define BATTERY_SUPPLY_FMT   \
	"\nBattery Supply:\n"    \
	"\tMax Power: %d mW\n"   \
	"\tMin Voltage: %d mV\n" \
	"\tMax Voltage: %d mV\n"

#define BATTERY_SUPPLY_FMT_ARGS( pdo )                                                                  \
	( ( pdo )->BatterySupply.MaxPowerIn250mW * 250 ), ( ( pdo )->BatterySupply.MinVoltageIn50mV * 50 ), \
		( ( pdo )->BatterySupply.MaxVoltageIn50mV * 50 )

typedef struct
{
	uint32_t MaxCurrentIn50mA : 7u;
	uint32_t Reserved_7bit : 1u; // shall be set to zero
	uint32_t MinVoltageIn100mV : 8u;
	uint32_t Reserved_16bit : 1u; // shall be set to zero
	uint32_t MaxVoltageIn100mV : 8u;
	uint32_t Reserved_25_26bit : 2u; // shall be set to zero
	uint32_t PPSpowerLimited : 1u;
	USBPD_AugmentedPDO_e AugmentedType : 2u; // shall be eUSBPD_APDO_SPR_PPS
	USBPD_PowerDataObject_e PDOType : 2u; // shall be eUSBPD_PDO_AUGMENTED
} USBPD_SPR_PPS_APDO_t;

#define SPR_PPS_FMT          \
	"\nPPS Supply:\n"        \
	"\tMax Current: %d mA\n" \
	"\tMin Voltage: %d mV\n" \
	"\tMax Voltage: %d mV\n" \
	"\tPPS Power Limited: %s\n"

#define SPR_PPS_FMT_ARGS( pdo )                                                               \
	( ( pdo )->SPR_PPS.MaxCurrentIn50mA * 50 ), ( ( pdo )->SPR_PPS.MinVoltageIn100mV * 100 ), \
		( ( pdo )->SPR_PPS.MaxVoltageIn100mV * 100 ), ( ( pdo )->SPR_PPS.PPSpowerLimited ? "Yes" : "No" )

typedef struct
{
	uint32_t PDPIn1W : 8u;
	uint32_t MinVoltageIn100mV : 8u;
	uint32_t Reserved_16bit : 1u; // shall be set to zero
	uint32_t MaxVoltageIn100mV : 9u;
	USBPD_PeakCurrent_e PeakCurrent : 2u;

	USBPD_AugmentedPDO_e AugmentedType : 2u; // shall be eUSBPD_APDO_EPR_AVS
	USBPD_PowerDataObject_e PDOType : 2u; // shall be eUSBPD_PDO_AUGMENTED
} USBPD_EPR_AVS_APDO_t;

#define EPR_AVS_FMT          \
	"\nEPR AVS Supply:\n"    \
	"\tPDP: %d W\n"          \
	"\tMin Voltage: %d mV\n" \
	"\tMax Voltage: %d mV\n" \
	"\tPeak Current: %d\n"

#define EPR_AVS_FMT_ARGS( pdo )                                                 \
	( ( pdo )->EPR_AVS.PDPIn1W ), ( ( pdo )->EPR_AVS.MinVoltageIn100mV * 100 ), \
		( ( pdo )->EPR_AVS.MaxVoltageIn100mV * 100 ), ( ( pdo )->EPR_AVS.PeakCurrent )

typedef struct
{
	uint32_t MaxCurrent15To20VIn10mA : 10u;
	uint32_t MaxCurrent9to15VIn10mA : 10u;
	uint32_t Reserved_20_25bit : 6u; // shall be set to zero
	USBPD_PeakCurrent_e PeakCurrent : 2u;
	USBPD_AugmentedPDO_e AugmentedType : 2u; // shall be eUSBPD_APDO_SPR_AVS
	USBPD_PowerDataObject_e PDOType : 2u; // shall be eUSBPD_PDO_AUGMENTED
} USBPD_SPR_AVS_APDO_t;

#define SPR_AVS_FMT                 \
	"\nSPR AVS Supply:\n"           \
	"\tMax Current 15-20V: %d mA\n" \
	"\tMax Current 9-15V: %d mA\n"  \
	"\tPeak Current: %d\n"

#define SPR_AVS_FMT_ARGS( pdo )                                                                          \
	( ( pdo )->SPR_AVS.MaxCurrent15To20VIn10mA * 10 ), ( ( pdo )->SPR_AVS.MaxCurrent9to15VIn10mA * 10 ), \
		( ( pdo )->SPR_AVS.PeakCurrent )

typedef union
{
	USBPD_PDOHeader_t Header;
	USBPD_SourceFixedSupplyPDO_t FixedSupply;
	USBPD_VariablePDO_t VariableSupply;
	USBPD_BatteryPDO_t BatterySupply;
	USBPD_SPR_PPS_APDO_t SPR_PPS;
	USBPD_EPR_AVS_APDO_t EPR_AVS;
	USBPD_SPR_AVS_APDO_t SPR_AVS;
} USBPD_SourcePDO_t;

static_assert( sizeof( USBPD_SourcePDO_t ) == sizeof( uint32_t ), "USBPD_SourcePDO_t size mismatch" );

typedef enum // Do not PACK, messes up alignment
{
	eUSBPD_FAST_ROLE_SWAP_NOT_SUPPORTED = 0,
	eUSBPD_FAST_ROLE_SWAP_DEFAULT = 1,
	eUSBPD_FAST_ROLE_SWAP_1A5 = 2, // 1.5A @ 5V
	eUSBPD_FAST_ROLE_SWAP_3A = 3, // 3A @ 5V
} USBPD_FastRoleSwapRequiredCurrent_e;

typedef struct
{
	uint32_t CurrentIn10mA : 10u;
	uint32_t VoltageIn50mV : 10u;
	uint32_t Reserved_20_22bit : 3u; // shall be set to zero
	USBPD_FastRoleSwapRequiredCurrent_e FastRoleSwap : 2u;
	uint32_t DualRoleData : 1u;
	uint32_t USBComsCapable : 1u;
	uint32_t UnconstrainedPower : 1u;
	uint32_t HigherCapability : 1u;
	uint32_t DualRolePower : 1u;
	USBPD_PowerDataObject_e PDOType : 2u; // shall be eUSBPD_PDO_FIXED
} USBPD_SinkFixedSupplyPDO_t;

typedef union
{
	USBPD_PDOHeader_t Header;
	USBPD_SinkFixedSupplyPDO_t FixedSupply;
	USBPD_VariablePDO_t VariableSupply;
	USBPD_BatteryPDO_t BatterySupply;
	USBPD_SPR_PPS_APDO_t SPR_PPS;
	USBPD_EPR_AVS_APDO_t EPR_AVS;
	USBPD_SPR_AVS_APDO_t SPR_AVS;
} USBPD_SinkPDO_t;

static_assert( sizeof( USBPD_SourcePDO_t ) == sizeof( uint32_t ), "USBPD_SourcePDO_t size mismatch" );
static_assert( sizeof( USBPD_SinkPDO_t ) == sizeof( uint32_t ), "USBPD_SinkPDO_t size mismatch" );

typedef union
{
	USBPD_SourcePDO_t Source[7];
	USBPD_SinkPDO_t Sink[7];
} USBPD_SPR_CapabilitiesMessage_t;

static_assert( sizeof( USBPD_SPR_CapabilitiesMessage_t ) == ( 7 * sizeof( uint32_t ) ),
	"USBPD_SPR_CapabilitiesMessage_t size mismatch" );

typedef struct
{
	uint32_t MaxCurrentIn10mA : 10u;
	uint32_t OperatingCurrentIn10mA : 10u;
	uint32_t Reserved_20_21bit : 2u; // shall be set to zero
	uint32_t ERPCapable : 1u;
	uint32_t UnchunkedExtendedMessage : 1u;
	uint32_t NoUSBSuspended : 1u;
	uint32_t USBComsCapable : 1u;
	uint32_t CapabilityMissmatch : 1u;
	uint32_t Giveback : 1u; // Deprecated, shall be set to zero
	uint32_t ObjectPosition : 4u; // Reserved and shall not be used
} USBPD_FixedAndVariableRDO_t;

typedef struct
{
	uint32_t MaxPowerIn250mW : 10u;
	uint32_t OperatingPowerIn250mW : 10u;
	uint32_t Reserved_20_21bit : 2u; // shall be set to zero
	uint32_t ERPCapable : 1u;
	uint32_t UnchunkedExtendedMessage : 1u;
	uint32_t NoUSBSuspended : 1u;
	uint32_t USBComsCapable : 1u;
	uint32_t CapabilityMissmatch : 1u;
	uint32_t Giveback : 1u; // Deprecated, shall be set to zero
	uint32_t ObjectPosition : 4u; // Reserved and shall not be used
} USBPD_BatteryRDO_t;

typedef struct
{
	uint32_t OperatingCurrentIn50mA : 7u;
	uint32_t Reserved_7_8bit : 2u; // shall be set to zero
	uint32_t OutputVoltageIn20mV : 12u;
	uint32_t Reserved_21bit : 1u; // shall be set to zero
	uint32_t ERPCapable : 1u;
	uint32_t UnchunkedExtendedMessage : 1u;
	uint32_t NoUSBSuspended : 1u;
	uint32_t USBComsCapable : 1u;
	uint32_t CapabilityMissmatch : 1u;
	uint32_t Reserved_27bit : 1u; // Deprecated, shall be set to zero
	uint32_t ObjectPosition : 4u; // Reserved and shall not be used
} USBPD_PPS_RDO_t;

typedef struct
{
	uint32_t OperatingCurrentIn50mA : 7u;
	uint32_t Reserved_7_8bit : 2u; // shall be set to zero
	uint32_t OutputVoltageIn100mV : 12u; // NOTE: Output voltage in 25mV units, the least two significant bits Shall
	                                     // be set to zero making the effective voltage step size 100mV.
	uint32_t Reserved_21bit : 1u; // shall be set to zero
	uint32_t ERPCapable : 1u;
	uint32_t UnchunkedExtendedMessage : 1u;
	uint32_t NoUSBSuspended : 1u;
	uint32_t USBComsCapable : 1u;
	uint32_t CapabilityMissmatch : 1u;
	uint32_t Reserved_27bit : 1u; // Deprecated, shall be set to zero
	uint32_t ObjectPosition : 4u; // Reserved and shall not be used
} USBPD_AVS_RDO_t;

typedef union
{
	USBPD_FixedAndVariableRDO_t FixedAndVariable;
	USBPD_BatteryRDO_t Battery;
	USBPD_PPS_RDO_t PPS;
} USBPD_RequestDataObject_t;

static_assert( sizeof( USBPD_RequestDataObject_t ) == sizeof( uint32_t ), "USBPD_RequestDataObject_t size mismatch" );

// TODO: Define the rest of the message types sections 6.4.3 -> 6.5.16

typedef enum
{
	eUSBPD_MAX_EXTENDED_MSG_LEN = 260,
	eUSBPD_MAX_EXTENDED_MSG_CHUNK_LEN = 26,
	eUSBPD_MAX_EXTENDED_MSG_LEGACY_LEN = 26,
} USBPD_ValueParameters_t;

typedef enum
{
	eUSBPD_OK = 0,
	eUSBPD_BUSY,
	eUSBPD_ERROR,
	eUSBPD_ERROR_ARGS,
	eUSBPD_ERROR_NOT_SUPPORTED,
	eUSBPD_ERROR_TIMEOUT,
} USBPD_Result_e;

typedef enum
{
	eUSBPD_VCC_3V3 = 0,
	eUSBPD_VCC_5V0 = 1,
} USBPD_VCC_e;

typedef enum
{
	eUSBPD_CCNONE = 0,
	eUSBPD_CC1 = 1,
	eUSBPD_CC2 = 2,
} USBPD_CC_e;

typedef enum
{
	eSTATE_IDLE,
	eSTATE_CABLE_DETECT,
	eSTATE_SOURCE_CAP,
	eSTATE_WAIT_ACCEPT,
	eSTATE_WAIT_PS_RDY,
	eSTATE_PS_RDY,
    /* Manual retry only: synchronize both partners without tearing CC down. */
    eSTATE_SOFT_RESET_WAIT_ACCEPT,
    eSTATE_SOFT_RESET_FAILED,
    /* A hardware Hard Reset invalidates the current contract. Diagnostic
     * trial deliberately requires unplug/replug before any new Request. */
    eSTATE_HARD_RESET_DETECTED,
	eSTATE_MAX,
} USBPD_State_e;

/**
 * @brief  Initialize the USB PD module
 * @param  vcc: VCC voltage level (3.3V or 5V)
 * @return USBPD_Result_e
 */
USBPD_Result_e USBPD_Init( USBPD_VCC_e vcc );

/**
 * @brief  Negotiate with the USB PD Source, must be called periodically
 * @param  None
 * @return USBPD_BUSY if negotiation is in progress, eUSBPD_OK if successful, or an error code
 */
USBPD_Result_e USBPD_SinkNegotiate( void );

/**
 * @brief Ask an attached PD Source to retransmit Source_Capabilities.
 * @note Recovery aid for sources that do not spontaneously retransmit
 *       after the sink PD receiver is reinitialized. May be called only
 *       while waiting for Source_Capabilities (Cable Detected state).
 */
USBPD_Result_e USBPD_RequestSourceCapabilities( void );

/* Begin SOP Soft_Reset on an established contract. Returns BUSY unless
 * BMC TX is idle and the negotiation state is PS_RDY. Not a Hard Reset. */
USBPD_Result_e USBPD_BeginSoftReset( void );
/* Arm exactly one 7V/1A direct PPS request after a successful manual Soft_Reset.
 * Diagnostic experiment: avoids the intermediate fixed-5V Request. */
void USBPD_ArmDirectPPSAfterSoftReset( uint32_t mv, uint32_t ma );
bool USBPD_DirectPPSContractReady( void );

/**
 * @brief  Reset the USB PD module
 * @param  None
 * @return None
 */
void USBPD_Reset( void );

/**
 * @brief  Get the current state of the USB PD module
 * @param  None
 * @return USBPD_State_e representing the current state of the module
 */
USBPD_State_e USBPD_GetState( void );
/* Diagnostic: called at tick_session entry, only captures when CAPS ACK active. */
void USBPD_M4CProbeMainTick(uint8_t mode);

/**
 * @brief Nonintrusively sample the currently selected CC comparator.
 * @return false if the active CC voltage is no longer detected.
 * @warning Valid only once SinkNegotiate has selected CC1/CC2 and
 *          entered a non-IDLE state.  Does not change CC_SEL while BMC
 *          communication is active; the application must debounce this.
 */
bool USBPD_ActiveCCPresent( void );
/* Snapshot the selected CC comparator once; line=1/2, 0 if unset. */
bool USBPD_ActiveCCSample( uint8_t *line, uint16_t *raw_cc );


/**
 * @brief  Convert USB PD state to string
 * @param state: USBPD_State_e to convert
 * @return Pointer to a string representing the state
 */
const char *USBPD_StateToStr( USBPD_State_e state );

/**
 * @brief  Convert USB PD result to string
 * @param  result: USBPD_Result_e to convert
 * @return Pointer to a string representing the result
 */
const char *USBPD_ResultToStr( USBPD_Result_e result );

/**
 * @brief  Select a new Power Data Object (PDO) to be used. No re-negotiation is needed.
 * @param  index: Index of the PDO to select (0-based)
 * @param  voltageIn100mV: Desired output voltage in 100mV units (e.g., 500 for 5V) (only applicable for PPS)
 * @return USBPD_Result_e
 */
USBPD_Result_e USBPD_SelectPDO( uint8_t index, uint32_t voltageIn100mV );

/**
 * @brief Request a selected SPR PPS APDO with explicit voltage and current limit.
 * @param index: Zero-based Source PDO index.
 * @param voltageInmV: Exact PPS output voltage, in millivolts (20 mV step).
 * @param currentInmA: Exact PPS requested operating current/limit (50 mA step).
 * @return eUSBPD_OK only means the Request message was submitted.
 *         To observe acceptance, wait for eSTATE_PS_RDY.
 * @note This local extension does NOT change the original USBPD_SelectPDO API.
 */
USBPD_Result_e USBPD_SelectPPS( uint8_t index, uint32_t voltageInmV, uint32_t currentInmA );

/**
 * @brief  Get the capabilities of the USB PD Source
 * @param[out] capabilities: Pointer to a pointer where the capabilities message structure is stored
 * @return Number of Power Data Objects (PDOs) in the capabilities message
 */
size_t USBPD_GetCapabilities( USBPD_SPR_CapabilitiesMessage_t **capabilities );
/* Atomic snapshot for main-loop consumers; avoids ISR updates during traversal. */
size_t USBPD_CopyCapabilities( USBPD_SPR_CapabilitiesMessage_t *dest );

/**
 * @brief  Check if the Power Data Object is a Programmable Power Supply (PPS)
 * @param[in] pdo: Pointer to the Power Data Object to check
 * @return true if the PDO is a PPS, false otherwise
 */
bool USBPD_IsPPS( const USBPD_SourcePDO_t *pdo );

/**
 * @brief  Get the USB PD Specification Revision
 * @param  None
 * @return USBPD_SpecificationRevision_e representing the USB PD specification revision
 */
USBPD_SpecificationRevision_e USBPD_GetVersion( void );

#if defined( USBPD_IMPLEMENTATION )

#include <string.h>

typedef struct
{
	uint32_t ccCount;
	volatile USBPD_State_e state;
	USBPD_SpecificationRevision_e pdVersion;
	USBPD_CC_e lastCCLine;
	USBPD_SPR_CapabilitiesMessage_t caps;
	uint8_t messageID;
	volatile uint8_t pdoCount;
	bool gotSourceGoodCRC;
    /* Outbound MessageID advances only on matching GoodCRC. */
    bool txAckPending;
    uint8_t txAckID;
    /* Remember the last validated Source_Capabilities to reject retransmits. */
    bool sourceCapsSeen;
    uint8_t sourceCapsMessageID;
    bool hardResetLatched;
    bool softResetAcked;
    bool directPPSArmed;
    bool directPPSRequested;
    bool directPPSReady;
    /* Bounded Source_Capabilities supersessions during a direct PPS Request.
     * This is not a count of all advertisements, only those interrupting a
     * direct Request in WAIT_ACCEPT. */
    uint8_t directPPSCapSupersessions;
    uint32_t directPPSmv;
    uint32_t directPPSma;
} USBPD_Instance_t;

/* Diagnostic build: reserve for the controller's maximum 510-byte RX frame.
 * This avoids risking DMA overwrite of the original 34-byte SPR-only buffer
 * while we observe the CH32X035 byte counter on real hardware. */
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
static __attribute__( ( aligned( 4 ) ) ) uint8_t s_buffer[512];
static volatile uint32_t s_m4c_diag_seq;
static volatile uint16_t s_m4c_diag_bytes;
static volatile uint8_t s_m4c_diag_pdos;
/* 1: rejected length, 2: rejected PDO values, 3: accepted */
static volatile uint8_t s_m4c_diag_verdict;
/* IRQ producer / main-loop consumer: preserve successive advertisements.
 * Fixed-size records only; no printf or allocation in the IRQ. */
#define M4C_RX_TRACE_SLOTS 16u
typedef struct {
    uint16_t rx_bytes;
    uint8_t pdo_count;
    uint8_t verdict;
    uint8_t msg_id;
    uint8_t prior_state;
    uint32_t last_pdo;
    /* Only Source_Capabilities records populate these fields. Snapshot the
     * received DMA bytes before SendMessage(GoodCRC) reuses s_buffer. */
    uint16_t raw_header;
    uint16_t irq_status;
    uint16_t irq_control;
    uint16_t irq_config;
    uint8_t raw_pdo_count;
    uint32_t raw_pdos[PD_CAPS_MAX_PDOS];
} M4C_RxTrace_t;
static volatile M4C_RxTrace_t s_m4c_rx_trace[M4C_RX_TRACE_SLOTS];
static volatile uint32_t s_m4c_rx_trace_written;
/* Limit extra post-clear IRQ snapshots to avoid overflowing the trace ring. */
static volatile uint8_t s_m4c_reset_postclear_captures;
/* Use free-running SysTick count (core-clock ticks). IRQ logging is deferred. */
static volatile uint32_t s_m4c_last_reset_tick;
static volatile uint32_t s_m4c_last_tx_end_tick;
static volatile uint8_t s_m4c_reset_tick_valid;
static volatile uint8_t s_m4c_tx_end_tick_valid;
static volatile uint8_t s_m4c_last_tx_end_kind;
static volatile uint8_t s_m4c_reset_interval_captures;
/* Deadline probe for SPR Source_Capabilities -> Request. Record the
 * GoodCRC TX_END time in the IRQ, not the later main-loop print time. */
static volatile uint32_t s_m4c_caps_ack_tick;
static volatile uint8_t s_m4c_caps_ack_valid;
static volatile uint8_t s_m4c_caps_request_started;
/* One-shot control-path probe for the Source_Capabilities -> Request gap.
 * No printing in the IRQ and no polling/timing changes to PD decisions. */
static volatile uint8_t s_m4c_path_main_calls;
static volatile uint8_t s_m4c_path_negotiate_calls;
static volatile uint8_t s_m4c_path_sourcecap_branch;
static volatile uint8_t s_m4c_path_last_mode;
static volatile uint8_t s_m4c_path_first_main_valid;
static volatile uint32_t s_m4c_path_first_main_ticks;
static volatile uint32_t s_m4c_path_first_negotiate_ticks;
static volatile uint8_t s_m4c_path_first_negotiate_valid;

/* TX metadata is latched at SendMessage(), not read back from the shared
 * DMA buffer in IF_TX_END (RX may already have overwritten that buffer). */
static volatile uint8_t s_m4c_last_tx_kind; /* 1=GoodCRC, 2=Request, 3=Get_Source_Cap, 4=other */
static volatile uint8_t s_m4c_last_tx_id;
static volatile uint8_t s_m4c_last_tx_size;

static void m4c_rx_trace_add(uint16_t bytes, uint8_t count, uint8_t verdict,
                             uint8_t msg_id, uint8_t prior_state, uint32_t last_pdo)
{
    const uint32_t seq = s_m4c_rx_trace_written;
    volatile M4C_RxTrace_t *entry = &s_m4c_rx_trace[seq % M4C_RX_TRACE_SLOTS];
    entry->rx_bytes = bytes;
    entry->pdo_count = count;
    entry->verdict = verdict;
    entry->msg_id = msg_id;
    entry->prior_state = prior_state;
    entry->last_pdo = last_pdo;
    entry->raw_pdo_count = 0u;
    __asm volatile( "" ::: "memory" );
    s_m4c_rx_trace_written = seq + 1u;
}

/* Reset event interval evidence. Two intervals are captured atomically as
 * part of the ring entry; 0xffffffff means there is no preceding event. */
static void m4c_rx_trace_reset_intervals(uint32_t since_reset_ticks,
                                          uint32_t since_tx_ticks,
                                          uint16_t status, uint8_t control,
                                          uint8_t config, uint8_t tx_kind,
                                          uint8_t state)
{
    const uint32_t seq = s_m4c_rx_trace_written;
    volatile M4C_RxTrace_t *entry = &s_m4c_rx_trace[seq % M4C_RX_TRACE_SLOTS];
    entry->rx_bytes = status;
    entry->pdo_count = control;
    entry->verdict = 13u;
    entry->msg_id = config;
    entry->prior_state = state;
    entry->last_pdo = since_reset_ticks;
    entry->raw_header = tx_kind;
    entry->irq_status = (uint16_t)since_tx_ticks;
    entry->irq_control = (uint16_t)(since_tx_ticks >> 16);
    entry->raw_pdo_count = 0u;
    __asm volatile( "" ::: "memory" );
    s_m4c_rx_trace_written = seq + 1u;
}

/* Dedicated Source_Capabilities trace entry. Do not read packet bytes
 * after the GoodCRC reply has replaced the shared DMA/TX buffer. The
 * producer is the USBPD interrupt; commit sequence is written last. */
static void m4c_rx_trace_caps(uint16_t bytes, uint8_t count, uint8_t verdict,
                              uint8_t msg_id, uint8_t prior_state,
                              uint32_t last_pdo, uint16_t header,
                              uint16_t status, uint16_t control, uint16_t config,
                              const uint32_t *pdos, uint8_t captured)
{
    const uint32_t seq = s_m4c_rx_trace_written;
    volatile M4C_RxTrace_t *entry = &s_m4c_rx_trace[seq % M4C_RX_TRACE_SLOTS];
    entry->rx_bytes = bytes;
    entry->pdo_count = count;
    entry->verdict = verdict;
    entry->msg_id = msg_id;
    entry->prior_state = prior_state;
    entry->last_pdo = last_pdo;
    entry->raw_header = header;
    entry->irq_status = status;
    entry->irq_control = control;
    entry->irq_config = config;
    entry->raw_pdo_count = 0u;
    if (captured > PD_CAPS_MAX_PDOS) captured = PD_CAPS_MAX_PDOS;
    for (uint8_t i = 0u; i < captured; ++i)
        entry->raw_pdos[i] = pdos[i];
    entry->raw_pdo_count = captured;
    __asm volatile( "" ::: "memory" );
    s_m4c_rx_trace_written = seq + 1u;
}

#else
static __attribute__( ( aligned( 4 ) ) ) uint8_t s_buffer[34];
#endif
static USBPD_Instance_t s_instance = {
	.pdVersion = eUSBPD_REV_30,
};

static USBPD_CC_e GetActiveCCLine( void );
static void SwitchRXMode( void );
static void SendMessage( uint8_t size );
static void ParsePacket( void );

USBPD_Result_e USBPD_Init( USBPD_VCC_e vcc )
{
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO;
	RCC->AHBPCENR |= RCC_USBPD;

	GPIOC->CFGHR &= ~( 0xf << ( ( 14 & 7 ) << 2 ) );
	GPIOC->CFGHR |= ( GPIO_Speed_10MHz | GPIO_CNF_OUT_OD ) << ( ( 14 & 7 ) << 2 );
	GPIOC->CFGHR &= ~( 0xf << ( ( 15 & 7 ) << 2 ) );
	GPIOC->CFGHR |= ( GPIO_Speed_10MHz | GPIO_CNF_OUT_OD ) << ( ( 15 & 7 ) << 2 );

	AFIO->CTLR |= USBPD_IN_HVT;
	if ( vcc == eUSBPD_VCC_3V3 )
	{
		AFIO->CTLR |= USBPD_PHY_V33;
	}

	USBPD->DMA = (uint32_t)s_buffer;
	USBPD->CONFIG = IE_RX_ACT | IE_RX_RESET | IE_TX_END | PD_DMA_EN | PD_FILT_EN;
	USBPD->STATUS = BUF_ERR | IF_RX_BIT | IF_RX_BYTE | IF_RX_ACT | IF_RX_RESET | IF_TX_END;

	// disable CC comparators
	USBPD->PORT_CC1 &= ~( CC_CMP_MASK | PA_CC_AI );
	USBPD->PORT_CC2 &= ~( CC_CMP_MASK | PA_CC_AI );
	// set CC comparator voltage
	USBPD->PORT_CC1 |= CC_CMP_66;
	USBPD->PORT_CC2 |= CC_CMP_66;

	return eUSBPD_OK;
}

void USBPD_M4CProbeMainTick(uint8_t mode)
{
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
    if (s_m4c_caps_ack_valid && !s_m4c_caps_request_started) {
        if (s_m4c_path_main_calls != 255u) ++s_m4c_path_main_calls;
        s_m4c_path_last_mode = mode;
        if (!s_m4c_path_first_main_valid) {
            s_m4c_path_first_main_ticks = (uint32_t)(SysTick->CNTL - s_m4c_caps_ack_tick);
            s_m4c_path_first_main_valid = 1u;
        }
    }
#else
    (void)mode;
#endif
}

USBPD_Result_e USBPD_SinkNegotiate( void )
{
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
    if (s_m4c_caps_ack_valid && !s_m4c_caps_request_started) {
        if (s_m4c_path_negotiate_calls != 255u) ++s_m4c_path_negotiate_calls;
        if (!s_m4c_path_first_negotiate_valid) {
            s_m4c_path_first_negotiate_ticks = (uint32_t)(SysTick->CNTL - s_m4c_caps_ack_tick);
            s_m4c_path_first_negotiate_valid = 1u;
        }
    }
#endif
    if (s_instance.hardResetLatched) return eUSBPD_BUSY;
	switch ( s_instance.state )
	{
		case eSTATE_IDLE:;
			const uint8_t ccLine = GetActiveCCLine();
			if ( ccLine == eUSBPD_CCNONE )
			{
				s_instance.ccCount = 0;
				s_instance.lastCCLine = eUSBPD_CCNONE;
				break;
			}

			if ( s_instance.lastCCLine != ccLine )
			{
				s_instance.lastCCLine = ccLine;
				s_instance.ccCount = 0;
			}
			else
			{
				s_instance.ccCount++;
			}

			if ( s_instance.ccCount > 10 )
			{
				if ( ccLine == eUSBPD_CC2 )
				{
					USBPD->CONFIG |= CC_SEL;
				}
				else
				{
					USBPD->CONFIG &= ~CC_SEL;
				}

				s_instance.ccCount = 0;
				s_instance.state = eSTATE_CABLE_DETECT;

				SwitchRXMode();
				// NVIC_SetPriority(USBPD_IRQn, 0x00); // TODO: Is this needed?
				NVIC_EnableIRQ( USBPD_IRQn );
			}
			break;

		case eSTATE_SOURCE_CAP:
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
            if (s_m4c_caps_ack_valid && !s_m4c_caps_request_started)
                s_m4c_path_sourcecap_branch = 1u;
#endif
            /* Experimental, manual-Soft_Reset-only path. USB PD permits
             * selecting an advertised PPS APDO in the first Request; a
             * preceding fixed 5V contract is not required. Never request
             * PPS if the CURRENT advertisement lacks a suitable APDO. */
            if (s_instance.directPPSArmed) {
                const uint32_t mv = s_instance.directPPSmv;
                const uint32_t ma = s_instance.directPPSma;
                while (USBPD->CONTROL & PD_TX_EN);
                for (uint8_t i = 0u; i < s_instance.pdoCount; ++i) {
                    const USBPD_SourcePDO_t *p = &s_instance.caps.Source[i];
                    if (!USBPD_IsPPS(p)) continue;
                    if (mv < p->SPR_PPS.MinVoltageIn100mV * 100u ||
                        mv > p->SPR_PPS.MaxVoltageIn100mV * 100u ||
                        ma > p->SPR_PPS.MaxCurrentIn50mA * 50u) continue;
                    if (USBPD_SelectPPS(i, mv, ma) == eUSBPD_OK) {
                        s_instance.directPPSArmed = false;
                        s_instance.directPPSRequested = true;
                        return eUSBPD_BUSY;
                    }
                }
                /* New advertisement has no qualifying PPS APDO: use only
                 * fixed PDO[0], never a cached PPS RDO. */
                s_instance.directPPSArmed = false;
                s_instance.directPPSRequested = false;
            }
			// Wait for the GoodCRC TX (started inside ParsePacket) to finish before
			// sending the Request. Without this, SelectPDO's SendMessage races with
			// the in-progress GoodCRC transmission (~450 µs at 300 kHz BMC), causing
			// strict chargers to reject or ignore the request.
			while ( USBPD->CONTROL & PD_TX_EN );
			USBPD_SelectPDO( 0, 0 ); // Select the first PDO by default
			s_instance.state = eSTATE_WAIT_ACCEPT;
			break;

		case eSTATE_PS_RDY: return eUSBPD_OK;

		default: break;
	}

	return eUSBPD_BUSY;
}

USBPD_Result_e USBPD_RequestSourceCapabilities( void )
{
    if (s_instance.hardResetLatched ||
        s_instance.state != eSTATE_CABLE_DETECT ||
         ( USBPD->CONTROL & PD_TX_EN ) )
    {
        return eUSBPD_BUSY;
    }

    /* Control messages have a 2-byte header and zero data objects. */
    *(USBPD_MessageHeader_t *)&s_buffer[0] = (USBPD_MessageHeader_t){
        .MessageID = s_instance.messageID,
        .MessageType = eUSBPD_CTRL_MSG_GET_SOURCE_CAP,
        .NumberOfDataObjects = 0u,
        .SpecificationRevision = s_instance.pdVersion,
        .PortPowerRole = eUSBPD_PORTPOWEROLE_SINK,
        .PortDataRole = eUSBPD_PORTDATAROLE_UFP,
    };
    SendMessage( sizeof( USBPD_MessageHeader_t ) );
    return eUSBPD_OK;
}

USBPD_Result_e USBPD_BeginSoftReset( void )
{
    /* Only the manual-retry path calls this; leave the first attachment
     * handshake and PPS refresh unchanged.  Refuse overlapping TX. */
    if (s_instance.hardResetLatched ||
        s_instance.state != eSTATE_PS_RDY || (USBPD->CONTROL & PD_TX_EN) ||
        s_instance.txAckPending)
        return eUSBPD_BUSY;

    NVIC_DisableIRQ(USBPD_IRQn);
    /* USB PD Soft Reset starts a new SOP message-ID epoch at zero. */
    s_instance.messageID = 0u;
    s_instance.txAckPending = false;
    s_instance.softResetAcked = false;
    s_instance.directPPSArmed = false;
    s_instance.directPPSRequested = false;
    s_instance.directPPSReady = false;
    s_instance.directPPSCapSupersessions = 0u;
    s_instance.sourceCapsSeen = false;
    s_instance.pdoCount = 0u;
    memset(&s_instance.caps, 0, sizeof(s_instance.caps));
    s_instance.state = eSTATE_SOFT_RESET_WAIT_ACCEPT;
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
    s_m4c_caps_ack_valid = 0u;
    s_m4c_caps_request_started = 0u;
    s_m4c_path_main_calls = 0u;
    s_m4c_path_negotiate_calls = 0u;
    s_m4c_path_sourcecap_branch = 0u;
    s_m4c_path_first_main_valid = 0u;
    s_m4c_path_first_negotiate_valid = 0u;
#endif

    *(USBPD_MessageHeader_t *)&s_buffer[0] = (USBPD_MessageHeader_t){
        .MessageID = 0u,
        .MessageType = eUSBPD_CTRL_MSG_SOFT_RESET,
        .NumberOfDataObjects = 0u,
        .SpecificationRevision = s_instance.pdVersion,
        .PortPowerRole = eUSBPD_PORTPOWEROLE_SINK,
        .PortDataRole = eUSBPD_PORTDATAROLE_UFP,
    };
    /* Start this one control message atomically: the normal SendMessage()
     * diagnostic helper re-enables the IRQ before setting BMC TX registers.
     * During Soft_Reset we must not permit an incoming packet to overwrite
     * the shared DMA buffer between header setup and TX launch. */
    s_instance.txAckID = 0u;
    s_instance.txAckPending = true;
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
    s_m4c_last_tx_kind = 5u; /* Soft Reset */
    s_m4c_last_tx_id = 0u;
    s_m4c_last_tx_size = sizeof(USBPD_MessageHeader_t);
    m4c_rx_trace_add((uint16_t)sizeof(USBPD_MessageHeader_t), 0u, 5u,
                     0u, (uint8_t)s_instance.state,
                     eUSBPD_CTRL_MSG_SOFT_RESET);
#endif
    USBPD->BMC_CLK_CNT = UPD_TMR_TX;
    USBPD->TX_SEL = UPD_SOP0;
    USBPD->BMC_TX_SZ = sizeof(USBPD_MessageHeader_t);
    USBPD->STATUS = 0;
    USBPD->CONTROL |= BMC_START | PD_TX_EN;
    NVIC_EnableIRQ(USBPD_IRQn);
    return eUSBPD_OK;
}

void USBPD_ArmDirectPPSAfterSoftReset( uint32_t mv, uint32_t ma )
{
    /* Requires successfully started Soft_Reset. Fixed 7V/1A trial
     * limits are checked here and again against the received APDO. */
    if (s_instance.hardResetLatched ||
        s_instance.state != eSTATE_SOFT_RESET_WAIT_ACCEPT ||
        mv != 7000u || ma != 1000u) return;
    NVIC_DisableIRQ(USBPD_IRQn);
    s_instance.directPPSmv = mv;
    s_instance.directPPSma = ma;
    s_instance.directPPSArmed = true;
    NVIC_EnableIRQ(USBPD_IRQn);
}

bool USBPD_DirectPPSContractReady( void )
{
    return s_instance.directPPSReady && s_instance.state == eSTATE_PS_RDY;
}

void USBPD_Reset( void )
{
	NVIC_DisableIRQ( USBPD_IRQn );
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
    s_m4c_reset_postclear_captures = 0u;
    s_m4c_reset_interval_captures = 0u;
    s_m4c_reset_tick_valid = 0u;
    s_m4c_tx_end_tick_valid = 0u;
    s_m4c_last_tx_end_kind = 0u;
    s_m4c_caps_ack_valid = 0u;
    s_m4c_caps_request_started = 0u;
    s_m4c_path_main_calls = 0u;
    s_m4c_path_negotiate_calls = 0u;
    s_m4c_path_sourcecap_branch = 0u;
    s_m4c_path_last_mode = 0xffu;
    s_m4c_path_first_main_valid = 0u;
    s_m4c_path_first_negotiate_valid = 0u;

#endif
	s_instance = ( USBPD_Instance_t ){
		.pdVersion = eUSBPD_REV_30,
	};
}

USBPD_State_e USBPD_GetState( void )
{
	return s_instance.state;
}

bool USBPD_ActiveCCSample( uint8_t *line, uint16_t *raw_cc )
{
    uint8_t selected = 0u;
    uint16_t sample = 0u;
    if (s_instance.lastCCLine == eUSBPD_CC1) {
        selected = 1u;
        sample = (uint16_t)USBPD->PORT_CC1;
    } else if (s_instance.lastCCLine == eUSBPD_CC2) {
        selected = 2u;
        sample = (uint16_t)USBPD->PORT_CC2;
    }
    if (line) *line = selected;
    if (raw_cc) *raw_cc = sample;
    return selected != 0u && (sample & PA_CC_AI) != 0u;
}

bool USBPD_ActiveCCPresent( void )
{
    /* GetActiveCCLine() switches CC_SEL and would corrupt live BMC RX.
     * Instead, read the comparator of the CC selected during negotiation.
     * CC1 = PC14; CC2 = PC15 (verify the board/package pinout).
     * The MCU reference manual calls bit 0 PA_CC1_AI / PA_CC2_AI.
     */
    if ( s_instance.lastCCLine == eUSBPD_CC1 )
        return ( USBPD->PORT_CC1 & PA_CC_AI ) != 0;
    if ( s_instance.lastCCLine == eUSBPD_CC2 )
        return ( USBPD->PORT_CC2 & PA_CC_AI ) != 0;
    return false;
}


#if FUNCONF_USBPD_NO_STR
const char *USBPD_StateToStr( USBPD_State_e state )
{
	(void)state;
	return "";
}

const char *USBPD_ResultToStr( USBPD_Result_e result )
{
	(void)result;
	return "";
}
#else
const char *USBPD_StateToStr( USBPD_State_e state )
{
	switch ( state )
	{
		case eSTATE_IDLE: return "Idle";
		case eSTATE_CABLE_DETECT: return "Cable Detected";
		case eSTATE_SOURCE_CAP: return "Got Source Capabilities";
		case eSTATE_WAIT_ACCEPT: return "Waiting for Accept";
		case eSTATE_WAIT_PS_RDY: return "Waiting for PS_Ready";
		case eSTATE_PS_RDY: return "Power Supply Ready";
        case eSTATE_SOFT_RESET_WAIT_ACCEPT: return "Soft Reset: Waiting for Accept";
        case eSTATE_SOFT_RESET_FAILED: return "Soft Reset: Failed";
        case eSTATE_HARD_RESET_DETECTED: return "Hard Reset detected: reconnect required";
		default: return "Unknown State";
	}
};

const char *USBPD_ResultToStr( USBPD_Result_e result )
{
	switch ( result )
	{
		case eUSBPD_OK: return "OK";
		case eUSBPD_BUSY: return "Busy";
		case eUSBPD_ERROR: return "Error";
		case eUSBPD_ERROR_ARGS: return "Error Args";
		case eUSBPD_ERROR_NOT_SUPPORTED: return "Error Not Supported";
		case eUSBPD_ERROR_TIMEOUT: return "Error Timeout";
		default: return "Unknown Result";
	}
}
#endif // FUNCONF_USBPD_NO_STR

USBPD_Result_e USBPD_SelectPDO( uint8_t index, uint32_t voltageIn100mV )
{
    if (s_instance.hardResetLatched) return eUSBPD_BUSY;
	if ( index >= s_instance.pdoCount )
	{
		return eUSBPD_ERROR_ARGS;
	}

	const USBPD_SourcePDO_t *const pdo = &s_instance.caps.Source[index];

	*(USBPD_MessageHeader_t *)&s_buffer[0] = ( USBPD_MessageHeader_t ){
		.MessageID = s_instance.messageID,
		.MessageType = eUSBPD_DATA_MSG_REQUEST,
		.NumberOfDataObjects = 1u,
		.SpecificationRevision = s_instance.pdVersion,
	};
	USBPD_RequestDataObject_t *const rdo = (USBPD_RequestDataObject_t *)&s_buffer[sizeof( USBPD_MessageHeader_t )];

	if ( USBPD_IsPPS( pdo ) )
	{
		// Clamp voltage to min/max NOTE: Maybe we should return an error if the voltage is out of range?
		const uint32_t minVoltage = pdo->SPR_PPS.MinVoltageIn100mV;
		const uint32_t maxVoltage = pdo->SPR_PPS.MaxVoltageIn100mV;
		voltageIn100mV = voltageIn100mV > maxVoltage ? maxVoltage : voltageIn100mV;
		voltageIn100mV = voltageIn100mV < minVoltage ? minVoltage : voltageIn100mV;

		*rdo = ( USBPD_RequestDataObject_t ){
			.PPS =
				{
					.ObjectPosition = index + 1,
					.OutputVoltageIn20mV = voltageIn100mV * 5,
					.OperatingCurrentIn50mA = pdo->SPR_PPS.MaxCurrentIn50mA,
					.NoUSBSuspended = 1u,
					.USBComsCapable = 1u, // TODO: Should have these are arguments or define
				},
		};
	}
	else
	{
		*rdo = ( USBPD_RequestDataObject_t ){
			.FixedAndVariable =
				{
					.ObjectPosition = index + 1,
					.MaxCurrentIn10mA = pdo->FixedSupply.MaxCurrentIn10mA,
					.OperatingCurrentIn10mA = pdo->FixedSupply.MaxCurrentIn10mA,
					.USBComsCapable = 1u,
					.NoUSBSuspended = 1u,
				},
		};
	}

	SendMessage( 6 );

	return eUSBPD_OK;
}

/*
 * Local extension: exact PPS voltage and requested current limit.
 * Keep the ordinary USBPD_SelectPDO() unmodified for compatibility.
 * The ISR in this header processes Accept/Reject/PS_RDY asynchronously.
 */
USBPD_Result_e USBPD_SelectPPS( uint8_t index, uint32_t voltageInmV, uint32_t currentInmA )
{
    if (s_instance.hardResetLatched) return eUSBPD_BUSY;
	if ( index >= s_instance.pdoCount )
	{
		return eUSBPD_ERROR_ARGS;
	}

	const USBPD_SourcePDO_t *const pdo = &s_instance.caps.Source[index];
	if ( !USBPD_IsPPS( pdo ) )
	{
		return eUSBPD_ERROR_NOT_SUPPORTED;
	}

	/* USB PD PPS RDO units: 20 mV and 50 mA. */
	if ( ( voltageInmV % 20u ) || ( currentInmA % 50u ) || ( currentInmA < 50u ) )
	{
		return eUSBPD_ERROR_ARGS;
	}

	const uint32_t minVoltageInmV = pdo->SPR_PPS.MinVoltageIn100mV * 100u;
	const uint32_t maxVoltageInmV = pdo->SPR_PPS.MaxVoltageIn100mV * 100u;
	const uint32_t maxCurrentInmA = pdo->SPR_PPS.MaxCurrentIn50mA * 50u;
	if ( voltageInmV < minVoltageInmV || voltageInmV > maxVoltageInmV ||
		currentInmA > maxCurrentInmA || currentInmA > 6350u )
	{
		return eUSBPD_ERROR_ARGS;
	}

    /* A direct PPS Request is allowed only for the first Source_Capabilities
     * following a successfully acknowledged manual Soft_Reset. */
    const bool direct = s_instance.state == eSTATE_SOURCE_CAP &&
                        s_instance.directPPSArmed;
	if ( (s_instance.state != eSTATE_PS_RDY && !direct) ||
         ( USBPD->CONTROL & PD_TX_EN ) )
	{
		return eUSBPD_BUSY;
	}

	*(USBPD_MessageHeader_t *)&s_buffer[0] = ( USBPD_MessageHeader_t ){
		.MessageID = s_instance.messageID,
		.MessageType = eUSBPD_DATA_MSG_REQUEST,
		.NumberOfDataObjects = 1u,
		.SpecificationRevision = s_instance.pdVersion,
	};

	USBPD_RequestDataObject_t *const rdo =
		(USBPD_RequestDataObject_t *)&s_buffer[sizeof( USBPD_MessageHeader_t )];
	*rdo = ( USBPD_RequestDataObject_t ){
		.PPS = {
			.ObjectPosition = index + 1u,
			.OutputVoltageIn20mV = voltageInmV / 20u,
			.OperatingCurrentIn50mA = currentInmA / 50u,
			.NoUSBSuspended = 1u,
			.USBComsCapable = 1u,
		},
	};

	/* Unlike the original SelectPDO, mark the re-negotiation in progress
	 * so that PS_RDY (and Reject/Wait) can be observed by the application. */
	s_instance.state = eSTATE_WAIT_ACCEPT;
	SendMessage( 6 );
	return eUSBPD_OK;
}

/**
 * @brief  Get the capabilities of the USB PD Source
 * @param[out] capabilities: pointer to the capabilities message structure
 * @return Number of Power Data Objects (PDOs) in the capabilities message
 */
size_t USBPD_CopyCapabilities( USBPD_SPR_CapabilitiesMessage_t *dest )
{
        if (!dest) return 0u;
        /* Main-loop API only. A 28-byte copy keeps the USBPD IRQ disabled
         * briefly so count and PDO words belong to one completed frame. */
        NVIC_DisableIRQ( USBPD_IRQn );
        const uint8_t count = s_instance.pdoCount;
        if (count > 0u && count <= PD_CAPS_MAX_PDOS)
                memcpy(dest, &s_instance.caps, sizeof(*dest));
        NVIC_EnableIRQ( USBPD_IRQn );
        return (count <= PD_CAPS_MAX_PDOS) ? count : 0u;
}

size_t USBPD_GetCapabilities( USBPD_SPR_CapabilitiesMessage_t **capabilities )
{
	// Barrier: caps is written by the IRQ via memcpy; without this, LTO can
	// prove the main-thread call chain never writes caps and fold all reads to
	// zero. The barrier is placed here so every caller gets correct data
	// regardless of which state the negotiation exited through.
	__asm volatile( "" ::: "memory" );

	if ( s_instance.pdoCount == 0 )
	{
		return 0;
	}

	if ( capabilities )
	{
		*capabilities = &s_instance.caps;
	}

	return s_instance.pdoCount;
}

bool USBPD_IsPPS( const USBPD_SourcePDO_t *pdo )
{
	return ( pdo->Header.PDOType == eUSBPD_PDO_AUGMENTED ) && ( pdo->Header.AugmentedType == eUSBPD_APDO_SPR_PPS );
}

USBPD_SpecificationRevision_e USBPD_GetVersion( void )
{
	return s_instance.pdVersion;
}

/**
 * @brief  Check CC line status
 * @param  None
 * @return USBPD_CC_t
 */
static USBPD_CC_e GetActiveCCLine( void )
{
	// Switch to CC1
	USBPD->CONFIG &= ~CC_SEL;
	Delay_Us( 1 );
	// check if CC1 is connected
	if ( USBPD->PORT_CC1 & PA_CC_AI )
	{
		return eUSBPD_CC1;
	}

	// Switch to CC2
	USBPD->CONFIG |= CC_SEL;
	Delay_Us( 1 );
	if ( USBPD->PORT_CC2 & PA_CC_AI )
	{
		return eUSBPD_CC2;
	}

	return eUSBPD_CCNONE;
}

/**
 * @brief  Switch to RX mode
 * @param  None
 * @return None
 */
static void SwitchRXMode( void )
{
	USBPD->BMC_CLK_CNT = UPD_TMR_RX;
	USBPD->CONTROL = ( USBPD->CONTROL & ~PD_TX_EN ) | BMC_START;
}

/**
 * @brief  Begin transmission of PD message
 * @param  size: size of the message in bytes
 * @return None
 */
static void SendMessage( uint8_t size )
{
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
    /* Capture outgoing Get_Source_Cap / Request, not auto GoodCRC replies.
     * Keep the shared event sequence ordered with RX IRQ records.
     * The PD IRQ is disabled only around the fixed-size trace copy. */
    const USBPD_MessageHeader_t tx = *(USBPD_MessageHeader_t *)&s_buffer[0];
    if (!(tx.NumberOfDataObjects == 0u &&
          tx.MessageType == eUSBPD_CTRL_MSG_GOODCRC)) {
        uint32_t payload = 0u;
        if (tx.NumberOfDataObjects && size >= 6u)
            memcpy(&payload, &s_buffer[2], sizeof(payload));
        NVIC_DisableIRQ(USBPD_IRQn);
        m4c_rx_trace_add((uint16_t)size, (uint8_t)tx.NumberOfDataObjects,
                         5u, (uint8_t)tx.MessageID,
                         (uint8_t)s_instance.state,
                         (tx.NumberOfDataObjects ? payload :
                          (uint32_t)tx.MessageType));
        NVIC_EnableIRQ(USBPD_IRQn);
    }
#endif
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
    /* Save the outgoing type before the shared DMA buffer is reused. */
    const USBPD_MessageHeader_t outgoing = *(USBPD_MessageHeader_t *)&s_buffer[0];
    const uint8_t tx_kind = outgoing.NumberOfDataObjects ? 2u :
        (outgoing.MessageType == eUSBPD_CTRL_MSG_GOODCRC ? 1u :
         (outgoing.MessageType == eUSBPD_CTRL_MSG_GET_SOURCE_CAP ? 3u :
          (outgoing.MessageType == eUSBPD_CTRL_MSG_SOFT_RESET ? 5u : 4u)));
    s_m4c_last_tx_kind = tx_kind;
    s_m4c_last_tx_id = (uint8_t)outgoing.MessageID;
    s_m4c_last_tx_size = size;
    if (tx_kind == 2u && s_m4c_caps_ack_valid &&
        !s_m4c_caps_request_started) {
        /* The first Request following this Source_Capabilities ACK. */
        const uint32_t elapsed = (uint32_t)(SysTick->CNTL - s_m4c_caps_ack_tick);
        s_m4c_caps_request_started = 1u;
        m4c_rx_trace_add(0u, 0u, 15u, s_m4c_last_tx_id,
                         (uint8_t)s_instance.state, elapsed);
    }

    /* A GoodCRC is started in the RX interrupt, so record it directly there.
     * Main-thread Requests and Get_Source_Cap already have TX=... events. */
    if (tx_kind == 1u)
        m4c_rx_trace_add((uint16_t)size, 0u, 8u,
                         (uint8_t)outgoing.MessageID,
                         (uint8_t)s_instance.state, 0u);
#endif
    /* GoodCRC is an automatic response and must never replace the pending
     * Request/Get_Source_Cap acknowledgment. Both IDs are independent. */
    const USBPD_MessageHeader_t tx_header = *(USBPD_MessageHeader_t *)&s_buffer[0];
    if (!(tx_header.NumberOfDataObjects == 0u &&
          tx_header.MessageType == eUSBPD_CTRL_MSG_GOODCRC)) {
        s_instance.txAckID = (uint8_t)tx_header.MessageID;
        s_instance.txAckPending = true;
    }
	USBPD->BMC_CLK_CNT = UPD_TMR_TX;
	USBPD->TX_SEL = UPD_SOP0;
	USBPD->BMC_TX_SZ = size;
	USBPD->STATUS = 0;
	USBPD->CONTROL |= BMC_START | PD_TX_EN;
}

/**
 * @brief  Parse the received packet
 * @param  None
 * @return None
 */
static void ParsePacket( void )
{
    /* A Hard Reset invalidates the contract and message-ID epoch. Ignore
     * subsequent packets until physical detach/re-attach. In particular,
     * never generate a GoodCRC for packets in this fail-closed state: the
     * normal packet parser cannot assume a synchronized PD connection. */
    if (s_instance.hardResetLatched) return;
	bool sendGoodCRC = true;
	USBPD_MessageHeader_t message = *(USBPD_MessageHeader_t *)s_buffer;

	USBPD_State_e nextState = s_instance.state;

	if ( message.NumberOfDataObjects == 0u )
	{
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
        /* Log received Accept/Reject/Wait/PS_RDY without IRQ printf.
         * A separate trace result code 4 denotes a control message.
         * GoodCRC is intentionally omitted to avoid flooding the ring. */
        if (message.MessageType == eUSBPD_CTRL_MSG_ACCEPT ||
            message.MessageType == eUSBPD_CTRL_MSG_REJECT ||
            message.MessageType == eUSBPD_CTRL_MSG_WAIT ||
            message.MessageType == eUSBPD_CTRL_MSG_PS_RDY) {
            m4c_rx_trace_add((uint16_t)USBPD->BMC_BYTE_CNT, 0u, 4u,
                             (uint8_t)message.MessageID,
                             (uint8_t)s_instance.state,
                             (uint32_t)message.MessageType);
        }
#endif
		switch ( (USBPD_ControlMessage_e)message.MessageType )
		{
			case eUSBPD_CTRL_MSG_GOODCRC:
				sendGoodCRC = false;
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
                /* last_pdo contains prior local TX ID plus match flag in bit 8. */
                m4c_rx_trace_add((uint16_t)USBPD->BMC_BYTE_CNT, 0u,
                                 6u, (uint8_t)message.MessageID,
                                 (uint8_t)s_instance.state,
                                 (uint32_t)(s_instance.messageID & 7u) |
                                 ((s_instance.txAckPending &&
                                   s_instance.txAckID == (uint8_t)message.MessageID)
                                      ? 0x100u : 0u));
#endif
                if (s_instance.txAckPending &&
                    s_instance.txAckID == (uint8_t)message.MessageID) {
                    s_instance.txAckPending = false;
                    s_instance.messageID = (uint8_t)((s_instance.messageID + 1u) & 7u);
                    if (s_instance.state == eSTATE_SOFT_RESET_WAIT_ACCEPT)
                        s_instance.softResetAcked = true;
                }
				break;

            case eUSBPD_CTRL_MSG_ACCEPT:
                if (s_instance.state == eSTATE_SOFT_RESET_WAIT_ACCEPT) {
                    /* The Soft_Reset must have been acknowledged before its
                     * Accept is treated as a protocol reset completion. */
                    if (s_instance.softResetAcked) {
                        s_instance.sourceCapsSeen = false;
                        s_instance.pdoCount = 0u;
                        nextState = eSTATE_CABLE_DETECT;
                    }
                } else {
                    nextState = eSTATE_WAIT_PS_RDY;
                }
                break;

            case eUSBPD_CTRL_MSG_REJECT:
                nextState = (s_instance.state == eSTATE_SOFT_RESET_WAIT_ACCEPT)
                    ? eSTATE_SOFT_RESET_FAILED : eSTATE_SOURCE_CAP;
                break;

            case eUSBPD_CTRL_MSG_PS_RDY:
                if (s_instance.state != eSTATE_SOFT_RESET_WAIT_ACCEPT) {
                    if (s_instance.directPPSRequested &&
                        s_instance.state == eSTATE_WAIT_PS_RDY) {
                        s_instance.directPPSReady = true;
                        s_instance.directPPSRequested = false;
                    }
                    nextState = eSTATE_PS_RDY;
                }
                break;

            case eUSBPD_CTRL_MSG_WAIT:
                nextState = (s_instance.state == eSTATE_SOFT_RESET_WAIT_ACCEPT)
                    ? eSTATE_SOFT_RESET_FAILED : eSTATE_CABLE_DETECT;
                break;

			default: break;
		}
	}
	else
	{
		switch ( (USBPD_DataMessage_e)message.MessageType )
		{

            case eUSBPD_DATA_MSG_SOURCE_CAP: {
                /* Until the Soft_Reset Accept arrives, do not start a new
                 * Request or accept a stale Source_Capabilities. Still
                 * return GoodCRC through the ordinary common path. */
                if (s_instance.state == eSTATE_SOFT_RESET_WAIT_ACCEPT)
                    break;
                const size_t n = message.NumberOfDataObjects;
                const size_t rx_bytes = USBPD->BMC_BYTE_CNT;
                uint32_t words[PD_CAPS_MAX_PDOS] = {0};
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
                const uint8_t diag_prior_state = (uint8_t)s_instance.state;
                /* Hardware status is read before our GoodCRC TX clears it. */
                const uint16_t diag_raw_header = message.data;
                const uint16_t diag_rx_status = (uint16_t)USBPD->STATUS;
                const uint16_t diag_rx_control = (uint16_t)USBPD->CONTROL;
                const uint16_t diag_rx_config = (uint16_t)USBPD->CONFIG;
#endif
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
                s_m4c_diag_bytes = (uint16_t)rx_bytes;
                s_m4c_diag_pdos = (uint8_t)n;
#endif
                if (!pd_caps_frame_length_valid(n, rx_bytes)) {
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
                    s_m4c_diag_verdict = 1u;
                    ++s_m4c_diag_seq;
                    m4c_rx_trace_caps((uint16_t)rx_bytes, (uint8_t)n, 1u,
                                      (uint8_t)message.MessageID, diag_prior_state,
                                      0u, diag_raw_header, diag_rx_status,
                                      diag_rx_control, diag_rx_config,
                                      NULL, 0u);
#endif
                    /* Reject truncated or oversized messages. Do not publish
                     * stale/partial PDO data as a new source advertisement. */
                    break;
                }
                memcpy(words, &s_buffer[2], n * sizeof(uint32_t));
                if (!pd_caps_words_plausible(words, n)) {
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
                    s_m4c_diag_verdict = 2u;
                    ++s_m4c_diag_seq;
                    m4c_rx_trace_caps((uint16_t)rx_bytes, (uint8_t)n, 2u,
                                      (uint8_t)message.MessageID, diag_prior_state,
                                      words[n - 1u], diag_raw_header, diag_rx_status,
                                      diag_rx_control, diag_rx_config,
                                      words, (uint8_t)n);
#endif
                    break;
                }
                /* Same MessageID + identical validated PDO list is a retransmit,
                 * not a new policy event. Still send GoodCRC below. The 6->5
                 * transition uses a different ID/content and is not suppressed. */
                if (s_instance.sourceCapsSeen &&
                    s_instance.sourceCapsMessageID == (uint8_t)message.MessageID &&
                    s_instance.pdoCount == n &&
                    memcmp(s_instance.caps.Source, words, n * sizeof(uint32_t)) == 0) {
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
                    m4c_rx_trace_caps((uint16_t)rx_bytes, (uint8_t)n, 9u,
                                      (uint8_t)message.MessageID, diag_prior_state,
                                      words[n - 1u], diag_raw_header, diag_rx_status,
                                      diag_rx_control, diag_rx_config,
                                      words, (uint8_t)n);
#endif
                    break;
                }
                /* A source may advertise the SAME capabilities with a NEW
                 * MessageID after PS_RDY.  The previous policy treated that
                 * announcement as a fresh request trigger, causing an
                 * unbounded Source_Cap -> Request -> Accept -> PS_RDY loop.
                 * While the current contract is ready, acknowledge an
                 * unchanged advertisement but DO NOT re-negotiate it.
                 * Do not apply this shortcut while WAIT_ACCEPT/WAIT_PS_RDY:
                 * interrupting an outstanding Request requires the existing
                 * supersession logic.  Changed PDO values/count still go
                 * through the ordinary negotiation/fallback path. */
                if (s_instance.state == eSTATE_PS_RDY &&
                    s_instance.sourceCapsSeen &&
                    s_instance.pdoCount == n &&
                    memcmp(s_instance.caps.Source, words,
                           n * sizeof(uint32_t)) == 0) {
                    s_instance.sourceCapsMessageID =
                        (uint8_t)message.MessageID;
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
                    m4c_rx_trace_caps((uint16_t)rx_bytes, (uint8_t)n,
                                      10u, (uint8_t)message.MessageID,
                                      diag_prior_state, words[n - 1u],
                                      diag_raw_header, diag_rx_status,
                                      diag_rx_control, diag_rx_config,
                                      words, (uint8_t)n);
#endif
                    break;
                }
                /* A genuinely new Source_Capabilities while our direct-PPS
                 * Request awaits Accept supersedes the pending selection.
                 * Preserve the caller's direct-PPS policy, NOT the old RDO:
                 * Source Cap processing below replaces every PDO, and the
                 * main loop will re-select against that fresh advertisement.
                 * Retrying fixed PDO[0] here negated the direct-PPS test.
                 * Never pretend a pending Request was accepted or reuse an
                 * old APDO. Cap consecutive supersessions to avoid an
                 * uncontrolled loop if the source keeps advertising. */
                const bool interruptedDirectPPS =
                    s_instance.directPPSRequested &&
                    s_instance.state == eSTATE_WAIT_ACCEPT;
                if (interruptedDirectPPS) {
                    s_instance.directPPSRequested = false;
                    s_instance.directPPSReady = false;
                    ++s_instance.directPPSCapSupersessions;
                    if (s_instance.directPPSCapSupersessions > 2u) {
                        /* Failed closed. The caller instructs unplug/replug;
                         * it must not issue an unbounded stream of Requests. */
                        nextState = eSTATE_SOFT_RESET_FAILED;
                    } else {
                        s_instance.directPPSArmed = true;
                    }
                } else {
                    s_instance.directPPSRequested = false;
                    s_instance.directPPSReady = false;
                }
                /* Publish the entire validated list before its count/state. */
                memset(&s_instance.caps, 0, sizeof(s_instance.caps));
                memcpy(&s_instance.caps, words, n * sizeof(uint32_t));
                s_instance.pdVersion = message.SpecificationRevision;
                s_instance.pdoCount = (uint8_t)n;
                s_instance.sourceCapsSeen = true;
                s_instance.sourceCapsMessageID = (uint8_t)message.MessageID;
                if (nextState != eSTATE_SOFT_RESET_FAILED)
                    nextState = eSTATE_SOURCE_CAP;
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
                s_m4c_diag_verdict = 3u;
                ++s_m4c_diag_seq;
                m4c_rx_trace_caps((uint16_t)rx_bytes, (uint8_t)n, 3u,
                                      (uint8_t)message.MessageID, diag_prior_state,
                                      words[n - 1u], diag_raw_header, diag_rx_status,
                                      diag_rx_control, diag_rx_config,
                                      words, (uint8_t)n);
#endif
                break;
            }

			default: break;
		}
	}

	if ( message.Extended || sendGoodCRC )
	{
		Delay_Us( 30 );
		USBPD_ControlMessage_t reply = ( USBPD_ControlMessage_t ){
			.MessageID = message.MessageID,
			.MessageType = eUSBPD_CTRL_MSG_GOODCRC,
			.SpecificationRevision = s_instance.pdVersion,
		};
		*(uint16_t *)&s_buffer[0] = reply.data;
		SendMessage( sizeof( reply ) );
	}

	s_instance.state = nextState;
}

void USBPD_IRQHandler( void ) __attribute__( ( interrupt ) );
void USBPD_IRQHandler( void )
{
    /* Preserve the status as it appeared on entry. A read/modify/write of
     * this W1C register can inadvertently clear unrelated pending IRQs. */
    const uint16_t irq_entry_status = (uint16_t)USBPD->STATUS;
	// Receive complete interrupt
	if ( USBPD->STATUS & IF_RX_ACT )
	{
		// Check if we received a SOP0 packet
		if ( ( ( USBPD->STATUS & BMC_AUX_MASK ) == BMC_AUX_SOP0 ) && ( USBPD->BMC_BYTE_CNT >= 6 ) )
		{
			ParsePacket();
		}
		/* W1C: clear ONLY RX_ACT, not other pending interrupt flags. */
        USBPD->STATUS = IF_RX_ACT;
	}

	// Transmit complete interrupt (applies to all transmitted PD messages)
	if ( USBPD->STATUS & IF_TX_END )
	{
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
        s_m4c_last_tx_end_tick = SysTick->CNTL;
        s_m4c_tx_end_tick_valid = 1u;
        s_m4c_last_tx_end_kind = s_m4c_last_tx_kind;
        if (s_m4c_last_tx_kind == 1u &&
            s_instance.state == eSTATE_SOURCE_CAP) {
            s_m4c_caps_ack_tick = s_m4c_last_tx_end_tick;
            s_m4c_caps_ack_valid = 1u;
            s_m4c_caps_request_started = 0u;
            s_m4c_path_main_calls = 0u;
            s_m4c_path_negotiate_calls = 0u;
            s_m4c_path_sourcecap_branch = 0u;
            s_m4c_path_last_mode = 0xffu;
            s_m4c_path_first_main_valid = 0u;
            s_m4c_path_first_negotiate_valid = 0u;
            m4c_rx_trace_add(0u, 0u, 14u, 0u,
                             (uint8_t)s_instance.state, 0u);
        }

        /* Record before switching back to RX. In the trace, last_pdo holds
         * TX kind in bits[7:0], PD_TX_EN in bit 8, and BMC_TX_SZ in [23:16].
         * The metadata identifies the most recent SendMessage call, not an
         * independent bus-level capture. */
        m4c_rx_trace_add((uint16_t)USBPD->BMC_BYTE_CNT,
                         s_m4c_last_tx_kind, 7u,
                         s_m4c_last_tx_id,
                         (uint8_t)s_instance.state,
                         (uint32_t)s_m4c_last_tx_kind |
                         ((USBPD->CONTROL & PD_TX_EN) ? 0x100u : 0u) |
                         ((uint32_t)s_m4c_last_tx_size << 16));
#endif
		SwitchRXMode();
		/* W1C: avoid a read/modify/write clearing a simultaneous RX_RESET. */
        USBPD->STATUS = IF_TX_END;
	}

	// Reset interrupt. The reference manual identifies BMC_AUX=2 as
    // SOP' OR Hard Reset; WITH IF_RX_RESET asserted, a Hard Reset is the
    // working interpretation (as opposed to a normal SOP' data packet).
	if ( USBPD->STATUS & IF_RX_RESET )
	{
        const uint16_t reset_status = (uint16_t)USBPD->STATUS;
        const uint8_t reset_aux = (uint8_t)(reset_status & BMC_AUX_MASK);
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
        const uint32_t now_tick = SysTick->CNTL;
        const uint32_t since_reset = s_m4c_reset_tick_valid ?
            (uint32_t)(now_tick - s_m4c_last_reset_tick) : UINT32_MAX;
        const uint32_t since_tx = s_m4c_tx_end_tick_valid ?
            (uint32_t)(now_tick - s_m4c_last_tx_end_tick) : UINT32_MAX;
        if (s_m4c_caps_ack_valid) {
            const uint32_t caps_age = (uint32_t)(now_tick - s_m4c_caps_ack_tick);
            m4c_rx_trace_add(0u, s_m4c_caps_request_started,
                             16u, 0u, (uint8_t)s_instance.state, caps_age);
            /* Snapshot the active foreground stage at the reset IRQ.
             * A large age with stage 22 suggests synchronous retry printf
             * is delaying tick_session; no ISR printf or busy wait. */
            m4c_rx_trace_add((uint16_t)s_m4c_loop_stage, 0u, 19u,
                             0u, (uint8_t)s_instance.state,
                             (uint32_t)(now_tick - s_m4c_loop_stage_tick));
            /* Pack path counters and first-entry timing into two compact
             * trace entries; retain the existing 16-slot trace capacity. */
            m4c_rx_trace_add(
                (uint16_t)s_m4c_path_main_calls |
                    ((uint16_t)s_m4c_path_negotiate_calls << 8),
                s_m4c_path_sourcecap_branch, 17u,
                s_m4c_path_last_mode, (uint8_t)s_instance.state,
                s_m4c_path_first_main_valid ? s_m4c_path_first_main_ticks : UINT32_MAX);
            m4c_rx_trace_add(0u, s_m4c_path_first_negotiate_valid,
                             18u, 0u, (uint8_t)s_instance.state,
                             s_m4c_path_first_negotiate_valid ?
                                 s_m4c_path_first_negotiate_ticks : UINT32_MAX);
            s_m4c_caps_ack_valid = 0u;
        }
        s_m4c_last_reset_tick = now_tick;
        s_m4c_reset_tick_valid = 1u;
        if (s_m4c_reset_interval_captures < 4u) {
            m4c_rx_trace_reset_intervals(since_reset, since_tx,
                reset_status, (uint8_t)(USBPD->CONTROL & 0xffu),
                (uint8_t)(USBPD->CONFIG & 0xffu),
                s_m4c_last_tx_end_kind, (uint8_t)s_instance.state);
            ++s_m4c_reset_interval_captures;
        }

        /* Store raw status before clearing this W1C interrupt bit. */
        m4c_rx_trace_add(reset_status,
                         (uint8_t)(USBPD->CONTROL & 0xffu), 11u,
                         (uint8_t)(USBPD->CONFIG & 0xffu),
                         (uint8_t)s_instance.state,
                         (uint32_t)irq_entry_status |
                         ((uint32_t)USBPD->BMC_BYTE_CNT << 16));
#endif
        if (reset_aux == 2u) {
            /* USB PD 3.1 section 6.8: Hard Reset ends the prior explicit
             * contract and reinitializes protocol message counters.  In
             * this diagnostic build we deliberately do NOT autonomously
             * renegotiate while VBUS may still be vSafe0V. */
            s_instance.hardResetLatched = true;
            s_instance.messageID = 0u;
            s_instance.txAckPending = false;
            s_instance.softResetAcked = false;
            s_instance.directPPSArmed = false;
            s_instance.directPPSRequested = false;
            s_instance.directPPSReady = false;
            s_instance.directPPSCapSupersessions = 0u;
            s_instance.sourceCapsSeen = false;
            s_instance.pdoCount = 0u;
            s_instance.state = eSTATE_HARD_RESET_DETECTED;
        }
		/* W1C: clear ONLY RX_RESET, preserving any other pending flags. */
        USBPD->STATUS = IF_RX_RESET;
#if defined(M4C_RX_DIAGNOSTIC) && M4C_RX_DIAGNOSTIC
        /* STATUS read AFTER W1C, before SwitchRXMode. This distinguishes a
         * persistent IF_RX_RESET flag from a freshly received reset later.
         * Do not print in the IRQ. Capture only four reset events/session. */
        if (s_m4c_reset_postclear_captures < 4u) {
            const uint16_t after_clear = (uint16_t)USBPD->STATUS;
            m4c_rx_trace_add(after_clear,
                             (uint8_t)(USBPD->CONTROL & 0xffu), 12u,
                             (uint8_t)(USBPD->CONFIG & 0xffu),
                             (uint8_t)s_instance.state,
                             (uint32_t)reset_status |
                             ((uint32_t)USBPD->BMC_BYTE_CNT << 16));
            ++s_m4c_reset_postclear_captures;
        }
#endif
		SwitchRXMode();
	}
}

#endif
