/***
 * file interfacing the netIOL chip.
 * This chip is a multipurpose chip addressing ioLink specific usages
 *
 * This file contains the structure representing the netIOL-chip. It makes use of general hilscher habl functions
 *
 * There are several function pointer avariable in global struct. All functions fit into the general struct to not need a COMMAND_HANDLER,
 * they are assigned as they are.
 *
 * see: run commands for netIOL, gdb etc. at the end of the file
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <jtag/jtag.h> //  ‘TAP_IRUPDATE’ undeclared (first use in this function)
#include "target.h" //src/target/target_type.h:112:29: error: ‘enum target_register_class’ declared inside parameter list [-Werror] int *reg_list_size, enum target_register_class reg_class);
#include "target_type.h"
#include "register.h"
#include "hinetiol.h"
#include "target/breakpoints.h"

#include "target/riscv/debug_defines.h"
#include "target/riscv/riscv.h"
#include "command.h" // in original file it's not here, where does it come from?

#define TARGET_NAME "hinetiol"



/**
 * @brief detects the errorcause of a faulte register read
 * @details Use this function, when ever a config register does not match
 * any of the exspected values.
 * @param assesment The value read and does  not match any pattern.
 * @return 1 in case of that the cause was detected
 */
uint32_t hi_ahbl_assess_incorrect_read(struct target *target,uint32_t assesment){
	uint32_t cause_detected=0;
	if (assesment == 0xFFFFFFFF){
		LOG_ERROR("[hi][err][assesment]: a constant 1-level `can` indicate a not connected powersource, replug!");
		cause_detected=1;
	}else if(assesment == AHBL_ID_IOL){
		LOG_ERROR("Your targets TAP is not initialised, beacuse TAP ID was read instead of performing actions on the controller.");
		LOG_ERROR("Will examine() target, this shold fix the issiue. Init AHBL in writing 0xA to IR-Register");
		target->type->examine(target);
		cause_detected=2;
	}
	return cause_detected;
}

/**
 * @brief Set TAP int ID read, read ID, set to IDLE(operating)-state
 *
 * @details The 32bit ID is provided in the data-register of the TAP-Controller.
 * The purpose of this function is testing only.
 */
int hi_ahbl_get_id(struct target *target){

	// go to ir-state and shift
	struct scan_field field_ir[1]; // all must be init and used. number of feelds determining scans!

	uint8_t buffer_resp_ir = 0x00;
	uint8_t irq = OC_IR_IDCODE;
	field_ir[0].num_bits = 4;
	field_ir[0].out_value = &irq;
	field_ir[0].in_value = &buffer_resp_ir; // not sur what to exspect, just have a look!

	// Debugging / Exploring controlls.
	assert(target != NULL);
	assert(target->tap != NULL);
	if(target == NULL){
		LOG_ERROR("target is a zero pointer");
		assert(target != NULL);
	}

	unsigned num_tap = jtag_tap_count_enabled();
	assert(num_tap == 1); // netIOL only has one tap, so this is a error should be checked for.
	LOG_DEBUG("Num Tap created: %d",num_tap);


	//void jtag_add_ir_scan_noverify(struct jtag_tap *tap,
	//	const struct scan_field *fields, tap_state_t state);
	// TODO: ich bin mir nicht sicher, ob er wirklich den Update ausführt,
	// arm gehen nur bis zur Pause, ka, warum. => tap braucht nicht ueber update zu gehen, denen langt pause...
	jtag_add_ir_scan_noverify(target->tap,
		field_ir, TAP_IDLE);

	// AHBL should now be in a state where to read the ID

	struct scan_field field_dr[1];
	uint8_t local_Buffer[8];
	memset(local_Buffer,0x00,sizeof(local_Buffer));
	field_dr[0].num_bits = AHBL_REGISTER_LEN;
	field_dr[0].out_value = NULL;
	field_dr[0].in_value = local_Buffer; // IN, coming form controller TO host

	// Read the TAP-register as a control output
	// read the ID from DRRegister
	jtag_add_dr_scan(target->tap,sizeof(field_dr)/sizeof(field_dr[0]),
	 	field_dr, TAP_IDLE);

	// this should run the queue and fill the structure
	jtag_execute_queue();

	//uint64_t result = le_to_h_u64(local_Buffer);
	//LOG_DEBUG("Read Value: 0x%016lx",result);
	LOG_DEBUG("Read (default 0x00)%#.2x %#.2x %#.2x %#.2x %#.2x", local_Buffer[4], local_Buffer[3], local_Buffer[2], local_Buffer[1], local_Buffer[0] );
	return le_to_h_u32(local_Buffer);
}



/**
 * @brief This function will activate the AHB on the netIOL
 * @detials The tap-controller is in a default state after a powercycle.
 * The default state will retun on DO only the TAP-ID of the controller.
 * This behaviour is a standadsized behaviour among several controllers
 * allowing the debugger to retrive the target type. This is something like a
 * VID/PID of a USB device.
 *
 * To bring the TAP in activv mode, saying perfoming actions on the controller
 * the active mode must be activated. This happens through one command. This command
 * has to write 0xA in the 4bit instructionregister of the controller. Afterwards reads
 * and also writes are performed on the controller.
 *
 * For one-time initialization tasks, use target_was_examined()
 * and target_set_examined().  For example, probe the hardware
 * before setting up chip-specific state, and then set that
 * flag so you don't do that again.
 */
int hilnetiol_examine_target(struct target *target){


	////LOG_DEBUG("[hi]: enter");

	// retrieves and prints the ID of the TAP
	//uint32_t id_read = hi_ahbl_get_id(target);

	// if(AHBL_ID_IOL != id_read ){
	// 	LOG_ERROR("Exspected to read the ID of iol 0x%08x but read 0x%08x",AHBL_ID_IOL, id_read);
	// 	return ERROR_FAIL;
	// }
	if (netIOL_riscv_init_registers(target) != ERROR_OK)
		return ERROR_FAIL;


	// Switch to AHBL modus
	struct scan_field field_activate_ahbl[1]; // all must be init and used. number of feelds determining scans!

	uint8_t buffer_resp_activate = 0x00;
	field_activate_ahbl[0].num_bits = 4;
	field_activate_ahbl[0].out_value = &cOC_IR_ACTIVATE_AHBL;
	field_activate_ahbl[0].in_value = &buffer_resp_activate; // not sure what to exspect, just have a look!
	// TODO: use control values if avariabel
	// field_activate_ahbl[0].check_value =
	// field_activate_ahbl[0].check_mask  =

	jtag_add_ir_scan_noverify(target->tap,
		field_activate_ahbl, TAP_IDLE);


	// TODO: also perform a read and don't care for the response. Care only from the returning status bit.

	jtag_execute_queue();

	target_set_examined(target);

	LOG_DEBUG("Leaving examine!");
	return 0;
}


/**
 * @brief Upper level function performing a read.
 * @details this function is communication with AHBL-Unit. It is more complex
 * compared to the write function because recieved packages must be retrieved.
 *
 * Using this function for consecutive single reads might be a bottle neck. If this function is used for older
 * targets which have not implemented the fast access methodes,
 * this might be a huge overhead. Probably there is a possebilety to work with function call backs.
 * This function is automatically switching between singel and fast write.
 * fast write is supported by newer AHBL-Typs like netIOL. If an older controller is used, write a wrapper
 * which only demands one byte at a time (shortcut). More performance reached when using full potential of
 * host sided JTAG queue.
 *
 * @param size 1,2 or 4 bytes
 * @param count Number of size-packages
 * @param buffer character buffer. Only conaining as much bytes as necessarly needed.
 */
