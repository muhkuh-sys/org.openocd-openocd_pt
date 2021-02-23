#include "hiahbl.h"


const uint8_t cOC_IR_ACTIVATE_AHBL = 0xA;


/**
 * open Todo's
 - write config script for attaching to netiol
 - Add iol to comon target structure
 - look, if OOCD with netIOL compiles
 - run first printf from implemented functin set, return default values as dummies!
 -- there will be problems, so be aware of the scedule
 - go deeper into jtag command
 - how to jtag, is tap chain working?
 - shift dr => we'll recieve a default value
 - shift ir
 FIRST CONTACT IS MADE!
 - Go and implement the RW-Functions
 - Test RW-Functions
 - Get deep into the RISCV-Register, have a look at the already existing structure
 - general register functions
 - Stop/resume CPU
 - read processor registers
 - Breakpoints
 - Keep in mind to control the transactions with the errorbit.
 - Endianess, think about tests.

 - rename ahbl-functions which are still in the IOL.c and move tehm
 - put comment into headder
*/

/**
 * @file
 * This file is describing the communication with the Hilscher AHBL dap.
 * The AHBL is designed to consume as less space on the silicon as possible.
 * It is only capeable of handling one CPU. It's herachi is small and flat.
 * Once activsated all the communication runns ober the DR-Register.
 * Read and write transactions are issiued with a maxiumum of 3x DR-SHIFT + Running over Update-dr.
 * for newer targets a fast read/write mode is in place for performing access with a single package.
 * 
 * [JTAG-calbe] --- [TAP-statemachine] --- [AHBL] --- [AHB-Protocoll] --- [CPU]
 * 
 * The DR-Register is 36 bit width. bit[35:32] are the comamnd bits/status bits.
 * the DR-Packages are generated from several options, commands and the payload.
 * 
 * The crafting function is hi_ahbl_genrate_package(). The cmd-structure is passed throu all
 * instances of the generate functions. The additional parameters do matter doing the creation-process.
 * The structure can be reused, when reset. This minimizes the allocation of the memory.
 * 
 * .status_cmd is used to determine the proceeding of the package. Failing to obey the order of the functions
 * will lead into a assertion and end the openOCD.
 * Other elements are used for sending, or generating the final package.
 * 
 * All enums used as values for the struct have a default value of zero indicationg that they are NOT used.
 * 
 */


/**
 * checks incomming addresses and access_width
 * 
 * returns with error in case:
 *     - the address exceeds 32bit max
 *     - the access with is not 1,2,4 byte
 *     - the access is unaligned depending on access with
 * Otherwise it returns the relative alignment offset.
 * So wanne read from address 0x3407 one byte, it will return 3. 
 * @param address asked for
 * @param access_width 1,2,4 byte
 */
uint32_t hi_ahbl_check_address_return_alignment(uint32_t address, uint32_t access_with){
    // refer to 32bit alignment
    
    if(access_with!=1 && access_with != 2 && access_with != 4){
        LOG_ERROR("Your access_with is not supported (1,2,4)Byte but is %d Byte",access_with);
        return 0xFFFFFFFF;
    }
    if(0xFFFFFFFF < address){
        return 0xFFFFFFFF;
       // LOG_ERROR("Your address exeeds the addressable space of the 32bit AHBL! %d !<= %ld ",0xFFFFFFFF ,address);
    }
    uint32_t offset = address % 4;
    if( offset % access_with){
        // unaligned access
        LOG_ERROR("[hi][ALIGNM][err][0x%08x]mod.4=offset, offset[%d], acceswidth[%d]",address,offset, access_with);
        return 0xFFFFFFFF;
    }else{
        return offset;
    }
}


/**
 * @brief Resets the commd structure
 * 
 * @details Zeroing all values like memset is not possible.
 * Some of the values are in set state zero. So wiping with zero wold not make sence
 * @param cmd Command to be reset
 */
void hi_ahbl_generate_cmdstruct_reset(HI_JC * cmd){
    // unset vars will be erased
    static const HI_JC wipeCommand = {
        .data_host_to_device = NULL,
        .quantety = en_qn_none,
        .status_cmd = en_st_unused,
        .u8_source_opcode = OC_NOT_SET, 
        .rw_modus = en_rw_none,
        .access_width = en_aw_none,
        // buffer keep it zero
        // source data too        
        };
    *cmd = wipeCommand;
}


