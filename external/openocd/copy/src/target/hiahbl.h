#ifndef HILSCHER_TARGET_HIAHBL_H
#define HILSCHER_TARGET_HIAHBL_H
/**
 * The AHB-light is a reduced AHB controller. It is only capeable of maintaining ONE
 * controler. It was invented by Hilscher GmbH to reduce the units size of the die.
 * 
 * The AHBL is buildin in netX51/52 netX90? netIOL
 * The Fast functions can be used in netIOL and upwards
 * 
 * This code connects to the functions in core.c (scan Data Register of JTAG Statemachine etc...) -> driver.c -> commands.c
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <jtag/jtag.h>
#include <helper/types.h>

typedef uint64_t target_addr_t; // added for compatipility to  v0.10.0

extern const uint8_t cOC_IR_ACTIVATE_AHBL;


// basics
// TODO: may also be in the jtag.c be defined
#define BYTE_SIZE_08 1
#define BYTE_SIZE_16 2
#define BYTE_SIZE_32 4

/**
 * AHBL-Register properties
 */
#define AHBL_LENGTH_BIT_IR 4
#define AHBL_REGISTER_LEN_OPCODE 4
#define AHBL_REGISTER_LEN_DATA 32
#define AHBL_REGISTER_LEN AHBL_REGISTER_LEN_OPCODE + AHBL_REGISTER_LEN_DATA
#define AHBL_ID_IOL 0x101026ad
/**
 * Opcodes for the dr register
 * 
 * Once the ahbl is initialised the acces will be performed by the dataregister (dr) only
 * Commands are shiftet in and in the next cycle the response is shifted out. The upper
 */

/**
 * config (start access => OC_CFG_START_ACCESS) (Prepare read / write) 
 *
 * In Addition to the config opcode in [35:32] you'll need to provide an
 * instruction in the datapackage concerning the size (MAS) and the action (RW)
 * The datapackage will be build according to this occupiing the first 4 low bits [0:3]
 * datapackage[0:3] CFG_MAS | CFG_RW | CFG_TRANS 
 */
#define CFG_MAS_32BIT 0x8 // 0b1000  // [2:3]
#define CFG_MAS_16BIT 0x4 // 0b0100  // [2:3]
#define CFG_MAS_08BIT 0x0 // 0b0000  // [2:3]
#define CFG_RW_WRITE  0x2 // 0b0010  // [1]
#define CFG_RW_READ   0x0 // 0b0000  // [1]
#define CFG_ACTIVATE  0x1 // [0] activates the transmit of the

// #define BIT_OFFSET_STATUS 32  # every operation reading a shift of the dr-register outputs at this offset a flag. 1:err 0:ok

/**
 * Opcodes are used for telling the AHBL what to do. Mainly
 * performing read and write actions.
 * 
 * Defined are the heads of the opcode [35:32], Opcodes may have additional bits
 * in the dataarea. 
 * 
 * // instructiion shift in (TDO) to the controllers 36-bit-dateregister (dr)
 * [35:32][31:0] [opcode][datapackage]
 */

/* 32-bit-datapackage (dp) containing data to write ()
 * for 8 and 16 bit space will be filled with the data.
 * [16high][16low][16high][16low]
 * [8bit][8bit][8bit][8bit]*/
#define OC_NOT_SET 0xFF
#define OC_WDATA 0x0
/* where to read or to write from*/
#define OC_ADR 0x1
/* performing prepared an action*/
#define OC_CFG_START_ACCESS 0x2
/* Read data from Addres without incrementing (inc) address register afterwards */
#define OC_READ_ONLY 0x3
/** Read data from address and inc address,
 * data from next address will be ready in the next cycle
 * Be aware to set the config again!*/
#define OC_CFG_INC_ADR 0x4
/** continously write data to one address without incrementing the address
 * this can be handy for queues */
#define OC_WDATA_AUTOW 0x5
/** Writing data to autoincrementing addresses
 * handy for writing large chunks of data */
#define OC_WDATA_AUTOW_INC_ADR 0x6
/** Verificate the correct verifications of the past commands
 * in case of an error indicated by clocked in bit at
 * position (posi) 33 (TDI[33]==0). In case of a error, the 
 * address of the first "buggy" command is returned*/
#define OC_STATUS_READ 0x7
/** Clear errorflag
 * flag will be set to TDI[33]==1 */
#define OC_STATUS_CLEAR 0x8

/**
 * Opcodes for IR-Register.
 * Most important for activatiion is 0xA which allows to use the AHBL.
 * Also you would like to know that you can switch to the ID with 0xE.
 * The ID will then be provided in the Dataregister
 */