int netiol_read_memory(struct target *target, target_addr_t address,
			uint32_t size, uint32_t count, uint8_t *buffer){
		


	//LOG_DEBUG("Read size:%d number:%d",size,count);
	const uint32_t access_with = size;
	const uint32_t alignment_offset =  hi_ahbl_check_address_return_alignment(address, access_with);

	if(alignment_offset>4){
		LOG_ERROR("Wrong input parameter in read function");
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}

	/**
	 * This memory will be reused all along the generation process.
	 * From this the jtag-package will be generated
	 */
	HI_JC cmd;

	// set address
	// - gen address package
	hi_ahbl_generate_cmdstruct_reset(&cmd);
	hi_ahbl_generate_adr(&cmd, address);
	hi_ahbl_add_clock_jtag(&cmd,target->tap);

	// configure
	hi_ahbl_generate_cmdstruct_reset(&cmd);
	hi_ahbl_generate_cfg_read(&cmd, size);
	hi_ahbl_add_clock_jtag(&cmd,target->tap);

	/**
	 * there are only as many bytes in the buffer, as provided.
	 * a read request of 7 times 2 bytes has prepared a buffer for 14 bytes.
	 * no more.
	 */

	// allocating 32bit + 4 bit => 36 bit ==> 5 byte
	const uint8_t size_action = 5;
	uint8_t * ui8_lbuffer = (uint8_t*) malloc(count*size_action);

	hi_ahbl_generate_cmdstruct_reset(&cmd);
	cmd.access_width = hi_ahbl_generate_cfg_help_access_width(access_with);

	//LOG_DEBUG("Taken width (%d) converted to: 0x%0x ",access_with,cmd.access_width);

	if(1 == count){
		cmd.quantety = en_qn_single;
		cmd.data_host_to_device = ui8_lbuffer;
		hi_ahbl_generate_rdata(&cmd);
		hi_ahbl_add_clock_jtag(&cmd,target->tap);

		// we perform a single action. use the single access commands and check the statusbit accordingly.
	}else{
		cmd.quantety = en_qn_autoinc;
		hi_ahbl_generate_rdata(&cmd);
		// more then one action to perform?
		for(uint32_t posi = 0; posi<count*size_action; posi += size_action ){
			cmd.data_host_to_device = &ui8_lbuffer[posi];	// position in the temporary array every 5 bytes
			cmd.status_cmd = en_st_generated;
			hi_ahbl_add_clock_jtag(&cmd,target->tap);
		}
		// now all the jtag bits are pointing towards the local buffer.
		// those commands are only stacked. we need to force the jtagqueue to be executed

		// now we need to retrieve those earned bytes, and copy them into the global buffer.
		// --> jup over the array and retrieve the infos. Cold also check those statusbuits,
		// --> control the enums for configuration
		/// --> afterwards write the write function
		//todo: perform the single check!
	}

	/**
	 * the folowing part analyses the local buffer and
	 * extrtacting the recieved bytes from the jtag transactions
	 */
	hi_ahbl_jtag_execute_queue();

	uint32_t posi_source = 0;
	uint32_t posi_target = 0;
	uint64_t tmp_offset = 0;
	for(uint32_t it = 0 ; it<count*size_action; it += size_action ){
		// retrieving the relative offset of datapackage clocked in
		// rel offs base adr + func offset (it * access_with) ==  posi_target % data_package_size
		tmp_offset = (alignment_offset + posi_target) %4;
		/*
		mod = i_data_adr_config % 4
        uint_read= ( uint_read>>(mod*8) ) & 0xFF
		*/
		memcpy( &buffer[posi_target], &ui8_lbuffer[tmp_offset+posi_source],access_with);

		// run over source array

		posi_source += size_action; // 5 Byte for the 36 bit
		posi_target += access_with; // 1,2,4 Byte, provided in Source buffer
	}
	free(ui8_lbuffer);
	return ERROR_OK;
}

/**
 * @brief Upper level funtion for performing write actions.
 * @details This function uses AHBL-functinalety to perform write-actions.
 * It is less complex compared to the ead function. It's just writing down.
 * No warries about upcomming packages an pointers what so ever.
 *
 * This function is automatically switching between singel and fast write.
 * fast write is supported by newer AHBL-Typs like netIOL. If an older controller is used, write a wrapper
 * which only demands one byte at a time (shortcut). More performance reached when using full potential of
 * host sided JTAG queue.
 *
 * @param size 1,2 or 4 bytes
 * @param count Number of size-packages
 * @param buffer character buffer. Only conaining as much bytes as necessarly needed.
 *
 */
int netiol_write_memory(struct target *target, target_addr_t address,
			uint32_t size, uint32_t count, const uint8_t *buffer){

	//LOG_DEBUG("Write size:%d number:%d",size,count);

	const uint32_t access_with = size;
	const uint32_t alignment_offset =  hi_ahbl_check_address_return_alignment(address, access_with);
	if(alignment_offset>4){
		LOG_ERROR("Invalid input parameter for write-command");
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}

	HI_JC cmd;

	// generate address
	hi_ahbl_generate_cmdstruct_reset(&cmd);
	hi_ahbl_generate_adr(&cmd, address);
	hi_ahbl_add_clock_jtag(&cmd,target->tap);

	// generate first WDATA
	hi_ahbl_generate_cmdstruct_reset(&cmd);
	cmd.access_width = hi_ahbl_generate_cfg_help_access_width(access_with);
	cmd.quantety = en_qn_single;
	hi_ahbl_generate_wdata(&cmd,buffer);
	hi_ahbl_add_clock_jtag(&cmd,target->tap);

	// execute configured Address with WDATA
	hi_ahbl_generate_cmdstruct_reset(&cmd);
	hi_ahbl_generate_cfg_write(&cmd,access_with);
	hi_ahbl_add_clock_jtag(&cmd,target->tap);
	//___
	if(count > 1){
		// Take Previous configurations and increment address before the write
		hi_ahbl_generate_cmdstruct_reset(&cmd);
		cmd.access_width = hi_ahbl_generate_cfg_help_access_width(access_with);
		cmd.quantety = en_qn_autoinc;
		// write the data down.
		for(uint32_t it = 1;it < count;it++){
			hi_ahbl_generate_wdata(&cmd, &buffer[it*access_with]);
			hi_ahbl_add_clock_jtag(&cmd,target->tap);
		}
	}

	// for debugging purpose and finding memory leaks
	//LOG_DEBUG("Execute write queue");
	hi_ahbl_jtag_execute_queue();

	for(uint32_t posi = 0;posi < size * count;posi++){
		LOG_DEBUG("write buffer: 0x%02x",buffer[posi]);
	}


	/**
	 * main Part is done.
	 * now we need to add a check, if everythins has worked as exspected.
	 * after executing the queue, we can retrieve the errorflag end evaluate the result.
	 */

	uint8_t buffer_status[5] = {0xFF,0xFF,0xAA,0xFF,0xFF};
	hi_ahbl_generate_cmdstruct_reset(&cmd);
	hi_ahbl_generate_status_read(&cmd,buffer_status);
	hi_ahbl_add_clock_jtag(&cmd,target->tap);
	LOG_DEBUG("Execute status queue");
	hi_ahbl_jtag_execute_queue();

	// check recieved package:

	if(buffer_status[4] & 0x0){
		/**
		 * If the low bit of the last byte is set to zero an error
		 * occured.
		 */
		LOG_ERROR("AHBL detected a read access error at 0x%08x",le_to_h_u32(buffer_status));
	}
	return 0;
}


/**
 * @brief init target structures
 */
int netiol_init_target(struct command_context *cmd_ctx, struct target *target){
	// function used to init memory of internal structure, for now there is no internal memory
	LOG_USER("[hi][init][version]: %s",IOL_INTERNAL_VERSION);

	// this alocated struct will manage te registers
		LOG_DEBUG("netiol_init_target()");
		// reserve room for this structure
		target->arch_info = calloc(1, sizeof(riscv_info_t));
		if (!target->arch_info)
			return ERROR_FAIL;
		riscv_info_t *info = (riscv_info_t *) target->arch_info;
		// init structure with default values
		riscv_info_init(target, info);
		info->cmd_ctx = cmd_ctx;

		// for what use, i don't know
		//select_dtmcontrol.num_bits = target->tap->ir_length;
		//select_dbus.num_bits = target->tap->ir_length;
		//select_idcode.num_bits = target->tap->ir_length;
	return ERROR_OK;
}


uint32_t netiol_read_u32t(struct target *target, uint32_t addr){
	uint8_t buff[4];
	netiol_read_memory(target, addr, BYTE_SIZE_32, 1,buff);
	uint32_t read_32bit = le_to_h_u32(buff);
	return read_32bit;
}

/**
 * @brief prints exceptions wehn controller isin halt state.
 * @details the status of the exception is retrieved and the reult is printed to the User
 * @param target The target pointer enabeling communication to the target
 * @param the halt status retrieved earlier
 *
 * @warning some states come every time so they are ommitted, have a deeper look at them
 */
void netiol_poll_help_exception(struct target *target, uint32_t status_halt){

	// in case of a exception the caused address is stored in this register.
	// helpful for debugging I think, hu? (machiene exception programm counter)
	uint32_t mepc = netiol_read_u32t(target, NETIOL_ADDR_DEBUG_MEPC);

	switch(status_halt & 0x00000007){
		// List of known febug reasons, taken from DEBBUG_IE, used for DEBUG_CAUSE
		case DEBUG_CAUSE_ECALL:
			LOG_WARNING("[hi][poll][halt_status]: Environment call from M-Mode");
			LOG_WARNING("[hi][poll][halt_status][mepc][0x%08x]: returnvector from IR",mepc);
			break;
		case DEBUG_CAUSE_SAF:
			LOG_WARNING("[hi][poll][halt_status]: Store Access Fault (together with LAF)");
			LOG_WARNING("[hi][poll][halt_status][mepc][0x%08x]: returnvector from IR",mepc);
			break;
		case DEBUG_CAUSE_SAM:
			LOG_WARNING("[hi][poll][halt_status]: Store Address Misaligned (never traps)");break;
		case DEBUG_CAUSE_LAF:
			//LOG_WARNING("[hi][poll][halt_status]: Load Access Fault (together with SAF)");
			break;
		case DEBUG_CAUSE_LAM:
			LOG_WARNING("[hi][poll][halt_status]: Load Address Misaligned (never traps)");
			LOG_WARNING("[hi][poll][halt_status][mepc][0x%08x]: returnvector from IR",mepc); break;
		case DEBUG_CAUSE_BP:
		LOG_WARNING("[hi][poll][halt_status]: EBREAK instruction causes trap (set to DEBUGLevel for release)"); break;
		case DEBUG_CAUSE_ILL:
			LOG_WARNING("[hi][poll][halt_status]: Illegal Instruction");
			LOG_WARNING("[hi][poll][halt_status][mepc][0x%08x]: returnvector from IR",mepc);break;
		case DEBUG_CAUSE_IAF:
			//LOG_WARNING("[hi][poll][halt_status]: Instruction Access Fault (not implemented)");
			break;
		case DEBUG_CAUSE_IAM:
		LOG_WARNING("[hi][poll][halt_status]: Instruction Address Misaligned (never traps)"); break;
	}
}

