#ifndef HILSCHER_HINETIOL_H
#define HILSCHER_HINETIOL_H
#include "hiahbl.h" // for opcodes

/** maybe like arm_opcodes.h **/

/**
 * todo:
 * - adding any reset logic
 * - you can optimizing breakpoints retriving, when discarding unnecessary execute_queue() in the stepping command.
 * -- wrapper for read/write with disable queue-flushing
 * -- Errorcontrolling with callbacks? during read write execution?
 * - I should have a stack function for performing several reads / writs in a row. Supports stepping or other consecutive commands.
 * - step does not handle breakpoints on it's own
 * - check tap-State machine if taken shortest way for consecutive shift dr
 * - implement virtual SRST
 * - sauber machen
 * - there are some states mentioned in the control register, "debug cause". You could use this, to make this thing more accurate
 * - Is it possible to detect, if the netIOL is in any unexpected state? Say major failure interrrupt etc?
 * - Watchdog is active during debugging? If so, it will pull,, it's not blocked by openOCD! (you probably could do a event hook in the config file if so? special function register etc?)
 * - netIOL has a inernal flag indicating a Error in transaction. This flag has been treated ahabbily. Probably, need to double check.
 * - write all functions int to the netIOL headder
 * - merge this files into the riscV-Project?
 * - step over breakpoint, release breakpoint while stepping?
 * - struct scan_field, providing checks on the fly
 * - sometimes 0xFFFFFFFF is read, TAP-reset possible? => implement TAP-reset!?
 * - in examine routine, the returnbit is not controlled!
 * - Implment target_reset_mode, according to state of target.
 * - probably control the breakpoint for a hardware breakpoint and raise a error in that case.
 */



/* MEMORY AREAS          # last byte: */
#define IOL_INTERNAL_VERSION "IOLv0.0.5"
#define IOL_AREA_ROM  0x2000  
#define IOL_AREA_DRAM 0x6000
#define IOL_AREA_PRAM 0x8000
#define WORKING_BUFFER_SIZE 5 //! length of the cahr-array containing on datapackage for the loop (e.g. 36bit)


// 32 bit access only
#define DEBUG_CONTROL_ADDR 0x1000 	// start / stop / step CPU
#define DEBUG_HALT_CMD     0x00010000	// command to halt CPU
#define DEBUG_RESUME_CMD   0x00000000	// resums CPU when halted or does nothing
#define DEBUG_STEP_CMD     0x00000001	// Command to write into DEBUG_CONTROL_ADDR to proceed with next step. sets flag

#define DEBUG_HIT_ADDR 0x1004		// control step / sleep CPU
#define DEBUG_STEP_FLAG_CLEAR  0x00000001	// What to write in DEBUG_HIT_ADDR to clear the flag
#define DEBUG_STEP_FLAG_IS_SET    0x00000001   // indicating also set flag, but clearing is the same
#define DEBUG_STEP_FLAG_IS_CLEARED 0x00000000 // indicating a cleared flag
#define DEBUG_PC_NEXT_ADDR 0x1300	// programm counter next
#define DEBUG_PC_Prev_ADDR 0x1304	// programm counter previous

#define MATCH_EBREAK 0x100073 // from encoding.h riscV and riscV Spec v2.2 page 104

/**
 * @brief Converting register number to address offset of netIOL
 */
#define DEBUG_OFFSET_ABS 0x00001000
#define DEBUG_OFFSET_ABS_FIRST_REGISTER DEBUG_OFFSET_ABS_X00
#define DEBUG_NUM_SUPPORTED_REGISTERS 32 // x00 to x31
#define DEBUG_NUM_PC 32



#define DEBUG_OFFSET_REL_X00    0x80 // First general purpose register
#define DEBUG_OFFSET_REL_PC     0x300
#define DEBUG_OFFSET_REL_CSR_SIE        0x008  // hilscher regdef: debug_dbg_ie
#define DEBUG_OFFSET_REL_CSR_SSTATUS    0x308	// hilscher regdef: debug_mstatus
#define DEBUG_OFFSET_REL_CSR_STVEC      0x30c	// hilscher regdef: debug_mtvec
#define DEBUG_OFFSET_REL_CSR_SEPC       0x310	// hilscher regdef: debug_mepc
#define DEBUG_OFFSET_REL_CSR_SCAUSE     0x314	// hilscher regdef: debug_mcause
#define DEBUG_OFFSET_REL_CSR_SCOUNTEREN 0x318	// hilscher regdef: debug_pcer (performance counter enable)
//#define DEBUG_OFFSET_REL_ 0x31c	// hilscher regdef: debug_pcmr (performance counter mode register)