#define OC_IR_EXTEST 0x0
#define OC_IR_SCAN_N 0x2
#define OC_IR_SAMPLE_PRELOAD 0x3
#define OC_IR_RESTART 0x4
#define OC_IR_ETM9 0x6
#define OC_IR_CLAMP 0x5
#define OC_IR_HIGHZ 0x7
#define OC_IR_CLAMPZ 0x9
#define OC_IR_ACTIVATE_AHBL 0xA 
#define OC_IR_INTEST 0xC
#define OC_IR_IDCODE 0xE  
#define OC_IR_BYPASS 0xF

/**
 * TAP ID:
 * sysdebug_jtag_idcode {4'b0001, 8'b00000001, 8'b00000010, 11'h356,         1'b1};
 * ID_BOUNDARY_SCAN 0b10000000100000010011010101101
 * ID_BOUNDARY_SCAN_MASK 0b11111111111111111111111111111
 */

typedef enum hi_staus_cmd{ // status_cmd
    en_st_unused, // no action performed on dataset
    en_st_filled, // filled with all infos
    en_st_generated, // final step, data ready to be converted to the cahr-array
}hi_en_st_t;

typedef enum hi_access_width{ // access_width
    en_aw_none = 42,
    en_aw08 = CFG_MAS_08BIT,
    en_aw16 = CFG_MAS_16BIT,
    en_aw32 = CFG_MAS_32BIT,
}hi_en_aw_t;

typedef enum hi_quantety{
    en_qn_none,    //! may not exist, makes sure that switch case fails
    en_qn_single,  //! perform a single action without preparong a next to be written or read
    en_qn_buffer,  //! perform several actions on the same address
    en_qn_autoinc, //! perform several actions but incrementing the address automatically
}hi_en_qn_t;


/**
 * @brief only necessary for exec commands.
 * 
 * @details also used for determining if exec command does exist to select the correct bytes while creating package
 */
typedef enum hi_rw_modus{ // rw_modus
    en_rw_none = 42,
    en_rw_read = CFG_RW_READ,
    en_rw_write = CFG_RW_WRITE,
}hi_en_rw_t;


typedef struct hi_jtag_cmd{

    uint8_t * data_host_to_device;  // universal pointer to be castet as appropriated.
    uint8_t pu8_buffer[5]; // buffer clocked out, (36 bit buffer)

    hi_en_qn_t quantety;    // accesstype single/cont same/ cont autoinc   
    hi_en_st_t status_cmd; // could use enum but uses up 32bit. waste, also looking at alignment
    uint8_t u8_source_opcode; // command like WDATE, ADR, config... 
    hi_en_rw_t rw_modus; // read / write bits for MAS config
    hi_en_aw_t access_width; // 1,2,4
    
}HI_JC;

// returns the alignment offset or 0xFFFFFFF in case of error
uint32_t hi_ahbl_check_address_return_alignment(uint32_t address, uint32_t access_with);

hi_en_aw_t hi_ahbl_generate_cfg_help_access_width(uint32_t num_bytes_per_action);

// functions used for generating Packlages
// reset:
void hi_ahbl_generate_cmdstruct_reset(HI_JC * cmd);
// internal:
void hi_ahbl_genrate_package(HI_JC * cmd);
void hi_ahbl_generate_exec(HI_JC * cmd );

// generate finished packges (opper level function - callable):
// (1) Setting the address
void hi_ahbl_generate_adr(HI_JC * cmd, target_addr_t u32_addr);

// (2) Configuration 
// number to write/write size
void hi_ahbl_generate_cfg_read(HI_JC * cmd, uint32_t num_bytes_per_action);
void hi_ahbl_generate_cfg_write(HI_JC * cmd, uint32_t num_bytes_per_action);

// (3 tp N) where N is the number of actions
void hi_ahbl_generate_rdata(HI_JC * cmd);
void hi_ahbl_generate_wdata(HI_JC * cmd, const uint8_t * buf);

// (last) validate
void hi_ahbl_generate_status_read(HI_JC * cmd, uint8_t * pdata_host_to_device);
void hi_ahbl_generate_stauts_clear(HI_JC * cmd);

// performing the action in adding the package to the jtag clock
void hi_ahbl_add_clock_jtag(HI_JC * cmd,struct jtag_tap *active_tap);

/**
 * @brief a wrapper for queue execution
 * 
 * this triggers the stored packages in the jtag queue on
 * host to be send down to the controller.
 */ // das inline hat er nicht geocht, wenn die optemierung aus ist???
void hi_ahbl_jtag_execute_queue(void);

#endif // HILSCHER_TARGET_HIAHBL_H