/**
 * @brief Asking netIOL which state it's in.
 * @details complex routine handling the state of the controller.
 *
 * Poll is invoked to determine the state of the controller. As the name of the routine
 * says, it's a poll which is initiated several times a second. Every poll takes informations from the netIOL
 * via jtag. According to the state or changed state of the controller a GDB event is triggerd.
 * This event controlls GDB and are important to it.
 *
 * This routine also handels a halt on breakpoint. When a breakpoint ist hit, it must be replaced and the program
 * counter must be altered. Be aware, when ever the program counter is altered, the new assembler instruction is
 * preloaded. Alter the instruction at pc's address is insufficient. Old instruction will be executed.
 * This is the reason, why breakpoints are handled in here. When a breakpoint is hit, it will be replaced
 * instantly. The same breakpoint will be also replaced by GDB opoer openOCD, but this will have no effect.
 * Reason: It's done the same step again AND command is not taken. OpenOCD offers a "active state" for the breakpoints
 * this state is not used in this implementation. I'm not sure where ever all those flags are used. It does work without
 * them too.
 */
int netiol_poll(struct target *target){
	////LOG_DEBUG("[hi]: enter/leave");
	uint8_t buff_status_halted[4];
	uint8_t buff_debug_cause[4];
	uint8_t buff_pc_old[8];
	netiol_read_memory(target, DEBUG_CONTROL_ADDR, BYTE_SIZE_32, 1,buff_status_halted);
	netiol_read_memory(target, NETIOL_ADDR_DEBUG_CAUSE, BYTE_SIZE_32, 1,buff_debug_cause);
	// read prev and next pc before manipulating the pc
	netiol_read_memory(target, DEBUG_PC_NEXT_ADDR, BYTE_SIZE_32, 2, buff_pc_old);
	uint32_t status_halt = le_to_h_u32(buff_status_halted);
	uint32_t cause_halt = le_to_h_u32(buff_debug_cause);
	// assimilate pc's for later comparsion and decrementation
	uint32_t alter_pc_next;
	uint8_t  alter_pc_next_buff[4];
	uint32_t pre_alter_pc_next = le_to_h_u32(buff_pc_old);
	uint32_t pre_alter_pc_previous = le_to_h_u32( buff_pc_old + BYTE_SIZE_32); // should be the address of the breakpoint


	hi_ahbl_assess_incorrect_read(target,status_halt);
	uint32_t event_gdb_to_call = 0;
	if( (status_halt & DEBUG_HALT_CMD) == DEBUG_HALT_CMD ){
		if(status_halt & 0x00000007){
			netiol_poll_help_exception(target, status_halt);
		}

		// decide which event to take
		//LOG_DEBUG("[hi][poll[corr-pc][bp]: check for halted");
		if(cause_halt == DEBUG_CAUSE_BP && (pre_alter_pc_next != pre_alter_pc_previous )){  // target->state = TARGET_HALTED preventing from running into it twice
			//LOG_DEBUG("[hi][poll[corr-pc][bp]: has halted");
			struct breakpoint *p_breakpoint = NULL;
			event_gdb_to_call = TARGET_EVENT_DEBUG_HALTED;  // probably GDB does only count the breakpoints, if the TARGET_EVENT_HALTED signal is used???

			p_breakpoint = breakpoint_find(target, pre_alter_pc_previous);
			if (p_breakpoint != NULL) {
				// we hit a breakpoint.
				// a breakpoint is a ebreak-instruction in the code
				// this ebreak replaces a real code fragment stored in the breakpoint-element buffer
				// this buffer replaces the ebreak-command, when breakpoint is removed.
				// the programmcounter has already past the position of the breakpoint
				// so if nothing would change, the breakpoint replaces the ebreak command but it
				// would never be executed, because the pc s an executionpointer has passed by.

				// so we need to turn back the time, e.g. the pc pointing to the instruction which will be replaced
				// so this instruction can be executed.

				// for debugging reasons I belive, when the pc is altered a new command will be placed in the executionpipeline.
				// so when the pc is altered, the breakpoint has already to be altered.

				hi_netiol_remove_breakpoint(target, p_breakpoint);

				// correct pc for breakpoint size
				alter_pc_next = pre_alter_pc_next - p_breakpoint->length;
				h_u32_to_le(alter_pc_next_buff, alter_pc_next);

				netiol_write_memory(target,DEBUG_PC_NEXT_ADDR,BYTE_SIZE_32, 1,alter_pc_next_buff);
				LOG_DEBUG("[hi][poll][corr-pc][bp]: 0x%08" PRIx64 " [length]: %d",p_breakpoint->address,p_breakpoint->length);
				LOG_DEBUG("[hi][poll][corr-pc]: alter pc [from]:0x%08x [to]:0x%08x",pre_alter_pc_next, alter_pc_next);
				LOG_DEBUG("[hi][poll][corr-pc][control prev inst]: 0x%08x !== 0x%08x (assert)",pre_alter_pc_previous, alter_pc_next );


				if(pre_alter_pc_previous != alter_pc_next){
					assert(false); //! actually, the corrected pc should have the address of previous
				}
			}

		}else{
			event_gdb_to_call = TARGET_EVENT_HALTED;
		}
		target->state = TARGET_HALTED;

	}else{
		target->state = TARGET_RUNNING;
	}
	if(event_gdb_to_call){
		target_call_event_callbacks(target, event_gdb_to_call);
	}
	return ERROR_OK;
}


/**
 * @brief Set breakpoint to address
 *
 * @details To set a software breakpoint you have to excange the original instruction with the
 * EBREAK instruction on riscV instruction set. When the execution of the programm
 * traverses over the breakpoint, the controller set him self into hold and waiting for commands.
 * There are two breakpoints. one 4Byte ebreak and ond 2 Byte ebreak. Both wit the same asm command name.
 * gdb itself decides which breakpoint to take, so be prepared for both.
 * The ebreak assembler instruction raises holds the cpu, the state is detected by poll. gdb returns from execution.
 */
int netiol_set_breakpoint(struct target *target, struct breakpoint *breakpoint){
	uint32_t orig_instruction;
	if (breakpoint->type == BKPT_SOFT) {

		target_read_memory(target, breakpoint->address, breakpoint->length, 1, breakpoint->orig_instr);
		if(breakpoint->length == 4){
			// 4 byte instruction must be placed instead of the original command
			orig_instruction = le_to_h_u32(breakpoint->orig_instr);  // debug
			uint8_t breakpointArray_4[4];
			h_u32_to_le(breakpointArray_4, MATCH_EBREAK);
			netiol_write_memory(target, breakpoint->address,breakpoint->length, 1, breakpointArray_4);

		}else if(breakpoint->length==2){

			// 2 byte compressed instruction must be placed instead of the original command
			orig_instruction = le_to_h_u16(breakpoint->orig_instr);  //debug
			uint8_t breakpointArray_2[2];
			h_u16_to_le(breakpointArray_2, MATCH_C_EBREAK);
			netiol_write_memory(target, breakpoint->address,breakpoint->length, 1, breakpointArray_2);
		}else{
			LOG_ERROR("[hi][breakp][set][err]: len Breakpoint must equal 2 or 4 byte! bit is [0d%d]Byte",breakpoint->length);
			return ERROR_TARGET_UNALIGNED_ACCESS;
		}

		breakpoint->set = true;
		LOG_DEBUG("[hi][break][0x%08" PRIx64 "][set]: orig instr: %d(0x%08x)",breakpoint->address, breakpoint->length, orig_instruction);

	}else{
		LOG_INFO("[hi][break][0x%08" PRIx64 "][set][err]: netIOL (zeroRiscy) only supports software breakpoints!",breakpoint->address);
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}
	return ERROR_OK;
}


/**
 * @brief remove a breakpoint
 * @details function can only remove SW-Breakpoints realised with a ebrak command
 * which has replaced a original command. Hardwarebreakpoints are not supported by
 * the zero riscy
 */
int hi_netiol_remove_breakpoint(struct target *target,
		struct breakpoint *breakpoint){
	if (breakpoint->type == BKPT_SOFT) {
		target_write_memory(target, breakpoint->address, breakpoint->length, 1,
					breakpoint->orig_instr);
		int32_t orig_instr;
		if(breakpoint->length == 2){
			orig_instr = le_to_h_u16(breakpoint->orig_instr);
		}else{
			orig_instr = le_to_h_u32(breakpoint->orig_instr);
		}
		breakpoint->set = false;
		LOG_DEBUG("[hi][break][0x%08" PRIx64 "][remove]: removed BKPT_SOFT instr: (%d)0x%08x", breakpoint->address,breakpoint->length*8, orig_instr);
	}else{
		LOG_INFO("[hi][break][0x%08" PRIx64 "][remove][err]: netIOL (zeroRiscy) only supports software breakpoints!",breakpoint->address);
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}
	return ERROR_OK;
}


/**
 * @brief A universal function for halt or resume the core.
 * @details The netIOL makes the halt and resume kind of really simpel.
 * Just tollge a bit and the bit state reresents also the state of the
 * controller. For this the function can be easily used for bote,
 * halt and resume. The wrapper for this function is the one which goes
 * with a additional enum to the global target-function-pointer-struct
 * @param target The target structure
 * @param modus a enum deciding if to halt or tho resume the netIOL
 */