#define DEBUG_OFFSET_ABS_X00             DEBUG_OFFSET_ABS + DEBUG_OFFSET_REL_X00
#define DEBUG_OFFSTE_ABS_PC              DEBUG_OFFSET_ABS + DEBUG_OFFSET_REL_PC
#define DEBUG_OFFSTE_ABS_CSR_SIE         DEBUG_OFFSET_ABS + DEBUG_OFFSET_REL_CSR_SIE       
#define DEBUG_OFFSTE_ABS_CSR_SSTATUS     DEBUG_OFFSET_ABS + DEBUG_OFFSET_REL_CSR_SSTATUS   
#define DEBUG_OFFSTE_ABS_CSR_STVEC       DEBUG_OFFSET_ABS + DEBUG_OFFSET_REL_CSR_STVEC     
#define DEBUG_OFFSTE_ABS_CSR_SEPC        DEBUG_OFFSET_ABS + DEBUG_OFFSET_REL_CSR_SEPC      
#define DEBUG_OFFSTE_ABS_CSR_SCAUSE      DEBUG_OFFSET_ABS + DEBUG_OFFSET_REL_CSR_SCAUSE       //! Register stores th debug reason
#define DEBUG_OFFSTE_ABS_CSR_SCOUNTEREN  DEBUG_OFFSET_ABS + DEBUG_OFFSET_REL_CSR_SCOUNTEREN

#define NETIOL_RESET_COMMAND    0xDEAD
#define NETIOL_RESET_ADDR       0x00000504
#define NETIOL_RESET_FLAG_WDOG  0x1 //! The reset is solved over a watchdog catch
#define NETIOL_RESET_FLAG_HISPI 0x2   // additioional for completion
#define NETIOL_RESET_FLAG_ADDR  0x000003a8

// Debug cause numbers from debug_dbg_ie register (Bits for interupt enabeling debug mode)
#define NETIOL_ADDR_DEBUG_IE 0x1008		//! Flag-register for enabling Taps???
#define NETIOL_ADDR_DEBUG_CAUSE 0x100c	//! Halt Cause of the controller
#define NETIOL_ADDR_DEBUG_MEPC  0x1310	//! The backup for curren programmcounter in case of an exception


// List of known febug reasons, taken from DEBBUG_IE, used for DEBUG_CAUSE
#define DEBUG_CAUSE_ECALL 11  //! Environment call from M-Mode
#define DEBUG_CAUSE_SAF    7  //! Store Access Fault (together with LAF)
#define DEBUG_CAUSE_SAM    6  //! Store Address Misaligned (never traps)
#define DEBUG_CAUSE_LAF    5  //! Load Access Fault (together with SAF)
#define DEBUG_CAUSE_LAM    4  //! Load Address Misaligned (never traps)
#define DEBUG_CAUSE_BP     3  //! EBREAK instruction causes trap
#define DEBUG_CAUSE_ILL    2  //! Illegal Instruction
#define DEBUG_CAUSE_IAF    1  //! Instruction Access Fault (not implemented)
#define DEBUG_CAUSE_IAM    0  //! Instruction Address Misaligned (never traps)







typedef enum en_hr{
	hr_unset,
	hr_resume,
	hr_halt,
}en_hr_t;

uint32_t hi_ahbl_assess_incorrect_read(struct target *target,uint32_t assesment);
int hi_netiol_halt_resume(struct target *target, en_hr_t modus);
int hi_netIOL_hr_wrap_halt(struct target *target);
int hi_netIOL_hr_wrap_resume(struct target *target, int current, target_addr_t address,
			int handle_breakpoints, int debug_execution);

int hi_net_iol_step(struct target *target, int current, target_addr_t address,int handle_breakpoints);

int netiol_set_breakpoint(struct target *target, struct breakpoint *breakpoint);
int hi_netiol_remove_breakpoint(struct target *target,
		struct breakpoint *breakpoint);

const char * netIOL_riscv_init_registers(struct target *target);




typedef uint8_t hi_iol_xbufffer[WORKING_BUFFER_SIZE];
inline void clear_buffer(hi_iol_xbufffer *buffer){
    assert(buffer!=NULL);
    memset(buffer,0x00,WORKING_BUFFER_SIZE);
}

int netiol_read_memory(struct target *target, target_addr_t address,
			uint32_t size, uint32_t count, uint8_t *buffer);

int netiol_write_memory(struct target *target, target_addr_t address,
			uint32_t size, uint32_t count, const uint8_t *buffer);

int hi_netiol_assert_reset(struct target *target);

int hi_netiol_deassert_reset(struct target *target);

#endif // HILSCHER_HINETIOL_H