/**
 * Lowlevel function merging collected data together to a final package
 * 
 * function does not care about the content, only makes sure a final command is build,
 * ready to be converted to a cahr-array
 * @param cmd Command structure
 */ 
void hi_ahbl_genrate_package(HI_JC * cmd){
    assert(cmd!=NULL);
    assert(cmd->status_cmd == en_st_filled);
    // pack the opcode to [35:32] followed by data [31:0]
    cmd->pu8_buffer[4] = cmd->u8_source_opcode;
    cmd->status_cmd = en_st_generated;
}

/**
 * Generate Package to instruct AHBL to write a Address into the Address register
 * @param u32_addr Address telling AHBL where to perform next action
 * @param cmd Command structure
 */
void hi_ahbl_generate_adr(HI_JC * cmd, target_addr_t u32_addr){
    assert(0xFFFFFFFF >= u32_addr); // AHBL is 32 bit! no more

    //LOG_DEBUG("[hi]: enter, write addr: 0x%08lx",u32_addr);
    assert(cmd!=NULL);
    assert(cmd->status_cmd == en_st_unused);
    cmd->u8_source_opcode = OC_ADR;

    h_u32_to_le(cmd->pu8_buffer, u32_addr);
    
    cmd->status_cmd = en_st_filled;
    hi_ahbl_genrate_package(cmd);
    //LOG_DEBUG("[hi]: leave");
}

/**
 * @brief Generated package will check if a transaction since the last time was faulty
 * 
 * @details If it was faulty, the transaction will return the address which access was faulty in the first place
 * The statusflag should be cleared afterwards.
 * @param pdata_host_to_device Pointer to data for AHBL
 */
void hi_ahbl_generate_status_read(HI_JC * cmd, uint8_t * pdata_host_to_device){
    //LOG_DEBUG("[hi]: enter");

    const HI_JC readStatus = {
            .pu8_buffer[4] = OC_STATUS_READ,
            .status_cmd = en_st_generated,
        };
    
    *cmd = readStatus;
    cmd->data_host_to_device = pdata_host_to_device;
    //LOG_DEBUG("[hi]: leave");
}

/**
 * @brief Clearing the status bit in AHBL. This resets the error.
 * 
 * @todo Don't know if it's necessary to clear it after read.
 * @param cmd Command generated to clear the Status of AHBL
 */
void hi_ahbl_generate_stauts_clear(HI_JC * cmd){
    
    const HI_JC clearStatus = {
            .pu8_buffer[4] = OC_STATUS_CLEAR,
            .status_cmd = en_st_generated,
        };
    *cmd = clearStatus;
}

/**
 * @brief Generats the basic exec package
 * 
 * @details This package tells the AHBL what to prepeare, a read a write, something else.
 * basically it tell the AHBL which register to map behind the dr-scan-field.
 * 
 * @param cmd Command structure
 */
void hi_ahbl_generate_exec(HI_JC * cmd ){
    //LOG_DEBUG("[hi]: enter");
    assert(cmd!=NULL);
    assert(cmd->status_cmd==en_st_filled);
    assert(cmd->access_width != en_aw_none);
    assert(cmd->rw_modus != en_rw_none);
    assert(cmd->u8_source_opcode != OC_NOT_SET );
    cmd->pu8_buffer[0] = cmd->access_width | cmd->rw_modus | CFG_ACTIVATE;
    cmd->pu8_buffer[1] = 0x00;
    cmd->pu8_buffer[2] = 0x00;
    cmd->pu8_buffer[3] = 0x00;
    cmd->pu8_buffer[4] = cmd->u8_source_opcode;
    cmd->status_cmd = en_st_generated;
    //LOG_DEBUG("[hi]: leave");
}

/**
 * @brief acces with is given by openOCD as 1,2 or 4 bytes.
 * 
 * This is kind of onsafe and leads to magic numbers. To maintain read-
 * abilety this feature is added.
 */