int hi_netiol_halt_resume(struct target *target, en_hr_t modus){
	assert(modus!=hr_unset); // my not be unset!, call as wrapper!
	// write to halt register, which struct entry to set?

	if (target->state == TARGET_UNKNOWN)
		LOG_WARNING("target was in unknown state when halt was requested");

	uint8_t buffer_trans[4];
	uint8_t buffer_rec[8];
	uint8_t modus_compare;
	LOG_DEBUG("Entered hr in Mode: %s",modus == hr_halt?"!halt!":"!resume!");
	if(modus == hr_halt){
		h_u32_to_le(buffer_trans, 0x00010000);
		modus_compare = 0x01;
		target_call_event_callbacks(target,TARGET_EVENT_HALTED);
		// if (target->state == TARGET_HALTED) {
		// 	LOG_DEBUG("target was already halted");
		// 	return ERROR_OK;
		// }
	}
	else{
		h_u32_to_le(buffer_trans, 0x00000000);
		modus_compare = 0x00;
		register_cache_invalidate(target->reg_cache);	//#sv
		target_call_event_callbacks(target,TARGET_EVENT_RESUMED);
	}

	// Vorher
	netiol_read_memory(target, DEBUG_CONTROL_ADDR, 4, 2, buffer_rec);
	LOG_DEBUG("[debug_dbg_ctrl]: debug mode was [%s],  single step was [%s]",buffer_rec[0]?"enabled":"disabled",buffer_rec[2]?"enabled":"disabled");

	// read all statu bits ctrl / hit
	netiol_write_memory(target, DEBUG_CONTROL_ADDR,4, 1, buffer_trans);

	// nacher
	netiol_read_memory(target, DEBUG_CONTROL_ADDR, 4, 2, buffer_rec);
	LOG_DEBUG("[debug_dbg_ctrl]: debug mode is [%s],  single step is [%s]",buffer_rec[0]?"enabled":"disabled",buffer_rec[2]?"enabled":"disabled");
	if(buffer_rec[2] == modus_compare){	// check for sleeping mode
		// SWhen 1 written, core enters debug mode, when 0 written, core exits debug mode. When read, 1 means core is in debug mode
		target->debug_reason = DBG_REASON_DBGRQ;
		target->state = modus==hr_halt?TARGET_HALTED:TARGET_RUNNING;
		return ERROR_OK;
	}else{
		LOG_WARNING("[Warning]Exspected DEBUG_CONTROL_ADDR[0-7] to be 0x%02x but is 0x%02x", modus_compare,buffer_rec[2]);
		uint32_t cause = hi_ahbl_assess_incorrect_read(target,le_to_h_u32(buffer_rec));
		if(cause == 1){
			return ERROR_FAIL;
		}
		return ERROR_OK;
	}
}

int hi_netIOL_hr_wrap_halt(struct target *target){
	return hi_netiol_halt_resume(target,hr_halt);
}

int hi_netIOL_hr_wrap_resume(struct target *target, int current, target_addr_t address,
			int handle_breakpoints, int debug_execution){
	// warning, other flags are ommitted
	return hi_netiol_halt_resume(target,hr_resume);
}

/**
 * @brief Holds CPU to perform one step
 * @details The normal usage should be to run in this function with a halted CPU.
 * How ever ist is possible to run the step command even if the cpu is running.
 * CPU will be halted afterwards, the programm counter (pc) will claim that it does not
 * match anymore.
 * @todo write this nicet, replace read and write with inline-functions
 * @todo stepping can be speed up when several commands are executed in one row. see: jtag_execute_queue()
 * @warning The breakpoint logic is not supported for now. So a breakpoint will not be auto-replaced
 */
int hi_net_iol_step(struct target *target, int current, target_addr_t address, int handle_breakpoints)
{
	//#state
	// control Target state? mdw 0x1000 1 [16]!=1
	// --> runnning: Error, even if possible, but why?
	struct breakpoint *p_breakpoint = NULL;
	// will contain the breakpoint assembler command
	uint8_t val_original_brkp[4] = {0x00,0x00,0x00,0x00};



	// target is stopped
	// retrieve pc next/prev mdw 1300 2 [0:next][1:prev]
	// log
	uint8_t buff_status_halted[4];
	uint8_t buff_pc_old[8];
	uint8_t buff_pc_new[8];
	uint8_t buff_step_flag_set[4]={0xaa,0xaa,0xaa,0xaa};		// will hold the set flag after execution of a step
	uint8_t buff_step_flag_cleared[4]={0xFF,0xFF,0xFF,0xFF};	// will hold the cleared flag after flag is cleared again
	uint8_t buff_cpu_hold[4] = {0xFF,0xFF,0xFF,0xFF};
	uint8_t cmd_clear_flag[4];
	uint8_t cmd_step[4];


	h_u32_to_le(cmd_clear_flag,DEBUG_STEP_FLAG_CLEAR);
	h_u32_to_le(cmd_step, DEBUG_STEP_CMD);

	// red status netIOL (see if already holded, necessary for warning)
	netiol_read_memory(target, DEBUG_CONTROL_ADDR, BYTE_SIZE_32, 1,buff_status_halted);
	// read both pc
	netiol_read_memory(target, DEBUG_PC_NEXT_ADDR, BYTE_SIZE_32, 2, buff_pc_old);

	// and the next breakpoint is at ...
	uint32_t pre_step_pc_next = le_to_h_u32(buff_pc_old);

		// check for breakpoint check
	if (handle_breakpoints)
		LOG_DEBUG("[hi][step][bp]: check for brakpoint at pc: 0x%08x",pre_step_pc_next);
		p_breakpoint = breakpoint_find(target, pre_step_pc_next);
	if (p_breakpoint != NULL) {
		/**
		 * netIOL only supports SW-Breakpoints, so breakpoint type is noch checked.
		 */
		LOG_DEBUG("[hi][step][bp]: Found active software-breakpoint interferring with next step");
		// if we are here and the breakpoint we are looking at is not the one we want to see
		// domething major went wrong!
		assert(p_breakpoint->address == pre_step_pc_next);
		// read breakcommand, to be sure. (kind of unnecessary, because I know the command which "should"
		// be there)
		LOG_DEBUG("[hi][step][bp]: Read breakpoint command from memory");
		netiol_read_memory(target, pre_step_pc_next, p_breakpoint->length, 1, val_original_brkp);
		// replace the breakpoint with the original instruction
		LOG_DEBUG("[hi][step][bp]: Restore original command");
		netiol_write_memory(target, pre_step_pc_next, p_breakpoint->length,1,p_breakpoint->orig_instr);

		// debug
		uint8_t val_original_inst[4]={0x00,0x00,0x00,0x00};
		netiol_read_memory(target, pre_step_pc_next, p_breakpoint->length, 1, val_original_inst);
		uint32_t ui_original_instr = le_to_h_u32(val_original_inst);
		LOG_DEBUG("[hi][step][bp][control]: set original instruction in place: 0x%08x", ui_original_instr);
		// continue with executing step
		LOG_DEBUG("[hi][step][bp]: Continue with next instruction step");
	}


	// clear sticky step flag, if possible
	// > mww 0x1004 0x1               <- manual clear flag from debugger (writing a 1!)
	netiol_write_memory(target, DEBUG_HIT_ADDR, BYTE_SIZE_32, 1, cmd_clear_flag);
	// mww 0x1000 0x00000001 // step
	netiol_write_memory(target, DEBUG_CONTROL_ADDR, BYTE_SIZE_32, 1, cmd_step);



	// sync routine from riscV-step()
	// a step is performed, afterwards give gdb the chance that it could do something...
	target->state = TARGET_RUNNING;
	target_call_event_callbacks(target, TARGET_EVENT_RESUMED);
	target->state = TARGET_HALTED;
	target->debug_reason = DBG_REASON_SINGLESTEP;
	target_call_event_callbacks(target, TARGET_EVENT_DEBUG_HALTED); //# validate if correct! (or debug only?)

	// flag should be set (double check)
	// mdw 0x1004 1 [0] =^! 1
	netiol_read_memory(target, DEBUG_CONTROL_ADDR,	BYTE_SIZE_32, 1, buff_cpu_hold);
	netiol_read_memory(target, DEBUG_HIT_ADDR,	BYTE_SIZE_32, 1, buff_step_flag_set);
	// release step-control-flag
	// mww 0x1004 0x00000001
	netiol_write_memory(target, DEBUG_HIT_ADDR, BYTE_SIZE_32, 1, cmd_clear_flag);

	// netIOL should have executed command by now, replace set breakpoint, if active, again.
	if (p_breakpoint != NULL) {
		/**
		 * Using the buffer is more likeliy prudence, cold also done with just writing the
		 * breakpoint command according to the length.
		 */
		LOG_DEBUG("[hi][step][bp]: restore breakpoint from buffer");
		netiol_write_memory(target, pre_step_pc_next, p_breakpoint->length, 1, val_original_brkp);
		uint32_t cmd_breakpoint_from_memory = le_to_h_u32(val_original_brkp);
		uint32_t cmd_original_command = p_breakpoint->length==2?le_to_h_u16(p_breakpoint->orig_instr):le_to_h_u32(p_breakpoint->orig_instr);
		LOG_DEBUG("[hi][step][bp][report]: at pc:[0x%08x]:0x%08x (SW-breakpoint) was temporarly replaced by original 0x%08x",pre_step_pc_next,cmd_breakpoint_from_memory,cmd_original_command);
	}

	// mdw 0x1004 1 [0] =^! 0
	netiol_read_memory(target, DEBUG_HIT_ADDR,	BYTE_SIZE_32, 1, buff_step_flag_cleared);
	// retrieve prev/next

	// read both pc
	netiol_read_memory(target, DEBUG_PC_NEXT_ADDR, BYTE_SIZE_32, 2, buff_pc_new);

	register_cache_invalidate(target->reg_cache); //#sv

	// Setup will be clocked down, results will be evaluated afterwards!

	uint8_t err = 0;

	//if(le_to_h_u32(buff_cpu_hold) == 0x00010001 ){
	uint32_t status_halt = le_to_h_u32(buff_status_halted);
	if( (status_halt & DEBUG_HALT_CMD) == DEBUG_HALT_CMD ){
		// CPU was ion halt moe before
		//LOG_DEBUG("[hi][step][ok]: Was holded before STEP!");
	}else{
		LOG_USER("[hi][step][warning]: CPU was not haltet when recieved step command.");
		LOG_USER("[hi][step][warning]: However CPU may be halted now. The pc-check will probably fail.");
	}
	uint32_t debug_tmp_buffer = le_to_h_u32(buff_cpu_hold);
	// todo: you could improve this according to target state
	if(debug_tmp_buffer == 0x00010001 ){
		// cpu in stepping mode after performing a step
		// LOG_DEBUG("[hi][step][ok]: CPU-holded after STEP!");
		target->state = TARGET_HALTED;
	}else{
		err =1;
		LOG_ERROR("[hi][step][err]: Exspected the cpu to be in step mode affter giving step command 0x00010001 but is 0x%08x ",debug_tmp_buffer);
		hi_ahbl_assess_incorrect_read(target,debug_tmp_buffer);
	}

	if(le_to_h_u32(buff_step_flag_set) == DEBUG_STEP_FLAG_IS_SET){
		// controller has indicated that he has done one step
		//LOG_DEBUG("[hi][step][ok]: CPU set step Flag as exspected");
	}else{
		err =1;
		LOG_ERROR("[hi][step][err]: CPU has not set step falg after a step");
	}
	// if old next programm counter is the new previous programm counter...
	uint32_t pc_exp = le_to_h_u32(buff_pc_old);	// exspected programm counter AFTER step
	uint32_t pc_real = le_to_h_u32(&buff_pc_new[4]); // actual program counter afer step
	if( pc_exp == pc_real  ){
		// we really have dione a step!!!
		LOG_DEBUG("[hi][step][ok]: real pc at [0x%08x]",pc_real);
	}else{
		err =1;
		LOG_ERROR("[hi][step][warning]: final programm counter does not match [0x%08x] != 0x%08x",pc_exp,pc_real);
	}
	#ifdef ARB_DEBUG_OMIT
	if( le_to_h_u32(buff_step_flag_cleared) == DEBUG_STEP_FLAG_IS_CLEARED){
		// And flag is cleared again
		LOG_DEBUG("[hi][step][ok]: Leaving step falg cleared");
	}else{
		err =1;
		LOG_ERROR("[hi][step][err]: Didn't left step flag cleared!");
	}
	#endif


	return err?ERROR_FAIL:ERROR_OK;
}




/**
 * @brief used as get and set register
 * @todo functions must be implemented
 */

#define DIM(x)		(sizeof(x)/sizeof(*x))

struct {
	uint16_t low, high;
} *expose_csr;

struct csr_info {
	unsigned number;
	const char *name;
};



/**
 * @brief returning correct addres for register name
 * @details This is dirty. should do it better.
 */
int netiol_regnum_to_offset(uint32_t ui_index, target_addr_t * pui_address){
	// debug Offest 0x00001000
	// debug_gpr0 (x00) "ra(return address)" is at byte offset 0x80
	// x00 "ra" has the index 1
	// => debug_offset + x00 Offset + ( index -1 ) == register Offset
	// served are the 32 xRegisters and the pc

	assert(GDB_REGNO_MISA!=ui_index);


	if(ui_index < DEBUG_NUM_SUPPORTED_REGISTERS){
		*pui_address = DEBUG_OFFSET_ABS_FIRST_REGISTER + ( ui_index << 2 ); // shift of 2 bit (because those are bye addresses and 32 bit registers)
	}else if(ui_index == DEBUG_NUM_PC){
		*pui_address = DEBUG_OFFSTE_ABS_PC;
	}else{
		switch(ui_index){
			//---
			case DEBUG_NUM_PC:
				*pui_address = DEBUG_OFFSTE_ABS_PC;
				break;
			case GDB_REGNO_MSTATUS:
				*pui_address = DEBUG_OFFSTE_ABS_CSR_SSTATUS;
				break;
			case CSR_STVEC:
				*pui_address = DEBUG_OFFSTE_ABS_CSR_STVEC;
				break;
			case CSR_SCOUNTEREN:
				*pui_address = DEBUG_OFFSTE_ABS_CSR_SCOUNTEREN;
				break;
			case CSR_SEPC:
					// machine exception programm counter
				*pui_address = DEBUG_OFFSTE_ABS_CSR_SEPC;
				break;
			case CSR_SCAUSE:
				*pui_address = DEBUG_OFFSTE_ABS_CSR_SCAUSE;
				break;
			case CSR_SIE:
				*pui_address = DEBUG_OFFSTE_ABS_CSR_SIE;
				break;
			case CSR_DPC:		//#sv, DPC/DCSR -> log if asked for! rgister_name
				*pui_address = DEBUG_PC_NEXT_ADDR;
				break;
			default:
				return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
			//___
			// Following registers have not appeared
			// case DEBUG_NUM_PC:
			// 	*pui_address = DEBUG_OFFSTE_ABS_PC;
			// 	break;
			// case CSR_SSTATUS:
			// 	*pui_address = DEBUG_OFFSTE_ABS_CSR_SSTATUS;
			// 	break;
			// case CSR_STVEC:
			// 	*pui_address = DEBUG_OFFSTE_ABS_CSR_STVEC;
			// 	break;
			// case CSR_SCOUNTEREN:
			// 	*pui_address = DEBUG_OFFSTE_ABS_CSR_SCOUNTEREN;
			// 	break;
			// case CSR_SEPC:
			// 		// machine exception programm counter
			// 	*pui_address = DEBUG_OFFSTE_ABS_CSR_SEPC;
			// 	break;
			// case CSR_SCAUSE:
			// 	*pui_address = DEBUG_OFFSTE_ABS_CSR_SCAUSE;
			// 	break;
			// case CSR_SIE:
			// 	*pui_address = DEBUG_OFFSTE_ABS_CSR_SIE;
			// 	break;
			// default:
			// 	return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
		}
	}
	return ERROR_OK;
}



/**
 * @brief getter and setter wrapper for register
 * @todo needs to be implemented
 * @details Perform action on reading or writing registers
 */
int netiol_riscv_get_register(struct target * target, uint32_t register_number,
		uint32_t * value_to_get){



	target_addr_t register_address;
	uint8_t recieve_bufffer[4];
	if(netiol_regnum_to_offset(register_number, &register_address) != ERROR_OK){
		LOG_ERROR("No register infos avariable to register 0d%d",register_number);
		return ERROR_FAIL;
	}

	if( netiol_read_memory(target, register_address,
			sizeof(recieve_bufffer), 1,recieve_bufffer)
			!= ERROR_OK){
				return ERROR_FAIL;
			}
	*value_to_get = le_to_h_u32(recieve_bufffer);
	return ERROR_OK;
}

/**
 * @brief Wrapper for retriving registers
 * @details emulating the misa-register, which tells the GDB
 * something about the riscV architecture
 */
static int netiol_riscv_get_register_wrap(struct reg *reg)
{

	struct target *target = (struct target *) reg->arch_info;
	uint32_t value_to_get;

	uint32_t regnumber = reg->number;

	if(regnumber == GDB_REGNO_MISA ){
		// misadummy is for telling the gdb client to perform 32 bit access
		// we will emulate the target type
		// zero riscy: RV32IMC ISA
		// C:  [ 2] Compressed extension
		// I:  [ 8] RV32I based ISA
		// M:  [12] Integer Multiply/Divide extension
		// MXL32: [30] = 1
		// MXL32: [31] = 0
 		uint32_t misa_dummey =  1 << 2 | 1 << 8 | 1 << 12 | 1 << 30 | 0 << 31;
		memcpy(reg->value,&misa_dummey,sizeof(misa_dummey));
		//LOG_DEBUG("Asked for misa register and became told: 0x%08x",(uint32_t)*reg->value);
		LOG_DEBUG("EMULATING Architecture info register \"misa\" Flags [C]ompressed,RV32[I], Integer[M]ulti/Div-Ext., 32bit, . content: 0x%08x",misa_dummey);

	}else{
		int result = netiol_riscv_get_register(target,reg->number,&value_to_get);

		if (result != ERROR_OK){
			LOG_DEBUG("[hi][REG][get][Err!]: reg_num-[0d%04d] %s",reg->number,(reg->name?reg->name:""));
			return result;
		}
		buf_set_u32(reg->value, 0, reg->size, value_to_get); //#todo debug: is this working
	}
	LOG_DEBUG("[hi][REG][get]: reg_num-[0d%04d] val-[0x%08x] %s",reg->number,value_to_get,(reg->name?reg->name:""));
	reg->valid = true;
	return ERROR_OK;
}

/**
 * @brief performs the read-register action
 * @details From number, the address is retrieved and the value is set
 * Provides Emulatd Register "MISA" containing info of architecture.
 */