hi_en_aw_t hi_ahbl_generate_cfg_help_access_width(uint32_t num_bytes_per_action){
    //LOG_DEBUG("[hi]: enter");
       
    switch(num_bytes_per_action){
    case 1:
        return en_aw08;
        break;
    case 2:
        return en_aw16;
        break;
    case 4:
        return en_aw32;
        break;
    default:
        assert(false); // action not supported with given number of bits.
        return en_aw_none;
        break;
    }
    //LOG_DEBUG("[hi]: leave");
}


/**
 * @brief Set config for all data which is same in CFG package for
 * read and write
 * 
 * @details set the access size if 1,2, or 4 bytes. Also set the general opcode
 * config flag. does not affect the status of the structure
 * @param num_bytes_per_action Number of bytes for performing the action
 * @param cmd Command structure
 */
void hi_ahbl_generate_cfg_help_num_and_opcode(HI_JC * cmd,uint32_t num_bytes_per_action){
    cmd->u8_source_opcode = OC_CFG_START_ACCESS;
    cmd->access_width = hi_ahbl_generate_cfg_help_access_width(num_bytes_per_action);
}


/**
 * @brief Generation of the second package part for reading something.
 * 
 * @detials It does not matter, how many byts will be read afterwards.
 * @param cmd command structure, resetted
 * @param num_bytes_per_action You can perform 1, 2, or 4 Byte read access
 */
void hi_ahbl_generate_cfg_read(HI_JC * cmd, uint32_t num_bytes_per_action){
    //LOG_DEBUG("[hi]: enter");
    assert(cmd != NULL);
    assert(cmd->status_cmd == en_st_unused);

    cmd->rw_modus = en_rw_read;
    hi_ahbl_generate_cfg_help_num_and_opcode(cmd,num_bytes_per_action);

    cmd->status_cmd = en_st_filled;
    hi_ahbl_generate_exec(cmd);
    //LOG_DEBUG("[hi]: leave");
}

/**
 * @brief Generation of the second package part for write something.
 * @detials It does not matter, how many byts will be written afterwards.
 * @param cmd Command structure
 * @param num_bytes_per_action You can perform 1, 2, or 4 Byte write
 * access. (access_width)
 */
void hi_ahbl_generate_cfg_write(HI_JC * cmd, uint32_t num_bytes_per_action){

    //LOG_DEBUG("[hi]: enter");
    assert(cmd != NULL);
    assert(cmd->status_cmd == en_st_unused);

    cmd->rw_modus = en_rw_write;
    hi_ahbl_generate_cfg_help_num_and_opcode(cmd,num_bytes_per_action);

    cmd->status_cmd = en_st_filled;
    hi_ahbl_generate_exec(cmd);    
    //LOG_DEBUG("[hi]: leave");
}







/**
 * @brief generating the thired and consecutive packages of a command chain (read)
 * 
 * @detials Running this command will handle the prepared dr-register or
 * retrieving read data from.
 * The auto inc command uses a exec function, telling AHBL to prepare the next.
 * @param cmd Command to generate
 */
void hi_ahbl_generate_rdata(HI_JC * cmd){
    //LOG_DEBUG("[hi]: enter");
    assert(cmd!=NULL);
    assert(cmd->quantety != en_qn_none);
    assert(cmd->rw_modus == en_rw_none);
    
    switch(cmd->quantety){
        case en_qn_single:  // do I need generate here too?
            cmd->u8_source_opcode = OC_READ_ONLY;
            cmd->status_cmd = en_st_filled;
            hi_ahbl_genrate_package(cmd);
            break;
        case en_qn_buffer:
            cmd->u8_source_opcode = OC_READ_ONLY; // not sure, if supportet... or if a start access must be provided again...
            cmd->status_cmd = en_st_filled;
            hi_ahbl_genrate_package(cmd);
            break;
        case en_qn_autoinc:
            cmd->rw_modus = en_rw_read;
            cmd->u8_source_opcode = OC_CFG_INC_ADR;
            cmd->status_cmd = en_st_filled;
            hi_ahbl_generate_exec(cmd); //! be aware of exec!
            break;
        default:
            assert(false);   // quantety of bit retrieved is not defined!
            break;
    }
    //LOG_DEBUG("[hi]: leave");
}