int netiol_riscv_set_register(struct target * target, uint32_t register_number,
		uint32_t value_to_set){

	if(register_number != GDB_REGNO_MISA ){	// misa is the emulated non existing register. writes to it, will be discarded see 3.1.1 priviledged manual 1.10
		target_addr_t register_address;
		if(netiol_regnum_to_offset(register_number, &register_address) != ERROR_OK){
			LOG_ERROR("No register infos avariable to register 0d%d",register_number);
			return ERROR_FAIL;
		}

		uint8_t transmit_buffer[4];
		h_u32_to_le(transmit_buffer, value_to_set);
		// write to value to address...
		if(netiol_write_memory(target, register_address,
				sizeof(transmit_buffer),1, transmit_buffer) != ERROR_OK){
					LOG_ERROR("Read on register failed: %d",register_number);
					return ERROR_TARGET_FAILURE;
		}
	}
	return ERROR_OK;
}

/**
 * @brief Wrapper for setting register
 * @detils extact all necessary values and handels the local register cach
 * @param reg pointer to register
 * @param buf pointer to bits to be set
 */
static int netiol_riscv_set_register_wrap(struct reg *reg, uint8_t *buf)
{
	struct target *target = (struct target *) reg->arch_info;

	// extract dest value
	uint32_t value_to_set = buf_get_u32(buf, 0, reg->size);

	LOG_DEBUG("write 0x%" PRIx32 " to %s", value_to_set, reg->name);

	// Update register cach
	struct reg *r = &target->reg_cache->reg_list[reg->number];
	r->valid = true;
	memcpy(r->value, buf, (r->size + 7) / 8);

	// set register on target
	netiol_riscv_set_register(target, reg->number, value_to_set);
	LOG_DEBUG("[hi][REG][set]: reg_num-[0d%04d] val-[0x%08x] %s",reg->number,value_to_set,(reg->name?reg->name:""));
	return ERROR_OK;
}


static struct reg_arch_type riscv_reg_arch_type = {
	.get = netiol_riscv_get_register_wrap,
	.set = netiol_riscv_set_register_wrap
};

/**
 * Helper function init registers
 */
static int cmp_csr_info(const void *p1, const void *p2)
{
	return (int) (((struct csr_info *)p1)->number) - (int) (((struct csr_info *)p2)->number);
}

/**
 * @brief init register function derived from riscv.c
 * @details Init reisters of zero riscy
 * The Registers of zero risci are less then the registers of the other RISCV-Processors.
 * This function does NOT perform any actions in JTAG communication.
 * It just retrieves the infos from openOCD about the target registers which GDB ASSUMES the target
 * does provide. The assumtion is inherrited from the configscripts which tell openOCD
 * to use a netIOL. This function is part of the netIOL and specificaly written for it respecting
 * the individualety of this core.
 * The Zero-riscy is a 32bit lowcost RV32IMC core
 *
 * @param target The target structure triggerd by config file.
 */
const char * netIOL_riscv_init_registers(struct target *target)
{
	RISCV_INFO(info);

	if (target->reg_cache) {
		if (target->reg_cache->reg_list)
			free(target->reg_cache->reg_list);
		free(target->reg_cache);
	}

	target->reg_cache = calloc(1, sizeof(*target->reg_cache));
	target->reg_cache->name = "RISC-V Registers";
	target->reg_cache->num_regs = GDB_REGNO_COUNT;

	target->reg_cache->reg_list = calloc(GDB_REGNO_COUNT, sizeof(struct reg));

	const unsigned int max_reg_name_len = 12;
	if (info->reg_names)
		free(info->reg_names);
	info->reg_names = calloc(1, GDB_REGNO_COUNT * max_reg_name_len);
	char *reg_name = info->reg_names;	// reg_name is a runvar!

	// some magic taken from riscV-Project
	static struct reg_feature feature_cpu = {
		.name = "org.gnu.gdb.riscv.cpu"
	};
	static struct reg_feature feature_fpu = {
		.name = "org.gnu.gdb.riscv.fpu"
	};
	static struct reg_feature feature_csr = {
		.name = "org.gnu.gdb.riscv.csr"
	};

	// static struct reg_feature feature_virtual = {
	// 	.name = "org.gnu.gdb.riscv.virtual"
	// };

	// static struct reg_data_type type_ieee_single = {
	// 	.type = REG_TYPE_IEEE_SINGLE,
	// 	.id = "ieee_single"
	// };
	// static struct reg_data_type type_ieee_double = {
	// 	.type = REG_TYPE_IEEE_DOUBLE,
	// 	.id = "ieee_double"
	// };
	struct csr_info csr_info[] = {
//take defines, run over generated macros, have a nice huge array[][]...
#define DECLARE_CSR(name, number) { number, #name },
// encoding supplies the Macro with calls. It's inserted during compile time
#include "target/riscv/encoding.h" // done abuve for other defines
#undef DECLARE_CSR
	};
	/* encoding.h does not contain the registers in sorted order. */
	qsort(csr_info, DIM(csr_info), sizeof(*csr_info), cmp_csr_info);
	unsigned csr_info_index = 0;

	/* When gdb request register N, gdb_get_register_packet() assumes that this
	 * is register at index N in reg_list. So if there are certain registers
	 * that don't exist, we need to leave holes in the list (or renumber, but
	 * it would be nice not to have yet another set of numbers to translate
	 * between). */
	for (uint32_t number = 0; number < GDB_REGNO_COUNT; number++) {
		struct reg *r = &target->reg_cache->reg_list[number];
		r->dirty = false;
		r->valid = false;
		r->exist = true;
		r->type = &riscv_reg_arch_type;  //?
		r->arch_info = target;
		r->number = number;
		//r->size = riscv_xlen(target);
		r->size = 32; // assume all registers have 4 bytes in size
		/* r->size is set in riscv_invalidate_register_cache, maybe because the
		 * target is in theory allowed to change XLEN on us. But I expect a lot
		 * of other things to break in that case as well. */
		if (number <= GDB_REGNO_XPR31) {
			r->caller_save = true;
			switch (number) {
				case GDB_REGNO_ZERO:
					r->name = "zero";
					break;
				case GDB_REGNO_RA:
					r->name = "ra";
					break;
				case GDB_REGNO_SP:
					r->name = "sp";
					break;
				case GDB_REGNO_GP:
					r->name = "gp";
					break;
				case GDB_REGNO_TP:
					r->name = "tp";
					break;
				case GDB_REGNO_T0:
					r->name = "t0";
					break;
				case GDB_REGNO_T1:
					r->name = "t1";
					break;
				case GDB_REGNO_T2:
					r->name = "t2";
					break;
				// case GDB_REGNO_FP: // fp is alias for s0
				// 	r->name = "fp";
				// 	break;
				case GDB_REGNO_S0:
					r->name = "s0";
					break;
				case GDB_REGNO_S1:
					r->name = "s1";
					break;
				case GDB_REGNO_A0:
					r->name = "a0";
					break;
				case GDB_REGNO_A1:
					r->name = "a1";
					break;
				case GDB_REGNO_A2:
					r->name = "a2";
					break;
				case GDB_REGNO_A3:
					r->name = "a3";
					break;
				case GDB_REGNO_A4:
					r->name = "a4";
					break;
				case GDB_REGNO_A5:
					r->name = "a5";
					break;
				case GDB_REGNO_A6:
					r->name = "a6";
					break;
				case GDB_REGNO_A7:
					r->name = "a7";
					break;
				case GDB_REGNO_S2:
					r->name = "s2";
					break;
				case GDB_REGNO_S3:
					r->name = "s3";
					break;
				case GDB_REGNO_S4:
					r->name = "s4";
					break;
				case GDB_REGNO_S5:
					r->name = "s5";
					break;
				case GDB_REGNO_S6:
					r->name = "s6";
					break;
				case GDB_REGNO_S7:
					r->name = "s7";
					break;
				case GDB_REGNO_S8:
					r->name = "s8";
					break;
				case GDB_REGNO_S9:
					r->name = "s9";
					break;
				case GDB_REGNO_S10:
					r->name = "s10";
					break;
				case GDB_REGNO_S11:
					r->name = "s11";
					break;
				case GDB_REGNO_T3:
					r->name = "t3";
					break;
				case GDB_REGNO_T4:
					r->name = "t4";
					break;
				case GDB_REGNO_T5:
					r->name = "t5";
					break;
				case GDB_REGNO_T6:
					r->name = "t6";
					break;
			}
			r->group = "general";
			r->feature = &feature_cpu;
		} else if (number == GDB_REGNO_PC) {
			r->caller_save = true;	//something with alogythem
			sprintf(reg_name, "pc");
			r->group = "general";
			r->feature = &feature_cpu;
		} else if (number >= GDB_REGNO_FPR0 && number <= GDB_REGNO_FPR31) {

			r->exist = false; // no fpu in riscy

			switch (number) {
				case GDB_REGNO_FT0:
					r->name = "ft0";
					break;
				case GDB_REGNO_FT1:
					r->name = "ft1";
					break;
				case GDB_REGNO_FT2:
					r->name = "ft2";
					break;
				case GDB_REGNO_FT3:
					r->name = "ft3";
					break;
				case GDB_REGNO_FT4:
					r->name = "ft4";
					break;
				case GDB_REGNO_FT5:
					r->name = "ft5";
					break;
				case GDB_REGNO_FT6:
					r->name = "ft6";
					break;
				case GDB_REGNO_FT7:
					r->name = "ft7";
					break;
				case GDB_REGNO_FS0:
					r->name = "fs0";
					break;
				case GDB_REGNO_FS1:
					r->name = "fs1";
					break;
				case GDB_REGNO_FA0:
					r->name = "fa0";
					break;
				case GDB_REGNO_FA1:
					r->name = "fa1";
					break;
				case GDB_REGNO_FA2:
					r->name = "fa2";
					break;
				case GDB_REGNO_FA3:
					r->name = "fa3";
					break;
				case GDB_REGNO_FA4:
					r->name = "fa4";
					break;
				case GDB_REGNO_FA5:
					r->name = "fa5";
					break;
				case GDB_REGNO_FA6:
					r->name = "fa6";
					break;
				case GDB_REGNO_FA7:
					r->name = "fa7";
					break;
				case GDB_REGNO_FS2:
					r->name = "fs2";
					break;
				case GDB_REGNO_FS3:
					r->name = "fs3";
					break;
				case GDB_REGNO_FS4:
					r->name = "fs4";
					break;
				case GDB_REGNO_FS5:
					r->name = "fs5";
					break;
				case GDB_REGNO_FS6:
					r->name = "fs6";
					break;
				case GDB_REGNO_FS7:
					r->name = "fs7";
					break;
				case GDB_REGNO_FS8:
					r->name = "fs8";
					break;
				case GDB_REGNO_FS9:
					r->name = "fs9";
					break;
				case GDB_REGNO_FS10:
					r->name = "fs10";
					break;
				case GDB_REGNO_FS11:
					r->name = "fs11";
					break;
				case GDB_REGNO_FT8:
					r->name = "ft8";
					break;
				case GDB_REGNO_FT9:
					r->name = "ft9";
					break;
				case GDB_REGNO_FT10:
					r->name = "ft10";
					break;
				case GDB_REGNO_FT11:
					r->name = "ft11";
					break;
			}
			r->group = "float";
			r->feature = &feature_fpu;
		} else if (number >= GDB_REGNO_CSR0 && number <= GDB_REGNO_CSR4095) {
			// control status register, used for priviledged modes mapping, etc...
			// kind of mmu, supporting a running operating system. something like...
			r->group = "csr";
			r->feature = &feature_csr;
			unsigned csr_number = number - GDB_REGNO_CSR0;


			// run over registers. assign special or comon name.
			while (csr_info[csr_info_index].number < csr_number &&
					csr_info_index < DIM(csr_info) - 1) {
				csr_info_index++;
			}
			//
			if (csr_info[csr_info_index].number == csr_number) {
				r->name = csr_info[csr_info_index].name;
			} else {
				sprintf(reg_name, "csr%d", csr_number);
				/* Assume unnamed registers don't exist, unless we have some
				 * configuration that tells us otherwise. That's important
				 * because eg. Eclipse crashes if a target has too many
				 * registers, and apparently has no way of only showing a
				 * subset of registers in any case. */
				r->exist = false;
			}

			switch (csr_number) {
			// 	case CSR_FFLAGS:
			// 	case CSR_FRM:
			// 	case CSR_FCSR:
			// 		r->exist = riscv_supports_extension(target,
			// 				riscv_current_hartid(target), 'F');
			// 		r->group = "float";
			// 		r->feature = &feature_fpu;
			// 		break;
				case CSR_SSTATUS:
			 	case CSR_STVEC:
				 // mtvec Machine Trap Vector
			// 	case CSR_SIP:
			 	case CSR_SIE:
				 // Debug interrupt enable
				case CSR_SCOUNTEREN:
				// performance counter enable? pcer?
			// 	case CSR_SSCRATCH:
			 	case CSR_SEPC:
				// machine exception programm counter
				case CSR_SCAUSE:
				// EMULATED Register
				case CSR_MISA:
				// debug cause
			// 	case CSR_STVAL:
			// 	case CSR_SATP:
					r->exist = true;
					break;
			// 	case CSR_MEDELEG:
			// 	case CSR_MIDELEG:
			// 		/* "In systems with only M-mode, or with both M-mode and
			// 		 * U-mode but without U-mode trap support, the medeleg and
			// 		 * mideleg registers should not exist." */
			// 		r->exist = riscv_supports_extension(target, riscv_current_hartid(target), 'S') ||
			// 			riscv_supports_extension(target, riscv_current_hartid(target), 'N');
			// 		break;

			// 	case CSR_CYCLEH:
			// 	case CSR_TIMEH:
			// 	case CSR_INSTRETH:
			// 	case CSR_HPMCOUNTER3H:
			// 	case CSR_HPMCOUNTER4H:
			// 	case CSR_HPMCOUNTER5H:
			// 	case CSR_HPMCOUNTER6H:
			// 	case CSR_HPMCOUNTER7H:
			// 	case CSR_HPMCOUNTER8H:
			// 	case CSR_HPMCOUNTER9H:
			// 	case CSR_HPMCOUNTER10H:
			// 	case CSR_HPMCOUNTER11H:
			// 	case CSR_HPMCOUNTER12H:
			// 	case CSR_HPMCOUNTER13H:
			// 	case CSR_HPMCOUNTER14H:
			// 	case CSR_HPMCOUNTER15H:
			// 	case CSR_HPMCOUNTER16H:
			// 	case CSR_HPMCOUNTER17H:
			// 	case CSR_HPMCOUNTER18H:
			// 	case CSR_HPMCOUNTER19H:
			// 	case CSR_HPMCOUNTER20H:
			// 	case CSR_HPMCOUNTER21H:
			// 	case CSR_HPMCOUNTER22H:
			// 	case CSR_HPMCOUNTER23H:
			// 	case CSR_HPMCOUNTER24H:
			// 	case CSR_HPMCOUNTER25H:
			// 	case CSR_HPMCOUNTER26H:
			// 	case CSR_HPMCOUNTER27H:
			// 	case CSR_HPMCOUNTER28H:
			// 	case CSR_HPMCOUNTER29H:
			// 	case CSR_HPMCOUNTER30H:
			// 	case CSR_HPMCOUNTER31H:
			// 	case CSR_MCYCLEH:
			// 	case CSR_MINSTRETH:
			// 	case CSR_MHPMCOUNTER3H:
			// 	case CSR_MHPMCOUNTER4H:
			// 	case CSR_MHPMCOUNTER5H:
			// 	case CSR_MHPMCOUNTER6H:
			// 	case CSR_MHPMCOUNTER7H:
			// 	case CSR_MHPMCOUNTER8H:
			// 	case CSR_MHPMCOUNTER9H:
			// 	case CSR_MHPMCOUNTER10H:
			// 	case CSR_MHPMCOUNTER11H:
			// 	case CSR_MHPMCOUNTER12H:
			// 	case CSR_MHPMCOUNTER13H:
			// 	case CSR_MHPMCOUNTER14H:
			// 	case CSR_MHPMCOUNTER15H:
			// 	case CSR_MHPMCOUNTER16H:
			// 	case CSR_MHPMCOUNTER17H:
			// 	case CSR_MHPMCOUNTER18H:
			// 	case CSR_MHPMCOUNTER19H:
			// 	case CSR_MHPMCOUNTER20H:
			// 	case CSR_MHPMCOUNTER21H:
			// 	case CSR_MHPMCOUNTER22H:
			// 	case CSR_MHPMCOUNTER23H:
			// 	case CSR_MHPMCOUNTER24H:
			// 	case CSR_MHPMCOUNTER25H:
			// 	case CSR_MHPMCOUNTER26H:
			// 	case CSR_MHPMCOUNTER27H:
			// 	case CSR_MHPMCOUNTER28H:
			// 	case CSR_MHPMCOUNTER29H:
			// 	case CSR_MHPMCOUNTER30H:
			 	// case CSR_MHPMCOUNTER31H:

				// 	r->exist = true;
				// 	break;
			}

		}else{
			LOG_DEBUG("Assigned none for regnum: %d",number);
			r->group = "float";
			r->exist = false;
			r->feature = &feature_fpu;	// for not supported. gdb fails if unsupported feature?
		}

		if (reg_name[0])
			r->name = reg_name; // wenn der Wert belegt ist, dann herueber kopieren
		reg_name += strlen(reg_name) + 1;
		assert(reg_name < info->reg_names + GDB_REGNO_COUNT * max_reg_name_len);
		r->value = info->reg_cache_values[number];
	} // end for-loop done for every register
	return ERROR_OK;
}

/**
 * @brief GDB expects a register list.
 * @details This list contains all the values the RISCV supports.
 * Those registers which are not supported by the netIOL have to emulated. Oterwise
 * GDB refuses the contact to openOCD.
 */
static int netIOL_riscv_get_gdb_reg_list(struct target *target,
		struct reg **reg_list[], int *reg_list_size,
		enum target_register_class reg_class)
{
	RISCV_INFO(r);
	LOG_DEBUG("reg_class=%d", reg_class);
	LOG_DEBUG("rtos_hartid=%d current_hartid=%d", r->rtos_hartid, r->current_hartid);

	if (!target->reg_cache) {
		LOG_ERROR("Target not initialized. Return ERROR_FAIL.");
		return ERROR_FAIL;
	}