/**
 * @brief Generate the thierd command and consecutive commands the chain. (write)
 * 
 * @details The mechanism in the AHBL can only do a cp bit n to bit n.
 * It is not capeable of cp bit n to internal memory position m. The 
 * AHBL supports up to 32 bits.
 * 
 * @param buf Beffer containing the bytes to write. taken first four at maximum, depending on cmd->quantety
 * @param cmd Command structure to work with.
 */
void hi_ahbl_generate_wdata(HI_JC * cmd, const uint8_t * buf){
    //LOG_DEBUG("[hi]: enter");
    assert(cmd!=NULL);
    assert(buf!=NULL);
    assert(cmd->quantety != en_qn_none);
    assert(cmd->access_width!=en_aw_none);

    // todo: this stuffing /could/ be replaced by alignment_offset,
    // but should be minor advantage and makes code more inreadable

    switch(cmd->access_width){
        case en_aw08:
            cmd->pu8_buffer[0] = buf[0];
            cmd->pu8_buffer[1] = buf[0];
            cmd->pu8_buffer[2] = buf[0];
            cmd->pu8_buffer[3] = buf[0];
        break;
        case en_aw16:
            cmd->pu8_buffer[0] = buf[0];
            cmd->pu8_buffer[1] = buf[1];

            cmd->pu8_buffer[2] = buf[0];
            cmd->pu8_buffer[3] = buf[1];
        break;
        case en_aw32:
            cmd->pu8_buffer[0] = buf[0];
            cmd->pu8_buffer[1] = buf[1];
            cmd->pu8_buffer[2] = buf[2];
            cmd->pu8_buffer[3] = buf[3];
        break;
        default:
            assert(false); // acces type undefined        
    }

    uint8_t tmp_command;
    switch(cmd->quantety){
        case en_qn_single:
            tmp_command = OC_WDATA;
            break;
        case en_qn_buffer:
            tmp_command = OC_WDATA_AUTOW;
            break;
        case en_qn_autoinc:
            tmp_command = OC_WDATA_AUTOW_INC_ADR;
            break;
        default:
            tmp_command = 0x42; // to make the compiler happy!
            assert(false);   // quantety of bit retrieved is not defined!
            break;
    }

    cmd->u8_source_opcode = tmp_command;
    cmd->status_cmd = en_st_filled;
    hi_ahbl_genrate_package(cmd);
}



/**
 * @brief Generates one JTAG-Package according to the informations in cmd-struct
 * 
 * @details Package will be generated from buffer and additiional infos in Structure.
 * This package will then be pushed to the jtag queue. This queue will \b not be
 * executed in this function. So all the values asked for will be retrieved later.
 * All the values asked for are the values from the buffer.
 * 
 * @par cmd final command structure, packed and ready to produce a JTAG-Package
 * @param active_tap there is only one TAP in iol, but refering as active as a trahedition.
 */
void hi_ahbl_add_clock_jtag(HI_JC * cmd,struct jtag_tap *active_tap){
    //LOG_DEBUG("[hi]: enter");
    assert(cmd != NULL);
    assert(cmd->status_cmd == en_st_generated); // checking for reliabilety of structure

    /**
     * This "filed" is for handling the JTAG instructions.
     */
    struct scan_field field={
        .num_bits = AHBL_REGISTER_LEN,
        // those assignments will be NULL if non is set.
        .out_value = cmd->pu8_buffer,              //! host TDO --> TDI device
        .in_value =  cmd->data_host_to_device,      //! host TDI --> TDO device
        // .out_value = (uint8_t*)cmd->pu8_buffer,              //! host TDO --> TDI device
        // .in_value = (uint8_t*)cmd->data_host_to_device,      //! host TDI --> TDO device
    };

    assert( !(field.out_value == NULL && field.in_value == NULL )); // why perform a scan if no data goes in and out?!

    jtag_add_dr_scan(active_tap,
	    1,
	    &field,
	    TAP_IDLE); // other is IDLE. Only stabile states may choosen. assume, it will run over update not the short cut.
    //todo: Add here the scan chain and exchange the State TAP_IDLE to DR_PAUSE, see if compiles.
}

/**
 * Wrapper for more precise debugging log, when queue is executed and why
 */
void hi_ahbl_jtag_execute_queue(void){

    jtag_execute_queue();
}