	switch (reg_class) {
		case REG_CLASS_GENERAL:
			LOG_DEBUG("Register Class General exspected, collecting first 32 regs");
			*reg_list_size = 32;
			break;
		case REG_CLASS_ALL:
			LOG_DEBUG("Asked for all registers, collecting 4000 of them!");
			*reg_list_size = GDB_REGNO_COUNT;
			break;
		default:
			LOG_ERROR("Unsupported reg_class: %d", reg_class);
			return ERROR_FAIL;
	}

	*reg_list = calloc(*reg_list_size, sizeof(struct reg *));
	if (!*reg_list)
		return ERROR_FAIL;

	for (int i = 0; i < *reg_list_size; i++) {
		assert(!target->reg_cache->reg_list[i].valid ||
				target->reg_cache->reg_list[i].size > 0);
		(*reg_list)[i] = &target->reg_cache->reg_list[i];
	}

	return ERROR_OK;
}


/**
 * @brief called by gdb EVENTS
 * @details a dummy
 */
static int arch_state(struct target *target)
{
	return ERROR_OK;
}


/**
 * @brief Assert a reset
 * @details This function would activate the SRST for normal. But netIOL has no reset pin.
 * A system-reset is archived in writing a special value to a special register in the netIOL.
 */
int hi_netiol_assert_reset(struct target *target){
	uint8_t tmp_reset_flag[4];
	uint8_t tmp_reset_value[4];
	uint8_t tmp_reset_flag_back[4];
	uint32_t tmp32_reset_flag_back;

	LOG_DEBUG("[hi][assert_reset]: Enter");

	h_u32_to_le(tmp_reset_flag,NETIOL_RESET_FLAG_WDOG);
	h_u32_to_le(tmp_reset_value,NETIOL_RESET_COMMAND);
	// reset flag register (even is not one cycle is gone so or so, so don't reag just clear!)
	netiol_write_memory(target,NETIOL_RESET_FLAG_ADDR,4,1,tmp_reset_flag);
	// Reset Target
	netiol_write_memory(target,NETIOL_RESET_ADDR,4,1,tmp_reset_value);
	// Control if target has resetted successfully
	netiol_read_memory(target,NETIOL_RESET_FLAG_ADDR,4,1,tmp_reset_flag_back);
	tmp32_reset_flag_back = le_to_h_u32(tmp_reset_flag_back);
	// reset flag
	if( (tmp32_reset_flag_back & NETIOL_RESET_FLAG_WDOG) == NETIOL_RESET_FLAG_WDOG ){
		// reset-flag set, successful reset of controller, clear flag
		netiol_write_memory(target,NETIOL_RESET_FLAG_ADDR,4,1,tmp_reset_flag);
		LOG_DEBUG("[hi][assert_reset][success!]: leave");
		return ERROR_OK;
	}else{
		LOG_ERROR("[hi][assert_reset][fail]: Target was exspected to be resetted. But target had not set flag indicating a succesfull reset!");
		hi_ahbl_assess_incorrect_read(target,tmp32_reset_flag_back);
		return ERROR_TARGET_FAILURE;
	}
}

/**
 * @brief dirty copy&paste from arm 7/9
 * @details This function should release the reset-signal, so the controller should be able to start processing again.
 * Also sets AHBL back in active state (IR = 0xA)
 */
int hi_netiol_deassert_reset(struct target *target)
{
	int retval = ERROR_OK;
	LOG_DEBUG("[hi][dessert_reset]: Enter");
	LOG_DEBUG("target->state: %s", target_state_name(target));



	/**
	 * In case polling is disabled, we need to examine the
	 * target and poll here for this target to work correctly.
	 *
	 * Otherwise, e.g. halt will fail afterwards with bogus
	 * error messages as halt will believe that reset is
	 * still in effect.
	 */
	LOG_DEBUG("[hi][dessert_reset]: examine");
	retval = target_examine_one(target);
	if (retval != ERROR_OK)
		return retval;

	LOG_DEBUG("[hi][dessert_reset]: poll status");
	retval = target_poll(target);
	if (retval != ERROR_OK)
		return retval;

	LOG_DEBUG("[hi][dessert_reset]: leave");
	return retval;
}


/**
 * Corestructure of the project.
 * The assembly of this functions is is the key for openOCD
 * to communicate with the netIOL
 */
struct target_type hinetiol_target = {
    .name = TARGET_NAME,
	.examine = hilnetiol_examine_target,
	.read_memory = netiol_read_memory,
	.write_memory = netiol_write_memory,
	.init_target = netiol_init_target,
	.poll = netiol_poll,
	.halt = hi_netIOL_hr_wrap_halt,
	.resume = hi_netIOL_hr_wrap_resume,
	.step = hi_net_iol_step,
	.add_breakpoint = netiol_set_breakpoint,
	.remove_breakpoint = hi_netiol_remove_breakpoint,
	.get_gdb_reg_list = netIOL_riscv_get_gdb_reg_list,

	.assert_reset = hi_netiol_assert_reset,
	.deassert_reset = hi_netiol_deassert_reset,

	.arch_state = arch_state,
};

/* cross compine windows
# Build openOCD as a cross compiled version from Linux to windows

LIB_FTDI_BIN="/home/SambaShare/zProjects/2018_12_02_IOLinkJtag/jtag_openOCD/binaries/crosscompile_dependencies_openocd_Windows/libftdi1-1.4git_devkit_x86_x64_14June2018/lib64"
LIB_USB_BIN="/home/SambaShare/zProjects/2018_12_02_IOLinkJtag/jtag_openOCD/binaries/crosscompile_dependencies_openocd_Windows/libusb-1.0.22/MinGW64"
D2XX_LIB="/home/SambaShare/zProjects/2018_12_02_IOLinkJtag/jtag_openOCD/binaries/crosscompile_dependencies_openocd_Windows/CDM20814_WHQL_Certified"

# cd into folder where openocd's .configure exists
## Set include path's of mingw cross build libs for windows which will be used as include during the build process
CFLAGS="-O0 -g -L${LIB_USB_BIN} -L${LIB_FTDI_BIN}"
## export those CFLAGS to be present for the configure script
export CFLAGS
## run configure, read the output off the last lines carefully, this will display if it has failed
./configure --build=i686-pc-linux-gnu --host=x86_64-w64-mingw32 --disable-werror --with-ftd2xx-win32-zipdir=${D2XX_LIB} --with-ftd2xx-lib=static --enable-ft2232_ftd2xx --prefix=$PWD/_install_windows
## build, at once omit -jN
make
## pack all stuff together to a drag&drop folder (--prefix=$PWD/_install_windows)
## will create a new directory in openocd
make install
*/

/* compile Linux
# set CFLAG for gcc:
# -O0: no optimisation
# -g : use debugging symbols, so that "eclipse" can see the files
CFLAGS="-O0 -g"
export CFLAGS
./configure --prefix=$PWD/_install_linux
make -j8

*/

/**
 # netIOL
OPENOCD_BIN=/home/SambaShare/zProjects/2018_12_02_IOLinkJtag/jtag_openOCD/repos/openocd/src/openocd
OPENOCD_SCRIPTS='/home/SambaShare/zProjects/2018_12_02_IOLinkJtag/jtag_openOCD/repos/openocd/tcl/.'
OPENOCD_INTERFACE='tcl/interface/ftdi/hilscher_nxjtag_usb.cfg'
BOARD_CONFIG='/home/SambaShare/zProjects/2018_12_02_IOLinkJtag/jtag_openOCD/repos/openocd/tcl/board/hilscher_iolink_board.cfg'

valgrind $OPENOCD_BIN \
-c "gdb_port 3333" \
-s $OPENOCD_SCRIPTS -f $OPENOCD_INTERFACE -d3 \
-c "transport select jtag" -f $BOARD_CONFIG \
-c "init" \
-c "halt" \
-c "load_image /home/SambaShare/zProjects/2018_12_02_IOLinkJtag/images_netX_netIOL/netIOL_JTAG_LED_Image/netIOL_SPITest/build/debugrel/Targets/riscV_IOLM_DL/riscv_IOL_DL.elf" \
-c "mww 0x1300 0x8248" \
-c "resume"

-c "mww 0x00000394 0x008000" \
-c "mdw 0x00000394 1" \
-c "load_image /home/SambaShare/zProjects/2018_12_02_IOLinkJtag/images_netX_netIOL/netIOL_JTAG_LED_Image/riscv_IOL_DL.elf" \


# netX50
OPENOCD_BIN=/home/SambaShare/zProjects/2018_12_02_IOLinkJtag/jtag_openOCD/repos/openocd/src/openocd
OPENOCD_SCRIPTS='/home/SambaShare/zProjects/2018_12_02_IOLinkJtag/jtag_openOCD/repos/openocd/tcl/.'
OPENOCD_INTERFACE='tcl/interface/ftdi/hilscher_nxjtag_usb.cfg'
BOARD_CONFIG='/home/SambaShare/zProjects/2018_12_02_IOLinkJtag/jtag_openOCD/repos/openocd/tcl/board/hilscher_nxhx50.cfg'
valgrind $OPENOCD_BIN \
-s $OPENOCD_SCRIPTS -f $OPENOCD_INTERFACE -d3 \
-c "transport select jtag" -f $BOARD_CONFIG \
-c "init"
*/
/*
valgrind $OPENOCD_BIN \
-c "gdb_port 3333" \
-s $OPENOCD_SCRIPTS -f $OPENOCD_INTERFACE -d3 \
-c "transport select jtag" -f $BOARD_CONFIG \
-c "init" \
-c "halt"
 */